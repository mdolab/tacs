"""
pyBase_problem
"""

# =============================================================================
# Imports
# =============================================================================
from collections import OrderedDict
from baseclasses import StructProblem as MACHStructProblem
from pygeo import DVGeometry

import numpy as np
from mpi4py import MPI

import tacs.TACS
from srjz.ffd.apply_dvs import dvDict

from examples.nonlinear_cantilever.analysis import scale


# Define decorator functions for methods that must be called before initialize
def updateDVGeo(method):
    @wraps(method)
    def wrappedMethod(self, *args, **kwargs):
        if self.DVGeo is not None:
            if not self.DVGeo.pointSetUpToDate(self.ptSetName):
                coords = self.DVGeo.update(self.ptSetName, config=self.name)
                self.setNodes(coords)
        return method(self, *args, **kwargs)

    return wrappedMethod


class StructProblem(MACHStructProblem):
    """
    Base class for TACS problem types. Contains methods common to all TACS problems.
    """

    def __init__(
        self,
        staticProblem,
        FEAAssembler,
        DVGeo=None,
    ):
        """
        Parameters
        ----------
        problem : tacs.problems.StaticProblem
            StaticProblem object used for modeling and solving static cases.

        FEAAssembler : tacs.pytacs.pyTACS
            pyTACS assembler object

        dvGeo : pygeo.DVGeometry or None
            Object responsible for manipulating the constraints that
            this object is responsible for.
        """

        self.staticProblem = staticProblem
        self.FEAAssembler = FEAAssembler
        self.DVGeo = None
        self.numGeoDV = 0
        self.ptSetName = None

        if self.staticProblem.assembler != self.FEAAssembler.assembler:
            raise RuntimeError("Provided StaticProblem does not correspond to pyTACS assembler object.")

        self.comm = self.staticProblem.comm
        if DVGeo:
            self.setDVGeo(DVGeo)

        self.update = self.FEAAssembler.createVec(asBVec=True)
        self.oldUpdate = self.FEAAssembler.createVec(asBVec=True)
        self.temp0 = self.FEAAssembler.createVec(asBVec=True)
        self.temp1 = self.FEAAssembler.createVec(asBVec=True)
        self._Fext = self.FEAAssembler.createVec(asBVec=True)
        self._phi = self.FEAAssembler.createVec(asBVec=True)
        self._pLoad = self.FEAAssembler.createVec(asBVec=True)
        self._dSdu = self.FEAAssembler.createVec(asBVec=True)
        self._adjRHS = self.FEAAssembler.createVec(asBVec=True)
        self._dIdU = self.FEAAssembler.createVec(asBVec=True)
        self._matVecRHS = self.FEAAssembler.createVec(asBVec=True)
        self._matVecSolve = self.FEAAssembler.createVec(asBVec=True)
        self.adjoints = {funcName: self.FEAAssembler.createVec(asBVec=True) for funcName in self.evalFuncs}

        self.callCounter = 0
        self.doDamp = False

    @property
    def name(self):
        return self.staticProblem.name

    @property
    def Fext(self):
        return self._Fext.getArray()

    @Fext.setter
    def Fext(self, value):
        self._Fext[:] = value

    @property
    def phi(self):
        return self._phi.getArray()

    @phi.setter
    def phi(self, value):
        self._phi[:] = value

    @property
    def pLoad(self):
        return self._pLoad.getArray()

    @phi.setter
    def phi(self, value):
        self._phi[:] = value

    @property
    def dSdu(self):
        return self._dSdu.getArray()

    @dSdu.setter
    def dSdu(self, value):
        self._dSdu[:] = value

    @property
    def adjRHS(self):
        return self._adjRHS.getArray()

    @adjRHS.setter
    def adjRHS(self, value):
        self._adjRHS[:] = value

    @property
    def dIdU(self):
        return self._dIdU.getArray()

    @dIdU.setter
    def dIdU(self, value):
        self._dIdU[:] = value

    @property
    def matVecRHS(self):
        return self._matVecRHS.getArray()

    @matVecRHS.setter
    def matVecRHS(self, value):
        self._matVecRHS[:] = value

    @property
    def matVecSolve(self):
        return self._matVecSolve.getArray()

    @matVecSolve.setter
    def matVecSolve(self, value):
        self._matVecSolve[:] = value

    def getVarName(self):
        """
        Get name for the design variables in pyOpt. Only needed
        if more than 1 pytacs object is used in an optimization

        Returns
        ----------
        varName : str
            Name of the design variables used in setDesignVars() dict.
        """
        return self.staticProblem.getVarName()

    def setDVGeo(self, DVGeo, pointSetKwargs=None):
        """
        Set the DVGeometry object that will manipulate 'geometry' in
        this object. Note that TACS doe not **strictly** need a
        DVGeometry object, but if optimization with geometric
        changes is desired, then it is required.

        Parameters
        ----------
        dvGeo : A DVGeometry object.
            Object responsible for manipulating the constraints that
            this object is responsible for.

        pointSetKwargs : dict
            Keyword arguments to be passed to the DVGeo addPointSet call.
            Useful for DVGeometryMulti, specifying FFD projection tolerances, etc

        Examples
        --------
        >>> StructProblem.setDVGeo(DVGeo)
        """
        if self.DVGeo:
            raise RuntimeError("DVGeo has already been set.")

        self.DVGeo = DVGeo
        # Get the number of geometry variables
        self.numGeoDV = self.DVGeo.getNDV()

        self.ptSetName = "tacs_%s_coords" % self.name
        coords0 = self.staticProblem.getNodes()
        self.DVGeo.addPointSet(coords0, self.ptSetName, **pointSetKwargs)

    def setDesignVars(self, x):
        """
        Set the variables in the x-dict for this object.

        Parameters
        ----------
        x : dict
            Dictionary of variables which may or may not contain the
            design variable names this object needs
        """
        self.staticProblem.setDesignVars(x)

    def addVariablesPyOpt(self, optProb):
        """
        Add the current set of variables to the optProb object.

        Parameters
        ----------
        optProb : pyOpt_optimization class
            Optimization problem definition to which variables are added
        """
        value = self.getOrigDesignVars()
        lb, ub = self.getDesignVarRange()
        scale = self.getDesignVarScales()

        dvName = self.staticProblem.getVarName()
        optProb.addVar(dvName, "c", value=value, lower=lb, upper=ub, scale=scale)

    @updateDVGeo
    def getNodes(self):
        """
        Return the mesh coordinates of this problem.

        Returns
        -------
        coords : numpy.ndarray
            Structural coordinate in array of size (N * 3) where N is
            the number of structural nodes on this processor.
        """
        return self.staticProblem.getNodes()

    @updateDVGeo
    def solve(self, damp=1.0, useAitkenAcceleration=False, dampLB=0.2, loadScale=1.0):
        """
        Solution of the static problem for current load set. The
        forces must already be set.

        Parameters
        ----------
        Fext : numpy.ndarray or tacs.TACS.Vec, optional
            Distributed array containing additional loads (ex. aerodynamic forces for aerostructural coupling)
            to applied to RHS of the static problem, by default None
        damp : float
            Value to use to damp the solution update.
        useAitkenAcceleration : bool
            Flag to use aitkenAcceleration. Only applicable for aerostructural problems
        dampLB : float
            Lower bound for the damping parameter used in Aitken acceleration.
        loadScale : float
            Value to scale external loads by. Only useful for load step approach on nonlinear problems.
        """
        # Save old load scale and update with user-defined one
        loadScale0 = self.staticProblem.getLoadScale()
        self.staticProblem.setLoadScale(loadScale)

        # Save initial state vector
        u0 = self.temp1
        self.staticProblem.getVariables(u0)

        # Solve static problem w/o damping
        successFlag = self.staticProblem.solve(Fext=self._Fext)

        # Compute undamped update
        self.staticProblem.getVariables(self.update)
        self.update.axpy(-1.0, u0)

        # Reset problem back to unsolved state
        self.staticProblem.setVariables(u0)

        # Apply Aitken Acceleration if necessary:
        if useAitkenAcceleration:
            if self.doDamp:
                # Compute: temp0 = update - old_update
                self.temp0.zeroEntries()
                self.temp0.axpy(1.0, self.update)
                self.temp0.axpy(-1.0, self.oldUpdate)
                dnom = self.temp0.dot(self.temp0)
                damp = damp * (1.0 - self.temp0.dot(self.update) / dnom)

                # Clip to a reasonable range
                damp = np.clip(damp, dampLB, 1.0)
            self.doDamp = True

        # Update State Variables
        u1 = self.temp0
        u1.copyValues(u0)
        u1.axpy(damp, self.update)
        self.staticProblem.setVariables(u1)

        # Set the old update
        self.oldUpdate.copyValues(self.update)

        # Set load scale back
        self.staticProblem.setLoadScale(loadScale0)

        return successFlag

    @updateDVGeo
    def evalFunctions(self, funcs, evalFuncs, ignoreMissing=False):
        """
        No current functions

        Parameters
        ----------
        funcs : dict
            Dictionary into which the functions are save
        evalFuncs : iterable object containing strings
            The functions that the user wants evaluated
        """

        return self.staticProblem.evalFunctions(funcs, evalFuncs, ignoreMissing)

    @updateDVGeo
    def evalFunctionsSens(self, funcsSens, evalFuncs):
        """
        Evaluate the sensitivity of the desired functions

        Parameters
        ----------
        funcsSens : dict
            Dictionary into which the function sensitivities are saved
        evalFuncs : iterable object containing strings
            The functions that the user wants evaluated
        """

        self.staticProblem.evalFunctionsSens(funcsSens, evalFuncs)

        if self.DVGeo is not None:
            coordName = self.staticProblem.getCoordName()
            for funcName in evalFuncs:
                funcKey = f"{self.name}_{funcName}"
                if coordName in funcsSens[funcKey]:
                    dIdpt = funcsSens.pop(coordName).reshape(-1,3)
                    dIdx = self.DVGeo.totalSensitivity(dIdpt, self.ptSetName, comm=self.comm, config=self.name)
                    funcsSens[funcKey].update(dIdx)

    def getVarsPerNode(self):
        """
        The number of degrees of freedom used at each output location.

        Returns
        -------
        ndof : int
            number of degrees of freedom of each node in the domain
        """
        return self.staticProblem.getVarsPerNode()

    def getNumNodes(self):
        """
        Get the number of nodes on this processor.

        Returns
        -------
        nnodes : int
            number of nodes on this processor
        """
        return self.staticProblem.getNumOwnedNodes()

    def getOrigDesignVars(self):
        """
        Get an array holding all dvs values that have been added to TACS
        """
        local_dvs = self.FEAAssembler.getOrigDesignVars()
        all_local_dvs = self.comm.allgather(local_dvs)
        global_dvs = np.concatenate(all_local_dvs)
        return global_dvs.astype(float)

    def getDesignVarRange(self):
        """Get arrays containing the lower and upper bounds for the design variables,
        in the form needed by OpenMDAO's `add_design_variable` method.

        Returns
        -------
        list of ndarray
            lower and upper bounds for the design variables
        """
        local_lb, local_ub = self.FEAAssembler.getDesignVarRange()
        all_lb = self.comm.allgather(local_lb)
        global_lbs = np.concatenate(all_lb)
        all_ub = self.comm.allgather(local_ub)
        global_ubs = np.concatenate(all_ub)
        return global_lbs.astype(float), global_ubs.astype(float)

    def getDesignVarScales(self):
        """Get an array containing the scaling factors for the design
        variables, in the form needed by OpenMDAO's `add_design_variable`
        method.

        Returns
        -------
        array
            Scaling values
        """
        return np.array(self.FEAAssembler.scaleList)

    def getNumDesignVars(self):
        """
        Get total number of structural design variables across all procs
        """
        return self.FEAAssembler.getTotalNumDesignVars()

    def getStates(self, states=None):
        """
        Return the current state values for the
        problem

        Parameters
        ----------
        states : tacs.TACS.Vec or numpy.ndarray
            Vector to place current state variables into (optional)

        Returns
        ----------
        states : numpy.ndarray
            current state vector
        """
        return self.staticProblem.getVariables(states)

    def setStates(self, states=None):
        """
        Set the current state values for the
        problem

        Parameters
        ----------
        states : tacs.TACS.Vec or numpy.ndarray
            Vector to replace current state variables with
        """
        self.staticProblem.setVariables(states)

    def getExternalForce(self, Fext=None):
        """
        Return the current external coupling force vector for the
        problem

        Parameters
        ----------
        Fext : tacs.TACS.Vec or numpy.ndarray
            Vector to place current external coupling force vector into (optional)

        Returns
        ----------
        Fext : numpy.ndarray
            current external coupling force vector
        """
        if isinstance(Fext, tacs.TACS.Vec):
            Fext.copyValues(self._Fext)
        elif isinstance(Fext, np.ndarray):
            Fext[:] = self._Fext[:]

        return self._Fext.getArray().copy()

    def setExternalForce(self, Fext):
        """
        Return the current external coupling force vector for the
        problem

        Parameters
        ----------
        Fext : tacs.TACS.Vec or numpy.ndarray
            Vector to place current external coupling force vector into (optional)
        """
        if isinstance(Fext, tacs.TACS.Vec):
            self._Fext.copyValues(Fext)
        elif isinstance(Fext, np.ndarray):
            self._Fext.getArray()[:] = Fext[:]

    def getStateNorm(self):
        u = self.temp0
        self.staticProblem.getVariables(u)
        return u.norm()

    def getResidual(self):
        """
        This routine is used to evaluate directly the structural
        residual. Only typically used with aerostructural analysis.

        Returns
        -------
        res : tacs.TACS.Vec or numpy.ndarray
            Computed residuals for problem.

        """
        res = self.temp0
        self.staticProblem.getResidual(res, Fext=self._Fext)
        resArray = res.getArray().copy()
        self.temp0.zeroEntries()
        return resArray

    def getResNorms(self):
        """Return the initial, starting and final Res Norms. Note that
        the same norms are used for both solution and adjoint
        computations"""
        initNorm = np.real(self.staticProblem.initNorm)
        startNorm = np.real(self.staticProblem.startNorm)
        finalNorm = np.real(self.staticProblem.finalNorm)

        return (initNorm, startNorm, finalNorm)

    def setResNorms(self, initNorm=None, startNorm=None, finalNorm=None):
        """Set one of these norms if not None.
        This is typically only used with aerostructural analysis
        """
        if initNorm is not None:
            self.staticProblem.initNorm = initNorm
        if startNorm is not None:
            self.staticProblem.startNorm = startNorm
        if finalNorm is not None:
            self.staticProblem.finalNorm = finalNorm

    def zeroVectors(self):
        """Zero all the tacs b-vecs"""
        self.staticProblem.zeroVariables()
        self.update.zeroEntries()

    @property
    def evalFuncs(self):
        """Get strings for added eval functions"""
        return list(self.staticProblem.getFunctionKeys())

    def getAdjoint(self, objective):
        """Return the adjoint values for objective if they
        exist. Otherwise just return zeros"""

        if objective in self.adjoints:
            vals = self.adjoints[objective].getArray().copy()
        else:
            vals = self.FEAAssembler.createVec()
        return vals

    def setAdjoint(self, adjoint, objective=None):
        """
        Set externally supplied adjoint values.

        Parameters
        ----------
        adjoint : float, numpy array
            An array of size getStateSize() to
            be copied to the structural adjoint variables
        objective : str
            Name of objective to set adjoint for
        """
        self.phi[:] = adjoint
        if objective is not None:
            if objective not in self.adjoints:
                self.adjoints[objective] = self.FEAAssembler.createVec(asBVec=True)
            self.adjoints[objective].getArray()[:] = adjoint[:]

    def getdSduVec(self, inVec):
        """Evaluate the direct residual"""
        self.phi[:] = inVec
        self.staticProblem.K.mult(self._phi, self.temp0)
        outVec = self.temp0.getArray().copy()
        self.temp0.zeroEntries()

        return outVec

    def getdSduTVec(self, inVec):
        """Evaluate the adjoint residual"""
        self.phi[:] = inVec
        self.temp0.zeroEntries()
        self.staticProblem.addTransposeJacVecProduct(self._phi, self.temp0, scale=1.0)
        outVec = self.temp0.getArray().copy()
        self.temp0.zeroEntries()

        return outVec

    def getdIdXpt(self, func):
        """Get the (partial) derivative of the structural objective
        with respect to all structural nodes for the current
        structProblem"""

        dIdXpt = self.FEAAssembler.createNodeVec()
        # Check to see if func is actually a valid TACS function
        if func in self.evalFuncs:
            self.staticProblem.addXptSens([func], [dIdXpt], scale=1.0)

        return dIdXpt

    def getdIdXdv(self, func):
        """Get the (partial) derivative of the structural objective
        wrt all structural design variables"""

        dIdXdv = self.FEAAssembler.createDesignVec()

        # Check to see if func is actually a valid TACS function
        if func in self.evalFuncs:
            self.staticProblem.addDVSens([func], [dIdXdv], scale=1.0)

        dvVal = self.comm.bcast(dIdXdv, root=0)

        return dvVal

    def getdIduTransProd(self, vecT, evalFuncs=None):
        """Perform the transpose matvec of the getdIduProd function and
        return a TACS vector. The result is stored locally.

        We assume that structProblem is set and that evalFuncs is valid."""

        # The old way (for testing purposes)
        self._matVecRHS.zeroEntries()
        svList = [self.FEAAssembler.createVec() for f in evalFuncs]
        self.staticProblem.addSVSens(evalFuncs, svList)
        for i, f in enumerate(evalFuncs):
            f_mangled = self.name+'_%s'%f
            self._matVecRHS.axpy(vecT[f_mangled][0], svList[i])

        return self.matVecRHS.copy()

    def getdIdXdvTransProd(self, vecT, evalFuncs=None):
        """Perform the transpose matvec of the getdIdXdvProd function and
        return a vector

        We assume that structProblem is set and that handles is valid.

        Note that handles contains the list of function handles, whereas the
        names are contained in evalFuncs."""

        # # The old way (for testing purposes)
        prodDV = self.FEAAssembler.createDesignVec(asBVec=True)
        for f in evalFuncs:
            f_mangled = self.name+'_%s'%f
            self.staticProblem.addDVSens([evalFuncs], [prodDV], scale=vecT[f_mangled][0])

        # Convert result back into a dictionary
        prodDict = self.convertDesignVecToDict(prodDV.getArray())

        if self.DVGeo is not None:
            prodXpt = self.FEAAssembler.createNodeVec(asBVec=True)
            for f in evalFuncs:
                f_mangled = self.name + '_%s' % f
                self.staticProblem.addXptSens([evalFuncs], [prodXpt], scale=vecT[f_mangled][0])
            xArray = prodXpt.getArray()
            xdot = self.DVGeo.totalSensitivity(xArray.reshape(-1, 3), self.ptSetName, comm=self.comm, config=self.name)
            prodDict.update(xdot)

        return prodDict

        if self.DVGeo is not None:
            prod.update(self.DVGeo.convertSensitivityToDict(np.atleast_2d(prod_arr[self.dvNum:]), out1D=True))

        return prod

    def globalNKPreCon(self, inVec):
        """This function is ONLY used as a preconditioner to the
        global Aero-Structural system"""

        # Place the in_vec into the residual vector
        res = self.temp0
        update = self.temp1
        res.getArray()[:] = inVec


        # Solve
        self.staticProblem.linearSolver.solve(res, update)
        outVec = update.getArray()

        # Restore Pointers
        res.zeroEntries()
        update.zeroEntries()

        return outVec

    def globalAdjointPreCon(self, inVec):
        """This function is ONLY used as a preconditioner to the
        global Aero-Structural adjoint system. For the linear
        symmetric case this is actually identical to the NKPreCon
        function above"""

        # Place the in_vec into the residual vector
        self.temp0.getArray()[:] = inVec[:]

        # Apply the preconditioner ONLY.
        self.staticProblem.PC.applyFactor(self.temp0, self.temp1)
        outVec = self.temp1.getArray().copy()

        # Zero values
        self.temp0.zeroEntries()
        self.temp1.zeroEntries()

        return outVec

    def globalDirectPreCon(self, inVec):
        """This function is ONLY used as a preconditioner to the
        global Aero-Structural direct system. For the linear
        symmetric case this is actually identical to the NKPreCon
        function above"""
        return self.globalAdjointPreCon(inVec)

    def assembleAdjointRHS(self):
        """Compute the final adjoint RHS:
        Adjoint RHS should be: dIdu - dAdu^T*psi - dSdu^T*phi
        """
        # Set RHS to dIdu
        self._adjRHS.copyValues(self.dIdu)

        # Subtract dSdu if non-zero
        self._adjRHS.axpy(-1.0, self._dSdu)

        # Subtract pLoad
        self._adjRHS.axpy(-1.0, self._pLoad)

        self.staticProblem.initNorm = np.real(self._adjRHS.norm())

        self.oldUpdate.zeroEntries()

    def solveAdjoint(self, damp=1.0):
        """
        Solve the structural adjoint.

        Parameters
        ----------
        damp : float
            A damping variable for adjoint update. Typically only used
            in multidisciplinary analysis
        """
        res = self.temp0
        res.zeroEntries()

        # First compute the residual
        self.staticProblem.addTransposeJacVecProduct(self._phi, res)
        res.axpy(-1.0, self._adjRHS)  # Add the -RHS

        # Starting Norm for this computation
        self.staticProblem.startNorm = np.real(res.norm())

        # Solve Linear System
        self.update.zeroEntries()
        self.staticProblem.solveAdjoint(res, self.update)

        # Update the adjoint vector with the (damped) update
        self._phi.axpy(-damp, self.update)

        # Compute actual final FEA Norm
        res.zeroEntries()
        self.staticProblem.addTransposeJacVecProduct(self._phi, res)
        res.axpy(-1.0, self._adjRHS)  # Add the RHS
        self.staticProblem.finalNorm = np.real(res.norm())

    def getdRdXptPhi(self, objectives):
        """Get the result of :math:`[dR/dX_{nodes}]^T \phi` for each of the objectives in the objective list.

        This is the total sensitivity calculation.
        """

        products = [self.FEAAssembler.createNodeVec() for obj in objectives]
        phiList = [self.getAdjoint(obj) for obj in objectives]
        self.staticProblem.addAdjointResXptSensProducts(phiList, products, scale=1.0)

        return products

    def getdRdXdvPhi(self, objectives):
        """Get the result of :math:`[dR/dX_{dv}]^T \phi` for the total sensitivity calculation."""

        products = [self.FEAAssembler.createDesignVec() for obj in objectives]
        phiList = [self.getAdjoint(obj) for obj in objectives]
        self.staticProblem.addAdjointResSensProducts(phiList, products, scale=1.0)
        dvVals = [self.comm.bcast(dvVec, root=0) for dvVec in products]

        return dvVals

    def getdRdXdvTransProd(self):
        """Perform the transpose matvec of getdRdXdvProd. The result is
        returned as a dict of numpy arrays.

        Returns
        -------
        prod : dict
        """
        dvProd = self.FEAAssembler.createDesignVec(asBVec=True)
        self.staticProblem.addAdjointResProducts([self._matVecSolve], [dvProd], scale=1.0)
        # Convert result back into a dictionary
        prodDict = self.convertDesignVecToDict(dvProd.getArray())

        if self.DVGeo is not None:
            xptProd = self.FEAAssembler.createNodeVec(asBVec=True)
            self.staticProblem.addAdjointResXptSensProducts([self._matVecSolve], [xptProd], scale=1.0)
            xArray = xptProd.getArray()
            xdot = self.DVGeo.totalSensitivity(xArray.reshape(-1, 3), self.ptSetName, comm=self.comm, config=self.name)
            prodDict.update(xdot)

        return prodDict

    def convertDesignVecToDict(self, dvVec):
        if isinstance(dvVec, tacs.TACS.Vec):
            dvVec = dvVec.getArray()

        dvVals = self.comm.bcast(dvVec, root=0)
        dvDict = {self.staticProblem.getVarName(): dvVals}

        return dvDict


    def writeSolution(self, outputDir=None, baseName=None, number=None):
        """
        This is a generic shell function that writes the output
        file(s).  The intent is that the user or calling program can
        call this function and pyTACS writes all the files that the
        user has defined. It is recommended that this function is used
        along with the associated logical flags in the options to
        determine the desired writing procedure

        Parameters
        ----------
        outputDir : str or None
            Use the supplied output directory
        baseName : str or None
            Use this supplied string for the base filename. Typically
            only used from an external solver.
        number : int or None
            Use the user supplied number to index solution. Again, only
            typically used from an external solver
        """
        self.staticProblem.writeSolution(outputDir, baseName, number)
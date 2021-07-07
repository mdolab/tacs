#ifndef TACS_DIRECTOR_H
#define TACS_DIRECTOR_H

#include "TACSElementAlgebra.h"
#include "TACSElementVerification.h"

/*
  The director class.

  Given a reference vector, t, from the element geometry, the director computes
  the exact or approximate rate of change of the displacement t.
*/
class TACSLinearizedRotation {
 public:
  static const int NUM_PARAMETERS = 3;

  /**
    Compute the rotation matrices at each node

    @param vars The full variable vector
    @param C The rotation matrices at each point
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void computeRotationMat( const TacsScalar vars[],
                                  TacsScalar C[] ){
    const TacsScalar *q = &vars[offset];
    for ( int i = 0; i < num_nodes; i++ ){
      // C = I - q^{x}
      C[0] = C[4] = C[8] = 1.0;
      setMatSkew(-1.0, q, C);

      C += 9;
      q += vars_per_node;
    }
  }

  /*
    Compute the derivative of the rotation matrices at each node

  */
  template <int vars_per_node, int offset, int num_nodes>
  static void computeRotationMatDeriv( const TacsScalar vars[],
                                       const TacsScalar varsd[],
                                       TacsScalar C[],
                                       TacsScalar Cd[] ){
    const TacsScalar *q = &vars[offset];
    const TacsScalar *qd = &varsd[offset];
    for ( int i = 0; i < num_nodes; i++ ){
      // C = I - q^{x}
      C[0] = C[4] = C[8] = 1.0;
      setMatSkew(-1.0, q, C);

      // Cd = - qd^{x}
      Cd[0] = Cd[4] = Cd[8] = 0.0;
      setMatSkew(-1.0, qd, Cd);

      C += 9;
      Cd += 9;

      q += vars_per_node;
      qd += vars_per_node;
    }
  }


  /*
    Add the residual rotation matrix
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void addRotationMatResidual( const TacsScalar vars[],
                                      const TacsScalar dC[],
                                      TacsScalar res[] ){
    TacsScalar *r = &res[offset];

    for ( int i = 0; i < num_nodes; i++ ){
      r[0] += -(dC[7] - dC[5]);
      r[1] += -(dC[2] - dC[6]);
      r[2] += -(dC[3] - dC[1]);

      r += vars_per_node;
      dC += 9;
    }
  }

  /*
    Add the
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void addRotationMatJacobian( const TacsScalar vars[],
                                      const TacsScalar d2C[],
                                      TacsScalar mat[] ){
    const int size = vars_per_node*num_nodes;
    const int csize = 9*num_nodes;

    for ( int i = 0; i < num_nodes; i++ ){
      TacsScalar *m = &mat[offset*(size + 1)];

      for ( int j = 0; j < num_nodes; j++ ){
        // r[0] += -(dC[7] - dC[5]);
        // r[1] += -(dC[2] - dC[6]);
        // r[2] += -(dC[3] - dC[1]);
        // r += vars_per_node;
        // dC += 9;

        // Add the non-zero entries
        // m[0] += d2C[];



        m += vars_per_node;

        d2C += 9;
      }

      mat += vars_per_node*size;
      d2C += 8*csize;
    }
  }

  /**
    Compute the director and rates at all nodes.

    d = Q(q)*t = (C(q)^{T} - I)*t
    ddot = d/dt(Q(q))*t

    @param vars The full variable vector
    @param dvars The first time derivative of the variables
    @param ddvars The second derivatives of the variables
    @param t The reference directions
    @param d The director values
    @param ddot The first time derivative of the director
    @param dddot The second time derivative of the director
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void computeDirectorRates( const TacsScalar vars[],
                                    const TacsScalar dvars[],
                                    const TacsScalar t[],
                                    TacsScalar d[],
                                    TacsScalar ddot[] ){
    const TacsScalar *q = &vars[offset];
    const TacsScalar *qdot = &dvars[offset];
    for ( int i = 0; i < num_nodes; i++ ){
      crossProduct(q, t, d);
      crossProduct(qdot, t, ddot);

      t += 3;
      d += 3;
      ddot += 3;

      q += vars_per_node;
      qdot += vars_per_node;
    }
  }

  /**
    Compute the director and rates at all nodes.

    d = Q(q)*t = (C(q)^{T} - I)*t
    ddot = d/dt(Q(q))*t
    dddot = d^2/dt^2(Q(q))*t

    @param vars The full variable vector
    @param dvars The first time derivative of the variables
    @param ddvars The second derivatives of the variables
    @param t The reference directions
    @param d The director values
    @param ddot The first time derivative of the director
    @param dddot The second time derivative of the director
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void computeDirectorRates( const TacsScalar vars[],
                                    const TacsScalar dvars[],
                                    const TacsScalar ddvars[],
                                    const TacsScalar t[],
                                    TacsScalar d[],
                                    TacsScalar ddot[],
                                    TacsScalar dddot[] ){
    const TacsScalar *q = &vars[offset];
    const TacsScalar *qdot = &dvars[offset];
    const TacsScalar *qddot = &ddvars[offset];
    for ( int i = 0; i < num_nodes; i++ ){
      crossProduct(q, t, d);
      crossProduct(qdot, t, ddot);
      crossProduct(qddot, t, dddot);

      t += 3;
      d += 3;
      ddot += 3;
      dddot += 3;

      q += vars_per_node;
      qdot += vars_per_node;
      qddot += vars_per_node;
    }
  }

  /**
    Compute the director and rates at all nodes and the derivative.

    d = Q(q)*t = (C(q)^{T} - I)*t
    ddot = d/dt(Q(q))*t
    dddot = d^2/dt^2(Q(q))*t

    @param vars The full variable vector
    @param dvars The first time derivative of the variables
    @param ddvars The second derivatives of the variables
    @param varsd The full variable vector derivative
    @param t The reference directions
    @param C The rotation matrices at each point
    @param d The director values
    @param ddot The first time derivative of the director
    @param dddot The second time derivative of the director
    @param Cd The derivative of the rotation matrices at each point
    @param dd The derivator of the director values
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void computeDirectorRatesDeriv( const TacsScalar vars[],
                                         const TacsScalar dvars[],
                                         const TacsScalar ddvars[],
                                         const TacsScalar varsd[],
                                         const TacsScalar t[],
                                         TacsScalar d[],
                                         TacsScalar ddot[],
                                         TacsScalar dddot[],
                                         TacsScalar dd[] ){
    const TacsScalar *q = &vars[offset];
    const TacsScalar *qdot = &dvars[offset];
    const TacsScalar *qddot = &ddvars[offset];
    const TacsScalar *qd = &varsd[offset];
    for ( int i = 0; i < num_nodes; i++ ){
      crossProduct(q, t, d);
      crossProduct(qdot, t, ddot);
      crossProduct(qddot, t, dddot);

      // Cd = - qd^{x}
      crossProduct(qd, t, dd);

      t += 3;
      d += 3;
      dd += 3;
      ddot += 3;
      dddot += 3;

      q += vars_per_node;
      qdot += vars_per_node;
      qddot += vars_per_node;
      qd += vars_per_node;
    }
  }

  /**
    Given the derivatives of the kinetic energy expression with
    respect to time, add the contributions to the derivative of the

    Given the partial derivatives of the Lagrangian with respect to the
    director and the time derivative of the vector, compute

    dTdot = d/dt(dT/d(dot{d}))
    dT = dT/d(dot{d})
    dd = -dL/dd

    In general, the residual contribution is:

    res += dTdot*d(dot{d})/d(dot{q}) + dT/d(dot{d})*d/dt(dot{d})/d(dot{q}) + dd*d(d)/d(q)

    For the linearized rotation director these expressions are:

    d = q^{x} t
    dot{d} = - t^{x} \dot{q}
    d(dot{d})/d(dot{q}) = - t^{x}
    d/dt(d(dot{d})/d(dot{q})) = 0

    @param vars The full variable vector
    @param dvars The first time derivative of the variables
    @param ddvars The second derivatives of the variables
    @param t The normal direction
    @param dTdot Time derivative of the derivative of the kinetic energy w.r.t. the director
    @param dT The derivative of the kinetic energy w.r.t. director
    @param dd The contribution from the derivative of the director
    @param res The output residual
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void addDirectorResidual( const TacsScalar vars[],
                                   const TacsScalar dvars[],
                                   const TacsScalar ddvars[],
                                   const TacsScalar t[],
                                   const TacsScalar dTdot[],
                                   const TacsScalar dT[],
                                   const TacsScalar dd[],
                                   TacsScalar res[] ){
    TacsScalar *r = &res[offset];

    for ( int i = 0; i < num_nodes; i++ ){
      crossProductAdd(1.0, t, dd, r);
      crossProductAdd(1.0, t, dTdot, r);

      r += vars_per_node;
      dd += 3;
      dTdot += 3;
      t += 3;
    }
  }

  /*
    Add terms from the Jacobian
  */
  template <int vars_per_node, int offset, int num_nodes>
  static void addDirectorJacobian( const TacsScalar vars[],
                                   const TacsScalar dvars[],
                                   const TacsScalar ddvars[],
                                   const TacsScalar t[],
                                   const TacsScalar d2d[],
                                   const TacsScalar d2du[],
                                   TacsScalar mat[] ){
    // Add the derivative due to and d2d
    const int dsize = 3*num_nodes;
    const int nvars = vars_per_node*num_nodes;

    // d = crossProduct(q, t, d)
    const TacsScalar *ti = t;
    for ( int i = 0; i < num_nodes; i++, ti += 3 ){
      TacsScalar *jac1 = &mat[(offset + vars_per_node*i)*nvars + 3];
      TacsScalar *jac2 = &mat[(offset + vars_per_node*i + 1)*nvars + 3];
      TacsScalar *jac3 = &mat[(offset + vars_per_node*i + 2)*nvars + 3];

      const TacsScalar *tj = t;
      for ( int j = 0; j < num_nodes; j++, tj += 3 ){
        // Add the derivative
        TacsScalar d[9];
        d[0] = d2d[0];
        d[1] = d2d[1];
        d[2] = d2d[2];

        d[3] = d2d[dsize];
        d[4] = d2d[dsize+1];
        d[5] = d2d[dsize+2];

        d[6] = d2d[2*dsize];
        d[7] = d2d[2*dsize+1];
        d[8] = d2d[2*dsize+2];

        TacsScalar tmp[9];
        mat3x3SkewMatSkewTransform(ti, d, tj, tmp);

        jac1[0] -= tmp[0];
        jac1[1] -= tmp[1];
        jac1[2] -= tmp[2];

        jac2[0] -= tmp[3];
        jac2[1] -= tmp[4];
        jac2[2] -= tmp[5];

        jac3[0] -= tmp[6];
        jac3[1] -= tmp[7];
        jac3[2] -= tmp[8];

        jac1 += vars_per_node;
        jac2 += vars_per_node;
        jac3 += vars_per_node;
        d2d += 3;
      }

      d2d += 2*dsize;
    }

    for ( int i = 0; i < num_nodes; i++ ){
      for ( int j = 0; j < num_nodes; j++ ){
        // Add the derivative
        TacsScalar d[9];
        d[0] = d2du[0];
        d[1] = d2du[1];
        d[2] = d2du[2];

        d[3] = d2du[dsize];
        d[4] = d2du[dsize+1];
        d[5] = d2du[dsize+2];

        d[6] = d2du[2*dsize];
        d[7] = d2du[2*dsize+1];
        d[8] = d2du[2*dsize+2];

        TacsScalar tmp[9];
        mat3x3SkewMatTransform(&t[3*i], d, tmp);

        for ( int ii = 0; ii < 3; ii++ ){
          for ( int jj = 0; jj < 3; jj++ ){
            int index =
              (vars_per_node*i + ii + offset)*nvars +
              vars_per_node*j + jj;

            mat[index] += tmp[3*ii + jj];
          }
        }

        for ( int ii = 0; ii < 3; ii++ ){
          for ( int jj = 0; jj < 3; jj++ ){
            int index =
              (vars_per_node*j + jj)*nvars +
              vars_per_node*i + ii + offset;

            mat[index] += tmp[3*ii + jj];
          }
        }

        d2du += 3;
      }

      d2du += 2*dsize;
    }
  }
};

/*
  The director class.

  Given a reference vector, t, from the element geometry, the director computes
  the exact or approximate rate of change of the displacement t.
*/

/**
  Compute the director at a point.

  d = Q(q)*t = (C(q)^{T} - I)*t

  @param q The input rotation parametrization
  @param t The reference direction
  @param d The director values
*/
// static void computeDirector( const TacsScalar q[],
//                              const TacsScalar t[],
//                              TacsScalar d[] ){
//   // Compute Q = C^{T} - I
//   TacsScalar Q[9];
//   Q[0] =-2.0*(q[2]*q[2] + q[3]*q[3]);
//   Q[1] = 2.0*(q[2]*q[1] - q[3]*q[0]);
//   Q[2] = 2.0*(q[3]*q[1] + q[2]*q[0]);

//   Q[3] = 2.0*(q[1]*q[2] + q[3]*q[0]);
//   Q[4] =-2.0*(q[1]*q[1] + q[3]*q[3]);
//   Q[5] = 2.0*(q[3]*q[2] - q[1]*q[0]);

//   Q[6] = 2.0*(q[1]*q[3] - q[2]*q[0]);
//   Q[7] = 2.0*(q[2]*q[3] + q[1]*q[0]);
//   Q[8] =-2.0*(q[1]*q[1] + q[2]*q[2]);

//   // Compute d = Q*t
//   d[0] = Q[0]*t[0] + Q[1]*t[1] + Q[2]*t[2];
//   d[1] = Q[3]*t[0] + Q[4]*t[1] + Q[5]*t[2];
//   d[2] = Q[6]*t[0] + Q[7]*t[1] + Q[8]*t[2];
// }


template <int dsize>
void TacsTestEvalDirectorEnergy( const TacsScalar Tlin[],
                                 const TacsScalar Tquad[],
                                 const TacsScalar Plin[],
                                 const TacsScalar Pquad[],
                                 const TacsScalar d[],
                                 const TacsScalar ddot[],
                                 TacsScalar *_T,
                                 TacsScalar *_P ){
  TacsScalar T = 0.0;
  TacsScalar P = 0.0;

  for ( int j = 0; j < dsize; j++ ){
    T += Tlin[j]*ddot[j];
    P += Plin[j]*d[j];
    for ( int i = 0; i < dsize; i++ ){
      T += Tquad[i + j*dsize]*ddot[i]*ddot[j];
      P += Pquad[i + j*dsize]*d[i]*d[j];
    }
  }

  *_T = T;
  *_P = P;
}

template <int dsize>
void TacsTestEvalDirectorEnergyDerivatives( const TacsScalar Tlin[],
                                            const TacsScalar Tquad[],
                                            const TacsScalar Plin[],
                                            const TacsScalar Pquad[],
                                            const TacsScalar d[],
                                            const TacsScalar ddot[],
                                            const TacsScalar dddot[],
                                            TacsScalar dTdot[],
                                            TacsScalar dT[],
                                            TacsScalar dd[] ){
  for ( int j = 0; j < dsize; j++ ){
    dT[j] = Tlin[j];
    dTdot[j] = 0.0;
    dd[j] = Plin[j];
  }

  for ( int j = 0; j < dsize; j++ ){
    for ( int i = 0; i < dsize; i++ ){
      dT[j] += Tquad[i + j*dsize]*ddot[i];
      dT[i] += Tquad[i + j*dsize]*ddot[j];

      dTdot[j] += Tquad[i + j*dsize]*dddot[i];
      dTdot[i] += Tquad[i + j*dsize]*dddot[j];

      dd[j] += Pquad[i + j*dsize]*d[i];
      dd[i] += Pquad[i + j*dsize]*d[j];
    }
  }
}

template <int vars_per_node, int offset, int num_nodes, class director>
int TacsTestDirectorResidual( double dh=1e-5,
                              int test_print_level=2,
                              double test_fail_atol=1e-5,
                              double test_fail_rtol=1e-5 ){
  const int size = vars_per_node*num_nodes;
  const int dsize = 3*num_nodes;
  const int csize = 9*num_nodes;

  // Generate random arrays for the state variables and their time derivatives
  TacsScalar vars[size], dvars[size], ddvars[size];
  TacsGenerateRandomArray(vars, size);
  TacsGenerateRandomArray(dvars, size);
  TacsGenerateRandomArray(ddvars, size);

  // Compute/normalize the normals
  TacsScalar t[dsize];
  TacsGenerateRandomArray(t, dsize);
  for ( int i = 0; i < num_nodes; i++ ){
    TacsScalar tnrm = sqrt(vec3Dot(&t[3*i], &t[3*i]));
    vec3Scale(1.0/tnrm, &t[3*i]);
  }

  // Compute the director rates
  TacsScalar d[dsize], ddot[dsize], dddot[dsize];
  director::template
    computeDirectorRates<vars_per_node, offset, num_nodes>(vars, dvars, ddvars, t, d, ddot, dddot);

  // The kinetic energy is computed as
  TacsScalar Tlin[dsize], Plin[dsize];
  TacsGenerateRandomArray(Tlin, dsize);
  TacsGenerateRandomArray(Plin, dsize);

  TacsScalar Tquad[dsize*dsize], Pquad[dsize*dsize];
  TacsGenerateRandomArray(Tquad, dsize*dsize);
  TacsGenerateRandomArray(Pquad, dsize*dsize);

  // Compute the derivatives of the kinetic and potential energies
  TacsScalar dTdot[dsize], dT[dsize], dd[dsize];
  TacsTestEvalDirectorEnergyDerivatives<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, dddot,
                                               dTdot, dT, dd);

  // Compute the residual
  TacsScalar res[size];
  memset(res, 0, size*sizeof(TacsScalar));
  director::template addDirectorResidual<vars_per_node, offset, num_nodes>(vars, dvars, ddvars, t,
                                                                           dTdot, dT, dd, res);

  // Compute the values of the variables at (t + dt)
  TacsScalar q[size], qdot[size];
  for ( int i = 0; i < size; i++ ){
    q[i] = vars[i] + dh*dvars[i];
    qdot[i] = dvars[i] + dh*ddvars[i];
  }

  // Evaluate the derivative w.r.t. dot{q}
  TacsScalar res1[size];
  for ( int i = 0; i < size; i++ ){
    // Evaluate the finite-difference for component i
    TacsScalar dqtmp = qdot[i];
#ifdef TACS_USE_COMPLEX
    TacsScalar T1, P1;
    qdot[i] = dqtmp + TacsScalar(0.0, dh);
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);
    res1[i] = TacsImagPart((T1 - P1))/dh;
#else
    TacsScalar T1, P1, T2, P2;
    qdot[i] = dqtmp + dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);

    qdot[i] = dqtmp - dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T2, &P2);
    res1[i] = 0.5*((T1 - P1) - (T2 - P2))/dh;
#endif
    qdot[i] = dqtmp;
  }

  // Compute the values of the variables at (t - dt)
  for ( int i = 0; i < size; i++ ){
    q[i] = vars[i] - dh*dvars[i];
    qdot[i] = dvars[i] - dh*ddvars[i];
  }

  // Evaluate the derivative w.r.t. dot{q}
  TacsScalar res2[size];
  for ( int i = 0; i < size; i++ ){
    // Evaluate the finite-difference for component i
    TacsScalar dqtmp = qdot[i];
#ifdef TACS_USE_COMPLEX
    TacsScalar T1, P1;
    qdot[i] = dqtmp + TacsScalar(0.0, dh);
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);
    res2[i] = TacsImagPart((T1 - P1))/dh;
#else
    TacsScalar T1, P1, T2, P2;
    qdot[i] = dqtmp + dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);

    qdot[i] = dqtmp - dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T2, &P2);
    res2[i] = 0.5*((T1 - P1) - (T2 - P2))/dh;
#endif
    qdot[i] = dqtmp;
  }

  // Evaluate the finite-difference for the first term in Largrange's
  // equations of motion
  TacsScalar fd[size];
  for ( int i = 0; i < size; i++ ){
    fd[i] = 0.5*(res1[i] - res2[i])/dh;
  }

  // Reset the values of q and dq at time t
  for ( int i = 0; i < size; i++ ){
    q[i] = vars[i];
    qdot[i] = dvars[i];
  }

  // Compute the contribution from dL/dq^{T}
  for ( int i = 0; i < size; i++ ){
    // Evaluate the finite-difference for component i
    TacsScalar qtmp = q[i];

#ifdef TACS_USE_COMPLEX
    TacsScalar T1, P1;
    q[i] = qtmp + TacsScalar(0.0, dh);
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);
    res1[i] = TacsImagPart((T1 - P1))/dh;
#else
    TacsScalar T1, P1, T2, P2;
    q[i] = qtmp + dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T1, &P1);

    q[i] = qtmp - dh;
    director::template computeDirectorRates<vars_per_node, offset, num_nodes>(q, qdot, t, d, ddot);
    TacsTestEvalDirectorEnergy<dsize>(Tlin, Tquad, Plin, Pquad, d, ddot, &T2, &P2);

    // Compute and store the approximation
    res1[i] = 0.5*((T1 - P1) - (T2 - P2))/dh;
#endif
    q[i] = qtmp;
  }

  // Add the result to the finite-difference result
  for ( int i = 0; i < size; i++ ){
    fd[i] -= res1[i];
  }

  // Variables to store the max error and indices
  int max_err_index, max_rel_index;
  double max_err, max_rel;

  // Keep track of the failure flag
  int fail = 0;

  // Compute the error
  max_err = TacsGetMaxError(res, fd, size, &max_err_index);
  max_rel = TacsGetMaxRelError(res, fd, size, &max_rel_index);

  if (test_print_level > 0){
    fprintf(stderr, "Testing the residual implementation\n");
    fprintf(stderr, "Max Err: %10.4e in component %d.\n",
            max_err, max_err_index);
    fprintf(stderr, "Max REr: %10.4e in component %d.\n",
            max_rel, max_rel_index);
  }
  // Print the error if required
  if (test_print_level > 1){
    TacsPrintErrorComponents(stderr, "res", res, fd, size);
  }
  if (test_print_level){ fprintf(stderr, "\n"); }

  fail = (max_err > test_fail_atol || max_rel > test_fail_rtol);

  return fail;
}

template <int vars_per_node, int offset, int num_nodes, class director>
int TacsTestDirector( double dh=1e-7,
                      int test_print_level=2,
                      double test_fail_atol=1e-5,
                      double test_fail_rtol=1e-5 ){
  const int size = vars_per_node*num_nodes;
  const int dsize = 3*num_nodes;
  const int csize = 9*num_nodes;

  // Generate random arrays for the state variables and their time derivatives
  TacsScalar vars[size], dvars[size], ddvars[size];
  TacsGenerateRandomArray(vars, size);
  TacsGenerateRandomArray(dvars, size);
  TacsGenerateRandomArray(ddvars, size);

  // Compute the rotation matrices
  TacsScalar C[csize];
  director::template computeRotationMat<vars_per_node, offset, num_nodes>(vars, C);

  // Create a random array
  TacsScalar dC[csize], d2C[csize*csize];
  TacsGenerateRandomArray(dC, csize);
  TacsGenerateRandomArray(d2C, csize*csize);
  for ( int i = 0; i < csize; i++ ){
    for ( int j = 0; j < i; j++ ){
      d2C[j + i*csize] = d2C[i + j*csize];
    }
  }

  // Compute the residual
  TacsScalar res[size];
  memset(res, 0, size*sizeof(TacsScalar));
  director::template addRotationMatResidual<vars_per_node, offset, num_nodes>(vars, dC, res);

  TacsScalar mat[size*size];
  memset(mat, 0, size*size*sizeof(TacsScalar));
  director::template addRotationMatJacobian<vars_per_node, offset, num_nodes>(vars, d2C, mat);

  //
  TacsScalar fdmat[size*size];
  for ( int k = 0; k < size; k++ ){
    TacsScalar varst[size];
    memcpy(varst, vars, size*sizeof(TacsScalar));

#ifdef TACS_USE_COMPLEX
    varst[k] = vars[k] + TacsScalar(0.0, dh);
#else
    varst[k] = vars[k] + dh;
#endif // TACS_USE_COMPLEX

    TacsScalar Ct[csize];
    director::template computeRotationMat<vars_per_node, offset, num_nodes>(varst, Ct);

    // Add the contributions from the
    TacsScalar dCt[csize];
    for ( int i = 0; i < csize; i++ ){
      dCt[i] = dC[i];

      for ( int j = 0; j < csize; j++ ){
        dCt[i] += d2C[j + i*csize]*(Ct[j] - C[j]);
      }
    }

    TacsScalar rest[size];
    memset(rest, 0, size*sizeof(TacsScalar));
    director::template addRotationMatResidual<vars_per_node, offset, num_nodes>(varst, dCt, rest);

    for ( int j = 0; j < size; j++ ){
#ifdef TACS_USE_COMPLEX
      fdmat[k + size*j] = TacsImagPart(rest[j])/dh;
#else
      fdmat[k + size*j] = (rest[j] - res[j])/dh;
#endif // TACS_USE_COMPLEX
    }
  }

  // Variables to store the max error and indices
  int max_err_index, max_rel_index;
  double max_err, max_rel;

  // Keep track of the failure flag
  int fail = 0;

  // Compute the error
  max_err = TacsGetMaxError(mat, fdmat, size*size, &max_err_index);
  max_rel = TacsGetMaxRelError(mat, fdmat, size*size, &max_rel_index);

  if (test_print_level > 0){
    fprintf(stderr, "Testing the derivative of the rotation matrix w.r.t. vars\n");
    fprintf(stderr, "Max Err: %10.4e in component %d.\n",
            max_err, max_err_index);
    fprintf(stderr, "Max REr: %10.4e in component %d.\n",
            max_rel, max_rel_index);
  }
  // Print the error if required
  if (test_print_level > 1){
    TacsPrintErrorComponents(stderr, "mat", mat, fdmat, size*size);
  }
  if (test_print_level){ fprintf(stderr, "\n"); }

  fail = (max_err > test_fail_atol || max_rel > test_fail_rtol);

  return fail;
}

#endif // TACS_DIRECTOR_H

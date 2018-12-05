#ifndef TACS_PLANE_STRESS_COUPLED_THERMO_MULTI_QUAD_H
#define TACS_PLANE_STRESS_COUPLED_THERMO_MULTI_QUAD_H

/*
  Plane stress element with coupled thermoelastic analysis
  
  Copyright (c) 2010-2015 Graeme Kennedy. All rights reserved. 
  Not for commercial purposes.

  The following code uses templates to allow for arbitrary order elements.
*/
#include "TACS2DCoupledThermoElement.h"
#include "FElibrary.h"
#include "TACSElement.h"

template <int order>
class PlaneStressCoupledThermoQuad : public TACS2DCoupledThermoElement<order*order>{
 public:
  PlaneStressCoupledThermoQuad( CoupledThermoPlaneStressStiffness *_stiff, 
                                ElementBehaviorType type=LINEAR, 
                                int _componentNum=0,
                                int _use_lobatto_quadrature=0 );
  ~PlaneStressCoupledThermoQuad();

  // Return the name of this element
  // -------------------------------
  const char *elementName(){ return elemName; }

  // Retrieve the shape functions
  // ----------------------------
  void getShapeFunctions( const double pt[], double N[] );
  void getShapeFunctions( const double pt[], double N[],
			  double Na[], double Nb[] );

  // Retrieve the Gauss points/weights
  // ---------------------------------
  int getNumGaussPts();
  double getGaussWtsPts( const int num, double pt[] ); 

  // Functions for post-processing
  // -----------------------------
  void addOutputCount( int *nelems, int *nnodes, int *ncsr );
  void getOutputData( unsigned int out_type, 
		      double *data, int ld_data, 
		      const TacsScalar Xpts[],
		      const TacsScalar vars[] );
  void getOutputConnectivity( int *con, int node );

 private:
  static const int NUM_NODES = order*order;

  // The Gauss quadrature scheme
  int numGauss;
  const double *gaussWts, *gaussPts;

  // Store the name of the element
  static const char *elemName;
};

template <int order>
PlaneStressCoupledThermoQuad<order>::PlaneStressCoupledThermoQuad( CoupledThermoPlaneStressStiffness *_stiff, 
                                                     ElementBehaviorType type, 
                                                     int _componentNum,
                                                     int use_lobatto_quadrature ):
TACS2DCoupledThermoElement<order*order>(_stiff, type, _componentNum){
  if (use_lobatto_quadrature){

  }
  else {
    numGauss = FElibrary::getGaussPtsWts(order, &gaussPts, &gaussWts);
  }
}

template <int order>
PlaneStressCoupledThermoQuad<order>::~PlaneStressCoupledThermoQuad(){}

template <int order>
const char *PlaneStressCoupledThermoQuad<order>::elemName = "PlaneStressCoupledThermoQuad";
/*
  Get the number of Gauss points in the Gauss quadrature scheme
*/
template <int order>
int PlaneStressCoupledThermoQuad<order>::getNumGaussPts(){
  return numGauss*numGauss;
}

/*
  Get the Gauss points
*/
template <int order>
double PlaneStressCoupledThermoQuad<order>::getGaussWtsPts( int npoint, double pt[] ){
  // Compute the n/m/p indices of the Gauss quadrature scheme
  int m = (int)((npoint)/(numGauss));
  int n = npoint - numGauss*m;
  
  pt[0] = gaussPts[n];
  pt[1] = gaussPts[m];
  
  return gaussWts[n]*gaussWts[m];
}

/*
  Evaluate the shape functions and their derivatives
*/
template <int order>
void PlaneStressCoupledThermoQuad<order>::getShapeFunctions( const double pt[], 
                                                             double N[] ){
  FElibrary::biLagrangeSF(N, pt, order);
}

/*
  Compute the shape functions and their derivatives w.r.t. the
  parametric element location 
*/
template <int order>
void PlaneStressCoupledThermoQuad<order>::getShapeFunctions( const double pt[], double N[],
                                                             double Na[], double Nb[] ){
  FElibrary::biLagrangeSF(N, Na, Nb, pt, order);
}

/*
  Get the number of elemens/nodes and CSR size of the contributed by
  this element.  
*/
template <int order>
void PlaneStressCoupledThermoQuad<order>::addOutputCount( int *nelems, 
                                                          int *nnodes, int *ncsr ){
  *nelems += (order-1)*(order-1);
  *nnodes += order*order;
  *ncsr += 4*(order-1)*(order-1);
}

/*
  Get the output data from this element and place it in a real
  array for visualization later. The values generated for visualization
  are determined by a bit-wise selection variable 'out_type' which is 
  can be used to simultaneously write out different data. Note that this
  is why the bitwise operation & is used below. 

  The output may consist of the following:
  - the nodal locations
  - the displacements and rotations
  - the strains or strains within the element
  - extra variables that are used for optimization
  
  output:
  data:     the data to write to the file (eventually)

  input:
  out_type: the bit-wise variable used to specify what data to generate
  vars:     the element variables
  Xpts:     the element nodal locations
*/
template <int order>
void PlaneStressCoupledThermoQuad<order>::getOutputData( unsigned int out_type, 
                                                         double *data, int ld_data,
                                                         const TacsScalar Xpts[],
                                                         const TacsScalar vars[] ){
  
  for ( int m = 0; m < order; m++ ){
    for ( int n = 0; n < order; n++ ){
      int p = n + order*m;
      int index = 0;
      // Set the parametric point to extract the data
      double pt[2];
      pt[0] = -1.0 + 2.0*n/(order - 1.0);
      pt[1] = -1.0 + 2.0*m/(order - 1.0);
      // Compute the shape functions
      double N[NUM_NODES];
      double Na[NUM_NODES], Nb[NUM_NODES];
      getShapeFunctions(pt, N, Na, Nb);

      if (out_type & TACSElement::OUTPUT_NODES){
	for ( int k = 0; k < 3; k++ ){
          TacsScalar X = 0.0;
          for (int i = 0; i < NUM_NODES; i++){
            X += N[i]*Xpts[3*p+k];
          }
          data[index+k] = TacsRealPart(X);	 
	}
        index += 3;
      }
      if (out_type & TACSElement::OUTPUT_DISPLACEMENTS){
	for ( int k = 0; k < 3; k++ ){
          TacsScalar u = 0.0;
          for (int i = 0; i < NUM_NODES; i++){
            u += N[i]*vars[3*p+k];
          }
          data[index+k] = TacsRealPart(u);
	}
        index += 3;
      }
      
      // Compute the derivative of X with respect to the
      // coordinate directions
      TacsScalar X[3], Xa[4];
      this->planeJacobian(X, Xa, N, Na, Nb, Xpts);
      
      // Compute the determinant of Xa and the transformation
      TacsScalar J[4];
      FElibrary::jacobian2d(Xa, J);

      // Compute the strain
      TacsScalar strain[3];
      this->evalStrain(strain, J, Na, Nb, vars);
      if (out_type & TACSElement::OUTPUT_STRAINS){
        for ( int k = 0; k < 3; k++ ){
          data[index+k] = TacsRealPart(strain[k]);
        }
        index += 3;
      }
      if (out_type & TACSElement::OUTPUT_STRESSES){
        // Calculate the effective stress at the current point
        TacsScalar stress[3];//,estrain[3];
        // ---------------------------------------------------------
        // ---------------------------------------------------------
        //TacsScalar aT = this->stiff->getThermalAlpha();
        TacsScalar T = 0.0;
        for ( int k = 0; k < NUM_NODES; k++){
          T += N[k]*vars[3*k+2];
        }
        
        /* // Effective strain */
        /* estrain[0] = strain[0]-aT*T; */
        /* estrain[1] = strain[1]-aT*T; */
        /* estrain[2] = strain[2]; */
        
        this->stiff->calculateStress(pt, strain, stress);
        /* stress[0] = estrain[0]; */
        /* stress[1] = estrain[1]; */
        /* stress[2] = estrain[2]; */
        // ---------------------------------------------------------
        // ---------------------------------------------------------
        for ( int k = 0; k < 3; k++ ){
          data[index+k] = TacsRealPart(stress[k]);
        }
        index += 3;
      }      
      if (out_type & TACSElement::OUTPUT_EXTRAS){
        // Get the temperature
        // Compute the failure value
	TacsScalar lambda = 0.0;
        data[index] = TacsRealPart(lambda);

	this->stiff->buckling(strain, &lambda);
	data[index+1] = TacsRealPart(lambda);
        
        data[index+2] = TacsRealPart(this->stiff->getDVOutputValue(0, pt));
	data[index+3] = TacsRealPart(this->stiff->getDVOutputValue(1, pt));
        
        index += this->NUM_EXTRAS;
      }

      data += ld_data;
    }
  }
}

/*
  Get the element connectivity for visualization purposes. Since each
  element consists of a series of sub-elements used for visualization,
  we also need the connectivity of these visualization elements.

  output:
  con:  the connectivity of the local visualization elements contributed
  by this finite-element

  input:
  node:  the node offset number - so that this connectivity is more or 
  less global
*/
template <int order>
void PlaneStressCoupledThermoQuad<order>::getOutputConnectivity( int *con, int node ){
  int p = 0;
  for ( int m = 0; m < order-1; m++ ){
    for ( int n = 0; n < order-1; n++ ){
      con[4*p]   = node + n   + m*order;
      con[4*p+1] = node + n+1 + m*order; 
      con[4*p+2] = node + n+1 + (m+1)*order;
      con[4*p+3] = node + n   + (m+1)*order;
      p++;
    }
  }
}

#endif // TACS_PLANE_STRESS_THERMO_QUAD_H

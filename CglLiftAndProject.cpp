// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WARNING: work-in-progress. Not functional.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <cfloat>
#include <iostream>

#include "CoinHelperFunctions.hpp"
#include "CglLiftAndProject.hpp"
#include "OsiPackedVector.hpp"
#include "CoinSort.hpp"
#include "OsiPackedMatrix.hpp"

//-----------------------------------------------------------------------------
// Generate knapsack cover cuts
//------------------------------------------------------------------- 
void CglLiftAndProject::generateCuts(const OsiSolverInterface & si, 
						OsiCuts & cs ) const
{
  // Assumes the mixed 0-1 problem 
  //
  //   min {cx: <Atilde,x> >= btilde} 
  //
  // is in cannonical form with all bounds,
  // including x_t>=0, -x_t>=-1 for x_t binary,
  // explicitly stated in the constraint matrix. 
  // See ~/COIN/Examples/Cgl2/cgl2.cpp 
  // for a general purpose "convert" function. 

  // Reference [BCC]: Balas, Ceria, and Corneujols,
  // "A lift-and-project cutting plane algorithm
  // for mixed 0-1 program", Math Prog 58, (1993) 
  // 295-324.

  // This implementation uses Normalization 1.

  // Given cannonical problem and
  // the lp-relaxation solution, x,
  // the LAP cut generator attempts to construct
  // a cut for every x_j such that 0<x_j<1
  // [BCC:307]
 

  // x_j is the strictly fractional binary variable
  // the cut is generated from
  int j = 0; 
  int jSpot1, jSpot2;// position of x_j in BIndices 

  // Get basic problem information
  // let Atilde be an m by n matrix
  const int m = si.getNumRows(); 
  const int n = si.getNumCols(); 
  const double * x = si.getColSolution();

  // Remember - Atildes may have gaps..
  const OsiPackedMatrix * Atilde = si.getMatrixByRow();
  const double * AtildeElements =  Atilde->getElements();
  const int * AtildeIndices =  Atilde->getIndices();
  const int * AtildeStarts = Atilde->getVectorStarts();
  const int * AtildeLengths = Atilde->getVectorLengths();  
  const int AtildeFullSize = AtildeStarts[m+1];
  const double * btilde = si.getRowLower();

  // Set up memory for system (10) [BCC:307]
  // (the problem over the polar cone)
  // 
  // min <<x^T,Atilde^T>,u> + x_ju_0
  // s.t.
  //     <B,w> = 0
  //        w   >= 0
  // where 
  // w = (u,v,beta,v_0,u_0)in BCC notation and
  //  
  // B = Atilde^T  -Atilde^T  e_0 -e_j e_j
  //     btilde^T   e_0^T     -1   0   0
  //     e_0^T      btilde^T  -1   1   0

  // ^T indicates Transpose
  // e_0 is a (AtildeNCols x 1) vector of all zeros 
  // e_j is e_0 with a 1 in the jth position

  // Storing B in column order. B is a (n+2 x 2m+3) matrix 
  // But need to allow for possible gaps in Atilde.
  // At each iteration, only need to change 2 cols and objfunc
  // Sane design of OsiSolverInterface does not permit mucking
  // with matrix.
  // Becuase we must delete and add cols to alter matrix,
  // and we can only add columns on the end of the matrix
  // put the v_0 and u_0 columns on the end.
  // rather than as described in [BCC]
 
  // Initially allocating B with space for v_0 and u_O cols
  // but not populating.

  int twoM = 2*m;
  int BNumRows = n+2;
  int BNumCols = twoM+3;
  int BFullSize = 2*AtildeFullSize+twoM+5;
  double * BElements = new double[BFullSize];
  int * BIndices = new int[BFullSize];
  int * BStarts = new int[BNumCols+1];
  int * BLengths = new int[BNumCols];


  int i, ij, k=0;
  int nPlus1=n+1;
  int nPlus2=n+2;
  int offset = AtildeStarts[m]+m;
  for (i=0; i<m; i++){
    for (ij=AtildeStarts[i];ij<AtildeStarts[i]+AtildeLengths[i];ij++){
      BElements[k]=AtildeElements[ij];
      BElements[k+offset]=-AtildeElements[ij];
      BIndices[k]= AtildeIndices[ij];
      BIndices[k+offset]= AtildeIndices[ij];
	
      k++;
    }
    BElements[k]=btilde[i];
    BElements[k+offset]=btilde[i];
    BIndices[k]=nPlus1;
    BIndices[k+offset]=nPlus2;
    BStarts[i]= AtildeStarts[i]+i;
    BStarts[i+m]=offset+BStarts[i];// = AtildeStarts[m]+m+AtildeStarts[i]+i
    BLengths[i]= AtildeLengths[i]+1;
    BLengths[i+m]= AtildeLengths[i]+1;
    k++;
  }
  i=twoM;

  ////// Commented out b/c rearranging u_0 and v_0 vectors.
  // Store column corresponding to u_0
  // BElements[k]=1;
  // BIndices[k]=j;
  // k++;
  // BStarts[i]=BStarts[i-1]+AtildeLengths[m-1];
  // BLengths[i]= 1;
  // i++;

  // Store column corresponding to v_0
  // BElements[k]=-1;
  // BIndices[k]=j;
  // k++;
  // BElements[k]=1;
  // BIndices[k]=nPlus2;
  // k++
  // BStarts[i]=BStarts[i-1]+1;
  // BLengths[i]= 2;
  // i++;

  // Store column coresponding to beta
  BElements[k]=-1;
  BIndices[k]=nPlus1;
  k++;
  BElements[k]=-1;
  BIndices[k]=nPlus2;
  k++;
  BStarts[i]=BStarts[i-1]+AtildeLengths[m-1]; // only line that change w/rearr.
  BLengths[i]= 2;
  i++;

  // Mark end of BStarts
  BStarts[i]=BStarts[i-1]+2;

  // Set lower bound on u and v
  // u_0, v_0 >= 0 is implied but this is easier
  const double INFINITY = si.getInfinity();
  double * BColLowers = new double[BNumCols];
  double * BColUppers = new double[BNumCols];
  CoinFillN(BColLowers,BNumCols,0.0);  
  CoinFillN(BColUppers,BNumCols,INFINITY);  

  // Set row lowers and uppers to zero
  double * BRowLowers = new double[BNumRows];
  double * BRowUppers = new double[BNumRows];
  CoinFillN(BRowLowers,BNumCols,0.0);  
  CoinFillN(BRowUppers,BNumCols,0.0);  

  // Calculate base objective <<x^T,Atilde^T>,u>
  // at each iteration coefficient u_0
  // changes to <x^T,e_j>
  double * BObjective= new double[BNumCols];
  CoinFillN(BObjective,BNumCols,0.0);  

  // Number of cols and size of Elements vector
  // in B without v_0 and u_0 cols
  int BNumColsLessTwo = BNumCols-2;
  int BFullSizeLessThree = BFullSize-3;

  // Load B matrix into a column orders OsiPackedMatrix
  // Need to change to values in BIndices each iterations - ask LL.
  OsiPackedMatrix * BMatrix = new OsiPackedMatrix(true, BNumRows,
						  BNumColsLessTwo, 
						  BFullSizeLessThree,
						  BElements,BIndices, 
						  BStarts,BLengths);
  // Load problem into a solver interface 
  // ? Maybe better to assign problem rather than load ?
  OsiSolverInterface * coneSi = si.clone(false);
  coneSi->loadProblem (*BMatrix, BColLowers, BColUppers, 
		      BObjective,
		      BRowLowers, BRowUppers);

  // Problem sense should defalut to "min" by default, but ...
  coneSi->setObjSense(1.0);


  // B set up with out v_0 and u_0 columns
  //   but with empty memory allocated.
  // Calculate base objective
  // bounded = false;
  // int j=0;
  // while (!bounded){
  //   bool found = findFractionalBinary(si,x,&j,n);
  //                                  // starts looking at position j
  //                                  // and increments over all variables
  //                                  // returns new j & true, 
  //                                  // else returns n & false
  //   if (! found), return {clean up memory}
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   set objective[u_0]=x_j;
  //   solve min{objw:Bw=0, w>=0}
  //   if unbounded, continue;
  //   bounded = true;
  //   get warmstart;
  //   caculate and add cuts;
  //   reset objective[u_0] // maynot need (?)
  //   delete col for u_0   // what hapens to obj when col deleted?
  //   delete col for v_0
  // }
  // while(true) {
  //   bool found = findFractionalBinary(si,x,&j,n);
  //   if (! found), return {clean up memory} 
  //   (IMPROVEME:use warmstart info to see if j attractive) 
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   set objective[u_0]=x_j;
  //   set warmstart information
  //   solve min{objw:Bw=0, w>=0}
  //   if (bounded)unbounded
  //     get warmstart information
  //     caculate and add cuts;
  //   reset objective[u_0] // may not need
  //   delete col for u_0   // what happen to obj when col deleted?
  //   delete col for v_0
  // }
  // clean up memory
  // return 0;

  // B without u_0 and v_0, assign problem
  // Calculate base objective <<x^T,Atilde^T>,u>
  // bool haveWarmStart = false;
  // For (j=0; j<n, j++)
  //   if (!isBinary(x_j) || x_j<=0 || x_j>=1) continue
  //   // IMPROVEME: if(haveWarmStart) check if j attractive
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   objective[u_0] = u_0x_j 
  //   if (haveWarmStart) 
  //      set warmstart info
  //   solve min{objw:Bw=0, w>=0}
  //   if (bounded)
  //      get warmstart info
  //      haveWarmStart=true;
  //      ustar = optimal u solution
  //      ustar_0 = optimal u_o solution
  //      alpha^T= <ustar^T,Atilde> -ustar_0e_j^T
  //      (double check <alpha^T,x> >= beta_ should be violated)
  //      add <alpha^T,x> >= beta_ to cutset 
  //   endif
  //   reset objective[u_0] // may not need
  //   delete col for u_0   // what happen to obj when col deleted?
  //   delete col for v_0
  // endFor
  // clean up memory
  // return 0;

  bool equalObj1, equalObj2;
  OsiRelFltEq eq;

  for (j=0;j<n;j++){
    if (!si.isBinary(j)) continue; // better to ask coneSi?
    equalObj1=eq(x[j],0);
    equalObj2=eq(x[j],1);
    if (equalObj1 || equalObj2) continue;

    // Would really be nice if I could set 
    // all objective coefficients in one shot...
    // 
    // calculate <<x^T,Atilde^T>,u>  = <Atilde, x
    // add u_0x_j    
    //    coneSi->setObjCoeff( int elementIndex, double elementValue );


    // cant do because don't know how the solver interface repres the matrix
    // const_cast<int*> (coneSi->getIndices());
 


  }







  // ---------JUNK ---------------------------
  // - krow will contain the coefficients and indices of the 
  // (potentially complemented) variables in the knapsack inequality.
  // - b is the rhs of knapsack inequality.
  // - complement[i] is 1 if the index i in krow refers to the complement
  // of the variable, and 0 otherwise. 
  {
  int nCols = si.getNumCols();
  int nRows = si.getNumRows();

  OsiPackedVector krow; 
  double b=0.0;
  int * complement= new int[nCols];
    
  // Create a local copy of the column solution (colsol), call it xstar, and
  // inititalize it. 
  // Assumes the lp-relaxation has been solved, and the solver interface
  // has a meaningful colsol.
  double * xstar= new double[nCols]; 
  const double *colsol = si.getColSolution(); 
  int k; // dummy iterator variable
  for (k=0; k<nCols; k++){
    xstar[k]=colsol[k];
    complement[k]=0;
  }
  
  // Main loop
  int rowIndex;
  for (rowIndex=0; rowIndex<nRows; rowIndex++){

#ifdef DEBUG
    cout << "Processing row " << rowIndex << endl;
#endif
    
    // Get a tight row 
    // (want to be able to 
    //  experiment by turning this on and off)
    //
    // const double * pi=si.rowprice(); 
    // if (fabs(pi[row]) < epsilon_){
    //  continue;
    // }
    
    // Skip rows with 1 or 2 vars
    // ToDo: 2 -> lprelax of exact knapsack cover
    // if (si.getMatrixByRow()->vectorSize(rowIndex)<3){
    if (si.getMatrixByRow()->getVectorSize(rowIndex)<2){
      continue;
    }
    
    //////////////////////////////////////////////////////
    // Derive a "cannonical"  knapsack                  //
    // inequality (in binary variables)                 //
    // from the model row in mixed integer variables    //
    //////////////////////////////////////////////////////
    
    if (!deriveAKnapsack(si, cs, krow, b, complement, 
			    xstar, rowIndex, 
			    si.getMatrixByRow()->getVector(rowIndex))){
      
      // Reset local data and continue to the next iteration 
      // of the rowIndex-loop
      for(k=0; k<krow.getNumElements(); k++) {
        if (complement[krow.getIndices()[k]]){
          xstar[krow.getIndices()[k]]= 1.0-xstar[krow.getIndices()[k]];
          complement[krow.getIndices()[k]]=0;        
        }
      }
      krow.setVector(0,NULL,NULL);
      continue;
    }

    
    //////////////////////////////////////////////////////
    // Look for a series of                             //
    // different types of minimal covers.               //
    // If a minimal cover is found,                     //
    // lift the associated minimal cover inequality,    //
    // uncomplement the vars                            //
    // and add it to the cut set.                       //
    // After the last type of cover is tried,           //
    // restore xstar values                             //
    //////////////////////////////////////////////////////
    
    //////////////////////////////////////////////////////
    // Try to generate a violated                       //
    // minimal cover greedily from fractional vars      //
    //////////////////////////////////////////////////////

    OsiPackedVector cover, remainder;  

    if (findGreedyCover(rowIndex, krow, b, xstar, cover, remainder) == 1){

      // Lift cover inequality and add to cut set 
      liftAndUncomplementAndAdd(si.getRowUpper()[rowIndex], krow, b,
				complement, rowIndex, cover, remainder, cs);   
    }  

    //////////////////////////////////////////////////////
    // Try to generate a violated                       //
    // minimal cover using pseudo John and Ellis logic  //
    //////////////////////////////////////////////////////

    // Reset the cover and remainder
    cover.setVector(0,NULL,NULL);
    remainder.setVector(0,NULL,NULL);

    if (findPseudoJohnAndEllisCover(rowIndex, krow, b,
				    xstar, cover, remainder) == 1){

      // (Sequence Independent) Lift cover inequality and add to cut set 
      liftAndUncomplementAndAdd(si.getRowUpper()[rowIndex], krow, b,
				complement, rowIndex, cover, remainder, cs);   

      // experimenting here...
      // (Sequence Dependent) Lift cover inequality and add to cut set
      seqLiftAndUncomplementAndAdd(nCols, xstar, complement, rowIndex,
				   krow.getNumElements(), b, cover, remainder,
				   cs);

    }  

    //////////////////////////////////////////////////////
    // Try to generate cuts using covers of unsat       //
    // vars on reduced krows with John and Ellis logic  //
    //////////////////////////////////////////////////////
    OsiPackedVector atOnes;
    OsiPackedVector fracCover; // different than cover

    // reset the remainder
    remainder.setVector(0,NULL,NULL);

    if (findJohnAndEllisCover(rowIndex, krow, b,
			      xstar, fracCover, atOnes, remainder) == 1){

      // experimenting here...
      // Sequence Dependent Lifting up on remainders and lifting down on the
      // atOnes 
      liftUpDownAndUncomplementAndAdd(nCols, xstar, complement, rowIndex,
				      krow.getNumElements(), b, fracCover,
				      atOnes, remainder, cs);
    }  

    //////////////////////////////////////////////////////
    // Try to generate a violated                       //
    // minimal cover by considering the                 //
    // most violated cover problem                      //
    //////////////////////////////////////////////////////

    // reset cover and remainder
    cover.setVector(0,NULL,NULL);
    remainder.setVector(0,NULL,NULL);

    // if the size of the krow is "small", 
    //    use an exact algorithm to find the most violated (minimal) cover, 
    // else, 
    //    use an lp-relaxation to find the most violated (minimal) cover.
    if(krow.getNumElements()<=15){
      if (findExactMostViolatedMinCover(nCols, rowIndex, krow, b,
					xstar, cover, remainder) == 1){
        
        // Lift cover inequality and add to cut set 
        liftAndUncomplementAndAdd(si.getRowUpper()[rowIndex], krow, b,
				  complement, rowIndex, cover, remainder, cs);
      }
    } 
    else {
      if (findLPMostViolatedMinCover(nCols, rowIndex, krow, b,
				     xstar, cover, remainder) == 1){
        
        // Lift cover inequality and add to cut set 
	liftAndUncomplementAndAdd(si.getRowUpper()[rowIndex], krow, b,
				  complement, rowIndex, cover, remainder, cs);
      }
    } 



    // Reset xstar and complement to their initialized values for the next
    // go-around 
    const double * rowupper = si.getRowUpper(); 
    int k;
    if (fabs(b-rowupper[rowIndex]) > epsilon_) {
      for(k=0; k<krow.getNumElements(); k++) {
        if (complement[krow.getIndices()[k]]){
          xstar[krow.getIndices()[k]]= 1.0-xstar[krow.getIndices()[k]];
          complement[krow.getIndices()[k]]=0;
        }
      }
    }
    krow.setVector(0,NULL,NULL);
  }
  
  // Clean up: free allocated memory
  delete[] xstar;
  delete[] complement;
 
}
}

//------------------------------------------------------------- 
// Lift and uncomplement cut. Add cut to the cutset
//-------------------------------------------------------------------
int 
CglLiftAndProject::liftAndUncomplementAndAdd(
         double rowub,
         OsiPackedVector & krow,
         double & b,
         int * complement,
         int row,
         OsiPackedVector & cover,
         OsiPackedVector & remainder,
         OsiCuts & cs ) const
{
  OsiPackedVector cut;
  double cutRhs = cover.getNumElements() - 1.0;
  
  if (remainder.getNumElements() > 0){
    // Construct lifted cover cut 
    liftCoverCut( 
      b, krow.getNumElements(), 
      cover, remainder,
      cut );
  }
  // The cover consists of every variable in the knapsack.
  // There is nothing to lift, so just add cut            
  else {
    cut.reserve(cover.getNumElements());
    cut.setConstant(cover.getNumElements(),cover.getIndices(),1.0);
  }
  
  // Uncomplement the complemented variables in the cut
  int k;
  if (fabs(b-rowub)> epsilon_) {
    double * elements = cut.getElements();
    int * indices = cut.getIndices();
    for (k=0; k<cut.getNumElements(); k++){
      if (complement[indices[k]]) {
        // Negate the k'th element in packedVector cut
        // and correspondingly adjust the rhs
        elements[k] *= -1;
        cutRhs += elements[k];
      }
    }
  }

  // Create row cut. Effectiveness defaults to 0.
  OsiRowCut rc;
  rc.setRow(cut);
  rc.setLb(-DBL_MAX);
  rc.setUb(cutRhs);
  //  rc.setEffectiveness(0);
  // Todo: put in a more useful measure such as  the violation. 
  
  // Add row cut to the cut set  
  cs.insert(rc);
  
  return 1; 
}

//-------------------------------------------------------------------
// deriveAKnapsack - returns 1 if the method is able to 
//                  derive a cannonical knapsack inequality
//                  in binary variables of the form ax<=b 
//                  from the rowIndex-th row of the constraint matrix.
//                  returns 0, otherwise.
//  Precondition: complement must be 0'd out!!!
//-------------------------------------------------------------------
int 
CglLiftAndProject::deriveAKnapsack(
       const OsiSolverInterface & si, 
       OsiCuts & cs,
       OsiPackedVector & krow, 
       double & b,
       int *  complement,
       double *  xstar,
       int rowIndex,
       const OsiPackedVectorBase & matrixRow ) const

{
  int i;

  krow.clear();

  // if the matrixRow represent a ge inequality, then
  //     leMatrixRow == -matrixRow
  // otherwise
  //     leMatrixRow == matrixRow.

  OsiPackedVector leMatrixRow(matrixRow); 

  double maxKrowElement = -DBL_MAX;
  double minKrowElement = DBL_MAX;
  
  // Get the sense of the row
  const char  rowsense = si.getRowSense()[rowIndex];
  
  // Skip equality and unbounded rows 
  if  (rowsense=='E' || rowsense=='N') {
    return 0; 
  }
  
  // le row  
  if (rowsense=='L'){
    b=si.getRightHandSide()[rowIndex];
  }

  // ge row  
  if (rowsense=='G'){
    b=-si.getRightHandSide()[rowIndex];
#if 1
    std::transform(leMatrixRow.getElements(),
		   leMatrixRow.getElements() + leMatrixRow.getNumElements(),
		   leMatrixRow.getElements(),
		   std::negate<double>());
#else
    for (i=0; i<leMatrixRow.getNumElements(); i++)
      leMatrixRow.setElement(i,-leMatrixRow.getElements()[i]);
#endif
  }
  
  // Finite, but unequal row bounds. 
  // Multiple ways to handle.  
  // For now, use the le relationship as the default.
  if  (rowsense=='R') {
    b=si.getRightHandSide()[rowIndex];
  }

  // nBinUnsat is a counter for the number of unsatisfied
  // (i.e. fractional) binary vars  
  int nBinUnsat =0;
  const double * colupper = si.getColUpper();
  const double * collower = si.getColLower();
  
  // At this point, leMatrixRow and b represent a le inequality in general
  // variables. 
  // To derive a cannonical knapsack inequality in free binary variable,
  // process out the continuous & non-binary integer & fixed binary variables.
  // If the non-free-binary variables can be appropriately bounded, 
  // net them out of the constraint, otherwise abandon this row and return 0.

  const int * indices = leMatrixRow.getIndices();
  const double * elements = leMatrixRow.getElements();
  // for every variable in the constraint
  for (i=0; i<leMatrixRow.getNumElements(); i++){
    // if the variable is not a free binary var
    if ( !si.isFreeBinary(indices[i]) ) {
      // and the coefficient is strictly negative
      if(elements[i]<-epsilon_){
	// and the variable has a finite upper bound
        if (colupper[indices[i]] < si.getInfinity()){
	  // then replace the variable with its upper bound.
          b=b-elements[i]*colupper[indices[i]];
        } 
        else {
          return 0;
        }
      }
      // if the coefficient is strictly positive
      else if(elements[i]>epsilon_){
	// and the variable has a finite lower bound
        if (collower[indices[i]] > -si.getInfinity()){
	  // then replace the variable with its lower bound.
          b=b-elements[i]*collower[indices[i]];
        }
        else {
          return 0;
        }
      }
      // note: if the coefficient is zero, the variable is not included in the 
      //       knapsack inequality.
    }
    // else the variable is a free binary var and is included in the knapsack
    // inequality. 
    // note: the variable is included regardless of its solution value to the
    // lp relaxation. 
    else{
      krow.insert(indices[i], elements[i]);

      // if the binary variable is unsatified (i.e. has fractional value),
      // increment the counter. 
      if(xstar[indices[i]] > epsilon_ && xstar[indices[i]] < onetol_)
        nBinUnsat++;

      // keep track of the largest and smallest elements in the knapsack
      // (the idea is if there is not a lot of variation in the knapsack
      // coefficients, it is unlikely we will find a violated minimal
      // cover from this knapsack so don't even bother trying) 
      if (fabs(elements[i]) > maxKrowElement) 
        maxKrowElement = fabs(elements[i]);
      if (fabs(elements[i]) < minKrowElement) 
        minKrowElement = fabs(elements[i]);
    }
  }
  
  // If there's little variation in the knapsack coefficients, return 0.
  // If there are no unsatisfied binary variables, return.
  // If there's only one binary, return.  
  // ToDo: but why return if 2 binary? ...there was some assumption in the
  // findVioMinCover..(?)   
  if (krow.getNumElements() < 3 ||
      nBinUnsat == 0 ||
      maxKrowElement-minKrowElement < 1.0 ) {
    return 0;
  }



  // At this point krow and b represent a le inequality in binary variables.  
  // To obtain an le inequality with all positive coefficients, complement
  // any variable with a negative coefficient by changing the sign of 
  // the coefficient, adjusting the rhs, and adjusting xstar, the column
  // solution vector.
  {
     const int s = krow.getNumElements();
     const int * indices = krow.getIndices();
     double * elements = krow.getElements();
     for(i=0; i<s; i++){
	 if (elements[i] < -epsilon_) {
	   complement[indices[i]]= 1;
	   elements[i] *= -1;
	   b+=elements[i]; 
	   xstar[indices[i]]=1.0-xstar[indices[i]];
	}
     }
  }

  // Quick feasibility check.
  // If the problem is infeasible, add an infeasible col cut to cut set
  // e.g. one that has lb > ub.
  // TODO: test this scenario in BCP
  if (b < 0 ){ 
    OsiColCut cc;
    int index = krow.getIndices()[0]; 
    const double fakeLb = colupper[index] + 1.0;;  // yes, colupper.
    const double fakeUb = collower[index];
    assert( fakeUb < fakeLb );
    cc.setLbs( 1, &index, &fakeLb);
    cc.setUbs( 1, &index, &fakeLb);
    cc.setEffectiveness(DBL_MAX);
    cs.insert(cc);
#ifdef DEBUG
    printf("Cgl: Problem is infeasible\n");
#endif
  }
  
  // At this point, krow and b represent a le inequality with postive
  // coefficients. 
  // If any coefficient a_j > b, then x_j = 0, return 0
  // If any complemented var has coef a_j > b, then x_j = 1, return 0 
  int fixed = 0;
  OsiPackedVector fixedBnd;  
  for(i=0; i<krow.getNumElements(); i++){
    if (krow.getElements()[i]> b){
      fixedBnd.insert(krow.getIndices()[i],complement[krow.getIndices()[i]]);
#ifdef DEBUG   
      printf("Variable %i being fixed to %i due to row %i.\n",
	     krow.getIndices()[i],complement[krow.getIndices()[i]],rowIndex); 
#endif
      fixed = 1;      
    }
  }

  // After all possible variables are fixed by adding a column cut with 
  // equivalent lower and upper bounds, return
  if (fixed) {
    OsiColCut cc;
    cc.setLbs(fixedBnd);
    cc.setUbs(fixedBnd);
    cc.setEffectiveness(DBL_MAX);
    return 0; 
  }  

  return 1;
}


//--------------------------------------------------
// Find a violated minimal cover from 
// a cannonical form knapsack inequality by
// solving the lp relaxation of the 
// -most- violated cover problem.
// Postprocess to ensure minimality.
// -----------------------------------------
int 
CglLiftAndProject::findLPMostViolatedMinCover(
      int nCols,
      int row,
      OsiPackedVector & krow,
      double & b,
      double * xstar, 
      OsiPackedVector & cover,
      OsiPackedVector & remainder) const
{
  
  // Assumes krow and b describe a knapsack inequality in cannonical form

  // Given a knapsack inequality sum a_jx_j <= b, and optimal lp solution
  // xstart, a violated minimal cover inequality exists if the following 0-1
  // programming problem has an optimal objective function value (oofv) < 1
  //     oofv = min sum (1-xstar_j)z_j
  //            s.t. sum a_jz_j > b
  //            z binary
  
  // The vector z is an incidence vector, defining the cover R with the 
  // associated cover inequality:
  //    (sum j in R) x_j <= |R|-1
  
  // This problem is itself a (min version of the) knapsack problem
  // but with a unsightly strict inequalty.
  
  // To transform transform it into a max version, 
  // complement the z's, z_j=1-y_j.
  // To compensate for the strict inequality, subtract epsilon from the rhs.
  
  //     oofv = (sum (1-xstar_j))-  max sum (1-xstar)y_j
  //                        s.t. sum a_jy_j <= (sum over j)a_j - b (- EPSILON)
  //                             y binary
  
  // If oofv < 1, then a violated min cover inequality has
  // incidence vector z with elements z_j=1-y_j and rhs= num of nonzero's in 
  // z, i.e. the number of 0's in y.

  // If the size of the knapsack is "small", we solve the problem exactly. 
  // If the size of the knapsack is large, we solve the (simpler) lp relaxation
  // of the  knapsack problem and postprocess to ensure the construction of a 
  // minimimal cover.
  
  // We also assume that testing/probing/fixing based on the knapsack structure
  // is done elsewhere. Only convenient-to-do sanity checks are done here.
  // (We do not assume that data is integer.)

  double elementSum = krow.sum();

  // Redundant/useless adjusted rows should have been trapped in the 
  // transformation to the cannonical form of knapsack inequality
  if (elementSum < b + epsilon_) {
    return -1; 
  }
  
  // Order krow in nonincreasing order of coefObj_j/a_j.  
  // (1-xstar_1)/a_1 >= (1-xstar_2)/a_2 >= ... >= (1-xstar_n)/a_n   
  // by defining this full-storage array "ratio" to be the external sort key.
  double * ratio= new double[nCols];
  memset(ratio, 0, (nCols*sizeof(double)));

  int i;
  for (i=0; i<krow.getNumElements(); i++){
    if (fabs(krow.getElements()[i])> epsilon_ ){
      ratio[krow.getIndices()[i]]=
	 (1.0-xstar[krow.getIndices()[i]]) / (krow.getElements()[i]);
    }
    else {
      ratio[krow.getIndices()[i]] = 0.0;
    }
  }

  // ToDo: would be nice to have sortkey NOT be full-storage vector
  CoinDecrSolutionOrdered dso(ratio);
  krow.sort(dso);   

  // Find the "critical" element index "r" in the knapsack lp solution
  int r = 0;
  double sum = krow.getElements()[0];
  while ( sum <= (elementSum - b - epsilon_ ) ){
    r++;
    sum += krow.getElements()[r];
  }    
  
  // Note: It is possible the r=0, and you get a violated minimal cover 
  // if (r=0), then you've got a var with a really large coeff. compared
  //   to the rest of the row.
  // r=0 says trivially that the 
  //   sum of ALL the binary vars in the row <= (cardinality of all the set -1)
  // Note: The cover may not be minimal if there are alternate optimals to the 
  // maximization problem, so the cover must be post-processed to ensure 
  // minimality.
  
  // "r" is the critical element 
  // The lp relaxation column solution is:
  // y_j = 1 for  j=0,...,(r-1)
  // y_r = (elementSum - b - sum + krow.element()[r])/krow.element()[r] 
  // y_j = 0 for j=r+1,...,krow.getNumElements() 
  
  // The number of nonzeros in the lp column solution is r+1 
  
  // if oofv to the lp knap >= 1, then no violated min cover is possible 
  int nCover;

  double lpoofv=0.0;
  for (i=r+1; i<krow.getNumElements(); i++){
    lpoofv += (1-xstar[krow.getIndices()[i]]);
  }
  double ipofv = lpoofv + (1-xstar[krow.getIndices()[r]]);

  // Couldn't find an lp violated min cover inequality 
  if (ipofv > 1.0 - epsilon_){    
    delete [] ratio;
    return -1;
  }
  else {
    // Partition knapsack into cover and noncover (i.e. remainder)
    // pieces
    nCover = krow.getNumElements() - r;
    double coverSum =0.0;
    cover.reserve(nCover);
    remainder.reserve(r);
    
    for (i=r; i<krow.getNumElements(); i++){
      cover.insert(krow.getIndices()[i],krow.getElements()[i]);
      coverSum += krow.getElements()[i];
    }
    for (i=0; i<r; i++){
      remainder.insert(krow.getIndices()[i],krow.getElements()[i]);
    }
    
#ifdef DEBUG
    if (coverSum <= b){
      printf("The identified cover is NOT a cover\n");
      abort();
    }
#endif
    
    // Sort cover in terms of knapsack row coefficients   
    cover.sortDecrElement();
    
    
    // We have a violated cover inequality.
    // Construct a -minimal- violated cover
    // by testing and potentially tossing smallest
    // elements 
    double oneLessCoverSum = coverSum - cover.getElements()[nCover-1];
    while (oneLessCoverSum > b){
      // move the excess cover member into the set of remainders
      remainder.insert(cover.getIndices()[nCover-1],
		       cover.getElements()[nCover-1]);
      cover.truncate(nCover-1);
      nCover--;
      oneLessCoverSum -= cover.getElements()[nCover-1];
    }

    if (nCover<2){
#ifdef DEBUG
      printf("nCover < 2...aborting\n");
#endif
      abort();
    }
    
#ifdef DEBUG   /* debug */
    printf("\
Lp relax of most violated minimal cover: row %i has cover of size %i.\n",
	   row,nCover);
    //double sumCover = 0.0;
    for (i=0; i<cover.getNumElements(); i++){
      printf("index %i, element %g, xstar value % g \n",
	     cover.getIndices()[i],cover.getElements()[i],
	     xstar[cover.getIndices()[i]]);
      //sumCover += cover.getElements()[i];
    }
    printf("The b = %g, and the cover sum is %g\n\n", b, cover.sum());
#endif

#ifdef P0201
      double ppsum=0.0;
      for (i=0; i<nCover; i++){
        ppsum += p0201[krow.getIndices()[i]];
      }
        
      if (ppsum > nCover-1){
          printf("\
\nBad cover from lp relax of most violated cover..aborting\n");
          abort();
      }
#endif
    
    /* clean up */
    delete [] ratio;
    return  1;
  }
}


//--------------------------------------------------
// Find a violated minimal cover from 
// a cannonical form knapsack inequality by
// solving the -most- violated cover problem
// and postprocess to ensure minimality
// -----------------------------------------
int 
CglLiftAndProject::findExactMostViolatedMinCover(
        int nCols,
        int row,
        OsiPackedVector & krow,
        double b, 
        double *  xstar, 
        OsiPackedVector & cover,
        OsiPackedVector & remainder) const 
{
  
  // assumes the row is in cannonical knapsack form 
  
  // A violated min.cover inequality exists if the
  // opt obj func value (oofv) < 1: 
  //     oofv = min sum (1-xstar_j)z_j
  //            s.t. sum a_jz_j > b
  //            x binary

  //     The vector z is the incidence vector 
  //     defines the set R and the cover inequality.
  //      (sum j in R) x_j <= |R|-1

  //    This is the min version of the knapsack problem.
  //    (note that strict inequalty...bleck)

  //    To obtain the max version, complement the z's, z_j=1-y_j and 
  //    adjust the constraint.

  //    oofv = (sum (1-xstar_j))-  max sum (1-xstar)y_j
  //                       s.t. sum a_jy_j <= (sum over j)a_j - b (- EPSILON)]
  //                   y binary

  // If oofv < 1, violated min cover inequality has
  //    incidence vector z=1-y and rhs= num of nonzero's in z, i.e.
  //    the number 0 in y.

  //    We solve the  0-1 knapsack problem by explicit ennumeration

  double elementSum = krow.sum();

  // Redundant/useless adjusted rows should have been trapped in 
  // transformation to cannonical form of knapsack inequality
  if (elementSum < b + epsilon_) {
#ifdef DEBUG
    printf("Redundant/useless adjusted row\n");
#endif
    return -1; 
  }

  // Order krow in nonincreasing order of coefObj_j/a_j.  
  // (1-xstar_1)/a_1 >= (1-xstar_2)/a_2 >= ... >= (1-xstar_n)/a_n   
  // by defining this full-storage array "ratio" to be the external sort key.  
  double * ratio= new double[nCols];
  memset(ratio, 0, (nCols*sizeof(double)));

  int i;
  {
     const int * indices = krow.getIndices();
     const double * elements = krow.getElements();
     for (i=0; i<krow.getNumElements(); i++){
	if (fabs(elements[i])> epsilon_ ){
	   ratio[indices[i]]= (1.0-xstar[indices[i]]) / elements[i];
	}
	else {
	   ratio[indices[i]] = 0.0;
	}
     }
  }

  // ToDo: would be nice to have sortkey NOT be full-storage vector
  CoinDecrSolutionOrdered dso(ratio);
  krow.sort(dso);   

#ifdef DEBUG
  // sanity check
  for ( i=1; i<krow.getNumElements(); i++ ) {
    double ratioim1 =  ratio[krow.getIndices()[i-1]];
    double ratioi= ratio[krow.getIndices()[i]];
    assert( ratioim1 >= ratioi );
  }  
#endif  
  
  // Recall:
  // oofv = (sum (1-xstar_j))-  max sum (1-xstar)y_j
  //           s.t. sum a_jy_j <= (sum over j)a_j - b (- epsilon_)]
  //           y binary

  double objConst = 0.0;
  double exactOptVal = -1.0;
  int * exactOptSol = new int[krow.getNumElements()];
  double * p = new double[krow.getNumElements()];
  double * w = new double[krow.getNumElements()];
  int kk;
  for (kk=0; kk<krow.getNumElements(); kk++){
    p[kk]=1.0-xstar[krow.getIndices()[kk]];
    w[kk]=krow.getElements()[kk];
    objConst+=p[kk];
  }
  
  // vectors are indexed in ratioSortIndex order 
  exactSolveKnapsack(krow.getNumElements(), (elementSum-b-epsilon_), p, w,
		     exactOptVal, exactOptSol);
  
  if(objConst-exactOptVal < 1){
    cover.reserve(krow.getNumElements());
    remainder.reserve(krow.getNumElements());

    // Partition the krow into the cover and the remainder.
    // The cover is complement of solution.
    double coverElementSum = 0;
    for(kk=0;kk<krow.getNumElements();kk++){
      if(exactOptSol[kk]==0){
        cover.insert(krow.getIndices()[kk],krow.getElements()[kk]);
        coverElementSum += krow.getElements()[kk];
      }
      else {
        remainder.insert(krow.getIndices()[kk],krow.getElements()[kk]);
      }
    }

    cover.sortDecrElement();

    // We have a violated cover inequality.
    // Construct a -minimal- violated cover
    // by testing and potentially tossing smallest
    // elements 
    double oneLessCoverElementSum =
       coverElementSum - cover.getElements()[cover.getNumElements()-1];
    while (oneLessCoverElementSum > b){
      // move the excess cover member into the set of remainders
      remainder.insert(cover.getIndices()[cover.getNumElements()-1],
		       cover.getElements()[cover.getNumElements()-1]);
      cover.truncate(cover.getNumElements()-1);
      oneLessCoverElementSum -= cover.getElements()[cover.getNumElements()-1];
    }

#ifdef DEBUG
    printf("Exact Most Violated Cover: row %i has cover of size %i.\n",
	   row,cover.getNumElements());
    //double sumCover = 0.0;
    for (i=0; i<cover.getNumElements(); i++){
      printf("index %i, element %g, xstar value % g \n",
	     cover.getIndices()[i], cover.getElements()[i],
	     xstar[cover.getIndices()[i]]);
      //sumCover += cover.getElements()[i];
    }
    printf("The b = %g, and the cover sum is %g\n\n", b, cover.sum() );
#endif
   
    // local clean up 
    delete [] exactOptSol;
    delete [] p;
    delete [] w;
    delete [] ratio;
    
    return  1; // found an exact one
  }

  // local clean up 
  delete [] exactOptSol;
  delete [] p;
  delete [] w;
  delete [] ratio;
  
  return 0; // didn't find an exact one

}  

//-------------------------------------------------------------------
// Find Pseudo John-and-Ellis Cover
// 
// only generates -violated- minimal covers
//-------------------------------------------------------------------
int
CglLiftAndProject::findPseudoJohnAndEllisCover(
     int row,
     OsiPackedVector & krow,
     double & b,
     double * xstar, 
     OsiPackedVector & cover,  
     OsiPackedVector & remainder) const

{
  // semi-mimic of John&Ellis's approach without taking advantage of SOS info
  // RLH: They find a minimal cover on unsatisfied variables, but it is 
  // not guaranteed to be violated by currently solution 

  // going for functional now, will make efficient when working 
  
  // look at unstatisfied binary vars with nonzero row coefficients only
  // get row in canonical form (here row is in cannonical form)
  // if complement var, complement soln val too. (already done)
  // (*) sort in increasing value of soln val
  // track who is the biggest coef and it's index.
  // if biggest > adjRhs, skip row. Bad knapsack.
  // margin = adjRhs
  // idea: if (possibly compl) soln >= .5 round up, else round down
  // they do more, but that's the essence
  // go through the elements {
  // if round down, skip
  // if round up, add to element to cover. adjust margin 
  // if current element = biggest, then get next biggest
  // if biggest > marg, you've got a cover. stop looking               
  // else try next element in the loop
  // }       
  // (*)RLH: I'm going to sort in decreasing order of soln val
  // b/c var's whose soln < .5 in can. form get rounded down 
  // and skipped. If you can get a min cover of the vars
  // whose soln is >= .5, I believe this gives the same as J&E.
  // But if not, maybe I can get something more.
       
  // (**)By checking largest value left, they ensure a minimal cover
  // on the unsatisfied variables
     
  // if you have a cover
  // sort the elements to be lifted in order of their reduced costs.
  // lift in this order.
  // ...I don't understand their lifting, so for now use sequence-indep lifting

  // J&E employ lifting up and down. 
  // Here I'm including the vars at one in the cover.
  // Adding these vars back in may cause the minimality of the cover to lost.
  // So, post-processing to establish minimality is required.
  
  cover.reserve(krow.getNumElements());
  remainder.reserve(krow.getNumElements());

  double unsatRhs = b;

  // working info on unsatisfied vars
  OsiPackedVector unsat;
  unsat.reserve(krow.getNumElements());

  // working info on vars with value one
  OsiPackedVector atOne;
  atOne.reserve(krow.getNumElements());

  // partition the (binary) variables in the cannonical knapsack
  // into those at zero, those at fractions, and those at one. 
  // Note: no consideration given to whether variables are free
  // or fixed at binary values.
  // Note: continuous and integer vars have already been netted out
  // to derive the cannonical knapsack form
  int i;
  for (i=0; i<krow.getNumElements(); i++){

    if (xstar[krow.getIndices()[i]] > onetol_){
      atOne.insert(krow.getIndices()[i],krow.getElements()[i]); 
      unsatRhs -= krow.getElements()[i];
    }   
    else if (xstar[krow.getIndices()[i]] >= epsilon_){
      unsat.insert(krow.getIndices()[i],krow.getElements()[i]) ;
    }
    else { 
      remainder.insert(krow.getIndices()[i],krow.getElements()[i]);
    }
  }

  // sort the indices of the unsat var in order of decreasing solution value
  CoinDecrSolutionOrdered decrSol(xstar);
  unsat.sort(decrSol);
  
#ifdef DEBUG
  // sanity check
  for (i=1; i<unsat.getNumElements(); i++){
    double xstarim1= xstar[unsat.getIndices()[i-1]];
    double xstari= xstar[unsat.getIndices()[i]];
    assert( xstarim1 >= xstari );
  }
#endif

  // get the largest coefficient among the unsatisfied variables   
  double bigCoef= 0.0;
  // double temp;
  int bigIndex = 0;
  for (i=0; i<unsat.getNumElements(); i++){
    if (unsat.getElements()[i]>bigCoef){
      bigCoef = unsat.getElements()[i];
      bigIndex = i;
    }
  }
  
  // initialize 
  i=0;
  double margin = unsatRhs;
  int gotCover=0;
  int j;
  
  // look in order through the unsatisfied vars which along with the 
  // the max element defines a cover
  while (i<unsat.getNumElements() && !gotCover){
    margin -= unsat.getElements()[i];
    
    // get the biggest row coef downstream in the given order   
    if (i == bigIndex){
      bigCoef = 0.0;
      bigIndex = 0;
      for (j=i+1; j<unsat.getNumElements(); j++){
        double temp = unsat.getElements()[j];
        if (temp > bigCoef ){
          bigCoef = temp;
          bigIndex = j;
        }
      }
    }
    
    if (bigCoef > margin) gotCover = 1;
    i++;          
  }

  // J&E approach; get first single one element that fills the margin   
  if(gotCover){
    j=i;
    if (j<unsat.getNumElements()){ // need this "if" incase nUnsat=1   
      while (unsat.getElements()[j]< margin) {
        j++;
      }
      // switch members so that first nUnsat define the cover
      unsat.swap(i,j);
      i++;
    }
    
    // check that detected cover is violated 
    // (would we want to save incase it's violated later?)
    int nCover = i;
    double coverElementSum = 0.0;
    double coverXstarSum = 0.0;
    int k;
    for (k=0; k<nCover; k++){
      coverElementSum += unsat.getElements()[k];
      coverXstarSum +=  xstar[unsat.getIndices()[k]];
    }
    
    // Split the unsatisfied elements into those in the cover and those
    // not in the cover. The elements not in the cover are considered
    // remainders. Variables atOne belong to the cover

    // Test if the detected cover is violated
    if (coverXstarSum > (nCover-1) && coverElementSum > unsatRhs){
      for (i=nCover; i<unsat.getNumElements(); i++) {
        remainder.insert(unsat.getIndices()[i],unsat.getElements()[i]);
      }
      unsat.truncate(nCover);
      cover = unsat;
      cover.append(atOne);

      for (k=nCover; k<cover.getNumElements(); k++){
        coverElementSum+=cover.getElements()[k];
        coverXstarSum+=xstar[cover.getIndices()[k]];
      }
      
      // Sanity check
      int size = cover.getNumElements() + remainder.getNumElements(); 
      int krowsize = krow.getNumElements();
      assert( size == krowsize );
      
      // Sort cover in terms of knapsack row coefficients   
      cover.sortDecrElement();

    // New!
    // We have a violated cover inequality.
    // Construct a -minimal- violated cover
    // by testing and potentially tossing smallest
    // elements 
    double oneLessCoverElementSum =
       coverElementSum - cover.getElements()[cover.getNumElements()-1];
    while (oneLessCoverElementSum > b){
      // move the excess cover member into the set of remainders
      remainder.insert(cover.getIndices()[cover.getNumElements()-1],
		       cover.getElements()[cover.getNumElements()-1]);
      cover.truncate(cover.getNumElements()-1);
      oneLessCoverElementSum -= cover.getElements()[cover.getNumElements()-1];
    }
  
#ifdef DEBUG
    if (coverXstarSum > (nCover-1) && coverElementSum > b){
      printf("John and Ellis: row %i has cover of size %i.\n",
	     row,cover.getNumElements());
      //double sumCover = 0.0;
      for (i=0; i<cover.getNumElements(); i++){
        printf("index %i, element %g, xstar value % g \n",
	       cover.getIndices()[i], cover.getElements()[i],
	       xstar[cover.getIndices()[i]]);
      }
      printf("The b = %g, and the cover element sum is %g\n\n",b,cover.sum());
    }
#endif

    }
    // if minimal cover is not violated, turn gotCover off
    else {
//    printf("heuristically found minimal cover is NOT violated by current lp solution");
      gotCover = 0;
    }          
  }

  // If no minimal cover was found, pack it in   
  if (!gotCover || cover.getNumElements() < 2) {
    return -1;
  }
  
  return  1;
}





//-------------------------------------------------------------------
// Find a "approx" John-and-Ellis Cover
// (i.e. that this approximates John & Ellis code)
//  Test to see if we generate the same covers, and lifted cuts
// 
// generates minimal covers, not necessarily violated ones.
//-------------------------------------------------------------------
int
CglLiftAndProject::findJohnAndEllisCover(
     int row,
     OsiPackedVector & krow,
     double & b,
     double * xstar, 
     OsiPackedVector & fracCover,  
     OsiPackedVector & atOne,
     OsiPackedVector & remainder) const

{
  // John Forrest and Ellis Johnson's approach as I see it.
  // RLH: They find a minimal cover on unsatisfied variables, 
  // which may not be violated by the solution to the lp relaxation

  // "functional before efficient" is my creed.
  
  // look at unstatisfied binary vars with nonzero row coefficients only
  // get row in canonical form (here krow is in cannonical form)
  // if complement var, complement soln val too. (already done)
  // (*) sort in increasing value of soln val
  // track who is the biggest coef and it's index.
  // if biggest > adjRhs, skip row. Bad knapsack.
  // margin = adjRhs
  // idea: if (possibly compl) soln >= .5 round up, else round down
  // they do more, but that's the essence
  // go through the elements {
  // if round down, skip
  // if round up, add to element to cover. adjust margin 
  // if current element = biggest, then get next biggest
  // if biggest > marg, you've got a cover. stop looking               
  // else try next element in the loop
  // }       
  // (*)RLH: I'm going to sort in decreasing order of soln val
  // b/c var's whose soln < .5 in can. form get rounded down 
  // and skipped. If you can get a min cover of the vars
  // whose soln is >= .5, I believe this gives the same as J&E.
  // But if not, maybe I can get something more.
       
  // (**)By checking largest value left, they ensure a minimal cover
  // on the unsatisfied variables
     
  // if you have a cover
  // sort the elements to be lifted in order of their reduced costs.
  // lift in this order.
  // They lift down on variables at one, in a sequence-dependent manner.
  // Partion the variables into three sets: those in the cover, those 
  // not in the cover at value one, and those remaining.
  
  fracCover.reserve(krow.getNumElements());
  remainder.reserve(krow.getNumElements());
  atOne.reserve(krow.getNumElements());

  double unsatRhs = b;

  // working info on unsatisfied vars
  OsiPackedVector unsat;
  unsat.reserve(krow.getNumElements());

  // partition the (binary) variables in the cannonical knapsack
  // into those at zero, those at fractions, and those at one. 
  // 
  // essentially, temporarily fix to one the free vars with lp soln value of
  // one by calculating the "unsatRhs". Call the result the "reduced krow".
  //
  // Note: continuous and integer vars, and variables fixed at 
  // binary values have already been netted out
  // in deriving the cannonical knapsack form
  int i;
  for (i=0; i<krow.getNumElements(); i++){

    if (xstar[krow.getIndices()[i]] > onetol_){
      atOne.insert(krow.getIndices()[i],krow.getElements()[i]); 
      unsatRhs -= krow.getElements()[i];
    }   
    else if (xstar[krow.getIndices()[i]] >= epsilon_){
      unsat.insert(krow.getIndices()[i],krow.getElements()[i]) ;
    }
    else { 
      remainder.insert(krow.getIndices()[i],krow.getElements()[i]);
    }
  }

  // sort the indices of the unsat var in order of decreasing solution value
  CoinDecrSolutionOrdered decrSol(xstar);
  unsat.sort(decrSol);
  
#ifdef DEBUG
  // sanity check
  for (i=1; i<unsat.getNumElements(); i++){
    double xstarim1 = xstar[unsat.getIndices()[i-1]];
    double xstari=  xstar[unsat.getIndices()[i]];
    assert( xstarim1 >= xstari );
  }
#endif

  // get the largest coefficient among the unsatisfied variables   
  double bigCoef= 0.0;
  // double temp;
  int bigIndex = 0;
  for (i=0; i<unsat.getNumElements(); i++){
    if (unsat.getElements()[i]>bigCoef){
      bigCoef = unsat.getElements()[i];
      bigIndex = i;
    }
  }
  
  // initialize 
  i=0;
  double margin = unsatRhs;
  int gotCover=0;
  int j;
  
  // look in order through the unsatisfied vars which along with the 
  // the max element defines a cover
  while (i<unsat.getNumElements() && !gotCover){
    margin -= unsat.getElements()[i];
    
    // get the biggest row coef downstream in the given order   
    if (i == bigIndex){
      bigCoef = 0.0;
      bigIndex = 0;
      for (j=i+1; j<unsat.getNumElements(); j++){
        double temp = unsat.getElements()[j];
        if (temp > bigCoef ){
          bigCoef = temp;
          bigIndex = j;
        }
      }
    }
    
    if (bigCoef > margin) gotCover = 1;
    i++;          
  }

  // J&E approach; get first single one element that fills the margin   
  if(gotCover){ 
    j=i;
    if (j<unsat.getNumElements()){ // need this "if" incase nUnsat=1   
      while (unsat.getElements()[j]< margin) {
        j++;
      }
      // switch members so that first nUnsat define the cover
      unsat.swap(i,j);
      i++;
    }
    
    // DEBUG: verify we have a cover over the reduced krow
    // (may not be violated)
    int nCover = i;
    double coverElementSum = 0.0;
    int k;
    for (k=0; k<nCover; k++){
      coverElementSum += unsat.getElements()[k];
    }
    
    // Split the unsatisfied elements into those in the "reduced krow" cover
    // and those not in the cover. The elements not in the cover are
    // considered remainders. Variables atOne are reported as atOne.


    // Test if the detected cover is violated
    if (coverElementSum > unsatRhs){
      for (i=nCover; i<unsat.getNumElements(); i++) {
        remainder.insert(unsat.getIndices()[i],unsat.getElements()[i]);
      }
      unsat.truncate(nCover);
      fracCover = unsat;
      // cover.append(atOne);

      // Sanity check
      int size = (fracCover.getNumElements() + remainder.getNumElements() +
		  atOne.getNumElements());
      int krowsize =  krow.getNumElements();
      assert( size == krowsize );
      
      // Sort cover in terms of knapsack row coefficients   
      fracCover.sortDecrElement();

    // We have a not-necessarily-violated "reduced krow" cover inequality.
    // Minimal on the "reduced krow" 

#if 0
      
      double oneLessCoverElementSum =
	 coverElementSum-fracCover.getElements()[fracCover.getNumElements()-1];
      while (oneLessCoverElementSum > b){
        // move the excess cover member into the set of remainders
        remainder.insert(fracCover.getIndices()[fracCover.getNumElements()-1],
			 fracCover.getElements()[fracCover.getNumElements()-1]);
        fracCover.truncate(fracCover.getNumElements()-1);
        oneLessCoverElementSum -=
	   fracCover.getElements()[fracCover.getNumElements()-1];
      }
#endif
  
#ifdef DEBUG
      printf("More Exactly John and Ellis:");
      printf(" row %i has -reduced--fractional- cover of size %i.\n",
	     row,fracCover.getNumElements());
      double sumFracCover = 0.0;
      for (i=0; i<fracCover.getNumElements(); i++){
        printf("index %i, element %g, xstar value % g \n",
	       fracCover.getIndices()[i],fracCover.getElements()[i],
	       xstar[fracCover.getIndices()[i]]);
        sumFracCover += fracCover.getElements()[i];
      }
      double sumAtOne = 0.0;
      printf("There are %i variables at one:\n",atOne.getNumElements());
      for (i=0; i<atOne.getNumElements(); i++){
        printf("index %i, element %g, xstar value % g \n",
	       atOne.getIndices()[i],atOne.getElements()[i],
	       xstar[atOne.getIndices()[i]]);
        sumAtOne += atOne.getElements()[i];
      }
      printf("The b = %g, sumAtOne = %g, unsatRhs = b-sumAtOne = %g, ",
	     b, sumAtOne, unsatRhs);
      printf("and the (fractional) cover element sum is %g\n\n", sumFracCover);
#endif

    }
    // if minimal cover is not violated, turn gotCover off
    else {
//    printf("heuristically found minimal cover is NOT violated by current lp solution");
      gotCover = 0;
    }          
  }

  // If no minimal cover was found, pack it in   
  //  if (!gotCover || cover.getNumElements() < 2) {
  if (!gotCover) {
    return -1;
  }
  
  return  1;
}

//-------------------------------------------------------------------
// findGreedyCover: attempts to find a violated minimal
//                  cover using a greedy approach
//
// If a cover is found, it the cover and the remainder are 
// sorted in nonincreasing order of the coefficients.
//-------------------------------------------------------------------
int
CglLiftAndProject::findGreedyCover(
     int row,
     OsiPackedVector & krow,
     double & b,
     double * xstar,
     OsiPackedVector & cover,
     OsiPackedVector & remainder
     ) const
  // the row argument is a hold over from debugging
  // ToDo: move the print cover statement out to the mainprogram 
  // and remove the row argument

{ 
  int i;
  int gotCover =0;
  
  cover.reserve(krow.getNumElements());
  remainder.reserve(krow.getNumElements());
  
  // sort knapsack in non-increasing size of row Coefficients 
  krow.sortDecrElement();  
  
  // greedily pack them in 
  // looking only at unsatisfied vars, i.e. 0<xstar[.]<1 
  double greedyElementSum = 0.0;
  double greedyXstarSum = 0.0;
  
  for (i=0;i<krow.getNumElements();i++){
    // if xstar fractional && no cover yet, consider it for the cover 
    if (xstar[krow.getIndices()[i]] >= epsilon_ &&
	xstar[krow.getIndices()[i]] <= onetol_ &&
	!gotCover){
      greedyElementSum += krow.getElements()[i];
      greedyXstarSum += xstar[krow.getIndices()[i]];
      cover.insert(krow.getIndices()[i],krow.getElements()[i]);
      if (greedyElementSum > b){
        gotCover = 1;
      }
    }
    else{
      remainder.insert(krow.getIndices()[i],krow.getElements()[i]);
    }
  }
  
  // sanity check
  int size =  remainder.getNumElements()+cover.getNumElements();
  int krowsize = krow.getNumElements();
  assert( size==krowsize );
  
  // if no violated minimal cover was found, pack it in 
  if ( (greedyXstarSum<=(cover.getNumElements()-1)) ||
       (!gotCover) ||
       (cover.getNumElements() < 2)){
    return -1;
  }
  
#ifdef DEBUG 
  printf("Greedy cover: row %i has cover of size %i\n",
	 row,cover.getNumElements());
  for (i=0; i<cover.getNumElements(); i++){
    printf("index %i, element %g, xstar value % g \n", 
	   cover.getIndices()[i], cover.getElements()[i],
	   xstar[cover.getIndices()[i]]);
  }
  printf("The b = %g, and the cover sum is %g\n\n", b, greedyElementSum);
#endif

  return  1;
}

//------------------------------------------------------------- 
// Lift Up, Down, and Uncomplement. Add the resutling cut to the cutset
//
// In the solution to the lp relaxtion, 
// the binary variable's solution value is either 0, 1 or fractional.
//
// Input:
// The variables in fracCover form a cover, when the vars atOne take value one.
// A cover for the krow would consist of the union of the fracCover and atOne vars
// (which may not be violated, and may need to be processed to acheieve minimal-ness).
//
// Rather than take the "union" cover and lift up the remainder variables, 
// we do something a little bit more interesting with the vars at one.
//
// The ip theory says that the lifted minimal cover cut can be strengthen by
// "lifting down" the vars atOne.
// -- this is what I believe John&Ellis were doing in OSL's knapsack cover cuts
//    with a lifting heuristic.
//
//-------------------------------------------------------------------
void 
CglLiftAndProject::liftUpDownAndUncomplementAndAdd(
         int nCols,
         double * xstar, 
         int * complement,
         int row,
         int nRowElem,
         double & b,

         // the following 3 packed vectors partition the krow:

	 // vars have frac soln values in lp relaxation
	 // and form cover with the vars atOne
         OsiPackedVector & fracCover, 
	 // vars have soln value of 1 in lp relaxation
         OsiPackedVector & atOne,
	 // and together with fracCover form minimal (?) cover. 
         OsiPackedVector & remainder,

	 OsiCuts & cs ) const
{
  OsiPackedVector cut;

  // reserve storage for the cut
  cut.reserve(nRowElem);
  
  // the cut coefficent for the members of the cover is 1.0
  cut.setConstant(fracCover.getNumElements(),fracCover.getIndices(),1.0);
  
  // Preserve the cutRhs which is |C|-1, where |C| is the size of the
  // fracCover.
  double cutRhs=fracCover.getNumElements()-1;

  // local variables
  // unsatRhs is the rhs for the reduced krow
  double unsatRhs = 0, sumAtOne = 0;
  int i;
  for (i=0; i<atOne.getNumElements(); i++){
    sumAtOne += atOne.getElements()[i];
  }
  unsatRhs=b-sumAtOne;
  assert ( unsatRhs > 0 );

  // If there is something to lift, then calcualte the lifted coefficients
  if ((remainder.getNumElements()+atOne.getNumElements())> 0){
    
    // What order to lift?
    // Take the remainder vars in decreasing order of their
    // xstar solution value. Sort remainder in order of decreasing 
    // xstar value.
    // Lift them "up"
    // (The lift "down" the variables atOne.
    CoinDecrSolutionOrdered dso1(xstar);
    remainder.sort(dso1);   
    
    // a is the part of krow corresponding to vars which have been lifted
    // alpha are the lifted coefficients with explicit storage of lifted zero
    // coefficients the a.getIndices() and alpha.getIndices() are identical
    OsiPackedVector a(fracCover);
    OsiPackedVector alpha;
    int i;
    for (i=0; i<fracCover.getNumElements(); i++){
      alpha.insert(fracCover.getIndices()[i],1.0);
    }
    // needed as an argument for exactSolveKnapsack
    int * x = new int[nRowElem];
    double psi_j=0.0;
    
    // Order alpha and a in nonincreasing order of alpha_j/a_j.  
    // alpha_1/a_1 >= alpha_2/a_2 >= ... >= alpha_n/a_n   
    // by defining this full-storage array "ratio" to be the external sort key.
    // right now external sort key must be full-storage.

    double * ratio= new double[nCols];
    memset(ratio, 0, (nCols*sizeof(double)));
    double alphasize = alpha.getNumElements();
    double asize = a.getNumElements();
    assert( alphasize == asize );
    
    for (i=0; i<a.getNumElements(); i++){
      if (fabs(a.getElements()[i])> epsilon_ ){
        ratio[a.getIndices()[i]]=alpha.getElements()[i]/a.getElements()[i];
      }
      else {
        ratio[a.getIndices()[i]] = 0.0;
      }
    }

    CoinDecrSolutionOrdered dso2(ratio);
    a.sort(dso2);   
    alpha.sort(dso2);
    
#ifdef DEBUG
    // sanity check
    for ( i=1; i<a.getNumElements(); i++ ) {
      int alphai=  alpha.getIndices()[i];
      int ai = a.getIndices()[i];
      assert( alphai == ai);
    }  
#endif  
    
    // Loop through the remainder variables to be lifted "up", and lift.
    int j;
    for (j=0; j<remainder.getNumElements(); j++){
      // calculate the lifted coefficient of x_j = cutRhs-psi_j
      // where 
      // psi_j =  max of the current lefthand side of the cut
      //          s.t. the reduced knapsack corresponding to vars that have
      //          been lifted <= unsatRhs-a_j
      
      // Note: For exact solve, must be sorted in
      // alpha_1/a_1>=alpha_2/a_2>=...>=alpha_n/a_n order
      // check if lifted var can take value 1 
      if (unsatRhs - remainder.getElements()[j] < epsilon_){
	psi_j = cutRhs;
      }
      else {	  
	exactSolveKnapsack(alpha.getNumElements(),
	      	 unsatRhs-remainder.getElements()[j],
		 alpha.getElements(),a.getElements(),psi_j,x);
      }
      
      // assert the new coefficient is nonegative?
      alpha.insert(remainder.getIndices()[j],cutRhs-psi_j);
      a.insert(remainder.getIndices()[j],remainder.getElements()[j]);
      
      // if the lifted coefficient is non-zero 
      // (i.e. psi_j != cutRhs), add it to the cut
      if (fabs(cutRhs-psi_j)>epsilon_)
	 cut.insert(remainder.getIndices()[j],cutRhs-psi_j);
      
      ratio[remainder.getIndices()[j]]=
	 (cutRhs-psi_j)/remainder.getElements()[j];
      CoinDecrSolutionOrdered dso(ratio);
      a.sort(dso);   
      alpha.sort(dso);    
    }

    // Loop throught the variables atOne and lift "down"
    for (j=0; j<atOne.getNumElements(); j++){
      // calculate the lifted coefficient of x_j = psi_j-cutRhs (now cutRhs
      // gets updated) where 
      // psi_j =  max of the current lefthand side of the cut
      //          s.t. the reduced knapsack corresponding to vars that have
      //          been lifted <= unsatRhs+a_j 
      
      // Note: For exact solve, must be sorted in
      // alpha_1/a_1>=alpha_2/a_2>=...>=alpha_n/a_n order 
      exactSolveKnapsack(alpha.getNumElements(),
			 unsatRhs+atOne.getElements()[j],
			 alpha.getElements(),a.getElements(),psi_j,x);
      alpha.insert(atOne.getIndices()[j],psi_j-cutRhs);
      a.insert(atOne.getIndices()[j],atOne.getElements()[j]);
      // if the lifted coefficient is non-zero (i.e. psi_j != cutRhs), add it
      // to the cut 
      if (fabs(psi_j-cutRhs)>epsilon_)
	 cut.insert(atOne.getIndices()[j],psi_j-cutRhs);

      assert ( fabs(atOne.getElements()[j])> epsilon_ );
      ratio[atOne.getIndices()[j]]=(psi_j-cutRhs)/atOne.getElements()[j];

      // update cutRhs and unsatRhs
      cutRhs = psi_j ;      
      unsatRhs += atOne.getElements()[j];

      CoinDecrSolutionOrdered dso(ratio);
      a.sort(dso);   
      alpha.sort(dso);    
    }
    delete [] x;
    delete [] ratio;
  }

  // If the cut is violated, add it to the pool
  // if (sum over cut.getIndices())
  //    cut.element()*xstar > cover.getNumElements()-1, un-complement
  // and add it to the pool.
  double sum=0;
  for (i=0; i<cut.getNumElements(); i++){
    sum+= cut.getElements()[i]*xstar[cut.getIndices()[i]];
  }
  if (sum > cutRhs){
#ifdef DEBUG
    printf("Sequentially lifted UpDown cover cut of ");
    printf("size %i derived from fracCover of size %i.\n",
	   cut.getNumElements(), fracCover.getNumElements());
    for (i=0; i<cut.getNumElements(); i++){
      printf("index %i, element %g, xstar value % g \n",
	     cut.getIndices()[i],cut.getElements()[i],
	     xstar[cut.getIndices()[i]]);
    }
    printf("The cutRhs = %g, and the alpha_j*xstar_j sum is %g\n\n",
	   cutRhs, sum);
#endif
    
    // de-complement
    int k;
    const int s = cut.getNumElements();
    const int * indices = cut.getIndices();
    double * elements = cut.getElements();
    for (k=0; k<s; k++){
      if (complement[indices[k]]) {
        // Negate the k'th element in packedVector cut
        // and correspondingly adjust the rhs
        elements[k] *= -1;
        cutRhs += elements[k];
      }
    }
    
   
    // Create row cut
    OsiRowCut rc;
    rc.setRow(cut);
    rc.setLb(-DBL_MAX);
    rc.setUb(cutRhs);
    // ToDo: what's the default effectiveness?
    //  rc.setEffectiveness(1.0);
    // Add row cut to the cut set  
    cs.insert(rc);
  }
}

//-------------------------------------------------------------------
// seqLiftCoverCut:  Given a cannonical knapsack inequality and a
//                cover, performs sequence-dependent lifting.
//                Reference: Nemhauser & Wolsey
//
// NW suggest a lifting heuristic order that requires an argmax operation.
// What's the strength vs performance tradeoff of using argmax over
// a heuristic that depends soley on an a-prioi ordering based on
// the optimal solution to the lp relaxation? ToDo:  Do both, and report.
//
//-------------------------------------------------------------------
void
CglLiftAndProject::seqLiftAndUncomplementAndAdd(
      int nCols,
      double * xstar, 
      int * complement,
      int row,                       // row index number: used for debugging 
                                     //     and to index into row bounds
      int nRowElem,                  // number of elements in the row, aka row
                                     //     size, row length. 
      double & b,                    // rhs of the cannonical knapsack
                                     //     inequality derived from row 
      OsiPackedVector & cover,       // need not be violated
      OsiPackedVector & remainder,
      OsiCuts & cs) const
{
  OsiPackedVector cut;
  
  // reserve storage for the cut
  cut.reserve(nRowElem);
  
  // the cut coefficent for the members of the cover is 1.0
  cut.setConstant(cover.getNumElements(),cover.getIndices(),1.0);
  
  // so preserve the cutRhs which is |C|-1, where |C| is the size of the cover.
  double cutRhs=cover.getNumElements()-1;
  
  // If there is something to lift, then calcualte the lifted coefficients
  if (remainder.getNumElements()> 0){
    
    // What order to lift?
    // Take the to-be-lifted vars in decreasing order of their
    // xstar solution value. Sort remainder in order of decreasing 
    // xstar value.
    CoinDecrSolutionOrdered dso1(xstar);
    remainder.sort(dso1);   
    
    // a is the part of krow corresponding to vars which have been lifted
    // alpha are the lifted coefficients with explicit storage of lifted zero
    // coefficients the a.getIndices() and alpha.getIndices() are identical
    OsiPackedVector a(cover);
    OsiPackedVector alpha;
    int i;
    for (i=0; i<cover.getNumElements(); i++){
      alpha.insert(cover.getIndices()[i],1.0);
    }
    // needed as an argument for exactSolveKnapsack
    int * x = new int[nRowElem]; 
    double psi_j=0.0;
    
    // Order alpha and a in nonincreasing order of alpha_j/a_j.  
    // alpha_1/a_1 >= alpha_2/a_2 >= ... >= alpha_n/a_n   
    // by defining this full-storage array "ratio" to be the external sort key.
    
    double * ratio= new double[nCols];
    memset(ratio, 0, (nCols*sizeof(double)));
    int alphasize =  alpha.getNumElements();
    int asize = a.getNumElements();
    assert( alphasize == asize );
    
    for (i=0; i<a.getNumElements(); i++){
      if (fabs(a.getElements()[i])> epsilon_ ){
        ratio[a.getIndices()[i]]=alpha.getElements()[i]/a.getElements()[i];
      }
      else {
        ratio[a.getIndices()[i]] = 0.0;
      }
    }
    
    // ToDo: would be nice to have sortkey NOT be full-storage vector
    CoinDecrSolutionOrdered dso2(ratio);
    // RLH:  JP, Is there a more efficient way?
    // The sort is identical for a and alpha, but I'm having to sort twice
    // here, and at every iteration in the loop below.
    a.sort(dso2);   
    alpha.sort(dso2);
    
#ifdef DEBUG
    // sanity check
    for ( i=1; i<a.getNumElements(); i++ ) {
      int alphai= alpha.getIndices()[i];
      int ai = a.getIndices()[i];
      assert( alphai == ai);
    }  
#endif  
    
    // Loop through the variables to be lifted, and lift.
    int j;
    for (j=0; j<remainder.getNumElements(); j++){
      // calculate the lifted coefficient of x_j = cutRhs-psi_j
      // where psi_j =  max of the current lefthand side of the cut
      // s.t. the knapsack corresponding to vars that have been lifted <= b-a_j
      
      // Note: For exact solve, must be sorted in
      // alpha_1/a_1>=alpha_2/a_2>=...>=alpha_n/a_n order
      exactSolveKnapsack(alpha.getNumElements(),
			 b-remainder.getElements()[j],
			 alpha.getElements(),a.getElements(),psi_j,x);
      alpha.insert(remainder.getIndices()[j],cutRhs-psi_j);
      a.insert(remainder.getIndices()[j],remainder.getElements()[j]);
      // if the lifted coefficient is non-zero (i.e. psi_j != cutRhs), add it
      // to the cut 
      if (fabs(cutRhs-psi_j)>epsilon_)
	 cut.insert(remainder.getIndices()[j],cutRhs-psi_j);
      
      ratio[remainder.getIndices()[j]]=
	 (cutRhs-psi_j)/remainder.getElements()[j];
      CoinDecrSolutionOrdered dso(ratio);
      a.sort(dso);   
      alpha.sort(dso);    
    }
    delete [] x;
    delete [] ratio;
  }

  // If the cut is violated, add it to the pool
  // if (sum over cut.getIndices())
  //    cut.element()*xstar > cover.getNumElements()-1, un-complement
  // and add it to the pool.
  double sum=0;
  int i;
  for (i=0; i<cut.getNumElements(); i++){
    sum+= cut.getElements()[i]*xstar[cut.getIndices()[i]];
  }
  if (sum > cutRhs){
#ifdef DEBUG
    printf("Sequentially lifted cover cut of size %i derived from cover of size %i.\n",cut.getNumElements(), cover.getNumElements());
    for (i=0; i<cut.getNumElements(); i++){
      printf("index %i, element %g, xstar value % g \n", cut.getIndices()[i],cut.getElements()[i], xstar[cut.getIndices()[i]]);
    }
    printf("The cutRhs = %g, and the alpha_j*xstar_j sum is %g\n\n", cutRhs, sum);
#endif
    
    int k;
    const int s = cut.getNumElements();
    const int * indices = cut.getIndices();
    double * elements = cut.getElements();
    for (k=0; k<s; k++){
      if (complement[indices[k]]) {
        // Negate the k'th element in packedVector cut
        // and correspondingly adjust the rhs
        elements[k] *= -1;
        cutRhs += elements[k];
      }
    }
    
    // Create a row cut and add it to the cut set
    OsiRowCut rc;
    rc.setRow(cut);
    rc.setLb(-DBL_MAX);
    rc.setUb(cutRhs);
    // ToDo: what's a meaningful effectivity?
    //  rc.setEffectiveness(1.0);
    cs.insert(rc);
  }
}

//-------------------------------------------------------------------
// liftCoverCut:  Given a cannonical knapsack inequality and a
//                cover, constructs a lift cover cut via
//                sequence-independent lifting.
//-------------------------------------------------------------------
int
CglLiftAndProject::liftCoverCut(
      double & b,
      int nRowElem,
      OsiPackedVector & cover,
      OsiPackedVector & remainder,
      OsiPackedVector & cut) const
{
  int i;
  // Given knapsack ax <=b, and a cover (e.g. cover corresponds to {0,...,nCover-1})
  // coverIndices are assumed in nondecr order of coverElements   
  // a_0>=a_1>=...>=a_(nCover-1)   

  // TODO: right now if the lifted coefficient is zero, 
  // then it's still in the cut. 
  // Should not carry explicit zero coefficients 

  // Calculate the sum of the knapsack coefficients of the cover variables 
  double sum = cover.sum();

  // Define lambda to be the "cover excess". 
  // By definition, lambda > 0. If this is not the case, something's screwy. Exit gracefully.
  double lambda = sum-b;
  if (lambda < epsilon_) {
    printf("lambda < epsilon....aborting. \n");
    std::cout << "lambda " << lambda << " epsilon " << epsilon_ << std::endl;
    abort();
  }

  // mu is vector of partial sums: 
  //   mu[i] = sum(j=0 to i) a_j where the cover is C={0,1,..,r}
  //   mu[0] = 0, mu[1]=a_0, mu[2]=a_0+a_1, etc.
  //   and C is assumed to be sorted in nondecreasing knapsack coefficient order.
  double * mu= new double[cover.getNumElements()+1];
  double * muMinusLambda= new double[cover.getNumElements()+1];
  memset(mu, 0, (cover.getNumElements()+1)*sizeof(double));
  memset(muMinusLambda, 0, (cover.getNumElements()+1)*sizeof(double));
  
  // mu[0] = 0, mu[1]= knapsack coef of cover element 0, etc.
  muMinusLambda[0]= -lambda;
  for(i=1; i<(cover.getNumElements()+1); i++){
    mu[i]=mu[i-1]+ cover.getElements()[i-1];
    muMinusLambda[i]=mu[i]-lambda;
  }

  cut.reserve(nRowElem);

  // the cut coefficent for the members of the cover is 1.0
  cut.setConstant(cover.getNumElements(),cover.getIndices(),1.0);
  
  // if f(z) is superadditive 
  int h;
  if (muMinusLambda[1] >= cover.getElements()[1]){
    for (h=0; h<remainder.getNumElements(); h++){
      if (remainder.getElements()[h] <= muMinusLambda[1]){
        // cutCoef[nCut] is 0, so don't bother storing 
      }    
      else{  
	// Todo: searching is inefficient. sort not in cover... 
        // change so that I sort remainder before the call to lift.
        int found=0;
        i=2;
        while (!found && i<(cover.getNumElements()+1)){
          if (remainder.getElements()[h] <= muMinusLambda[i]){
	    bool e = cut.isExistingIndex(remainder.getIndices()[h]);
            assert( !e );
            cut.insert( remainder.getIndices()[h], i-1.0 );
            found=1;
          }
          i++;
        }
        if (!found) {
#ifdef DEBUG
          printf("Error: Unable to fix lifted coefficient\n");
#endif
	  abort();
        }
      } // end else 
    }// end for each j not in C 
  } // end if f superadditive 

  // else use superadditive function g 
  else {
    double * rho= new double[cover.getNumElements()];
    rho[0]=lambda;
    for (i=1; i<cover.getNumElements(); i++){
      rho[i]=CoinMax(0.0, cover.getElements()[i]- muMinusLambda[1]);
    }
    
    int h;
    for (h=0; h<remainder.getNumElements(); h++){
      
      int found=0; // Todo: searcing is inefficient: sort...
      i=0;
      while(!found && i<cover.getNumElements()){
        if (remainder.getElements()[h] <= muMinusLambda[i+1]){
	  bool notE = !cut.isExistingIndex(remainder.getIndices()[h]);
          assert( notE );
          cut.insert( remainder.getIndices()[h], (double)i );
          found=1;
        }
        else if (remainder.getElements()[h] < muMinusLambda[i+1]+rho[i+1]){
	  bool notE = !cut.isExistingIndex(remainder.getIndices()[h]); 
          assert( notE );
          double cutCoef = i+1 
              - (muMinusLambda[i+1]+rho[i+1]-remainder.getElements()[h])/rho[1];    
          cut.insert( remainder.getIndices()[h], cutCoef );
          found=1;
        }
        i++;
      } // endwhile 
    } // end for j not in C
    delete [] rho;
  } // end else use g 

  delete [] muMinusLambda;
  delete [] mu;

  return 1;
}

//-------------------------------------------------------------------
// A goto-less implementation of the Horowitz-Sahni exact solution 
// procedure for solving knapsack problem.
//
// Reference: Martello and Toth, Knapsack Problems, Wiley, 1990, p30-31.
//
// ToDo: Implement a dynamic programming appraoch for case
//       of knapsacks with integral coefficients
//-------------------------------------------------------------------
int
CglLiftAndProject::exactSolveKnapsack(
       int n, 
       double c, 
       double const *pp, 
       double const *ww, 
       double & z, 
       int * x) const
{
  // The knapsack problem is to find:

  // max {sum(j=1,n) p_j*x_j st. sum (j=1,n)w_j*x_j <= c, x binary}

  // Notation:
  //     xhat : current solution vector
  //     zhat : current solution value = sum (j=1,n) p_j*xhat_j
  //     chat : current residual capacity = c - sum (j=1,n) w_j*xhat_j
  //     x    : best solution so far, n-vector.
  //     z    : value of the best solution so far =  sum (j=1,n) p_j*x_j
     

  // Input: n, the number of variables; 
  //        c, the rhs;
  //        p, n-vector of objective func. coefficients;
  //        w, n-vector of the row coeff.

  // Output: z, the optimal objective function value;
  //         x, the optimal (binary) solution n-vector;

  // Assumes items are sorted  p_1/w_1 >= p_2/w_2 >= ... >= p_n/w_n
  
  memset(x, 0, (n)*sizeof(int));
  int * xhat = new int[n+1];
  memset(xhat, 0, (n+1)*sizeof(int));
  int j;

  // set up: adding the extra element and
  // accounting for the FORTRAN vs C difference in indexing arrays.
  double * p = new double[n+2];
  double * w = new double[n+2];
  int ii;
  for (ii=1; ii<n+2; ii++){
    p[ii]=pp[ii-1];
    w[ii]=ww[ii-1];
  }

  // 1. initialize 
  double zhat = 0.0;
  z = 0.0;
  double chat = c;
  p[n+1] = 0.0;
  w[n+1]= DBL_MAX;
  j=1;

  while (1){
    // 2. compute upper bound u
    // "find r = min {i: sum k=j,i w_k>chat};"
    ii=j;
    double wSemiSum = w[j];
    double pSemiSum = p[j];
    while (wSemiSum <= chat && ii<n+2){
      ii++;
      wSemiSum+=w[ii];
      pSemiSum+=p[ii];
    }
    if (ii==n+2){
      printf("Exceeded iterator limit. Aborting...\n");
      abort();
    }
    // r = ii at this point 
    wSemiSum -= w[ii];
    pSemiSum -= p[ii];
    double u = pSemiSum + floor((chat - wSemiSum)*p[ii]/w[ii]);
    
    // "if (z >= zhat + u) goto 5: backtrack;"
    if (!(z >= zhat + u)) {
      do {
        // 3. perform a forward step 
        while (w[j] <= chat){
          chat = chat - w[j];
          zhat = zhat + p[j];
          xhat[j] = 1;
          j+=1;
        }
        if (j<=n) {
          xhat[j]= 0;
          j+=1;
        }
      } while(j==n); 

      // "if (j<n) goto 2: compute_ub;"
      if (j<n)
        continue;
      
      // 4. up date the best solution so far 
      if (zhat > z) {
        z=zhat;
        int k;
        for (k=0; k<n; k++){
          x[k]=xhat[k+1];
        }
      }
      j=n;
      if (xhat[n] == 1){
        chat = chat+ w[n];
        zhat = zhat-p[n];
        xhat[n]=0;
      }
    }
    // 5. backtrack 
    // "find i=max{k<j:xhat[k]=1};"
    int i=j-1; 
    while (!(xhat[i]==1) && i>0){
      i--;
    }
    
    // "if (no such i exists) return;"
    if (i==0){
      delete [] p;
      delete [] w;
      delete [] xhat;
      return 1;
    }
    
    chat = chat + w[i];
    zhat=zhat -p[i];
    xhat[i]=0;
    j=i+1;
    // "goto 2: compute_ub;"
  }
}

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
CglLiftAndProject::CglLiftAndProject ()
:
CglCutGenerator(),
beta_(1),
epsilon_(1.0e-08),
onetol_(1-epsilon_)
{
  // nothing to do here
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
CglLiftAndProject::CglLiftAndProject (const CglLiftAndProject & source) :
   CglCutGenerator(source),
   beta_(source.beta_),
   epsilon_(source.epsilon_),
   onetol_(source.onetol_)
{
  // Nothing to do here
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
CglLiftAndProject::~CglLiftAndProject ()
{
  // Nothing to do here
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
CglLiftAndProject &
CglLiftAndProject::operator=(
                                         const CglLiftAndProject& rhs)
{
  if (this != &rhs) {
    CglCutGenerator::operator=(rhs);
    beta_=rhs.beta_;
    epsilon_=rhs.epsilon_;
    onetol_=rhs.onetol_;
  }
  return *this;
}

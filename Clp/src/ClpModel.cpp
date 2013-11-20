// copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#include <cmath>
#include <cassert>
#include <cfloat>
#include <string>
#include <cstdio>
#include <iostream>


#include "CoinPragma.hpp"

#include "CoinHelperFunctions.hpp"
#include "CoinTime.hpp"
#include "ClpModel.hpp"
#include "ClpEventHandler.hpp"
#include "ClpPackedMatrix.hpp"
#ifndef SLIM_CLP
#include "ClpPlusMinusOneMatrix.hpp"
#endif
#ifndef CLP_NO_VECTOR
#include "CoinPackedVector.hpp"
#endif
#include "CoinIndexedVector.hpp"
#if SLIM_CLP==2
#define SLIM_NOIO
#endif
#ifndef SLIM_NOIO
#include "CoinMpsIO.hpp"
#include "CoinFileIO.hpp"
#include "CoinModel.hpp"
#endif
#include "ClpMessage.hpp"
#include "CoinMessage.hpp"
#include "ClpLinearObjective.hpp"
#ifndef SLIM_CLP
#include "ClpQuadraticObjective.hpp"
#include "CoinBuild.hpp"
#endif

//#############################################################################

ClpModel::ClpModel () :

  optimizationDirection_(1),
  objectiveValue_(0.0),
  smallElement_(1.0e-20),
  objectiveScale_(1.0),
  rhsScale_(1.0),
  numberRows_(0),
  numberColumns_(0),
  rowActivity_(NULL),
  columnActivity_(NULL),
  dual_(NULL),
  reducedCost_(NULL),
  rowLower_(NULL),
  rowUpper_(NULL),
  objective_(NULL),
  rowObjective_(NULL),
  columnLower_(NULL),
  columnUpper_(NULL),
  matrix_(NULL),
  rowCopy_(NULL),
  ray_(NULL),
  rowScale_(NULL),
  columnScale_(NULL),
  scalingFlag_(3),
  status_(NULL),
  integerType_(NULL),
  userPointer_(NULL),
  numberIterations_(0),
  solveType_(0),
  whatsChanged_(0),
  problemStatus_(-1),
  secondaryStatus_(0),
  lengthNames_(0),
  numberThreads_(0),
#ifndef CLP_NO_STD
  defaultHandler_(true),
  rowNames_(),
  columnNames_()
#else
  defaultHandler_(true)
#endif
{
  intParam_[ClpMaxNumIteration] = 2147483647;
  intParam_[ClpMaxNumIterationHotStart] = 9999999;

  dblParam_[ClpDualObjectiveLimit] = COIN_DBL_MAX;
  dblParam_[ClpPrimalObjectiveLimit] = COIN_DBL_MAX;
  dblParam_[ClpDualTolerance] = 1e-7;
  dblParam_[ClpPrimalTolerance] = 1e-7;
  dblParam_[ClpObjOffset] = 0.0;
  dblParam_[ClpMaxSeconds] = -1.0;
  dblParam_[ClpPresolveTolerance] = 1.0e-8;

#ifndef CLP_NO_STD
  strParam_[ClpProbName] = "ClpDefaultName";
#endif
  handler_ = new CoinMessageHandler();
  handler_->setLogLevel(1);
  eventHandler_ = new ClpEventHandler();
  messages_ = ClpMessage();
  coinMessages_ = CoinMessage();
  CoinSeedRandom(1234567);
}

//-----------------------------------------------------------------------------

ClpModel::~ClpModel ()
{
  if (defaultHandler_) {
    delete handler_;
    handler_ = NULL;
  }
  gutsOfDelete();
}
void ClpModel::gutsOfDelete()
{
  delete [] rowActivity_;
  rowActivity_=NULL;
  delete [] columnActivity_;
  columnActivity_=NULL;
  delete [] dual_;
  dual_=NULL;
  delete [] reducedCost_;
  reducedCost_=NULL;
  delete [] rowLower_;
  delete [] rowUpper_;
  delete [] rowObjective_;
  rowLower_=NULL;
  rowUpper_=NULL;
  rowObjective_=NULL;
  delete [] columnLower_;
  delete [] columnUpper_;
  delete objective_;
  columnLower_=NULL;
  columnUpper_=NULL;
  objective_=NULL;
  delete matrix_;
  matrix_=NULL;
  delete rowCopy_;
  rowCopy_=NULL;
  delete [] ray_;
  ray_ = NULL;
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
  delete [] integerType_;
  integerType_ = NULL;
  delete [] status_;
  status_=NULL;
  delete eventHandler_;
  eventHandler_=NULL;
  whatsChanged_=0;
}
//#############################################################################
void ClpModel::setPrimalTolerance( double value) 
{
  if (value>0.0&&value<1.0e10) 
    dblParam_[ClpPrimalTolerance]=value;
}
void ClpModel::setDualTolerance( double value) 
{
  if (value>0.0&&value<1.0e10)
    dblParam_[ClpDualTolerance]=value;
}
void ClpModel::setOptimizationDirection( double value) 
{
  optimizationDirection_=value;
}
void
ClpModel::gutsOfLoadModel (int numberRows, int numberColumns, 
		     const double* collb, const double* colub,   
		     const double* obj,
		     const double* rowlb, const double* rowub,
				const double * rowObjective)
{
  // save event handler in case already set
  ClpEventHandler * handler = eventHandler_->clone();
  gutsOfDelete();
  eventHandler_ = handler;
  numberRows_=numberRows;
  numberColumns_=numberColumns;
  rowActivity_=new double[numberRows_];
  columnActivity_=new double[numberColumns_];
  dual_=new double[numberRows_];
  reducedCost_=new double[numberColumns_];

  CoinZeroN(dual_,numberRows_);
  CoinZeroN(reducedCost_,numberColumns_);
  int iRow,iColumn;

  rowLower_=ClpCopyOfArray(rowlb,numberRows_,-COIN_DBL_MAX);
  rowUpper_=ClpCopyOfArray(rowub,numberRows_,COIN_DBL_MAX);
  double * objective=ClpCopyOfArray(obj,numberColumns_,0.0);
  objective_ = new ClpLinearObjective(objective,numberColumns_);
  delete [] objective;
  rowObjective_=ClpCopyOfArray(rowObjective,numberRows_);
  columnLower_=ClpCopyOfArray(collb,numberColumns_,0.0);
  columnUpper_=ClpCopyOfArray(colub,numberColumns_,COIN_DBL_MAX);
  // set default solution and clean bounds
  for (iRow=0;iRow<numberRows_;iRow++) {
    if (rowLower_[iRow]>0.0) {
      rowActivity_[iRow]=rowLower_[iRow];
    } else if (rowUpper_[iRow]<0.0) {
      rowActivity_[iRow]=rowUpper_[iRow];
    } else {
      rowActivity_[iRow]=0.0;
    }
    if (rowLower_[iRow]<-1.0e27)
      rowLower_[iRow]=-COIN_DBL_MAX;
    if (rowUpper_[iRow]>1.0e27)
      rowUpper_[iRow]=COIN_DBL_MAX;
  }
  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    if (columnLower_[iColumn]>0.0) {
      columnActivity_[iColumn]=columnLower_[iColumn];
    } else if (columnUpper_[iColumn]<0.0) {
      columnActivity_[iColumn]=columnUpper_[iColumn];
    } else {
      columnActivity_[iColumn]=0.0;
    }
    if (columnLower_[iColumn]<-1.0e27)
      columnLower_[iColumn]=-COIN_DBL_MAX;
    if (columnUpper_[iColumn]>1.0e27)
      columnUpper_[iColumn]=COIN_DBL_MAX;
  }
}
// This just loads up a row objective
void ClpModel::setRowObjective(const double * rowObjective)
{
  delete [] rowObjective_;
  rowObjective_=ClpCopyOfArray(rowObjective,numberRows_);
  whatsChanged_=0;
}
void
ClpModel::loadProblem (  const ClpMatrixBase& matrix,
		     const double* collb, const double* colub,   
		     const double* obj,
		     const double* rowlb, const double* rowub,
				const double * rowObjective)
{
  gutsOfLoadModel(matrix.getNumRows(),matrix.getNumCols(),
		  collb, colub, obj, rowlb, rowub, rowObjective);
  if (matrix.isColOrdered()) {
    matrix_=matrix.clone();
  } else {
    // later may want to keep as unknown class
    CoinPackedMatrix matrix2;
    matrix2.reverseOrderedCopyOf(*matrix.getPackedMatrix());
    matrix.releasePackedMatrix();
    matrix_=new ClpPackedMatrix(matrix2);
  }    
  matrix_->setDimensions(numberRows_,numberColumns_);
}
void
ClpModel::loadProblem (  const CoinPackedMatrix& matrix,
		     const double* collb, const double* colub,   
		     const double* obj,
		     const double* rowlb, const double* rowub,
				const double * rowObjective)
{
  gutsOfLoadModel(matrix.getNumRows(),matrix.getNumCols(),
		  collb, colub, obj, rowlb, rowub, rowObjective);
  if (matrix.isColOrdered()) {
    matrix_=new ClpPackedMatrix(matrix);
  } else {
    CoinPackedMatrix matrix2;
    matrix2.reverseOrderedCopyOf(matrix);
    matrix_=new ClpPackedMatrix(matrix2);
  }    
  matrix_->setDimensions(numberRows_,numberColumns_);
}
void
ClpModel::loadProblem ( 
			      const int numcols, const int numrows,
			      const CoinBigIndex* start, const int* index,
			      const double* value,
			      const double* collb, const double* colub,   
			      const double* obj,
			      const double* rowlb, const double* rowub,
			      const double * rowObjective)
{
  gutsOfLoadModel(numrows, numcols,
		  collb, colub, obj, rowlb, rowub, rowObjective);
  CoinPackedMatrix matrix(true,numrows,numcols,start[numcols],
			      value,index,start,NULL);
  matrix_ = new ClpPackedMatrix(matrix);
  matrix_->setDimensions(numberRows_,numberColumns_);
}
void
ClpModel::loadProblem ( 
			      const int numcols, const int numrows,
			      const CoinBigIndex* start, const int* index,
			      const double* value,const int* length,
			      const double* collb, const double* colub,   
			      const double* obj,
			      const double* rowlb, const double* rowub,
			      const double * rowObjective)
{
  gutsOfLoadModel(numrows, numcols,
		  collb, colub, obj, rowlb, rowub, rowObjective);
  // Compute number of elements
  int numberElements = 0;
  int i;
  for (i=0;i<numcols;i++) 
    numberElements += length[i];
  CoinPackedMatrix matrix(true,numrows,numcols,numberElements,
			      value,index,start,length);
  matrix_ = new ClpPackedMatrix(matrix);
}
#ifndef SLIM_NOIO
// This loads a model from a coinModel object - returns number of errors
int 
ClpModel::loadProblem (  CoinModel & modelObject,bool tryPlusMinusOne)
{
  if (modelObject.numberElements()==0)
    return 0;
  int numberErrors = 0;
  // Set arrays for normal use
  double * rowLower = modelObject.rowLowerArray();
  double * rowUpper = modelObject.rowUpperArray();
  double * columnLower = modelObject.columnLowerArray();
  double * columnUpper = modelObject.columnUpperArray();
  double * objective = modelObject.objectiveArray();
  int * integerType = modelObject.integerTypeArray();
  double * associated = modelObject.associatedArray();
  // If strings then do copies
  if (modelObject.stringsExist()) {
    numberErrors = modelObject.createArrays(rowLower, rowUpper, columnLower, columnUpper,
                                            objective, integerType,associated);
  }
  int numberRows = modelObject.numberRows();
  int numberColumns = modelObject.numberColumns();
  gutsOfLoadModel(numberRows, numberColumns,
		  columnLower, columnUpper, objective, rowLower, rowUpper, NULL);
  CoinBigIndex * startPositive = NULL;
  CoinBigIndex * startNegative = NULL;
  delete matrix_;
  if (tryPlusMinusOne) {
    startPositive = new CoinBigIndex[numberColumns+1];
    startNegative = new CoinBigIndex[numberColumns];
    modelObject.countPlusMinusOne(startPositive,startNegative,associated);
    if (startPositive[0]<0) {
      // no good
      tryPlusMinusOne=false;
      delete [] startPositive;
      delete [] startNegative;
    }
  }
#ifndef SLIM_CLP
  if (!tryPlusMinusOne) {
#endif
    CoinPackedMatrix matrix;
    modelObject.createPackedMatrix(matrix,associated);
    matrix_ = new ClpPackedMatrix(matrix);
#ifndef SLIM_CLP
  } else {
    // create +-1 matrix
    CoinBigIndex size = startPositive[numberColumns];
    int * indices = new int[size];
    modelObject.createPlusMinusOne(startPositive,startNegative,indices,
                                   associated);
    // Get good object
    ClpPlusMinusOneMatrix * matrix = new ClpPlusMinusOneMatrix();
    matrix->passInCopy(numberRows,numberColumns,
                       true,indices,startPositive,startNegative);
    matrix_=matrix;
  }
#endif
#ifndef CLP_NO_STD
  // Do names if wanted
  int numberItems;
  numberItems = modelObject.rowNames()->numberItems();
  if (numberItems) {
    const char *const * rowNames=modelObject.rowNames()->names();
    copyRowNames(rowNames,0,numberItems);
  }
  numberItems = modelObject.columnNames()->numberItems();
  if (numberItems) {
    const char *const * columnNames=modelObject.columnNames()->names();
    copyColumnNames(columnNames,0,numberItems);
  }
#endif
  // Do integers if wanted
  assert(integerType);
  for (int iColumn=0;iColumn<numberColumns;iColumn++) {
    if (integerType[iColumn])
      setInteger(iColumn);
  }
  if (rowLower!=modelObject.rowLowerArray()||
      columnLower!=modelObject.columnLowerArray()) {
    delete [] rowLower;
    delete [] rowUpper;
    delete [] columnLower;
    delete [] columnUpper;
    delete [] objective;
    delete [] integerType;
    delete [] associated;
    if (numberErrors)
      handler_->message(CLP_BAD_STRING_VALUES,messages_)
        <<numberErrors
        <<CoinMessageEol;
  }
  matrix_->setDimensions(numberRows_,numberColumns_);
  return numberErrors;
}
#endif
void
ClpModel::getRowBound(int iRow, double& lower, double& upper) const
{
  lower=-COIN_DBL_MAX;
  upper=COIN_DBL_MAX;
  if (rowUpper_)
    upper=rowUpper_[iRow];
  if (rowLower_)
    lower=rowLower_[iRow];
}
//------------------------------------------------------------------
#ifndef NDEBUG
// For errors to make sure print to screen
// only called in debug mode
static void indexError(int index,
			std::string methodName)
{
  std::cerr<<"Illegal index "<<index<<" in ClpModel::"<<methodName<<std::endl;
  throw CoinError("Illegal index",methodName,"ClpModel");
}
#endif
/* Set an objective function coefficient */
void 
ClpModel::setObjectiveCoefficient( int elementIndex, double elementValue )
{
#ifndef NDEBUG
  if (elementIndex<0||elementIndex>=numberColumns_) {
    indexError(elementIndex,"setObjectiveCoefficient");
  }
#endif
  objective()[elementIndex] = elementValue;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
/* Set a single row lower bound<br>
   Use -DBL_MAX for -infinity. */
void 
ClpModel::setRowLower( int elementIndex, double elementValue ) {
  if (elementValue<-1.0e27)
    elementValue=-COIN_DBL_MAX;
  rowLower_[elementIndex] = elementValue;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
      
/* Set a single row upper bound<br>
   Use DBL_MAX for infinity. */
void 
ClpModel::setRowUpper( int elementIndex, double elementValue ) {
  if (elementValue>1.0e27)
    elementValue=COIN_DBL_MAX;
  rowUpper_[elementIndex] = elementValue;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
    
/* Set a single row lower and upper bound */
void 
ClpModel::setRowBounds( int elementIndex,
	      double lower, double upper ) {
  if (lower<-1.0e27)
    lower=-COIN_DBL_MAX;
  if (upper>1.0e27)
    upper=COIN_DBL_MAX;
  CoinAssert (upper>=lower);
  rowLower_[elementIndex] = lower;
  rowUpper_[elementIndex] = upper;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
void ClpModel::setRowSetBounds(const int* indexFirst,
					    const int* indexLast,
					    const double* boundList)
{
#ifndef NDEBUG
  int n = numberRows_;
#endif
  double * lower = rowLower_;
  double * upper = rowUpper_;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
  while (indexFirst != indexLast) {
    const int iRow=*indexFirst++;
#ifndef NDEBUG
    if (iRow<0||iRow>=n) {
      indexError(iRow,"setRowSetBounds");
    }
#endif
    lower[iRow]= *boundList++;
    upper[iRow]= *boundList++;
    if (lower[iRow]<-1.0e27)
      lower[iRow]=-COIN_DBL_MAX;
    if (upper[iRow]>1.0e27)
      upper[iRow]=COIN_DBL_MAX;
    CoinAssert (upper[iRow]>=lower[iRow]);
  }
}
//-----------------------------------------------------------------------------
/* Set a single column lower bound<br>
   Use -DBL_MAX for -infinity. */
void 
ClpModel::setColumnLower( int elementIndex, double elementValue )
{
#ifndef NDEBUG
  int n = numberColumns_;
  if (elementIndex<0||elementIndex>=n) {
    indexError(elementIndex,"setColumnLower");
  }
#endif
  if (elementValue<-1.0e27)
    elementValue=-COIN_DBL_MAX;
  columnLower_[elementIndex] = elementValue;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
      
/* Set a single column upper bound<br>
   Use DBL_MAX for infinity. */
void 
ClpModel::setColumnUpper( int elementIndex, double elementValue )
{
#ifndef NDEBUG
  int n = numberColumns_;
  if (elementIndex<0||elementIndex>=n) {
    indexError(elementIndex,"setColumnUpper");
  }
#endif
  if (elementValue>1.0e27)
    elementValue=COIN_DBL_MAX;
  columnUpper_[elementIndex] = elementValue;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}

/* Set a single column lower and upper bound */
void 
ClpModel::setColumnBounds( int elementIndex,
				     double lower, double upper )
{
#ifndef NDEBUG
  int n = numberColumns_;
  if (elementIndex<0||elementIndex>=n) {
    indexError(elementIndex,"setColumnBounds");
  }
#endif
  if (lower<-1.0e27)
    lower=-COIN_DBL_MAX;
  if (upper>1.0e27)
    upper=COIN_DBL_MAX;
  columnLower_[elementIndex] = lower;
  columnUpper_[elementIndex] = upper;
  CoinAssert (upper>=lower);
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
}
void ClpModel::setColumnSetBounds(const int* indexFirst,
					    const int* indexLast,
					    const double* boundList)
{
  double * lower = columnLower_;
  double * upper = columnUpper_;
  whatsChanged_ = 0; // Can't be sure (use ClpSimplex to keep)
#ifndef NDEBUG
  int n = numberColumns_;
#endif
  while (indexFirst != indexLast) {
    const int iColumn=*indexFirst++;
#ifndef NDEBUG
    if (iColumn<0||iColumn>=n) {
      indexError(iColumn,"setColumnSetBounds");
    }
#endif
    lower[iColumn]= *boundList++;
    upper[iColumn]= *boundList++;
    CoinAssert (upper[iColumn]>=lower[iColumn]);
    if (lower[iColumn]<-1.0e27)
      lower[iColumn]=-COIN_DBL_MAX;
    if (upper[iColumn]>1.0e27)
      upper[iColumn]=COIN_DBL_MAX;
  }
}
//-----------------------------------------------------------------------------
//#############################################################################
// Copy constructor. 
ClpModel::ClpModel(const ClpModel &rhs, int scalingMode) :
  optimizationDirection_(rhs.optimizationDirection_),
  numberRows_(rhs.numberRows_),
  numberColumns_(rhs.numberColumns_)
{
  gutsOfCopy(rhs);
  if (scalingMode>=0&&matrix_&&
      matrix_->allElementsInRange(this,smallElement_,1.0e20)) {
    // really do scaling
    scalingFlag_=scalingMode;
    delete [] rowScale_;
    rowScale_ = NULL;
    delete [] columnScale_;
    columnScale_ = NULL;
    delete rowCopy_; // in case odd
    rowCopy_=NULL;
    if (scalingMode&&!matrix_->scale(this)) {
      // scaling worked - now apply
      gutsOfScaling();
      // pretend not scaled
      scalingFlag_ = -scalingFlag_;
    } else {
      // not scaled
      scalingFlag_=0;
    }
  }
  CoinSeedRandom(1234567);
}
// Assignment operator. This copies the data
ClpModel & 
ClpModel::operator=(const ClpModel & rhs)
{
  if (this != &rhs) {
    if (defaultHandler_) {
      delete handler_;
      handler_ = NULL;
    }
    gutsOfDelete();
    optimizationDirection_ = rhs.optimizationDirection_;
    numberRows_ = rhs.numberRows_;
    numberColumns_ = rhs.numberColumns_;
    gutsOfCopy(rhs);
  }
  return *this;
}
// Does most of copying
void 
ClpModel::gutsOfCopy(const ClpModel & rhs, bool trueCopy)
{
  defaultHandler_ = rhs.defaultHandler_;
  if (defaultHandler_) 
    handler_ = new CoinMessageHandler(*rhs.handler_);
   else 
    handler_ = rhs.handler_;
  eventHandler_ = rhs.eventHandler_->clone();
  messages_ = rhs.messages_;
  coinMessages_ = rhs.coinMessages_;
  intParam_[ClpMaxNumIteration] = rhs.intParam_[ClpMaxNumIteration];
  intParam_[ClpMaxNumIterationHotStart] = 
    rhs.intParam_[ClpMaxNumIterationHotStart];

  dblParam_[ClpDualObjectiveLimit] = rhs.dblParam_[ClpDualObjectiveLimit];
  dblParam_[ClpPrimalObjectiveLimit] = rhs.dblParam_[ClpPrimalObjectiveLimit];
  dblParam_[ClpDualTolerance] = rhs.dblParam_[ClpDualTolerance];
  dblParam_[ClpPrimalTolerance] = rhs.dblParam_[ClpPrimalTolerance];
  dblParam_[ClpObjOffset] = rhs.dblParam_[ClpObjOffset];
  dblParam_[ClpMaxSeconds] = rhs.dblParam_[ClpMaxSeconds];
  dblParam_[ClpPresolveTolerance] = rhs.dblParam_[ClpPresolveTolerance];
#ifndef CLP_NO_STD

  strParam_[ClpProbName] = rhs.strParam_[ClpProbName];
#endif

  optimizationDirection_ = rhs.optimizationDirection_;
  objectiveValue_=rhs.objectiveValue_;
  smallElement_ = rhs.smallElement_;
  objectiveScale_ = rhs.objectiveScale_;
  rhsScale_ = rhs.rhsScale_;
  numberIterations_ = rhs.numberIterations_;
  solveType_ = rhs.solveType_;
  whatsChanged_ = rhs.whatsChanged_;
  problemStatus_ = rhs.problemStatus_;
  secondaryStatus_ = rhs.secondaryStatus_;
  numberRows_ = rhs.numberRows_;
  numberColumns_ = rhs.numberColumns_;
  userPointer_ = rhs.userPointer_;
  scalingFlag_ = rhs.scalingFlag_;
  if (trueCopy) {
#ifndef CLP_NO_STD
    lengthNames_ = rhs.lengthNames_;
    if (lengthNames_) {
      rowNames_ = rhs.rowNames_;
      columnNames_ = rhs.columnNames_;
    }
#endif
    numberThreads_ = rhs.numberThreads_;
    integerType_ = CoinCopyOfArray(rhs.integerType_,numberColumns_);
    if (rhs.rowActivity_) {
      rowActivity_=new double[numberRows_];
      columnActivity_=new double[numberColumns_];
      dual_=new double[numberRows_];
      reducedCost_=new double[numberColumns_];
      ClpDisjointCopyN ( rhs.rowActivity_, numberRows_ ,
			  rowActivity_);
      ClpDisjointCopyN ( rhs.columnActivity_, numberColumns_ ,
			  columnActivity_);
      ClpDisjointCopyN ( rhs.dual_, numberRows_ , 
			  dual_);
      ClpDisjointCopyN ( rhs.reducedCost_, numberColumns_ ,
			  reducedCost_);
    } else {
      rowActivity_=NULL;
      columnActivity_=NULL;
      dual_=NULL;
      reducedCost_=NULL;
    }
    rowLower_ = ClpCopyOfArray ( rhs.rowLower_, numberRows_ );
    rowUpper_ = ClpCopyOfArray ( rhs.rowUpper_, numberRows_ );
    columnLower_ = ClpCopyOfArray ( rhs.columnLower_, numberColumns_ );
    columnUpper_ = ClpCopyOfArray ( rhs.columnUpper_, numberColumns_ );
    rowScale_ = ClpCopyOfArray(rhs.rowScale_,numberRows_);
    columnScale_ = ClpCopyOfArray(rhs.columnScale_,numberColumns_);
    if (rhs.objective_)
      objective_  = rhs.objective_->clone();
    else
      objective_ = NULL;
    rowObjective_ = ClpCopyOfArray ( rhs.rowObjective_, numberRows_ );
    status_ = ClpCopyOfArray( rhs.status_,numberColumns_+numberRows_);
    ray_ = NULL;
    if (problemStatus_==1&&!secondaryStatus_)
      ray_ = ClpCopyOfArray (rhs.ray_,numberRows_);
    else if (problemStatus_==2)
      ray_ = ClpCopyOfArray (rhs.ray_,numberColumns_);
    if (rhs.rowCopy_) {
      rowCopy_ = rhs.rowCopy_->clone();
    } else {
      rowCopy_=NULL;
    }
    matrix_=NULL;
    if (rhs.matrix_) {
      matrix_ = rhs.matrix_->clone();
    }
  } else {
    rowActivity_ = rhs.rowActivity_;
    columnActivity_ = rhs.columnActivity_;
    dual_ = rhs.dual_;
    reducedCost_ = rhs.reducedCost_;
    rowLower_ = rhs.rowLower_;
    rowUpper_ = rhs.rowUpper_;
    objective_ = rhs.objective_;
    rowObjective_ = rhs.rowObjective_;
    columnLower_ = rhs.columnLower_;
    columnUpper_ = rhs.columnUpper_;
    matrix_ = rhs.matrix_;
    rowCopy_ = NULL;
    ray_ = rhs.ray_;
    //rowScale_ = rhs.rowScale_;
    //columnScale_ = rhs.columnScale_;
    lengthNames_ = 0;
    numberThreads_ = rhs.numberThreads_;
#ifndef CLP_NO_STD
    rowNames_ = std::vector<std::string> ();
    columnNames_ = std::vector<std::string> ();
#endif
    integerType_ = NULL;
    status_ = rhs.status_;
  }
}
/* Borrow model.  This is so we dont have to copy large amounts
   of data around.  It assumes a derived class wants to overwrite
   an empty model with a real one - while it does an algorithm */
void 
ClpModel::borrowModel(ClpModel & rhs)
{
  if (defaultHandler_) {
    delete handler_;
    handler_ = NULL;
  }
  gutsOfDelete();
  optimizationDirection_ = rhs.optimizationDirection_;
  numberRows_ = rhs.numberRows_;
  numberColumns_ = rhs.numberColumns_;
  delete [] rhs.ray_;
  rhs.ray_=NULL;
  gutsOfCopy(rhs,false);
}
// Return model - nulls all arrays so can be deleted safely
void 
ClpModel::returnModel(ClpModel & otherModel)
{
  otherModel.objectiveValue_=objectiveValue_;
  otherModel.numberIterations_ = numberIterations_;
  otherModel.problemStatus_ = problemStatus_;
  otherModel.secondaryStatus_ = secondaryStatus_;
  rowActivity_ = NULL;
  columnActivity_ = NULL;
  dual_ = NULL;
  reducedCost_ = NULL;
  rowLower_ = NULL;
  rowUpper_ = NULL;
  objective_ = NULL;
  rowObjective_ = NULL;
  columnLower_ = NULL;
  columnUpper_ = NULL;
  matrix_ = NULL;
  rowCopy_ = NULL;
  delete [] otherModel.ray_;
  otherModel.ray_ = ray_;
  ray_ = NULL;
  //rowScale_=NULL;
  //columnScale_=NULL;
  // do status
  if (otherModel.status_!=status_) {
    delete [] otherModel.status_;
    otherModel.status_ = status_;
  }
  status_ = NULL;
  if (defaultHandler_) {
    delete handler_;
    handler_ = NULL;
  }
}
//#############################################################################
// Parameter related methods
//#############################################################################

bool
ClpModel::setIntParam(ClpIntParam key, int value)
{
  switch (key) {
  case ClpMaxNumIteration:
    if (value < 0)
      return false;
    break;
  case ClpMaxNumIterationHotStart:
    if (value < 0)
      return false;
    break;
  case ClpLastIntParam:
    return false;
  }
  intParam_[key] = value;
  return true;
}

//-----------------------------------------------------------------------------

bool
ClpModel::setDblParam(ClpDblParam key, double value)
{

  switch (key) {
  case ClpDualObjectiveLimit:
    break;

  case ClpPrimalObjectiveLimit:
    break;

  case ClpDualTolerance: 
    if (value<=0.0||value>1.0e10)
      return false;
    break;
    
  case ClpPrimalTolerance: 
    if (value<=0.0||value>1.0e10)
      return false;
    break;
    
  case ClpObjOffset: 
    break;

  case ClpMaxSeconds: 
    if(value>=0)
      value += CoinCpuTime();
    else
      value = -1.0;
    break;

  case ClpPresolveTolerance: 
    if (value<=0.0||value>1.0e10)
      return false;
    break;
    
  case ClpLastDblParam:
    return false;
  }
  dblParam_[key] = value;
  return true;
}

//-----------------------------------------------------------------------------
#ifndef CLP_NO_STD

bool
ClpModel::setStrParam(ClpStrParam key, const std::string & value)
{

  switch (key) {
  case ClpProbName:
    break;

  case ClpLastStrParam:
    return false;
  }
  strParam_[key] = value;
  return true;
}
#endif
// Useful routines
// Returns resized array and deletes incoming
double * resizeDouble(double * array , int size, int newSize, double fill,
		      bool createArray)
{
  if ((array||createArray)&&size!=newSize) {
    int i;
    double * newArray = new double[newSize];
    if (array)
      CoinMemcpyN(array,CoinMin(newSize,size),newArray);
    delete [] array;
    array = newArray;
    for (i=size;i<newSize;i++) 
      array[i]=fill;
  } 
  return array;
}
// Returns resized array and updates size
double * deleteDouble(double * array , int size, 
		      int number, const int * which,int & newSize)
{
  if (array) {
    int i ;
    char * deleted = new char[size];
    int numberDeleted=0;
    CoinZeroN(deleted,size);
    for (i=0;i<number;i++) {
      int j = which[i];
      if (j>=0&&j<size&&!deleted[j]) {
	numberDeleted++;
	deleted[j]=1;
      }
    }
    newSize = size-numberDeleted;
    double * newArray = new double[newSize];
    int put=0;
    for (i=0;i<size;i++) {
      if (!deleted[i]) {
	newArray[put++]=array[i];
      }
    }
    delete [] array;
    array = newArray;
    delete [] deleted;
  }
  return array;
}
char * deleteChar(char * array , int size, 
		  int number, const int * which,int & newSize,
		  bool ifDelete)
{
  if (array) {
    int i ;
    char * deleted = new char[size];
    int numberDeleted=0;
    CoinZeroN(deleted,size);
    for (i=0;i<number;i++) {
      int j = which[i];
      if (j>=0&&j<size&&!deleted[j]) {
	numberDeleted++;
	deleted[j]=1;
      }
    }
    newSize = size-numberDeleted;
    char * newArray = new char[newSize];
    int put=0;
    for (i=0;i<size;i++) {
      if (!deleted[i]) {
	newArray[put++]=array[i];
      }
    }
    if (ifDelete)
      delete [] array;
    array = newArray;
    delete [] deleted;
  }
  return array;
}
// Create empty ClpPackedMatrix
void 
ClpModel::createEmptyMatrix()
{
  delete matrix_;
  whatsChanged_ = 0;
  CoinPackedMatrix matrix2;
  matrix_=new ClpPackedMatrix(matrix2);
}
// Resizes 
void 
ClpModel::resize (int newNumberRows, int newNumberColumns)
{
  if (newNumberRows==numberRows_&&
      newNumberColumns==numberColumns_)
    return; // nothing to do
  whatsChanged_ = 0;
  rowActivity_ = resizeDouble(rowActivity_,numberRows_,
			      newNumberRows,0.0,true);
  dual_ = resizeDouble(dual_,numberRows_,
		       newNumberRows,0.0,true);
  rowObjective_ = resizeDouble(rowObjective_,numberRows_,
			       newNumberRows,0.0,false);
  rowLower_ = resizeDouble(rowLower_,numberRows_,
			   newNumberRows,-COIN_DBL_MAX,true);
  rowUpper_ = resizeDouble(rowUpper_,numberRows_,
			   newNumberRows,COIN_DBL_MAX,true);
  columnActivity_ = resizeDouble(columnActivity_,numberColumns_,
				 newNumberColumns,0.0,true);
  reducedCost_ = resizeDouble(reducedCost_,numberColumns_,
			      newNumberColumns,0.0,true);
  if (objective_)
    objective_->resize(newNumberColumns);
  else 
    objective_ = new ClpLinearObjective(NULL,newNumberColumns);
  columnLower_ = resizeDouble(columnLower_,numberColumns_,
			      newNumberColumns,0.0,true);
  columnUpper_ = resizeDouble(columnUpper_,numberColumns_,
			      newNumberColumns,COIN_DBL_MAX,true);
  if (newNumberRows<numberRows_) {
    int * which = new int[numberRows_-newNumberRows];
    int i;
    for (i=newNumberRows;i<numberRows_;i++) 
      which[i-newNumberRows]=i;
    matrix_->deleteRows(numberRows_-newNumberRows,which);
    delete [] which;
  }
  if (numberRows_!=newNumberRows||numberColumns_!=newNumberColumns) {
    // set state back to unknown
    problemStatus_ = -1;
    secondaryStatus_ = 0;
    delete [] ray_;
    ray_ = NULL;
  }
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
  if (status_) {
    if (newNumberColumns+newNumberRows) {
      unsigned char * tempC = new unsigned char [newNumberColumns+newNumberRows];
      unsigned char * tempR = tempC + newNumberColumns;
      memset(tempC,3,newNumberColumns*sizeof(unsigned char));
      memset(tempR,1,newNumberRows*sizeof(unsigned char));
      memcpy(tempC,status_,CoinMin(newNumberColumns,numberColumns_)*sizeof(unsigned char));
      memcpy(tempR,status_+numberColumns_,CoinMin(newNumberRows,numberRows_)*sizeof(unsigned char));
      delete [] status_;
      status_ = tempC;
    } else {
      // empty model - some systems don't like new [0]
      delete [] status_;
      status_ = NULL;
    }
  }
#ifndef CLP_NO_STD
  if (lengthNames_) {
    // redo row and column names
    if (numberRows_ < newNumberRows) {
      rowNames_.resize(newNumberRows);
      lengthNames_ = CoinMax(lengthNames_,8);
      char name[9];
      for (int iRow = numberRows_;iRow<newNumberRows;iRow++) {
        sprintf(name,"R%7.7d",iRow);
        rowNames_[iRow]=name;
      }
    }
    if (numberColumns_ < newNumberColumns) {
      columnNames_.resize(newNumberColumns);
      lengthNames_ = CoinMax(lengthNames_,8);
      char name[9];
      for (int iColumn = numberColumns_;iColumn<newNumberColumns;iColumn++) {
        sprintf(name,"C%7.7d",iColumn);
        columnNames_[iColumn]=name;
      }
    }
  }
#endif
  numberRows_ = newNumberRows;
  if (newNumberColumns<numberColumns_&&matrix_->getNumCols()) {
    int * which = new int[numberColumns_-newNumberColumns];
    int i;
    for (i=newNumberColumns;i<numberColumns_;i++) 
      which[i-newNumberColumns]=i;
    matrix_->deleteCols(numberColumns_-newNumberColumns,which);
    delete [] which;
  }
  if (integerType_) {
    char * temp = new char [newNumberColumns];
    CoinZeroN(temp,newNumberColumns);
    CoinMemcpyN(integerType_,
	   CoinMin(newNumberColumns,numberColumns_),temp);
    delete [] integerType_;
    integerType_ = temp;
  }
  numberColumns_ = newNumberColumns;
}
// Deletes rows
void 
ClpModel::deleteRows(int number, const int * which)
{
  if (!number)
    return; // nothing to do
  whatsChanged_ &= ~(1+2+4+8+16+32); // all except columns changed
  int newSize=0;
  rowActivity_ = deleteDouble(rowActivity_,numberRows_,
			      number, which, newSize);
  dual_ = deleteDouble(dual_,numberRows_,
			      number, which, newSize);
  rowObjective_ = deleteDouble(rowObjective_,numberRows_,
			      number, which, newSize);
  rowLower_ = deleteDouble(rowLower_,numberRows_,
			      number, which, newSize);
  rowUpper_ = deleteDouble(rowUpper_,numberRows_,
			      number, which, newSize);
  if (matrix_->getNumRows())
    matrix_->deleteRows(number,which);
  //matrix_->removeGaps();
  // status
  if (status_) {
    if (numberColumns_+newSize) {
      unsigned char * tempR  = (unsigned char *) deleteChar((char *)status_+numberColumns_,
							    numberRows_,
							    number, which, newSize,false);
      unsigned char * tempC = new unsigned char [numberColumns_+newSize];
      memcpy(tempC,status_,numberColumns_*sizeof(unsigned char));
      memcpy(tempC+numberColumns_,tempR,newSize*sizeof(unsigned char));
      delete [] tempR;
      delete [] status_;
      status_ = tempC;
    } else {
      // empty model - some systems don't like new [0]
      delete [] status_;
      status_ = NULL;
    }
  }
#ifndef CLP_NO_STD
  // Now works if which out of order
  if (lengthNames_) {
    char * mark = new char [numberRows_];
    CoinZeroN(mark,numberRows_);
    int i;
    for (i=0;i<number;i++)
      mark[which[i]]=1;
    int k=0;
    for ( i = 0; i < numberRows_; ++i) {
      if (!mark[i]) 
	rowNames_[k++] = rowNames_[i];
    }
    rowNames_.erase(rowNames_.begin()+k, rowNames_.end());
    delete [] mark;
  }
#endif
  numberRows_=newSize;
  // set state back to unknown
  problemStatus_ = -1;
  secondaryStatus_ = 0;
  delete [] ray_;
  ray_ = NULL;
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
}
// Deletes columns
void 
ClpModel::deleteColumns(int number, const int * which)
{
  if (!number)
    return; // nothing to do
  whatsChanged_ &= ~(1+2+4+8+64+128+256); // all except rows changed
  int newSize=0;
  columnActivity_ = deleteDouble(columnActivity_,numberColumns_,
			      number, which, newSize);
  reducedCost_ = deleteDouble(reducedCost_,numberColumns_,
			      number, which, newSize);
  objective_->deleteSome(number, which);
  columnLower_ = deleteDouble(columnLower_,numberColumns_,
			      number, which, newSize);
  columnUpper_ = deleteDouble(columnUpper_,numberColumns_,
			      number, which, newSize);
  // possible matrix is not full
  if (matrix_->getNumCols()<numberColumns_) {
    int * which2 = new int [number];
    int n=0;
    int nMatrix = matrix_->getNumCols();
    for (int i=0;i<number;i++) {
      if (which[i]<nMatrix)
	which2[n++]=which[i];
    }
    matrix_->deleteCols(n,which2);
    delete [] which2;
  } else {
    matrix_->deleteCols(number,which);
  }
  //matrix_->removeGaps();
  // status
  if (status_) {
    if (numberRows_+newSize) {
      unsigned char * tempC  = (unsigned char *) deleteChar((char *)status_,
							    numberColumns_,
							    number, which, newSize,false);
      unsigned char * temp = new unsigned char [numberRows_+newSize];
      memcpy(temp,tempC,newSize*sizeof(unsigned char));
      memcpy(temp+newSize,status_+numberColumns_,
	     numberRows_*sizeof(unsigned char));
      delete [] tempC;
      delete [] status_;
      status_ = temp;
    } else {
      // empty model - some systems don't like new [0]
      delete [] status_;
      status_ = NULL;
    }
  }
  integerType_ = deleteChar(integerType_,numberColumns_,
			    number, which, newSize,true);
#ifndef CLP_NO_STD
  // Now works if which out of order
  if (lengthNames_) {
    char * mark = new char [numberColumns_];
    CoinZeroN(mark,numberColumns_);
    int i;
    for (i=0;i<number;i++)
      mark[which[i]]=1;
    int k=0;
    for ( i = 0; i < numberColumns_; ++i) {
      if (!mark[i]) 
	columnNames_[k++] = columnNames_[i];
    }
    columnNames_.erase(columnNames_.begin()+k, columnNames_.end());
    delete [] mark;
  }
#endif
  numberColumns_=newSize;
  // set state back to unknown
  problemStatus_ = -1;
  secondaryStatus_ = 0;
  delete [] ray_;
  ray_ = NULL;
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
}
// Add one row
void 
ClpModel::addRow(int numberInRow, const int * columns,
                 const double * elements, double rowLower, double rowUpper)
{
  CoinBigIndex starts[2];
  starts[0]=0;
  starts[1]=numberInRow;
  addRows(1, &rowLower, &rowUpper,starts,columns,elements);
}
// Add rows
void 
ClpModel::addRows(int number, const double * rowLower, 
		  const double * rowUpper,
		  const int * rowStarts, const int * columns,
		  const double * elements)
{
  if (number) {
    whatsChanged_ &= ~(1+2+8+16+32); // all except columns changed
    int numberRowsNow = numberRows_;
    resize(numberRowsNow+number,numberColumns_);
    double * lower = rowLower_+numberRowsNow;
    double * upper = rowUpper_+numberRowsNow;
    int iRow;
    if (rowLower) {
      for (iRow = 0; iRow < number; iRow++) {
        double value = rowLower[iRow];
        if (value<-1.0e20)
          value = -COIN_DBL_MAX;
        lower[iRow]= value;
      }
    } else {
      for (iRow = 0; iRow < number; iRow++) {
        lower[iRow]= -COIN_DBL_MAX;
      }
    }
    if (rowUpper) {
      for (iRow = 0; iRow < number; iRow++) {
        double value = rowUpper[iRow];
        if (value>1.0e20)
          value = COIN_DBL_MAX;
        upper[iRow]= value;
      }
    } else {
      for (iRow = 0; iRow < number; iRow++) {
        upper[iRow]= COIN_DBL_MAX;
      }
    }
    // Deal with matrix
    
    delete rowCopy_;
    rowCopy_=NULL;
    if (!matrix_)
      createEmptyMatrix();
    delete [] rowScale_;
    rowScale_ = NULL;
    delete [] columnScale_;
    columnScale_ = NULL;
#ifndef CLP_NO_STD
    if (lengthNames_) {
      rowNames_.resize(numberRows_);
    }
#endif
    if (elements)
      matrix_->appendMatrix(number,0,rowStarts,columns,elements);
  }
}
// Add rows
void 
ClpModel::addRows(int number, const double * rowLower, 
		  const double * rowUpper,
		  const int * rowStarts, 
		  const int * rowLengths, const int * columns,
		  const double * elements)
{
  if (number) {
    CoinBigIndex numberElements=0;
    int iRow;
    for (iRow=0;iRow<number;iRow++) 
      numberElements += rowLengths[iRow];
    int * newStarts = new int[number+1];
    int * newIndex = new int[numberElements];
    double * newElements = new double[numberElements];
    numberElements=0;
    newStarts[0]=0;
    for (iRow=0;iRow<number;iRow++) {
      int iStart = rowStarts[iRow];
      int length = rowLengths[iRow];
      CoinMemcpyN(columns+iStart,length,newIndex+numberElements);
      CoinMemcpyN(elements+iStart,length,newElements+numberElements);
      numberElements += length;
      newStarts[iRow+1]=numberElements;
    }
    addRows(number, rowLower, rowUpper,
	    newStarts,newIndex,newElements);
    delete [] newStarts;
    delete [] newIndex;
    delete [] newElements;
  }
}
#ifndef CLP_NO_VECTOR
void 
ClpModel::addRows(int number, const double * rowLower, 
		  const double * rowUpper,
		  const CoinPackedVectorBase * const * rows)
{
  if (!number)
    return;
  whatsChanged_ &= ~(1+2+8+16+32); // all except columns changed
  int numberRowsNow = numberRows_;
  resize(numberRowsNow+number,numberColumns_);
  double * lower = rowLower_+numberRowsNow;
  double * upper = rowUpper_+numberRowsNow;
  int iRow;
  if (rowLower) {
    for (iRow = 0; iRow < number; iRow++) {
      double value = rowLower[iRow];
      if (value<-1.0e20)
	value = -COIN_DBL_MAX;
      lower[iRow]= value;
    }
  } else {
    for (iRow = 0; iRow < number; iRow++) {
      lower[iRow]= -COIN_DBL_MAX;
    }
  }
  if (rowUpper) {
    for (iRow = 0; iRow < number; iRow++) {
      double value = rowUpper[iRow];
      if (value>1.0e20)
	value = COIN_DBL_MAX;
      upper[iRow]= value;
    }
  } else {
    for (iRow = 0; iRow < number; iRow++) {
      upper[iRow]= COIN_DBL_MAX;
    }
  }
  // Deal with matrix

  delete rowCopy_;
  rowCopy_=NULL;
  if (!matrix_)
    createEmptyMatrix();
  if (rows)
    matrix_->appendRows(number,rows);
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
  if (lengthNames_) {
    rowNames_.resize(numberRows_);
  }
}
#endif
#ifndef SLIM_CLP
// Add rows from a build object
int
ClpModel::addRows(const CoinBuild & buildObject,bool tryPlusMinusOne,bool checkDuplicates)
{
  CoinAssertHint (buildObject.type()==0,"Looks as if both addRows and addCols being used"); // check correct
  int number = buildObject.numberRows();
  int numberErrors=0;
  if (number) {
    CoinBigIndex size=0;
    int iRow;
    double * lower = new double [number];
    double * upper = new double [number];
    if ((!matrix_||!matrix_->getNumElements())&&tryPlusMinusOne) {
      // See if can be +-1
      for (iRow=0;iRow<number;iRow++) {
        const int * columns;
        const double * elements;
        int numberElements = buildObject.row(iRow,lower[iRow],
                                                upper[iRow],
                                                columns,elements);
        for (int i=0;i<numberElements;i++) {
          // allow for zero elements
          if (elements[i]) {
            if (fabs(elements[i])==1.0) {
              size++;
            } else {
              // bad
              tryPlusMinusOne=false;
            }
          }
        }
        if (!tryPlusMinusOne)
          break;
      }
    } else {
      // Will add to whatever sort of matrix exists
      tryPlusMinusOne=false;
    }
    if (!tryPlusMinusOne) {
      CoinBigIndex numberElements = buildObject.numberElements();
      CoinBigIndex * starts = new CoinBigIndex [number+1];
      int * column = new int[numberElements];
      double * element = new double[numberElements];
      starts[0]=0;
      numberElements=0;
      for (iRow=0;iRow<number;iRow++) {
        const int * columns;
        const double * elements;
        int numberElementsThis = buildObject.row(iRow,lower[iRow],upper[iRow],
                                             columns,elements);
        CoinMemcpyN(columns,numberElementsThis,column+numberElements);
        CoinMemcpyN(elements,numberElementsThis,element+numberElements);
        numberElements += numberElementsThis;
        starts[iRow+1]=numberElements;
      }
      addRows(number, lower, upper,NULL);
      // make sure matrix has enough columns
      matrix_->setDimensions(-1,numberColumns_);
      numberErrors=matrix_->appendMatrix(number,0,starts,column,element,
                            checkDuplicates ? numberColumns_ : -1);
      delete [] starts;
      delete [] column;
      delete [] element;
    } else {
      char * which=NULL; // for duplicates
      if (checkDuplicates) {
        which = new char[numberColumns_];
        CoinZeroN(which,numberColumns_);
      }
      // build +-1 matrix
      // arrays already filled in
      addRows(number, lower, upper,NULL);
      CoinBigIndex * startPositive = new CoinBigIndex [numberColumns_+1];
      CoinBigIndex * startNegative = new CoinBigIndex [numberColumns_];
      int * indices = new int [size];
      CoinZeroN(startPositive,numberColumns_);
      CoinZeroN(startNegative,numberColumns_);
      int maxColumn=-1;
      // need two passes
      for (iRow=0;iRow<number;iRow++) {
        const int * columns;
        const double * elements;
        int numberElements = buildObject.row(iRow,lower[iRow],
                                                upper[iRow],
                                                columns,elements);
        for (int i=0;i<numberElements;i++) {
          int iColumn=columns[i];
          if (checkDuplicates) {
            if (iColumn>=numberColumns_) {
              if(which[iColumn])
                numberErrors++;
              else
                which[iColumn]=1;
            } else {
              numberErrors++;
              // and may as well switch off
              checkDuplicates=false;
            }
          }
          maxColumn = CoinMax(maxColumn,iColumn);
          if (elements[i]==1.0) {
            startPositive[iColumn]++;
          } else if (elements[i]==-1.0) {
            startNegative[iColumn]++;
          }
        }
        if (checkDuplicates) {
          for (int i=0;i<numberElements;i++) {
            int iColumn=columns[i];
            which[iColumn]=0;
          }
        }
      }
      // check size
      int numberColumns = maxColumn+1;
      CoinAssertHint (numberColumns<=numberColumns_,
                      "rows having column indices >= numberColumns_");
      size=0;
      int iColumn;
      for (iColumn=0;iColumn<numberColumns_;iColumn++) {
        CoinBigIndex n=startPositive[iColumn];
        startPositive[iColumn]=size;
        size+= n;
        n=startNegative[iColumn];
        startNegative[iColumn]=size;
        size+= n;
      }
      startPositive[numberColumns_]=size;
      for (iRow=0;iRow<number;iRow++) {
        const int * columns;
        const double * elements;
        int numberElements = buildObject.row(iRow,lower[iRow],
                                                upper[iRow],
                                                columns,elements);
        for (int i=0;i<numberElements;i++) {
          int iColumn=columns[i];
          maxColumn = CoinMax(maxColumn,iColumn);
          if (elements[i]==1.0) {
            CoinBigIndex position = startPositive[iColumn];
            indices[position]=iRow;
            startPositive[iColumn]++;
          } else if (elements[i]==-1.0) {
            CoinBigIndex position = startNegative[iColumn];
            indices[position]=iRow;
            startNegative[iColumn]++;
          }
        }
      }
      // and now redo starts
      for (iColumn=numberColumns_-1;iColumn>=0;iColumn--) {
        startPositive[iColumn+1]=startNegative[iColumn];
        startNegative[iColumn]=startPositive[iColumn];
      }
      startPositive[0]=0;
      for (iColumn=0;iColumn<numberColumns_;iColumn++) {
        CoinBigIndex start = startPositive[iColumn];
        CoinBigIndex end = startNegative[iColumn];
        std::sort(indices+start,indices+end);
        start = startNegative[iColumn];
        end = startPositive[iColumn+1];
        std::sort(indices+start,indices+end);
      }
      // Get good object
      delete matrix_;
      ClpPlusMinusOneMatrix * matrix = new ClpPlusMinusOneMatrix();
      matrix->passInCopy(numberRows_,numberColumns,
                         true,indices,startPositive,startNegative);
      matrix_=matrix;
      delete [] which;
    }
    delete [] lower;
    delete [] upper;
    // make sure matrix correct size
    matrix_->setDimensions(numberRows_,numberColumns_);
  }
  return numberErrors;
}
#endif
#ifndef SLIM_NOIO
// Add rows from a model object
int 
ClpModel::addRows( CoinModel & modelObject,bool tryPlusMinusOne,bool checkDuplicates)
{
  if (modelObject.numberElements()==0)
    return 0;
  bool goodState=true;
  int numberErrors=0;
  if (modelObject.columnLowerArray()) {
    // some column information exists
    int numberColumns2 = modelObject.numberColumns();
    const double * columnLower = modelObject.columnLowerArray();
    const double * columnUpper = modelObject.columnUpperArray();
    const double * objective = modelObject.objectiveArray();
    const int * integerType = modelObject.integerTypeArray();
    for (int i=0;i<numberColumns2;i++) {
      if (columnLower[i]!=0.0) 
        goodState=false;
      if (columnUpper[i]!=COIN_DBL_MAX) 
        goodState=false;
      if (objective[i]!=0.0) 
        goodState=false;
      if (integerType[i]!=0)
        goodState=false;
    }
  }
  if (goodState) {
    // can do addRows
    // Set arrays for normal use
    double * rowLower = modelObject.rowLowerArray();
    double * rowUpper = modelObject.rowUpperArray();
    double * columnLower = modelObject.columnLowerArray();
    double * columnUpper = modelObject.columnUpperArray();
    double * objective = modelObject.objectiveArray();
    int * integerType = modelObject.integerTypeArray();
    double * associated = modelObject.associatedArray();
    // If strings then do copies
    if (modelObject.stringsExist()) {
      numberErrors = modelObject.createArrays(rowLower, rowUpper, columnLower, columnUpper,
                                 objective, integerType,associated);
    }
    int numberRows = numberRows_; // save number of rows
    int numberRows2 = modelObject.numberRows();
    if (numberRows2&&!numberErrors) {
      CoinBigIndex * startPositive = NULL;
      CoinBigIndex * startNegative = NULL;
      int numberColumns = modelObject.numberColumns();
      if ((!matrix_||!matrix_->getNumElements())&&!numberRows&&tryPlusMinusOne) {
        startPositive = new CoinBigIndex[numberColumns+1];
        startNegative = new CoinBigIndex[numberColumns];
        modelObject.countPlusMinusOne(startPositive,startNegative,associated);
        if (startPositive[0]<0) {
          // no good
          tryPlusMinusOne=false;
          delete [] startPositive;
          delete [] startNegative;
        }
      } else {
        // Will add to whatever sort of matrix exists
        tryPlusMinusOne=false;
      }
      assert (rowLower);
      addRows(numberRows2, rowLower, rowUpper,NULL,NULL,NULL);
#ifndef SLIM_CLP
      if (!tryPlusMinusOne) {
#endif
        CoinPackedMatrix matrix;
        modelObject.createPackedMatrix(matrix,associated);
        assert (!matrix.getExtraGap());
        if (matrix_->getNumRows()) {
          // matrix by rows
          matrix.reverseOrdering();
          assert (!matrix.getExtraGap());
          const int * column = matrix.getIndices();
          //const int * rowLength = matrix.getVectorLengths();
          const CoinBigIndex * rowStart = matrix.getVectorStarts();
          const double * element = matrix.getElements();
          // make sure matrix has enough columns
          matrix_->setDimensions(-1,numberColumns_);
          numberErrors+=matrix_->appendMatrix(numberRows2,0,rowStart,column,element,
                                checkDuplicates ? numberColumns_ : -1);
        } else {
          delete matrix_;
          matrix_ = new ClpPackedMatrix(matrix);
        }
#ifndef SLIM_CLP
      } else {
        // create +-1 matrix
        CoinBigIndex size = startPositive[numberColumns];
        int * indices = new int[size];
        modelObject.createPlusMinusOne(startPositive,startNegative,indices,
                                       associated);
        // Get good object
        ClpPlusMinusOneMatrix * matrix = new ClpPlusMinusOneMatrix();
        matrix->passInCopy(numberRows2,numberColumns,
                           true,indices,startPositive,startNegative);
        delete matrix_;
        matrix_=matrix;
      }
      // Do names if wanted
      if (modelObject.rowNames()->numberItems()) {
        const char *const * rowNames=modelObject.rowNames()->names();
        copyRowNames(rowNames,numberRows,numberRows_);
      }
#endif
    }
    if (rowLower!=modelObject.rowLowerArray()) {
      delete [] rowLower;
      delete [] rowUpper;
      delete [] columnLower;
      delete [] columnUpper;
      delete [] objective;
      delete [] integerType;
      delete [] associated;
      if (numberErrors)
        handler_->message(CLP_BAD_STRING_VALUES,messages_)
          <<numberErrors
          <<CoinMessageEol;
    }
    return numberErrors;
  } else {
    // not suitable for addRows
    handler_->message(CLP_COMPLICATED_MODEL,messages_)
      <<modelObject.numberRows()
      <<modelObject.numberColumns()
      <<CoinMessageEol;
    return -1;
  }
}
#endif
// Add one column
void 
ClpModel::addColumn(int numberInColumn,
                 const int * rows,
                 const double * elements,
                 double columnLower, 
                 double  columnUpper,
                 double  objective)
{
  CoinBigIndex starts[2];
  starts[0]=0;
  starts[1]=numberInColumn;
  addColumns(1, &columnLower, &columnUpper,&objective,starts,rows,elements);
}
// Add columns
void 
ClpModel::addColumns(int number, const double * columnLower, 
		     const double * columnUpper,
		     const double * objIn,
		     const int * columnStarts, const int * rows,
		     const double * elements)
{
  // Create a list of CoinPackedVectors
  if (number) {
    whatsChanged_ &= ~(1+2+4+64+128+256); // all except rows changed
    int numberColumnsNow = numberColumns_;
    resize(numberRows_,numberColumnsNow+number);
    double * lower = columnLower_+numberColumnsNow;
    double * upper = columnUpper_+numberColumnsNow;
    double * obj = objective()+numberColumnsNow;
    int iColumn;
    if (columnLower) {
      for (iColumn = 0; iColumn < number; iColumn++) {
        double value = columnLower[iColumn];
        if (value<-1.0e20)
          value = -COIN_DBL_MAX;
        lower[iColumn]= value;
      }
    } else {
      for (iColumn = 0; iColumn < number; iColumn++) {
        lower[iColumn]= 0.0;
      }
    }
    if (columnUpper) {
      for (iColumn = 0; iColumn < number; iColumn++) {
        double value = columnUpper[iColumn];
        if (value>1.0e20)
          value = COIN_DBL_MAX;
        upper[iColumn]= value;
      }
    } else {
      for (iColumn = 0; iColumn < number; iColumn++) {
        upper[iColumn]= COIN_DBL_MAX;
      }
    }
    if (objIn) {
      for (iColumn = 0; iColumn < number; iColumn++) {
        obj[iColumn] = objIn[iColumn];
      }
    } else {
      for (iColumn = 0; iColumn < number; iColumn++) {
        obj[iColumn]= 0.0;
      }
    }
    // Deal with matrix
    
    delete rowCopy_;
    rowCopy_=NULL;
    if (!matrix_)
      createEmptyMatrix();
    delete [] rowScale_;
    rowScale_ = NULL;
    delete [] columnScale_;
    columnScale_ = NULL;
#ifndef CLP_NO_STD
    if (lengthNames_) {
      columnNames_.resize(numberColumns_);
    }
#endif
    if (elements)
      matrix_->appendMatrix(number,1,columnStarts,rows,elements);
  }
}
// Add columns
void 
ClpModel::addColumns(int number, const double * columnLower, 
		     const double * columnUpper,
		     const double * objIn,
		     const int * columnStarts, 
		     const int * columnLengths, const int * rows,
		     const double * elements)
{
  if (number) {
    CoinBigIndex numberElements=0;
    int iColumn;
    for (iColumn=0;iColumn<number;iColumn++) 
      numberElements += columnLengths[iColumn];
    int * newStarts = new int[number+1];
    int * newIndex = new int[numberElements];
    double * newElements = new double[numberElements];
    numberElements=0;
    newStarts[0]=0;
    for (iColumn=0;iColumn<number;iColumn++) {
      int iStart = columnStarts[iColumn];
      int length = columnLengths[iColumn];
      CoinMemcpyN(rows+iStart,length,newIndex+numberElements);
      CoinMemcpyN(elements+iStart,length,newElements+numberElements);
      numberElements += length;
      newStarts[iColumn+1]=numberElements;
    }
    addColumns(number, columnLower, columnUpper,objIn,
	    newStarts,newIndex,newElements);
    delete [] newStarts;
    delete [] newIndex;
    delete [] newElements;
  }
}
#ifndef CLP_NO_VECTOR
void 
ClpModel::addColumns(int number, const double * columnLower, 
		     const double * columnUpper,
		     const double * objIn,
		     const CoinPackedVectorBase * const * columns)
{
  if (!number)
    return;
  whatsChanged_ &= ~(1+2+4+64+128+256); // all except rows changed
  int numberColumnsNow = numberColumns_;
  resize(numberRows_,numberColumnsNow+number);
  double * lower = columnLower_+numberColumnsNow;
  double * upper = columnUpper_+numberColumnsNow;
  double * obj = objective()+numberColumnsNow;
  int iColumn;
  if (columnLower) {
    for (iColumn = 0; iColumn < number; iColumn++) {
      double value = columnLower[iColumn];
      if (value<-1.0e20)
	value = -COIN_DBL_MAX;
      lower[iColumn]= value;
    }
  } else {
    for (iColumn = 0; iColumn < number; iColumn++) {
      lower[iColumn]= 0.0;
    }
  }
  if (columnUpper) {
    for (iColumn = 0; iColumn < number; iColumn++) {
      double value = columnUpper[iColumn];
      if (value>1.0e20)
	value = COIN_DBL_MAX;
      upper[iColumn]= value;
    }
  } else {
    for (iColumn = 0; iColumn < number; iColumn++) {
      upper[iColumn]= COIN_DBL_MAX;
    }
  }
  if (objIn) {
    for (iColumn = 0; iColumn < number; iColumn++) {
      obj[iColumn] = objIn[iColumn];
    }
  } else {
    for (iColumn = 0; iColumn < number; iColumn++) {
      obj[iColumn]= 0.0;
    }
  }
  // Deal with matrix

  delete rowCopy_;
  rowCopy_=NULL;
  if (!matrix_)
    createEmptyMatrix();
  if (columns)
    matrix_->appendCols(number,columns);
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
  if (lengthNames_) {
    columnNames_.resize(numberColumns_);
  }
}
#endif
#ifndef SLIM_CLP
// Add columns from a build object
int 
ClpModel::addColumns(const CoinBuild & buildObject,bool tryPlusMinusOne,bool checkDuplicates)
{
  CoinAssertHint (buildObject.type()==1,"Looks as if both addRows and addCols being used"); // check correct
  int number = buildObject.numberColumns();
  int numberErrors=0;
  if (number) {
    CoinBigIndex size=0;
    int maximumLength=0;
    double * lower = new double [number];
    double * upper = new double [number];
    int iColumn;
    double * objective = new double [number];
    if ((!matrix_||!matrix_->getNumElements())&&tryPlusMinusOne) {
      // See if can be +-1
      for (iColumn=0;iColumn<number;iColumn++) {
        const int * rows;
        const double * elements;
        int numberElements = buildObject.column(iColumn,lower[iColumn],
                                                upper[iColumn],objective[iColumn],
                                                rows,elements);
        maximumLength = CoinMax(maximumLength,numberElements);
        for (int i=0;i<numberElements;i++) {
          // allow for zero elements
          if (elements[i]) {
            if (fabs(elements[i])==1.0) {
              size++;
            } else {
              // bad
              tryPlusMinusOne=false;
            }
          }
        }
        if (!tryPlusMinusOne)
          break;
      }
    } else {
      // Will add to whatever sort of matrix exists
      tryPlusMinusOne=false;
    }
    if (!tryPlusMinusOne) {
      CoinBigIndex numberElements = buildObject.numberElements();
      CoinBigIndex * starts = new CoinBigIndex [number+1];
      int * row = new int[numberElements];
      double * element = new double[numberElements];
      starts[0]=0;
      numberElements=0;
      for (iColumn=0;iColumn<number;iColumn++) {
        const int * rows;
        const double * elements;
        int numberElementsThis = buildObject.column(iColumn,lower[iColumn],upper[iColumn],
                                             objective[iColumn],rows,elements);
        CoinMemcpyN(rows,numberElementsThis,row+numberElements);
        CoinMemcpyN(elements,numberElementsThis,element+numberElements);
        numberElements += numberElementsThis;
        starts[iColumn+1]=numberElements;
      }
      addColumns(number, lower, upper,objective,NULL);
      // make sure matrix has enough rows
      matrix_->setDimensions(numberRows_,-1);
      numberErrors=matrix_->appendMatrix(number,1,starts,row,element,
                            checkDuplicates ? numberRows_ : -1);
      delete [] starts;
      delete [] row;
      delete [] element;
    } else {
      // arrays already filled in
      addColumns(number, lower, upper,objective,NULL);
      char * which=NULL; // for duplicates
      if (checkDuplicates) {
        which = new char[numberRows_];
        CoinZeroN(which,numberRows_);
      }
      // build +-1 matrix
      CoinBigIndex * startPositive = new CoinBigIndex [number+1];
      CoinBigIndex * startNegative = new CoinBigIndex [number];
      int * indices = new int [size];
      int * neg = new int[maximumLength];
      startPositive[0]=0;
      size=0;
      int maxRow=-1;
      for (iColumn=0;iColumn<number;iColumn++) {
        const int * rows;
        const double * elements;
        int numberElements = buildObject.column(iColumn,lower[iColumn],
                                                upper[iColumn],objective[iColumn],
                                                rows,elements);
        int nNeg=0;
        CoinBigIndex start = size;
        for (int i=0;i<numberElements;i++) {
          int iRow=rows[i];
          if (checkDuplicates) {
            if (iRow>=numberRows_) {
              if(which[iRow])
                numberErrors++;
              else
                which[iRow]=1;
            } else {
              numberErrors++;
              // and may as well switch off
              checkDuplicates=false;
            }
          }
          maxRow = CoinMax(maxRow,iRow);
          if (elements[i]==1.0) {
            indices[size++]=iRow;
          } else if (elements[i]==-1.0) {
            neg[nNeg++]=iRow;
          }
        }
        std::sort(indices+start,indices+size);
        std::sort(neg,neg+nNeg);
        startNegative[iColumn]=size;
        CoinMemcpyN(neg,nNeg,indices+size);
        size += nNeg;
        startPositive[iColumn+1]=size;
      }
      delete [] neg;
      // check size
      assert (maxRow+1<=numberRows_);
      // Get good object
      delete matrix_;
      ClpPlusMinusOneMatrix * matrix = new ClpPlusMinusOneMatrix();
      matrix->passInCopy(numberRows_,number,true,indices,startPositive,startNegative);
      matrix_=matrix;
      delete [] which;
    }
    delete [] objective;
    delete [] lower;
    delete [] upper;
  }
  return 0;
}
#endif
#ifndef SLIM_NOIO
// Add columns from a model object
int 
ClpModel::addColumns( CoinModel & modelObject,bool tryPlusMinusOne,bool checkDuplicates)
{
  if (modelObject.numberElements()==0)
    return 0;
  bool goodState=true;
  if (modelObject.rowLowerArray()) {
    // some row information exists
    int numberRows2 = modelObject.numberRows();
    const double * rowLower = modelObject.rowLowerArray();
    const double * rowUpper = modelObject.rowUpperArray();
    for (int i=0;i<numberRows2;i++) {
      if (rowLower[i]!=-COIN_DBL_MAX) 
        goodState=false;
      if (rowUpper[i]!=COIN_DBL_MAX) 
        goodState=false;
    }
  }
  if (goodState) {
    // can do addColumns
    int numberErrors = 0;
    // Set arrays for normal use
    double * rowLower = modelObject.rowLowerArray();
    double * rowUpper = modelObject.rowUpperArray();
    double * columnLower = modelObject.columnLowerArray();
    double * columnUpper = modelObject.columnUpperArray();
    double * objective = modelObject.objectiveArray();
    int * integerType = modelObject.integerTypeArray();
    double * associated = modelObject.associatedArray();
    // If strings then do copies
    if (modelObject.stringsExist()) {
      numberErrors = modelObject.createArrays(rowLower, rowUpper, columnLower, columnUpper,
                                 objective, integerType,associated);
    }
    int numberColumns = numberColumns_; // save number of columns
    int numberColumns2 = modelObject.numberColumns();
    if (numberColumns2&&!numberErrors) {
      CoinBigIndex * startPositive = NULL;
      CoinBigIndex * startNegative = NULL;
      if ((!matrix_||!matrix_->getNumElements())&&!numberColumns&&tryPlusMinusOne) {
        startPositive = new CoinBigIndex[numberColumns2+1];
        startNegative = new CoinBigIndex[numberColumns2];
        modelObject.countPlusMinusOne(startPositive,startNegative,associated);
        if (startPositive[0]<0) {
          // no good
          tryPlusMinusOne=false;
          delete [] startPositive;
          delete [] startNegative;
        }
      } else {
        // Will add to whatever sort of matrix exists
        tryPlusMinusOne=false;
      }
      assert (columnLower);
      addColumns(numberColumns2, columnLower, columnUpper,objective, NULL,NULL,NULL);
#ifndef SLIM_CLP
      if (!tryPlusMinusOne) {
#endif
        CoinPackedMatrix matrix;
        modelObject.createPackedMatrix(matrix,associated);
        assert (!matrix.getExtraGap());
        if (matrix_->getNumCols()) {
          const int * row = matrix.getIndices();
          //const int * columnLength = matrix.getVectorLengths();
          const CoinBigIndex * columnStart = matrix.getVectorStarts();
          const double * element = matrix.getElements();
          // make sure matrix has enough rows
          matrix_->setDimensions(numberRows_,-1);
          numberErrors+=matrix_->appendMatrix(numberColumns2,1,columnStart,row,element,
                                checkDuplicates ? numberRows_ : -1);
        } else {
          delete matrix_;
          matrix_ = new ClpPackedMatrix(matrix);
        }
#ifndef SLIM_CLP
      } else {
        // create +-1 matrix
        CoinBigIndex size = startPositive[numberColumns2];
        int * indices = new int[size];
        modelObject.createPlusMinusOne(startPositive,startNegative,indices,
                                       associated);
        // Get good object
        ClpPlusMinusOneMatrix * matrix = new ClpPlusMinusOneMatrix();
        matrix->passInCopy(numberRows_,numberColumns2,
                           true,indices,startPositive,startNegative);
        delete matrix_;
        matrix_=matrix;
      }
#endif
#ifndef CLP_NO_STD
      // Do names if wanted
      if (modelObject.columnNames()->numberItems()) {
        const char *const * columnNames=modelObject.columnNames()->names();
        copyColumnNames(columnNames,numberColumns,numberColumns_);
      }
#endif
      // Do integers if wanted
      assert(integerType);
      for (int iColumn=0;iColumn<numberColumns2;iColumn++) {
        if (integerType[iColumn])
          setInteger(iColumn+numberColumns);
      }
    }
    if (columnLower!=modelObject.columnLowerArray()) {
      delete [] rowLower;
      delete [] rowUpper;
      delete [] columnLower;
      delete [] columnUpper;
      delete [] objective;
      delete [] integerType;
      delete [] associated;
      if (numberErrors)
        handler_->message(CLP_BAD_STRING_VALUES,messages_)
          <<numberErrors
          <<CoinMessageEol;
    }
    return numberErrors;
  } else {
    // not suitable for addColumns
    handler_->message(CLP_COMPLICATED_MODEL,messages_)
      <<modelObject.numberRows()
      <<modelObject.numberColumns()
      <<CoinMessageEol;
    return -1;
  }
}
#endif
// chgRowLower
void 
ClpModel::chgRowLower(const double * rowLower) 
{
  int numberRows = numberRows_;
  int iRow;
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  if (rowLower) {
    for (iRow = 0; iRow < numberRows; iRow++) {
      double value = rowLower[iRow];
      if (value<-1.0e20)
		 value = -COIN_DBL_MAX;
      rowLower_[iRow]= value;
    }
  } else {
    for (iRow = 0; iRow < numberRows; iRow++) {
      rowLower_[iRow]= -COIN_DBL_MAX;
    }
  }
}
// chgRowUpper
void 
ClpModel::chgRowUpper(const double * rowUpper) 
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  int numberRows = numberRows_;
  int iRow;
  if (rowUpper) {
    for (iRow = 0; iRow < numberRows; iRow++) {
      double value = rowUpper[iRow];
      if (value>1.0e20)
		 value = COIN_DBL_MAX;
      rowUpper_[iRow]= value;
    }
  } else {
    for (iRow = 0; iRow < numberRows; iRow++) {
      rowUpper_[iRow]= COIN_DBL_MAX;;
    }
  }
}
// chgColumnLower
void 
ClpModel::chgColumnLower(const double * columnLower) 
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  int numberColumns = numberColumns_;
  int iColumn;
  if (columnLower) {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      double value = columnLower[iColumn];
      if (value<-1.0e20)
		 value = -COIN_DBL_MAX;
      columnLower_[iColumn]= value;
    }
  } else {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      columnLower_[iColumn]= 0.0;
    }
  }
}
// chgColumnUpper
void 
ClpModel::chgColumnUpper(const double * columnUpper) 
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  int numberColumns = numberColumns_;
  int iColumn;
  if (columnUpper) {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      double value = columnUpper[iColumn];
      if (value>1.0e20)
		 value = COIN_DBL_MAX;
      columnUpper_[iColumn]= value;
    }
  } else {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      columnUpper_[iColumn]= COIN_DBL_MAX;;
    }
  }
}
// chgObjCoefficients
void 
ClpModel::chgObjCoefficients(const double * objIn) 
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  double * obj = objective();
  int numberColumns = numberColumns_;
  int iColumn;
  if (objIn) {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      obj[iColumn] = objIn[iColumn];
    }
  } else {
    for (iColumn = 0; iColumn < numberColumns; iColumn++) {
      obj[iColumn]= 0.0;
    }
  }
}
// Infeasibility/unbounded ray (NULL returned if none/wrong)
double * 
ClpModel::infeasibilityRay() const
{
  double * array = NULL;
  if (problemStatus_==1&&!secondaryStatus_) 
    array = ClpCopyOfArray(ray_,numberRows_);
  return array;
}
double * 
ClpModel::unboundedRay() const
{
  double * array = NULL;
  if (problemStatus_==2) 
    array = ClpCopyOfArray(ray_,numberColumns_);
  return array;
}
void 
ClpModel::setMaximumIterations(int value)
{
  if(value>=0)
    intParam_[ClpMaxNumIteration]=value;
}
void 
ClpModel::setMaximumSeconds(double value)
{
  if(value>=0)
    dblParam_[ClpMaxSeconds]=value+CoinCpuTime();
  else
    dblParam_[ClpMaxSeconds]=-1.0;
}
// Returns true if hit maximum iterations (or time)
bool 
ClpModel::hitMaximumIterations() const
{
  // replaced - compiler error? bool hitMax= (numberIterations_>=maximumIterations());
  bool hitMax = (numberIterations_ >= intParam_[ClpMaxNumIteration]);
  if (dblParam_[ClpMaxSeconds]>=0.0&&!hitMax)
    hitMax = (CoinCpuTime()>=dblParam_[ClpMaxSeconds]);
  return hitMax;
}
// Pass in Message handler (not deleted at end)
void 
ClpModel::passInMessageHandler(CoinMessageHandler * handler)
{
  if (defaultHandler_)
    delete handler_;
  defaultHandler_=false;
  handler_=handler;
}
// Pass in Message handler (not deleted at end) and return current
CoinMessageHandler *
ClpModel::pushMessageHandler(CoinMessageHandler * handler,
			     bool & oldDefault)
{
  CoinMessageHandler * returnValue = handler_;
  oldDefault = defaultHandler_;
  defaultHandler_=false;
  handler_=handler;
  return returnValue;
}
// back to previous message handler
void
ClpModel::popMessageHandler(CoinMessageHandler * oldHandler,bool oldDefault)
{
  if (defaultHandler_)
    delete handler_;
  defaultHandler_=oldDefault;
  handler_=oldHandler;
}
// Set language
void 
ClpModel::newLanguage(CoinMessages::Language language)
{
  messages_ = ClpMessage(language);
}
#ifndef SLIM_NOIO
// Read an mps file from the given filename
int 
ClpModel::readMps(const char *fileName,
		  bool keepNames,
		  bool ignoreErrors)
{
  if (!strcmp(fileName,"-")||!strcmp(fileName,"stdin")) {
    // stdin
  } else {
    std::string name=fileName;
    bool readable = fileCoinReadable(name);
    if (!readable) {
      handler_->message(CLP_UNABLE_OPEN,messages_)
	<<fileName<<CoinMessageEol;
      return -1;
    }
  }
  CoinMpsIO m;
  m.passInMessageHandler(handler_);
  *m.messagesPointer()=coinMessages();
  bool savePrefix =m.messageHandler()->prefix();
  m.messageHandler()->setPrefix(handler_->prefix());
  double time1 = CoinCpuTime(),time2;
  int status=0;
  try {
    status=m.readMps(fileName,"");
  }
  catch (CoinError e) {
    e.print();
    status=-1;
  }
  m.messageHandler()->setPrefix(savePrefix);
  if (!status||ignoreErrors) {
    loadProblem(*m.getMatrixByCol(),
		m.getColLower(),m.getColUpper(),
		m.getObjCoefficients(),
		m.getRowLower(),m.getRowUpper());
    if (m.integerColumns()) {
      integerType_ = new char[numberColumns_];
      CoinMemcpyN(m.integerColumns(),numberColumns_,integerType_);
    } else {
      integerType_ = NULL;
    }
#ifndef SLIM_CLP
    // get quadratic part
    if (m.reader()->whichSection (  ) == COIN_QUAD_SECTION ) {
      int * start=NULL;
      int * column = NULL;
      double * element = NULL;
      status=m.readQuadraticMps(NULL,start,column,element,2);
      if (!status||ignoreErrors) 
	loadQuadraticObjective(numberColumns_,start,column,element);
      delete [] start;
      delete [] column;
      delete [] element;
    }
#endif
#ifndef CLP_NO_STD   
    // set problem name
    setStrParam(ClpProbName,m.getProblemName());
    // do names
    if (keepNames) {
      unsigned int maxLength=0;
      int iRow;
      rowNames_ = std::vector<std::string> ();
      columnNames_ = std::vector<std::string> ();
      rowNames_.reserve(numberRows_);
      for (iRow=0;iRow<numberRows_;iRow++) {
	const char * name = m.rowName(iRow);
	maxLength = CoinMax(maxLength,(unsigned int) strlen(name));
	  rowNames_.push_back(name);
      }
      
      int iColumn;
      columnNames_.reserve(numberColumns_);
      for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	const char * name = m.columnName(iColumn);
	maxLength = CoinMax(maxLength,(unsigned int) strlen(name));
	columnNames_.push_back(name);
      }
      lengthNames_=(int) maxLength;
    } else {
      lengthNames_=0;
    }
#endif
    setDblParam(ClpObjOffset,m.objectiveOffset());
    time2 = CoinCpuTime();
    handler_->message(CLP_IMPORT_RESULT,messages_)
      <<fileName
      <<time2-time1<<CoinMessageEol;
  } else {
    // errors
    handler_->message(CLP_IMPORT_ERRORS,messages_)
      <<status<<fileName<<CoinMessageEol;
  }

  return status;
}
// Read GMPL files from the given filenames
int 
ClpModel::readGMPL(const char *fileName,const char * dataName,
                   bool keepNames)
{
  FILE *fp=fopen(fileName,"r");
  if (fp) {
    // can open - lets go for it
    fclose(fp);
    if (dataName) {
      fp=fopen(dataName,"r");
      if (fp) {
        fclose(fp);
      } else {
        handler_->message(CLP_UNABLE_OPEN,messages_)
          <<dataName<<CoinMessageEol;
        return -1;
      }
    }
  } else {
    handler_->message(CLP_UNABLE_OPEN,messages_)
      <<fileName<<CoinMessageEol;
    return -1;
  }
  CoinMpsIO m;
  m.passInMessageHandler(handler_);
  *m.messagesPointer()=coinMessages();
  bool savePrefix =m.messageHandler()->prefix();
  m.messageHandler()->setPrefix(handler_->prefix());
  double time1 = CoinCpuTime(),time2;
  int status=m.readGMPL(fileName,dataName,keepNames);
  m.messageHandler()->setPrefix(savePrefix);
  if (!status) {
    loadProblem(*m.getMatrixByCol(),
		m.getColLower(),m.getColUpper(),
		m.getObjCoefficients(),
		m.getRowLower(),m.getRowUpper());
    if (m.integerColumns()) {
      integerType_ = new char[numberColumns_];
      CoinMemcpyN(m.integerColumns(),numberColumns_,integerType_);
    } else {
      integerType_ = NULL;
    }
#ifndef CLP_NO_STD
    // set problem name
    setStrParam(ClpProbName,m.getProblemName());
    // do names
    if (keepNames) {
      unsigned int maxLength=0;
      int iRow;
      rowNames_ = std::vector<std::string> ();
      columnNames_ = std::vector<std::string> ();
      rowNames_.reserve(numberRows_);
      for (iRow=0;iRow<numberRows_;iRow++) {
	const char * name = m.rowName(iRow);
	maxLength = CoinMax(maxLength,(unsigned int) strlen(name));
	  rowNames_.push_back(name);
      }
      
      int iColumn;
      columnNames_.reserve(numberColumns_);
      for (iColumn=0;iColumn<numberColumns_;iColumn++) {
	const char * name = m.columnName(iColumn);
	maxLength = CoinMax(maxLength,(unsigned int) strlen(name));
	columnNames_.push_back(name);
      }
      lengthNames_=(int) maxLength;
    } else {
      lengthNames_=0;
    }
#endif
    setDblParam(ClpObjOffset,m.objectiveOffset());
    time2 = CoinCpuTime();
    handler_->message(CLP_IMPORT_RESULT,messages_)
      <<fileName
      <<time2-time1<<CoinMessageEol;
  } else {
    // errors
    handler_->message(CLP_IMPORT_ERRORS,messages_)
      <<status<<fileName<<CoinMessageEol;
  }
  return status;
}
#endif
bool ClpModel::isPrimalObjectiveLimitReached() const
{
  double limit = 0.0;
  getDblParam(ClpPrimalObjectiveLimit, limit);
  if (limit > 1e30) {
    // was not ever set
    return false;
  }
   
  const double obj = objectiveValue();
  const double maxmin = optimizationDirection();

  if (problemStatus_ == 0) // optimal
    return maxmin > 0 ? (obj < limit) /*minim*/ : (-obj < limit) /*maxim*/;
  else if (problemStatus_==2)
    return true;
  else
    return false;
}

bool ClpModel::isDualObjectiveLimitReached() const
{

  double limit = 0.0;
  getDblParam(ClpDualObjectiveLimit, limit);
  if (limit > 1e30) {
    // was not ever set
    return false;
  }
   
  const double obj = objectiveValue();
  const double maxmin = optimizationDirection();

  if (problemStatus_ == 0) // optimal
    return maxmin > 0 ? (obj > limit) /*minim*/ : (-obj > limit) /*maxim*/;
  else if (problemStatus_==1)
    return true;
  else
    return false;

}
void 
ClpModel::copyInIntegerInformation(const char * information)
{
  delete [] integerType_;
  if (information) {
    integerType_ = new char[numberColumns_];
    CoinMemcpyN(information,numberColumns_,integerType_);
  } else {
    integerType_ = NULL;
  }
}
void
ClpModel::setContinuous(int index)
{

  if (integerType_) {
#ifndef NDEBUG
    if (index<0||index>=numberColumns_) {
      indexError(index,"setContinuous");
    }
#endif
    integerType_[index]=0;
  }
}
//-----------------------------------------------------------------------------
void
ClpModel::setInteger(int index)
{
  if (!integerType_) {
    integerType_ = new char[numberColumns_];
    CoinZeroN ( integerType_, numberColumns_);
  }
#ifndef NDEBUG
  if (index<0||index>=numberColumns_) {
    indexError(index,"setInteger");
  }
#endif
  integerType_[index]=1;
}
/* Return true if the index-th variable is an integer variable */
bool 
ClpModel::isInteger(int index) const
{
  if (!integerType_) {
    return false;
  } else {
#ifndef NDEBUG
    if (index<0||index>=numberColumns_) {
      indexError(index,"isInteger");
    }
#endif
    return (integerType_[index]!=0);
  }
}
#ifndef CLP_NO_STD
// Drops names - makes lengthnames 0 and names empty
void 
ClpModel::dropNames()
{
  lengthNames_=0;
  rowNames_ = std::vector<std::string> ();
  columnNames_ = std::vector<std::string> ();
}
#endif
// Drop integer informations
void 
ClpModel::deleteIntegerInformation()
{
  delete [] integerType_;
  integerType_ = NULL;
}
/* Return copy of status array (char[numberRows+numberColumns]),
   use delete [] */
unsigned char *  
ClpModel::statusCopy() const
{
  return ClpCopyOfArray(status_,numberRows_+numberColumns_);
}
// Copy in status vector
void 
ClpModel::copyinStatus(const unsigned char * statusArray)
{
  delete [] status_;
  if (statusArray) {
    status_ = new unsigned char [numberRows_+numberColumns_];
    memcpy(status_,statusArray,(numberRows_+numberColumns_)*sizeof(unsigned char));
  } else {
    status_=NULL;
  }
}
#ifndef SLIM_CLP
// Load up quadratic objective 
void 
ClpModel::loadQuadraticObjective(const int numberColumns, const CoinBigIndex * start,
			      const int * column, const double * element)
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  CoinAssert (numberColumns==numberColumns_);
  assert ((dynamic_cast< ClpLinearObjective*>(objective_)));
  double offset;
  ClpObjective * obj = new ClpQuadraticObjective(objective_->gradient(NULL,NULL,offset,false),
						 numberColumns,
						 start,column,element);
  delete objective_;
  objective_ = obj;

}
void 
ClpModel::loadQuadraticObjective (  const CoinPackedMatrix& matrix)
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  CoinAssert (matrix.getNumCols()==numberColumns_);
  assert ((dynamic_cast< ClpLinearObjective*>(objective_)));
  double offset;
  ClpQuadraticObjective * obj = 
    new ClpQuadraticObjective(objective_->gradient(NULL,NULL,offset,false),
			      numberColumns_,
			      NULL,NULL,NULL);
  delete objective_;
  objective_ = obj;
  obj->loadQuadraticObjective(matrix);
}
// Get rid of quadratic objective
void 
ClpModel::deleteQuadraticObjective()
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  ClpQuadraticObjective * obj = (dynamic_cast< ClpQuadraticObjective*>(objective_));
  if (obj)
    obj->deleteQuadraticObjective();
}
#endif
void 
ClpModel::setObjective(ClpObjective * objective)
{
  whatsChanged_ = 0; // Use ClpSimplex stuff to keep
  delete objective_;
  objective_=objective->clone();
}
// Returns resized array and updates size
double * whichDouble(double * array , int number, const int * which)
{
  double * newArray=NULL;
  if (array&&number) {
    int i ;
    newArray = new double[number];
    for (i=0;i<number;i++) 
      newArray[i]=array[which[i]];
  }
  return newArray;
}
char * whichChar(char * array , int number, const int * which)
{
  char * newArray=NULL;
  if (array&&number) {
    int i ;
    newArray = new char[number];
    for (i=0;i<number;i++) 
      newArray[i]=array[which[i]];
  }
  return newArray;
}
unsigned char * whichUnsignedChar(unsigned char * array , 
				  int number, const int * which)
{
  unsigned char * newArray=NULL;
  if (array&&number) {
    int i ;
    newArray = new unsigned char[number];
    for (i=0;i<number;i++) 
      newArray[i]=array[which[i]];
  }
  return newArray;
}
// Replace Clp Matrix (current is not deleted)
void 
ClpModel::replaceMatrix( ClpMatrixBase * matrix,bool deleteCurrent)
{
  if (deleteCurrent)
    delete matrix_;
  matrix_=matrix;
  whatsChanged_ = 0; // Too big a change
}
// Subproblem constructor
ClpModel::ClpModel ( const ClpModel * rhs,
		     int numberRows, const int * whichRow,
		     int numberColumns, const int * whichColumn,
		     bool dropNames, bool dropIntegers)
{
  defaultHandler_ = rhs->defaultHandler_;
  if (defaultHandler_) 
    handler_ = new CoinMessageHandler(*rhs->handler_);
   else 
    handler_ = rhs->handler_;
  eventHandler_ = rhs->eventHandler_->clone();
  messages_ = rhs->messages_;
  coinMessages_ = rhs->coinMessages_;
  intParam_[ClpMaxNumIteration] = rhs->intParam_[ClpMaxNumIteration];
  intParam_[ClpMaxNumIterationHotStart] = 
    rhs->intParam_[ClpMaxNumIterationHotStart];

  dblParam_[ClpDualObjectiveLimit] = rhs->dblParam_[ClpDualObjectiveLimit];
  dblParam_[ClpPrimalObjectiveLimit] = rhs->dblParam_[ClpPrimalObjectiveLimit];
  dblParam_[ClpDualTolerance] = rhs->dblParam_[ClpDualTolerance];
  dblParam_[ClpPrimalTolerance] = rhs->dblParam_[ClpPrimalTolerance];
  dblParam_[ClpObjOffset] = rhs->dblParam_[ClpObjOffset];
  dblParam_[ClpMaxSeconds] = rhs->dblParam_[ClpMaxSeconds];
  dblParam_[ClpPresolveTolerance] = rhs->dblParam_[ClpPresolveTolerance];
#ifndef CLP_NO_STD
  strParam_[ClpProbName] = rhs->strParam_[ClpProbName];
#endif

  optimizationDirection_ = rhs->optimizationDirection_;
  objectiveValue_=rhs->objectiveValue_;
  smallElement_ = rhs->smallElement_;
  objectiveScale_ = rhs->objectiveScale_;
  rhsScale_ = rhs->rhsScale_;
  numberIterations_ = rhs->numberIterations_;
  solveType_ = rhs->solveType_;
  whatsChanged_ = 0; // Too big a change
  problemStatus_ = rhs->problemStatus_;
  secondaryStatus_ = rhs->secondaryStatus_;
  // check valid lists
  int numberBad=0;
  int i;
  for (i=0;i<numberRows;i++)
    if (whichRow[i]<0||whichRow[i]>=rhs->numberRows_)
      numberBad++;
  CoinAssertHint(!numberBad,"Bad row list for subproblem constructor");
  numberBad=0;
  for (i=0;i<numberColumns;i++)
    if (whichColumn[i]<0||whichColumn[i]>=rhs->numberColumns_)
      numberBad++;
  CoinAssertHint(!numberBad,"Bad Column list for subproblem constructor");
  numberRows_ = numberRows;
  numberColumns_ = numberColumns;
  userPointer_ = rhs->userPointer_;
  numberThreads_=0;
#ifndef CLP_NO_STD
  if (!dropNames) {
    unsigned int maxLength=0;
    int iRow;
    rowNames_ = std::vector<std::string> ();
    columnNames_ = std::vector<std::string> ();
    rowNames_.reserve(numberRows_);
    for (iRow=0;iRow<numberRows_;iRow++) {
      rowNames_.push_back(rhs->rowNames_[whichRow[iRow]]);
      maxLength = CoinMax(maxLength,(unsigned int) strlen(rowNames_[iRow].c_str()));
    }
    int iColumn;
    columnNames_.reserve(numberColumns_);
    for (iColumn=0;iColumn<numberColumns_;iColumn++) {
      columnNames_.push_back(rhs->columnNames_[whichColumn[iColumn]]);
      maxLength = CoinMax(maxLength,(unsigned int) strlen(columnNames_[iColumn].c_str()));
    }
    lengthNames_=(int) maxLength;
  } else {
    lengthNames_ = 0;
    rowNames_ = std::vector<std::string> ();
    columnNames_ = std::vector<std::string> ();
  }
#endif
  if (rhs->integerType_&&!dropIntegers) {
    integerType_ = whichChar(rhs->integerType_,numberColumns,whichColumn);
  } else {
    integerType_ = NULL;
  }
  if (rhs->rowActivity_) {
    rowActivity_=whichDouble(rhs->rowActivity_,numberRows,whichRow);
    dual_=whichDouble(rhs->dual_,numberRows,whichRow);
    columnActivity_=whichDouble(rhs->columnActivity_,numberColumns,
				whichColumn);
    reducedCost_=whichDouble(rhs->reducedCost_,numberColumns,
				whichColumn);
  } else {
    rowActivity_=NULL;
    columnActivity_=NULL;
    dual_=NULL;
    reducedCost_=NULL;
  }
  rowLower_=whichDouble(rhs->rowLower_,numberRows,whichRow);
  rowUpper_=whichDouble(rhs->rowUpper_,numberRows,whichRow);
  columnLower_=whichDouble(rhs->columnLower_,numberColumns,whichColumn);
  columnUpper_=whichDouble(rhs->columnUpper_,numberColumns,whichColumn);
  if (rhs->objective_)
    objective_  = rhs->objective_->subsetClone(numberColumns,whichColumn);
  else
    objective_ = NULL;
  rowObjective_=whichDouble(rhs->rowObjective_,numberRows,whichRow);
  // status has to be done in two stages
  status_ = new unsigned char[numberColumns_+numberRows_];
  unsigned char * rowStatus = whichUnsignedChar(rhs->status_+rhs->numberColumns_,
						numberRows_,whichRow);
  unsigned char * columnStatus = whichUnsignedChar(rhs->status_,
						numberColumns_,whichColumn);
  CoinMemcpyN(rowStatus,numberRows_,status_+numberColumns_);
  delete [] rowStatus;
  CoinMemcpyN(columnStatus,numberColumns_,status_);
  delete [] columnStatus;
  ray_ = NULL;
  if (problemStatus_==1&&!secondaryStatus_)
    ray_ = whichDouble (rhs->ray_,numberRows,whichRow);
  else if (problemStatus_==2)
    ray_ = whichDouble (rhs->ray_,numberColumns,whichColumn);
  rowScale_ = NULL;
  columnScale_ = NULL;
  scalingFlag_ = rhs->scalingFlag_;
  rowCopy_=NULL;
  matrix_=NULL;
  if (rhs->matrix_) {
    matrix_ = rhs->matrix_->subsetClone(numberRows,whichRow,
					numberColumns,whichColumn);
  }
  CoinSeedRandom(1234567);
}
#ifndef CLP_NO_STD
// Copies in names
void 
ClpModel::copyNames(std::vector<std::string> & rowNames,
		 std::vector<std::string> & columnNames)
{
  unsigned int maxLength=0;
  int iRow;
  rowNames_ = std::vector<std::string> ();
  columnNames_ = std::vector<std::string> ();
  rowNames_.reserve(numberRows_);
  for (iRow=0;iRow<numberRows_;iRow++) {
    rowNames_.push_back(rowNames[iRow]);
    maxLength = CoinMax(maxLength,(unsigned int) strlen(rowNames_[iRow].c_str()));
  }
  int iColumn;
  columnNames_.reserve(numberColumns_);
  for (iColumn=0;iColumn<numberColumns_;iColumn++) {
    columnNames_.push_back(columnNames[iColumn]);
    maxLength = CoinMax(maxLength,(unsigned int) strlen(columnNames_[iColumn].c_str()));
  }
  lengthNames_=(int) maxLength;
}
// Return name or Rnnnnnnn
std::string 
ClpModel::getRowName(int iRow) const
{
#ifndef NDEBUG
  if (iRow<0||iRow>=numberRows_) {
    indexError(iRow,"getRowName");
  }
#endif
  int size = rowNames_.size();
  if (size>iRow) {
    return rowNames_[iRow];
  } else {
    char name[9];
    sprintf(name,"R%7.7d",iRow);
    std::string rowName(name);
    return rowName;
  }
}
// Set row name
void
ClpModel::setRowName(int iRow, std::string &name)
{
#ifndef NDEBUG
  if (iRow<0||iRow>=numberRows_) {
    indexError(iRow,"setRowName");
  }
#endif
  unsigned int maxLength=lengthNames_;
  int size = rowNames_.size();
  if (size<=iRow)
    rowNames_.resize(iRow+1);
  rowNames_[iRow]= name;
  maxLength = CoinMax(maxLength,(unsigned int) strlen(name.c_str()));
  // May be too big - but we would have to check both rows and columns to be exact
  lengthNames_=(int) maxLength;
}
// Return name or Cnnnnnnn
std::string 
ClpModel::getColumnName(int iColumn) const
{
#ifndef NDEBUG
  if (iColumn<0||iColumn>=numberColumns_) {
    indexError(iColumn,"getColumnName");
  }
#endif
  int size = columnNames_.size();
  if (size>iColumn) {
    return columnNames_[iColumn];
  } else {
    char name[9];
    sprintf(name,"C%7.7d",iColumn);
    std::string columnName(name);
    return columnName;
  }
}
// Set column name
void
ClpModel::setColumnName(int iColumn, std::string &name)
{
#ifndef NDEBUG
  if (iColumn<0||iColumn>=numberColumns_) {
    indexError(iColumn,"setColumnName");
  }
#endif
  unsigned int maxLength=lengthNames_;
  int size = columnNames_.size();
  if (size<=iColumn)
    columnNames_.resize(iColumn+1);
  columnNames_[iColumn]= name;
  maxLength = CoinMax(maxLength,(unsigned int) strlen(name.c_str()));
  // May be too big - but we would have to check both columns and columns to be exact
  lengthNames_=(int) maxLength;
}
// Copies in Row names - modifies names first .. last-1
void 
ClpModel::copyRowNames(const std::vector<std::string> & rowNames, int first, int last)
{
  unsigned int maxLength=lengthNames_;
  int size = rowNames_.size();
  if (size!=numberRows_)
    rowNames_.resize(numberRows_);
  int iRow;
  for (iRow=first; iRow<last;iRow++) {
    rowNames_[iRow]= rowNames[iRow-first];
    maxLength = CoinMax(maxLength,(unsigned int) strlen(rowNames_[iRow-first].c_str()));
  }
  // May be too big - but we would have to check both rows and columns to be exact
  lengthNames_=(int) maxLength;
}
// Copies in Column names - modifies names first .. last-1
void 
ClpModel::copyColumnNames(const std::vector<std::string> & columnNames, int first, int last)
{
  unsigned int maxLength=lengthNames_;
  int size = columnNames_.size();
  if (size!=numberColumns_)
    columnNames_.resize(numberColumns_);
  int iColumn;
  for (iColumn=first; iColumn<last;iColumn++) {
    columnNames_[iColumn]= columnNames[iColumn-first];
    maxLength = CoinMax(maxLength,(unsigned int) strlen(columnNames_[iColumn-first].c_str()));
  }
  // May be too big - but we would have to check both rows and columns to be exact
  lengthNames_=(int) maxLength;
}
// Copies in Row names - modifies names first .. last-1
void 
ClpModel::copyRowNames(const char * const * rowNames, int first, int last)
{
  unsigned int maxLength=lengthNames_;
  int size = rowNames_.size();
  if (size!=numberRows_)
    rowNames_.resize(numberRows_);
  int iRow;
  for (iRow=first; iRow<last;iRow++) {
    rowNames_[iRow]= rowNames[iRow-first];
    maxLength = CoinMax(maxLength,(unsigned int) strlen(rowNames[iRow-first]));
  }
  // May be too big - but we would have to check both rows and columns to be exact
  lengthNames_=(int) maxLength;
}
// Copies in Column names - modifies names first .. last-1
void 
ClpModel::copyColumnNames(const char * const * columnNames, int first, int last)
{
  unsigned int maxLength=lengthNames_;
  int size = columnNames_.size();
  if (size!=numberColumns_)
    columnNames_.resize(numberColumns_);
  int iColumn;
  for (iColumn=first; iColumn<last;iColumn++) {
    columnNames_[iColumn]= columnNames[iColumn-first];
    maxLength = CoinMax(maxLength,(unsigned int) strlen(columnNames[iColumn-first]));
  }
  // May be too big - but we would have to check both rows and columns to be exact
  lengthNames_=(int) maxLength;
}
#endif
// Primal objective limit
void 
ClpModel::setPrimalObjectiveLimit(double value)
{
  dblParam_[ClpPrimalObjectiveLimit]=value;
}
// Dual objective limit
void 
ClpModel::setDualObjectiveLimit(double value)
{
  dblParam_[ClpDualObjectiveLimit]=value;
}
// Objective offset
void 
ClpModel::setObjectiveOffset(double value)
{
  dblParam_[ClpObjOffset]=value;
}
// Solve a problem with no elements - return status
int ClpModel::emptyProblem(int * infeasNumber, double * infeasSum,bool printMessage)
{
  secondaryStatus_=6; // so user can see something odd
  if (printMessage)
    handler_->message(CLP_EMPTY_PROBLEM,messages_)
      <<numberRows_
      <<numberColumns_
      <<0
      <<CoinMessageEol;
  int returnCode=0;
  if (numberRows_||numberColumns_) {
    if (!status_) {
      status_ = new unsigned char[numberRows_+numberColumns_];
      CoinZeroN(status_,numberRows_+numberColumns_);
    }
  }
  // status is set directly (as can be used by Interior methods)
  // check feasible
  int numberPrimalInfeasibilities=0;
  double sumPrimalInfeasibilities=0.0;
  int numberDualInfeasibilities=0;
  double sumDualInfeasibilities=0.0;
  if (numberRows_) {
    for (int i=0;i<numberRows_;i++) {
      dual_[i]=0.0;
      if (rowLower_[i]<=rowUpper_[i]) {
	if (rowLower_[i]>-1.0e30||rowUpper_[i]<1.0e30) {
	  if (fabs(rowLower_[i])<fabs(rowUpper_[i]))
	    rowActivity_[i]=rowLower_[i];
	  else
	    rowActivity_[i]=rowUpper_[i];
	} else {
	  rowActivity_[i]=0.0;
	}
      } else {
	rowActivity_[i]=0.0;
	numberPrimalInfeasibilities++;
	sumPrimalInfeasibilities += rowLower_[i]-rowUpper_[i];
	returnCode=1;
      }
      status_[i+numberColumns_]=1;
    }
  }
  objectiveValue_=0.0;
  if (numberColumns_) {
    const double * cost = objective();
    for (int i=0;i<numberColumns_;i++) {
      reducedCost_[i]=cost[i];
      double objValue = cost[i]*optimizationDirection_;
      if (columnLower_[i]<=columnUpper_[i]) {
	if (columnLower_[i]>-1.0e30||columnUpper_[i]<1.0e30) {
	  if (!objValue) {
	    if (fabs(columnLower_[i])<fabs(columnUpper_[i])) {
	      columnActivity_[i]=columnLower_[i];
	      status_[i]=3;
	    } else {
	      columnActivity_[i]=columnUpper_[i];
	      status_[i]=2;
	    }
	  } else if (objValue>0.0) {
	    if (columnLower_[i]>-1.0e30) {
	      columnActivity_[i]=columnLower_[i];
	      status_[i]=3;
	    } else {
	      columnActivity_[i]=columnUpper_[i];
	      status_[i]=2;
	      numberDualInfeasibilities++;;
	      sumDualInfeasibilities += fabs(objValue);
	      returnCode |= 2;
	    }
	    objectiveValue_ += columnActivity_[i]*objValue;
	  } else {
	    if (columnUpper_[i]<1.0e30) {
	      columnActivity_[i]=columnUpper_[i];
	      status_[i]=2;
	    } else {
	      columnActivity_[i]=columnLower_[i];
	      status_[i]=3;
	      numberDualInfeasibilities++;;
	      sumDualInfeasibilities += fabs(objValue);
	      returnCode |= 2;
	    }
	    objectiveValue_ += columnActivity_[i]*objValue;
	  }
	} else {
	  columnActivity_[i]=0.0;
	  if (objValue) {
	    numberDualInfeasibilities++;;
	    sumDualInfeasibilities += fabs(objValue);
	    returnCode |= 2;
	  }
	  status_[i]=0;
	}
      } else {
	if (fabs(columnLower_[i])<fabs(columnUpper_[i])) {
	  columnActivity_[i]=columnLower_[i];
	  status_[i]=3;
	} else {
	  columnActivity_[i]=columnUpper_[i];
	  status_[i]=2;
	}
	numberPrimalInfeasibilities++;
	sumPrimalInfeasibilities += columnLower_[i]-columnUpper_[i];
	returnCode |= 1;
      }
    }
  }
  objectiveValue_ /= (objectiveScale_*rhsScale_);
  if (infeasNumber) {
    infeasNumber[0]=numberDualInfeasibilities;
    infeasSum[0]=sumDualInfeasibilities;
    infeasNumber[1]=numberPrimalInfeasibilities;
    infeasSum[1]=sumPrimalInfeasibilities;
  }
  if (returnCode==3) 
    returnCode=4;
  return returnCode;
}
#ifndef SLIM_NOIO
/* Write the problem in MPS format to the specified file.
   
Row and column names may be null.
formatType is
<ul>
<li> 0 - normal
<li> 1 - extra accuracy 
<li> 2 - IEEE hex (later)
</ul>

Returns non-zero on I/O error
*/
int 
ClpModel::writeMps(const char *filename, 
		   int formatType,int numberAcross,
		   double objSense) const 
{
  matrix_->setDimensions(numberRows_,numberColumns_);
  
  // Get multiplier for objective function - default 1.0
  double * objective = new double[numberColumns_];
  CoinMemcpyN(getObjCoefficients(),numberColumns_,objective);
  if (objSense*getObjSense()<0.0) {
    for (int i = 0; i < numberColumns_; ++i) 
      objective [i] = - objective[i];
  }
  // get names
  const char * const * const rowNames = rowNamesAsChar();
  const char * const * const columnNames = columnNamesAsChar();
  CoinMpsIO writer;
  writer.passInMessageHandler(handler_);
  *writer.messagesPointer()=coinMessages();
  writer.setMpsData(*(matrix_->getPackedMatrix()), COIN_DBL_MAX,
		    getColLower(), getColUpper(),
		    objective,
		    (const char*) 0 /*integrality*/,
		    getRowLower(), getRowUpper(),
		    columnNames, rowNames);
  // Pass in array saying if each variable integer
  writer.copyInIntegerInformation(integerInformation());
  writer.setObjectiveOffset(objectiveOffset());
  delete [] objective;
  CoinPackedMatrix * quadratic=NULL;
#ifndef SLIM_CLP
  // allow for quadratic objective
#ifndef NO_RTTI
  ClpQuadraticObjective * quadraticObj = (dynamic_cast< ClpQuadraticObjective*>(objective_));
#else
  ClpQuadraticObjective * quadraticObj = NULL;
  if (objective_->type()==2)
    quadraticObj = (static_cast< ClpQuadraticObjective*>(objective_));
#endif
  if (quadraticObj) 
    quadratic = quadraticObj->quadraticObjective();
#endif
  int returnCode = writer.writeMps(filename, 0 /* do not gzip it*/, formatType, numberAcross,
			 quadratic);
  if (rowNames) {
    deleteNamesAsChar(rowNames, numberRows_+1);
    deleteNamesAsChar(columnNames, numberColumns_);
  }
  return returnCode;
}
#ifndef CLP_NO_STD
// Create row names as char **
const char * const * const
ClpModel::rowNamesAsChar() const
{
  char ** rowNames = NULL;
  if (lengthNames()) {
    rowNames = new char * [numberRows_+1];
    for (int iRow=0;iRow<numberRows_;iRow++) {
      rowNames[iRow] = 
	strdup(rowName(iRow).c_str());
#ifdef STRIPBLANKS
      char * xx = rowNames[iRow];
      int i;
      int length = strlen(xx);
      int n=0;
      for (i=0;i<length;i++) {
	if (xx[i]!=' ')
	  xx[n++]=xx[i];
      }
      xx[n]='\0';
#endif
    }
    rowNames[numberRows_] = strdup("OBJROW");
  }
  return reinterpret_cast<const char * const *>(rowNames);
}
// Create column names as char **
const char * const * const
ClpModel::columnNamesAsChar() const
{
  char ** columnNames = NULL;
  if (lengthNames()) {
    columnNames = new char * [numberColumns_];
    for (int iColumn=0;iColumn<numberColumns_;iColumn++) {
      columnNames[iColumn] = 
	strdup(columnName(iColumn).c_str());
#ifdef STRIPBLANKS
      char * xx = columnNames[iColumn];
      int i;
      int length = strlen(xx);
      int n=0;
      for (i=0;i<length;i++) {
	if (xx[i]!=' ')
	  xx[n++]=xx[i];
      }
      xx[n]='\0';
#endif
    }
  }
  return /*reinterpret_cast<const char * const *>*/(columnNames);
}
// Delete char * version of names
void 
ClpModel::deleteNamesAsChar(const char * const * const names,int number) const
{
  for (int i=0;i<number;i++) {
    free(const_cast<char *>(names[i]));
  }
  delete [] const_cast<char **>(names);
}
#endif
#endif
// Pass in Event handler (cloned and deleted at end)
void 
ClpModel::passInEventHandler(const ClpEventHandler * eventHandler)
{
  delete eventHandler_;
  eventHandler_ = eventHandler->clone();
}
// Sets or unsets scaling, 0 -off, 1 on, 2 dynamic(later)
void 
ClpModel::scaling(int mode)
{
  // If mode changes then we treat as new matrix (need new row copy)
  if (mode!=scalingFlag_)
    whatsChanged_ &= ~(2+4+8);
  if (mode>0&&mode<4) {
    scalingFlag_=mode;
  } else if (!mode) {
    scalingFlag_=0;
    delete [] rowScale_;
    rowScale_ = NULL;
    delete [] columnScale_;
    columnScale_ = NULL;
  }
}
void 
ClpModel::times(double scalar,
		  const double * x, double * y) const
{
  if (rowScale_)
    matrix_->times(scalar,x,y,rowScale_,columnScale_);
  else
    matrix_->times(scalar,x,y);
}
void 
ClpModel::transposeTimes(double scalar,
			   const double * x, double * y) const 
{
  if (rowScale_)
    matrix_->transposeTimes(scalar,x,y,rowScale_,columnScale_);
  else
    matrix_->transposeTimes(scalar,x,y);
}
// Does much of scaling
void 
ClpModel::gutsOfScaling()
{
  int i;
  if (rowObjective_) {
    for (i=0;i<numberRows_;i++) 
      rowObjective_[i] /= rowScale_[i];
  }
  for (i=0;i<numberRows_;i++) {
    double multiplier = rowScale_[i];
    double inverseMultiplier = 1.0/multiplier;
    rowActivity_[i] *= multiplier;
    dual_[i] *= inverseMultiplier;
    if (rowLower_[i]>-1.0e30)
      rowLower_[i] *= multiplier;
    else
      rowLower_[i] = -COIN_DBL_MAX;
    if (rowUpper_[i]<1.0e30)
      rowUpper_[i] *= multiplier;
    else
      rowUpper_[i] = COIN_DBL_MAX;
  }
  for (i=0;i<numberColumns_;i++) {
    double multiplier = 1.0/columnScale_[i];
    columnActivity_[i] *= multiplier;
    reducedCost_[i] *= columnScale_[i];
    if (columnLower_[i]>-1.0e30)
      columnLower_[i] *= multiplier;
    else
      columnLower_[i] = -COIN_DBL_MAX;
    if (columnUpper_[i]<1.0e30)
      columnUpper_[i] *= multiplier;
    else
      columnUpper_[i] = COIN_DBL_MAX;
    
  }
  //now replace matrix
  //and objective
  matrix_->reallyScale(rowScale_,columnScale_);
  objective_->reallyScale(columnScale_);
}
/* If we constructed a "really" scaled model then this reverses the operation.
      Quantities may not be exactly as they were before due to rounding errors */
void 
ClpModel::unscale()
{
  if (rowScale_) {
    int i;
    // reverse scaling
    for (i=0;i<numberRows_;i++) 
      rowScale_[i] = 1.0/rowScale_[i];
    for (i=0;i<numberColumns_;i++) 
      columnScale_[i] = 1.0/columnScale_[i];
    gutsOfScaling();
  }
  
  scalingFlag_=0;
  delete [] rowScale_;
  rowScale_ = NULL;
  delete [] columnScale_;
  columnScale_ = NULL;
}
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpDataSave::ClpDataSave () 
{
  dualBound_ = 0.0;
  infeasibilityCost_ = 0.0;
  sparseThreshold_ = 0;
  pivotTolerance_=0.0;
  acceptablePivot_ = 0.0;
  objectiveScale_ = 1.0;
  perturbation_ = 0;
  forceFactorization_=-1;
  scalingFlag_=0;
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpDataSave::ClpDataSave (const ClpDataSave & rhs) 
{  
  dualBound_ = rhs.dualBound_;
  infeasibilityCost_ = rhs.infeasibilityCost_;
  pivotTolerance_ = rhs.pivotTolerance_;
  acceptablePivot_ = rhs.acceptablePivot_;
  objectiveScale_ = rhs.objectiveScale_;
  sparseThreshold_ = rhs.sparseThreshold_;
  perturbation_ = rhs.perturbation_;
  forceFactorization_=rhs.forceFactorization_;
  scalingFlag_=rhs.scalingFlag_;
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpDataSave::~ClpDataSave ()
{
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpDataSave &
ClpDataSave::operator=(const ClpDataSave& rhs)
{
  if (this != &rhs) {
    dualBound_ = rhs.dualBound_;
    infeasibilityCost_ = rhs.infeasibilityCost_;
    pivotTolerance_ = rhs.pivotTolerance_;
    acceptablePivot_ = rhs.acceptablePivot_;
    objectiveScale_ = rhs.objectiveScale_;
    sparseThreshold_ = rhs.sparseThreshold_;
    perturbation_ = rhs.perturbation_;
    forceFactorization_=rhs.forceFactorization_;
    scalingFlag_=rhs.scalingFlag_;
  }
  return *this;
}
// Create C++ lines to get to current state
void 
ClpModel::generateCpp( FILE * fp)
{
  // Stuff that can't be done easily
  if (!lengthNames_) {
    // no names
    fprintf(fp,"  clpModel->dropNames();\n");
  }
  ClpModel defaultModel;
  ClpModel * other = &defaultModel;
  int iValue1, iValue2;
  double dValue1, dValue2;
  iValue1 = this->maximumIterations();
  iValue2 = other->maximumIterations();
  fprintf(fp,"%d  int save_maximumIterations = clpModel->maximumIterations();\n",iValue1==iValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setMaximumIterations(%d);\n",iValue1==iValue2 ? 4 : 3,iValue1);
  fprintf(fp,"%d  clpModel->setMaximumIterations(save_maximumIterations);\n",iValue1==iValue2 ? 7 : 6);
  dValue1 = this->primalTolerance();
  dValue2 = other->primalTolerance();
  fprintf(fp,"%d  double save_primalTolerance = clpModel->primalTolerance();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setPrimalTolerance(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setPrimalTolerance(save_primalTolerance);\n",dValue1==dValue2 ? 7 : 6);
  dValue1 = this->dualTolerance();
  dValue2 = other->dualTolerance();
  fprintf(fp,"%d  double save_dualTolerance = clpModel->dualTolerance();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setDualTolerance(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setDualTolerance(save_dualTolerance);\n",dValue1==dValue2 ? 7 : 6);
  iValue1 = this->numberIterations();
  iValue2 = other->numberIterations();
  fprintf(fp,"%d  int save_numberIterations = clpModel->numberIterations();\n",iValue1==iValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setNumberIterations(%d);\n",iValue1==iValue2 ? 4 : 3,iValue1);
  fprintf(fp,"%d  clpModel->setNumberIterations(save_numberIterations);\n",iValue1==iValue2 ? 7 : 6);
  dValue1 = this->maximumSeconds();
  dValue2 = other->maximumSeconds();
  fprintf(fp,"%d  double save_maximumSeconds = clpModel->maximumSeconds();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setMaximumSeconds(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setMaximumSeconds(save_maximumSeconds);\n",dValue1==dValue2 ? 7 : 6);
  dValue1 = this->optimizationDirection();
  dValue2 = other->optimizationDirection();
  fprintf(fp,"%d  double save_optimizationDirection = clpModel->optimizationDirection();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setOptimizationDirection(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setOptimizationDirection(save_optimizationDirection);\n",dValue1==dValue2 ? 7 : 6);
  dValue1 = this->objectiveScale();
  dValue2 = other->objectiveScale();
  fprintf(fp,"%d  double save_objectiveScale = clpModel->objectiveScale();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setObjectiveScale(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setObjectiveScale(save_objectiveScale);\n",dValue1==dValue2 ? 7 : 6);
  dValue1 = this->rhsScale();
  dValue2 = other->rhsScale();
  fprintf(fp,"%d  double save_rhsScale = clpModel->rhsScale();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setRhsScale(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setRhsScale(save_rhsScale);\n",dValue1==dValue2 ? 7 : 6);
  iValue1 = this->scalingFlag();
  iValue2 = other->scalingFlag();
  fprintf(fp,"%d  int save_scalingFlag = clpModel->scalingFlag();\n",iValue1==iValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->scaling(%d);\n",iValue1==iValue2 ? 4 : 3,iValue1);
  fprintf(fp,"%d  clpModel->scaling(save_scalingFlag);\n",iValue1==iValue2 ? 7 : 6);
  dValue1 = this->getSmallElementValue();
  dValue2 = other->getSmallElementValue();
  fprintf(fp,"%d  double save_getSmallElementValue = clpModel->getSmallElementValue();\n",dValue1==dValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setSmallElementValue(%g);\n",dValue1==dValue2 ? 4 : 3,dValue1);
  fprintf(fp,"%d  clpModel->setSmallElementValue(save_getSmallElementValue);\n",dValue1==dValue2 ? 7 : 6);
  iValue1 = this->logLevel();
  iValue2 = other->logLevel();
  fprintf(fp,"%d  int save_logLevel = clpModel->logLevel();\n",iValue1==iValue2 ? 2 : 1);
  fprintf(fp,"%d  clpModel->setLogLevel(%d);\n",iValue1==iValue2 ? 4 : 3,iValue1);
  fprintf(fp,"%d  clpModel->setLogLevel(save_logLevel);\n",iValue1==iValue2 ? 7 : 6);
}
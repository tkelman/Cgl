// Copyright (C) 2005, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef CglProcess_H
#define CglProcess_H

#include <string>
#include <vector>

#include "CoinFinite.hpp"
#include "CoinMessageHandler.hpp"
#include "OsiSolverInterface.hpp"
#include "OsiPresolve.hpp"
#include "CglCutGenerator.hpp"

//#############################################################################

/** Class for preProcessing and postProcessing.

    While cuts can be added at any time in the tree, some cuts are actually just
    stronger versions of existing constraints.  In this case they can replace those
    constraints rather than being added as new constraints.  This is awkward in the
    tree but reasonable at the root node.

    This is a general process class which uses other cut generators to strengthen
    constraints, establish that constraints are redundant, fix variables and
    find relationships such as x + y == 1.

    Presolve will also be done.

*/

class CglProcess  {
  
public:

  ///@name Main methods 
  //@{
  /** preProcess problem - returning new problem.
      If makeEquality true then <= cliques converted to ==.
      Presolve will be done numberPasses times.

      Returns NULL if infeasible
  */
  OsiSolverInterface * preProcess(OsiSolverInterface & model, 
                                  bool makeEquality=true, int numberPasses=5);
  /// Creates solution in original model
  void postProcess(OsiSolverInterface &model);
  //@}

  //---------------------------------------------------------------------------

  /**@name Parameter set/get methods

     The set methods return true if the parameter was set to the given value,
     false if the value of the parameter is out of range.

     The get methods return the value of the parameter.

  */
  //@{
  /** Set cutoff bound on the objective function.

    When using strict comparison, the bound is adjusted by a tolerance to
    avoid accidentally cutting off the optimal solution.
  */
  void setCutoff(double value) ;

  /// Get the cutoff bound on the objective function - always as minimize
  double getCutoff() const;
  //@}

  ///@name Cut generator methods 
  //@{
  /// Get the number of cut generators
  inline int numberCutGenerators() const
  { return numberCutGenerators_;};
  /// Get the list of cut generators
  inline CglCutGenerator ** cutGenerators() const
  { return generator_;};
  ///Get the specified cut generator
  inline CglCutGenerator * cutGenerator(int i) const
  { return generator_[i];};
  /** Add one generator - up to user to delete generators.
  */
  void addCutGenerator(CglCutGenerator * generator);
//@}
    
  /**@name Setting/Accessing application data */
  //@{
    /** Set application data.

	This is a pointer that the application can store into and
	retrieve.
	This field is available for the application to optionally
	define and use.
    */
    void setApplicationData (void * appData);

    /// Get application data
    void * getApplicationData() const;
  //@}
  
  //---------------------------------------------------------------------------

  /**@name Message handling */
  //@{
  /// Pass in Message handler (not deleted at end)
  void passInMessageHandler(CoinMessageHandler * handler);
  /// Set language
  void newLanguage(CoinMessages::Language language);
  void setLanguage(CoinMessages::Language language)
  {newLanguage(language);};
  /// Return handler
  CoinMessageHandler * messageHandler() const
  {return handler_;};
  /// Return messages
  CoinMessages messages() 
  {return messages_;};
  /// Return pointer to messages
  CoinMessages * messagesPointer() 
  {return &messages_;};
  //@}
  //---------------------------------------------------------------------------


  ///@name Constructors and destructors etc
  //@{
  /// Constructor
  CglProcess(); 
  
  /// Copy constructor .
  CglProcess(const CglProcess & rhs);
  
  /// Assignment operator 
  CglProcess & operator=(const CglProcess& rhs);
  
  /// Destructor 
  ~CglProcess ();
  
  /// Clears out as much as possible
  void gutsOfDestructor();
  //@}
private:

  ///@name private methods
  //@{
  /** Return model with useful modifications.  
      If constraints true then returns any x+y=1 or x-y=0 constraints
  */
  OsiSolverInterface * modified(OsiSolverInterface * model,
                                bool constraints,
                                int & numberChanges);
  //@}

//---------------------------------------------------------------------------

private:
  ///@name Private member data 
  //@{

  /// The original solver associated with this model.
  OsiSolverInterface * originalModel_;
  /// Solver after making clique equalities (may == original)
  OsiSolverInterface * startModel_;
  /// Number of solvers at various stages
  int numberSolvers_;
  /// Copies of solver at various stages after presolve
  OsiSolverInterface ** model_;
  /// Copies of solver at various stages after presolve after modifications
  OsiSolverInterface ** modifiedModel_;
  /// Matching presolve information
  OsiPresolve ** presolve_;

   /// Message handler
  CoinMessageHandler * handler_;

  /** Flag to say if handler_ is the default handler.
  
    The default handler is deleted when the model is deleted. Other
    handlers (supplied by the client) will not be deleted.
  */
  bool defaultHandler_;

  /// Cgl messages
  CoinMessages messages_;

  /// Pointer to user-defined data structure
  void * appData_;
  /// Number of cut generators
  int numberCutGenerators_;
  // Cut generators
  CglCutGenerator ** generator_;
 //@}
};

#endif

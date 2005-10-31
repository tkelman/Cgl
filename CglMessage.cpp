// Copyright (C) 2005, International Business Machines
// Corporation and others.  All Rights Reserved.

#include "CoinPragma.hpp"
#include "CglMessage.hpp"
/// Structure for use by CglMessage.cpp
typedef struct {
  CGL_Message internalNumber;
  int externalNumber; // or continuation
  char detail;
  const char * message;
} Cgl_message;
static Cgl_message us_english[]=
{
  {CGL_INFEASIBLE,0,1,"Cut generators found to be infeasible!"},
  {CGL_CLIQUES,1,2,"%d cliques of average size %g"},
  {CGL_FIXED,2,1,"%d variables fixed"},
  {CGL_PROCESS_STATS,3,1,"%d fixed, %d tightened bounds, %d strengthened rows"},
  {CGL_DUMMY_END,999999,0,""}
};
/* Constructor */
CglMessage::CglMessage(Language language) :
  CoinMessages(sizeof(us_english)/sizeof(Cgl_message))
{
  language_=language;
  strcpy(source_,"Cgl");
  class_ = 3; // Cuts
  Cgl_message * message = us_english;

  while (message->internalNumber!=CGL_DUMMY_END) {
     CoinOneMessage oneMessage(message->externalNumber,message->detail,
			       message->message);
     addMessage(message->internalNumber,oneMessage);
     message ++;
}

}

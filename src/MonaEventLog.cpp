/*=========================================================================

  Program:   Visualization Toolkit
  Module:    MonaEventLog.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "MonaEventLog.hpp"
#include "MonaController.hpp"
#include <vtkObjectFactory.h>
#include <mpi.h>
#include <mpe.h>

int MonaEventLog::LastEventId = 0;

vtkStandardNewMacro(MonaEventLog);

void MonaEventLog::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

MonaEventLog::MonaEventLog()
{

  this->Active = 0;

}

void MonaEventLog::InitializeLogging()
{
  MPE_Init_log();
}

void MonaEventLog::FinalizeLogging(const char* fname)
{
  MPE_Finish_log(const_cast<char*>(fname));
}

int MonaEventLog::SetDescription(const char* name, const char* desc)
{
  int err, processId;
  if ( (err = MPI_Comm_rank(MPI_COMM_WORLD,&processId))
       != MPI_SUCCESS)
  {
    char *msg = MonaController::ErrorString(err);
    vtkErrorMacro("MPI error occurred: " << msg);
    delete[] msg;
    return 0;
  }

  this->Active = 1;
  if (processId == 0)
  {
    this->BeginId = MPE_Log_get_event_number();
    this->EndId = MPE_Log_get_event_number();
    MPE_Describe_state(this->BeginId, this->EndId, const_cast<char*>(name),
                       const_cast<char*>(desc));
  }
  MPI_Bcast(&this->BeginId, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&this->EndId, 1, MPI_INT, 0, MPI_COMM_WORLD);

  return 1;
}

void MonaEventLog::StartLogging()
{
  if (!this->Active)
  {
    vtkWarningMacro("This MonaEventLog has not been initialized. Can not log event.");
    return;
  }

  MPE_Log_event(this->BeginId, 0, "begin");
}

void MonaEventLog::StopLogging()
{
  if (!this->Active)
  {
    vtkWarningMacro("This MonaEventLog has not been initialized. Can not log event.");
    return;
  }
  MPE_Log_event(this->EndId, 0, "end");
}

MonaEventLog::~MonaEventLog()
{
}


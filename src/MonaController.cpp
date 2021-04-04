/*=========================================================================

Program:   Visualization Toolkit
Module:    MonaController.cxx

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "MonaController.hpp"

#include <vtkIntArray.h>
#include <vtkObjectFactory.h>
#include <vtkOutputWindow.h>
#include <vtkSmartPointer.h>

#include <cassert>

#include <spdlog/spdlog.h>
#include "Mona.hpp"

#define VTK_CREATE(type, name) vtkSmartPointer<type> name = vtkSmartPointer<type>::New()

#ifdef DEBUG_BUILD
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define DEBUG(...)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

int MonaController::Initialized = 0;
char MonaController::ProcessorName[MPI_MAX_PROCESSOR_NAME] = "";
int MonaController::UseSsendForRMI = 0;

// Output window which prints out the process id
// with the error or warning messages
class MonaOutputWindow : public vtkOutputWindow
{
public:
  vtkTypeMacro(MonaOutputWindow, vtkOutputWindow);

  void DisplayText(const char* t) override
  {
    if (this->Controller && MonaController::Initialized)
    {
      cout << "Process id: " << this->Controller->GetLocalProcessId() << " >> ";
    }
    cout << t;
  }

  MonaOutputWindow() { this->Controller = 0; }

  friend class MonaController;

protected:
  MonaController* Controller;
  MonaOutputWindow(const MonaOutputWindow&);
  void operator=(const MonaOutputWindow&);
};

void MonaController::CreateOutputWindow()
{
  DEBUG("{}", __FUNCTION__ );
  MonaOutputWindow* window = new MonaOutputWindow;
  window->InitializeObjectBase();
  window->Controller = this;
  this->OutputWindow = window;
  vtkOutputWindow::SetInstance(this->OutputWindow);
}

vtkStandardNewMacro(MonaController);

//----------------------------------------------------------------------------
MonaController::MonaController()
{
  DEBUG("{} constructor", __FUNCTION__ );
  // If MPI was already initialized obtain rank and size.
  if (MonaController::Initialized)
  {
    this->InitializeCommunicator(MonaCommunicator::GetWorldCommunicator());
    // TODO, consider the RMICommunicator when it is needed
    this->RMICommunicator = NULL;
  }

  this->OutputWindow = 0;
}

//----------------------------------------------------------------------------
MonaController::~MonaController()
{
  DEBUG("{} destructor", __FUNCTION__ );
  this->SetCommunicator(0);
  if (this->RMICommunicator)
  {
    this->RMICommunicator->Delete();
  }
}

//----------------------------------------------------------------------------
void MonaController::PrintSelf(ostream& os, vtkIndent indent)
{
  DEBUG("{}", __FUNCTION__ );
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Initialized: " << (MonaController::Initialized ? "(yes)" : "(no)") << endl;
}

MonaCommunicator* MonaController::WorldRMICommunicator = 0;

//----------------------------------------------------------------------------
void MonaController::TriggerRMIInternal(
  int remoteProcessId, void* arg, int argLength, int rmiTag, bool propagate)
{
  DEBUG("{}: argLength={}, rmiTag={} propagate={}", __FUNCTION__, argLength, rmiTag, propagate);
  MonaCommunicator* mpiComm = MonaCommunicator::SafeDownCast(this->RMICommunicator);
  int use_ssend = mpiComm->GetUseSsend();
  if (MonaController::UseSsendForRMI == 1 && use_ssend == 0)
  {
    mpiComm->SetUseSsend(1);
  }

  this->Superclass::TriggerRMIInternal(remoteProcessId, arg, argLength, rmiTag, propagate);

  if (MonaController::UseSsendForRMI == 1 && use_ssend == 0)
  {
    mpiComm->SetUseSsend(0);
  }
}

//----------------------------------------------------------------------------
void MonaController::Initialize(int*, char***, int)
{
  DEBUG("{}", __FUNCTION__ );
  if (MonaController::Initialized)
  {
    vtkWarningMacro("Already initialized.");
    return;
  }

  // Can be done once in the program.
  MonaController::Initialized = 1;
  //the actual operations to create the mona comm
  //and register it into the controller
  this->InitializeCommunicator(MonaCommunicator::GetWorldCommunicator());

  // XXX TODO fill out processor name (mona self address)

  // TODO? set it as GetWorldCommunicator or null currently
  // consider this thing when the RMICommunicator will be used actually
  // create the WorldRMICommunicator
  MonaController::WorldRMICommunicator = NULL;

  // set the necessary info into the new communicator
  // ((MonaCommunicator*)(this->Communicator))->Duplicate(MonaController::WorldRMICommunicator);

  this->RMICommunicator = NULL;
  // Since we use Delete to get rid of the reference, we should use nullptr to
  // register.
  // TODO, consider RMICommunicator in future
  // this->RMICommunicator->Register(nullptr);
  this->Modified();
}

//use the mona created outside by the caller
void MonaController::Initialize(mona_comm_t mona_comm)
{
  DEBUG("{}: comm={}",  __FUNCTION__, (void*)mona_comm);

  //this may initilized multiple times, with new comm
  if (MonaController::Initialized)
  {
    vtkWarningMacro("Already initialized.");
    return;
  }

  // Can be done once in the program.
  MonaController::Initialized = 1;
  //the actual operations to create the mona comm
  //and register it into the controller
  this->InitializeCommunicator(MonaCommunicator::GetWorldCommunicatorByMona(mona_comm));

  // XXX TODO fill out processor name (mona self address)

  // TODO? set it as GetWorldCommunicator or null currently
  // consider this thing when the RMICommunicator will be used actually
  // create the WorldRMICommunicator
  MonaController::WorldRMICommunicator = NULL;

  // set the necessary info into the new communicator
  // ((MonaCommunicator*)(this->Communicator))->Duplicate(MonaController::WorldRMICommunicator);

  this->RMICommunicator = NULL;
  // Since we use Delete to get rid of the reference, we should use nullptr to
  // register.
  // TODO, consider RMICommunicator in future
  // this->RMICommunicator->Register(nullptr);
  this->Modified();
}


const char* MonaController::GetProcessorName()
{
  DEBUG("{}", __FUNCTION__ );
  return ProcessorName;
}

// Good-bye world
void MonaController::Finalize()
{
  DEBUG("{}", __FUNCTION__ );
  if (MonaController::Initialized)
  {
    if(MonaCommunicator::WorldCommunicator!=0){
        MonaCommunicator::WorldCommunicator->Delete();
        MonaCommunicator::WorldCommunicator = 0;
    }
    this->SetCommunicator(0);
    if (this->RMICommunicator)
    {
      this->RMICommunicator->Delete();
      this->RMICommunicator = 0;
    }
    MonaController::Initialized = 0;
    this->Modified();
  }
}

// Called by SetCommunicator and constructor.
void MonaController::InitializeCommunicator(MonaCommunicator* comm)
{
  DEBUG("{}: comm={}, mona_comm={}",
          __FUNCTION__, (void*)comm, (void*)(comm ? comm->MonaComm->GetHandle() : nullptr));
  if (this->Communicator != comm)
  {
    if (this->Communicator != 0)
    {
      this->Communicator->UnRegister(this);
    }
    this->Communicator = comm;
    if (this->Communicator != 0)
    {
      this->Communicator->Register(this);
    }

    this->Modified();
  }
}

// Delete the previous RMI communicator and creates a new one
// by duplicating the user communicator.

void MonaController::InitializeRMICommunicator()
{
  DEBUG("{}", __FUNCTION__ );
  if (this->RMICommunicator)
  {
    this->RMICommunicator->Delete();
    this->RMICommunicator = 0;
  }
  if (this->Communicator)
  {
    this->RMICommunicator = MonaCommunicator::New();
    ((MonaCommunicator*)this->RMICommunicator)->Duplicate((MonaCommunicator*)this->Communicator);
  }
}

void MonaController::SetCommunicator(MonaCommunicator* comm)
{
    if(comm) {
        DEBUG("{}: comm={} mona_comm={}",  __FUNCTION__, (void*)comm, (void*)comm->MonaComm->GetHandle());
    } else {
        DEBUG("{}: comm=null", __FUNCTION__);
    }
  //TODO free the old communicator
  this->InitializeCommunicator(comm);
  //TODO consider RMI communication in future
  //this->InitializeRMICommunicator();
}

//----------------------------------------------------------------------------
// Execute the method set as the SingleMethod.
void MonaController::SingleMethodExecute()
{
  DEBUG("{}", __FUNCTION__ );
  if (!MonaController::Initialized)
  {
    vtkWarningMacro("MPI has to be initialized first.");
    return;
  }

  if (this->GetLocalProcessId() < this->GetNumberOfProcesses())
  {
    if (this->SingleMethod)
    {
      vtkMultiProcessController::SetGlobalController(this);
      (this->SingleMethod)(this, this->SingleData);
    }
    else
    {
      vtkWarningMacro("SingleMethod not set.");
    }
  }
}

//----------------------------------------------------------------------------
// Execute the methods set as the MultipleMethods.
void MonaController::MultipleMethodExecute()
{
  DEBUG("{}", __FUNCTION__ );
  if (!MonaController::Initialized)
  {
    vtkWarningMacro("MPI has to be initialized first.");
    return;
  }

  int i = this->GetLocalProcessId();

  if (i < this->GetNumberOfProcesses())
  {
    vtkProcessFunctionType multipleMethod;
    void* multipleData;
    this->GetMultipleMethod(i, multipleMethod, multipleData);
    if (multipleMethod)
    {
      vtkMultiProcessController::SetGlobalController(this);
      (multipleMethod)(this, multipleData);
    }
    else
    {
      vtkWarningMacro("MultipleMethod " << i << " not set.");
    }
  }
}

char* MonaController::ErrorString(int err)
{
    char* error = new char[64];
    memset(error, 0, 64);
    strcpy(error, std::to_string(err).c_str());
    return error;
}

//-----------------------------------------------------------------------------
MonaController* MonaController::CreateSubController(vtkProcessGroup* group)
{
    throw std::runtime_error("MonaController::CreateSubController not implemented");
    return nullptr;
}

//-----------------------------------------------------------------------------
MonaController* MonaController::PartitionController(int localColor, int localKey)
{
    throw std::runtime_error("MonaController::PartitionController not implemented");
    return nullptr;
}

//-----------------------------------------------------------------------------
int MonaController::WaitSome(
  const int count, MonaCommunicator::Request rqsts[], vtkIntArray* completed)
{
  DEBUG("{}", __FUNCTION__ );
  assert("pre: completed array is nullptr!" && (completed != nullptr));

  // Allocate set of completed requests
  completed->SetNumberOfComponents(1);
  completed->SetNumberOfTuples(count);

  // Downcast to Mona communicator
  MonaCommunicator* myMonaCommunicator = (MonaCommunicator*)this->Communicator;

  // Delegate to Mona communicator
  int N = 0;
  int rc = myMonaCommunicator->WaitSome(count, rqsts, N, completed->GetPointer(0));
  assert("post: Number of completed requests must N > 0" && (N > 0) && (N < (count - 1)));
  completed->Resize(N);

  return (rc);
}

//-----------------------------------------------------------------------------
bool MonaController::TestAll(const int count, MonaCommunicator::Request requests[])
{
  DEBUG("{}", __FUNCTION__ );
  int flag = 0;

  // Downcast to MPI communicator
  MonaCommunicator* myMonaCommunicator = (MonaCommunicator*)this->Communicator;

  myMonaCommunicator->TestAll(count, requests, flag);
  if (flag)
  {
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
bool MonaController::TestAny(const int count, MonaCommunicator::Request requests[], int& idx)
{
  DEBUG("{}", __FUNCTION__ );
  int flag = 0;

  // Downcast to MPI communicator
  MonaCommunicator* myMonaCommunicator = (MonaCommunicator*)this->Communicator;

  myMonaCommunicator->TestAny(count, requests, idx, flag);
  if (flag)
  {
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
bool MonaController::TestSome(
  const int count, MonaCommunicator::Request requests[], vtkIntArray* completed)
{
  DEBUG("{}", __FUNCTION__ );
  assert("pre: completed array is nullptr" && (completed != nullptr));

  // Allocate set of completed requests
  completed->SetNumberOfComponents(1);
  completed->SetNumberOfTuples(count);

  // Downcast to MPI communicator
  MonaCommunicator* myMonaCommunicator = (MonaCommunicator*)this->Communicator;

  int N = 0;
  myMonaCommunicator->TestSome(count, requests, N, completed->GetPointer(0));
  assert("post: Number of completed requests must N > 0" && (N > 0) && (N < (count - 1)));

  if (N > 0)
  {
    completed->Resize(N);
    return true;
  }
  else
  {
    completed->Resize(0);
    return false;
  }
}

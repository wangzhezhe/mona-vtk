/*=========================================================================

  Program:   Visualization Toolkit
  Module:    MonaCommunicator.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "MonaCommunicator.hpp"

#include "Mona.hpp"
#include "MonaController.hpp"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"
#include "vtkProcessGroup.h"
#include "vtkRectilinearGrid.h"
#include "vtkSmartPointer.h"
#include "vtkStructuredGrid.h"
#include "vtkToolkits.h"

#include <spdlog/spdlog.h>

#define VTK_CREATE(type, name) vtkSmartPointer<type> name = vtkSmartPointer<type>::New()

#include <cassert>
#include <vector>

#ifdef DEBUG_BUILD
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define DEBUG(...)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

vtkStandardNewMacro(MonaCommunicator);

MonaCommunicator* MonaCommunicator::WorldCommunicator = 0;

#define MONA_COMM_BARRIER_TAG 1000
#define MONA_COMM_BCAST_TAG 1001
#define MONA_COMM_GATHER_TAG 1002
#define MONA_COMM_REDUCE_TAG 1003
#define MONA_COMM_ALLREDUCE_TAG 1004

MonaCommunicatorOpaqueComm::MonaCommunicatorOpaqueComm(mona_comm_t handle)
{
    DEBUG("{}: handle={}", __FUNCTION__, (void*)handle);
  this->Handle = handle;
}

mona_comm_t MonaCommunicatorOpaqueComm::GetHandle()
{
  DEBUG("{}", __FUNCTION__);
  return this->Handle;
}

//----------------------------------------------------------------------------
// I wish I could think of a better way to convert a VTK type enum to an MPI
// type enum and back.

// Make sure MPI_LONG_LONG and MPI_UNSIGNED_LONG_LONG are defined, if at all
// possible.
#ifndef MPI_LONG_LONG
#ifdef MPI_LONG_LONG_INT
// lampi only has MPI_LONG_LONG_INT, not MPI_LONG_LONG
#define MPI_LONG_LONG MPI_LONG_LONG_INT
#endif
#endif

#ifndef MPI_UNSIGNED_LONG_LONG
#ifdef MPI_UNSIGNED_LONG_LONG_INT
#define MPI_UNSIGNED_LONG_LONG MPI_UNSIGNED_LONG_LONG_INT
#elif defined(MPI_LONG_LONG)
// mpich does not have an unsigned long long.  Just using signed should
// be OK.  Everything uses 2's complement nowadays, right?
#define MPI_UNSIGNED_LONG_LONG MPI_LONG_LONG
#endif
#endif

inline MPI_Datatype MonaCommunicatorGetMPIType(int vtkType)
{
  switch (vtkType)
  {
    case VTK_CHAR:
      return MPI_CHAR;
#ifdef MPI_SIGNED_CHAR
    case VTK_SIGNED_CHAR:
      return MPI_SIGNED_CHAR;
#else
    case VTK_SIGNED_CHAR:
      return MPI_CHAR;
#endif
    case VTK_UNSIGNED_CHAR:
      return MPI_UNSIGNED_CHAR;
    case VTK_SHORT:
      return MPI_SHORT;
    case VTK_UNSIGNED_SHORT:
      return MPI_UNSIGNED_SHORT;
    case VTK_INT:
      return MPI_INT;
    case VTK_UNSIGNED_INT:
      return MPI_UNSIGNED;
    case VTK_LONG:
      return MPI_LONG;
    case VTK_UNSIGNED_LONG:
      return MPI_UNSIGNED_LONG;
    case VTK_FLOAT:
      return MPI_FLOAT;
    case VTK_DOUBLE:
      return MPI_DOUBLE;

#ifdef VTK_USE_64BIT_IDS
#if VTK_SIZEOF_LONG == 8
    case VTK_ID_TYPE:
      return MPI_LONG;
#elif defined(MPI_LONG_LONG)
    case VTK_ID_TYPE:
      return MPI_LONG_LONG;
#else
    case VTK_ID_TYPE:
      vtkGenericWarningMacro(
        "This systems MPI doesn't seem to support 64 bit ids and you have 64 bit IDs turned on. "
        "Please seek assistance on the VTK Discourse (https://discourse.vtk.org/).");
      return MPI_LONG;
#endif
#else  // VTK_USE_64BIT_IDS
    case VTK_ID_TYPE:
      return MPI_INT;
#endif // VTK_USE_64BIT_IDS

#ifdef MPI_LONG_LONG
    case VTK_LONG_LONG:
      return MPI_LONG_LONG;
    case VTK_UNSIGNED_LONG_LONG:
      return MPI_UNSIGNED_LONG_LONG;
#endif

    default:
      vtkGenericWarningMacro("Could not find a supported MPI type for VTK type " << vtkType);
      return MPI_BYTE;
  }
}

inline int MonaCommunicatorGetVTKType(MPI_Datatype type)
{
  DEBUG("{}", __FUNCTION__);
  if (type == MPI_FLOAT)
    return VTK_FLOAT;
  if (type == MPI_DOUBLE)
    return VTK_DOUBLE;
  if (type == MPI_BYTE)
    return VTK_CHAR;
  if (type == MPI_CHAR)
    return VTK_CHAR;
  if (type == MPI_UNSIGNED_CHAR)
    return VTK_UNSIGNED_CHAR;
#ifdef MPI_SIGNED_CHAR
  if (type == MPI_SIGNED_CHAR)
    return VTK_SIGNED_CHAR;
#endif
  if (type == MPI_SHORT)
    return VTK_SHORT;
  if (type == MPI_UNSIGNED_SHORT)
    return VTK_UNSIGNED_SHORT;
  if (type == MPI_INT)
    return VTK_INT;
  if (type == MPI_UNSIGNED)
    return VTK_UNSIGNED_INT;
  if (type == MPI_LONG)
    return VTK_LONG;
  if (type == MPI_UNSIGNED_LONG)
    return VTK_UNSIGNED_LONG;
#ifdef MPI_LONG_LONG
  if (type == MPI_LONG_LONG)
    return VTK_LONG_LONG;
#endif
#ifdef MPI_UNSIGNED_LONG_LONG
  if (type == MPI_UNSIGNED_LONG_LONG)
    return VTK_UNSIGNED_LONG_LONG;
#endif

  vtkGenericWarningMacro("Received unrecognized MPI type.");
  return VTK_CHAR;
}

inline int MonaCommunicatorCheckSize(vtkIdType length)
{
  DEBUG("{}: length={}", __FUNCTION__, length);
  if (length > VTK_INT_MAX)
  {
    vtkGenericWarningMacro(<< "This operation not yet supported for more than " << VTK_INT_MAX
                           << " objects");
    return 0;
  }
  else
  {
    return 1;
  }
}


template <class T>
int MonaCommunicatorSendData(const T* data, int length, int sizeoftype, int remoteProcessId,
  int tag, mona_comm_t monacomm, int useCopy, int useSsend)
{
  DEBUG("{}: length={}, typeSize={}, dest={}, tag={}, comm={}", __FUNCTION__,
          length, sizeoftype, remoteProcessId, tag, (void*)monacomm);
  int retStatus;
  if (useCopy)
  {
    char* tmpData = MonaCommunicator::Allocate(length * sizeoftype);
    memcpy(tmpData, data, length * sizeoftype);
    // execute mona comm send
    mona_comm_send(monacomm, tmpData, length * sizeoftype, remoteProcessId, tag);
    MonaCommunicator::Free(tmpData);
    return retStatus;
  }
  else
  {
    retStatus =
      mona_comm_send(monacomm, const_cast<T*>(data), length * sizeoftype, remoteProcessId, tag);
    return retStatus;
  }
}

int MonaCommunicator::ReceiveDataInternal(char* data, int length, int sizeoftype,
  int remoteProcessId, int tag, mona_comm_t monacomm, int useCopy, int& senderId)
{
  DEBUG("{}: length={}, typeSize={}, src={}, tag={}, comm={}",
          __FUNCTION__, length, sizeoftype, remoteProcessId, tag, (void*)monacomm);

  if (remoteProcessId == vtkMultiProcessController::ANY_SOURCE)
  {
    // the -1 represent the any source in colza
    remoteProcessId = -1;
  }

  int retVal;

  if (useCopy)
  {
    char* tmpData = MonaCommunicator::Allocate(length * sizeoftype);
    na_size_t recv_size;
    retVal = mona_comm_recv(
      monacomm, tmpData, length * sizeoftype, remoteProcessId, tag, &recv_size, NULL, NULL);
    memcpy(data, tmpData, length * sizeoftype);
    MonaCommunicator::Free(tmpData);
  }
  else
  {
    na_size_t recv_size;
    retVal = mona_comm_recv(
      monacomm, data, length * sizeoftype, remoteProcessId, tag, &recv_size, NULL, NULL);
  }

  if (retVal == 0)
  {
    senderId = remoteProcessId;
  }
  return retVal;
}

//-----------------------------------------------------------------------------
// Method for converting an MPI operation to a
// vtkMultiProcessController::Operation.
// MPIAPI is defined in the microsoft mpi.h which decorates
// with the __stdcall decoration.
static vtkCommunicator::Operation* CurrentOperation;
extern "C" void MonaCommunicatorUserFunction(
  void* invec, void* inoutvec, int* len, MPI_Datatype* datatype)
{
  DEBUG("{}", __FUNCTION__);
  int vtkType = MonaCommunicatorGetVTKType(*datatype);
  CurrentOperation->Function(invec, inoutvec, *len, vtkType);
}

//----------------------------------------------------------------------------
// Return the world communicator (i.e. MPI_COMM_WORLD).
// Create one if necessary (singleton).
MonaCommunicator* MonaCommunicator::GetWorldCommunicatorByMona(mona_comm_t mona_comm)
{
  DEBUG("{}: comm={}", __FUNCTION__, (void*)mona_comm);

  if (MonaCommunicator::WorldCommunicator == 0)
  {
    MonaCommunicator* comm = MonaCommunicator::New();
    comm->MonaComm->Handle = mona_comm;
    comm->InitializeNumberOfProcesses();
    comm->Initialized = 1;
    comm->KeepHandleOn();
    MonaCommunicator::WorldCommunicator = comm;
  }
  return MonaCommunicator::WorldCommunicator;
}

MonaCommunicator* MonaCommunicator::GetWorldCommunicator()
{
  DEBUG("{}", __FUNCTION__);
  int err, size;

  if (MonaCommunicator::WorldCommunicator == 0)
  {
      throw std::runtime_error("MonaCommunicator::GetWorldCommunicator: WorldCommunicator not set "
              "(use MonaCommunicator::GetWorldCommunicatorByMona instead)");
  }
  return MonaCommunicator::WorldCommunicator;
}

//----------------------------------------------------------------------------
void MonaCommunicator::PrintSelf(ostream& os, vtkIndent indent)
{
  DEBUG("{}", __FUNCTION__);
  this->Superclass::PrintSelf(os, indent);
  os << indent << "MPI Communicator handler: ";
  if (this->MonaComm->Handle)
  {
    os << this->MonaComm->Handle << endl;
  }
  else
  {
    os << "(none)\n";
  }
  os << indent << "UseSsend: " << (this->UseSsend ? "On" : " Off") << endl;
  os << indent << "Initialized: " << (this->Initialized ? "On\n" : "Off\n");
  os << indent << "Keep handle: " << (this->KeepHandle ? "On\n" : "Off\n");
  if (this != MonaCommunicator::WorldCommunicator)
  {
    os << indent << "World communicator: ";
    if (MonaCommunicator::WorldCommunicator)
    {
      os << endl;
      MonaCommunicator::WorldCommunicator->PrintSelf(os, indent.GetNextIndent());
    }
    else
    {
      os << "(none)";
    }
    os << endl;
  }
  return;
}

//----------------------------------------------------------------------------
MonaCommunicator::MonaCommunicator()
{
  DEBUG("{} constructor", __FUNCTION__);
  this->MonaComm = new MonaCommunicatorOpaqueComm;
  this->Initialized = 0;
  this->KeepHandle = 0;
  this->LastSenderId = -1;
  this->UseSsend = 0;
}

//----------------------------------------------------------------------------
MonaCommunicator::~MonaCommunicator()
{
  DEBUG("{} destructor", __FUNCTION__);
  // Free the handle if required and asked for.
  if (this->MonaComm)
  {
    if (this->MonaComm->Handle && !this->KeepHandle)
    {
      if (this->MonaComm->Handle != NULL)
      {
        mona_comm_free(this->MonaComm->Handle);
      }
    }
    delete this->MonaComm;
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Initialize(vtkProcessGroup* group)
{
    throw std::runtime_error("MonaCommunicator::Initialize(vtkProcessGroup* group) not implemented");
    return 1;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::SplitInitialize(vtkCommunicator* oldcomm, int color, int key)
{
    throw std::runtime_error("MonaCommunicator::SplitInitialize not implemented");
    return 1;
}

int MonaCommunicator::InitializeExternal(MonaCommunicatorOpaqueComm* comm)
{
  DEBUG("{}: comm={}, mona_comm={}", __FUNCTION__, (void*)comm, (void*)comm->GetHandle());
  this->KeepHandleOn();
  free(this->MonaComm->Handle); // XXX ??
  this->MonaComm->Handle = comm->GetHandle();
  this->InitializeNumberOfProcesses();
  this->Initialized = 1;

  this->Modified();

  return 1;
}

//----------------------------------------------------------------------------
// Start the copying process

void MonaCommunicator::InitializeCopy(MonaCommunicator* source)
{
    throw std::runtime_error("MonaCommunicator::InitializeCopy it not implemented");
}

//-----------------------------------------------------------------------------
// Set the number of processes and maximum number of processes
// to the size obtained from Mona.
int MonaCommunicator::InitializeNumberOfProcesses()
{
  DEBUG("{}",__FUNCTION__);
  int ret;
  this->Modified();
  mona_comm_t mona_comm = this->MonaComm->Handle;
  // get size and rank
  int size, rank;
  ret = mona_comm_size(mona_comm, &size);
  //std::cout << "debug size " << size << std::endl;
  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona size");
    return -1;
  }

  ret = mona_comm_rank(mona_comm, &rank);
  //std::cout << "debug rank " << rank << std::endl;

  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona rank");
    return -1;
  }

  this->MaximumNumberOfProcesses = size;
  this->NumberOfProcesses = this->MaximumNumberOfProcesses;
  // get the rank id
  // TODO check error
  this->LocalProcessId = rank;
  return 1;
}

//----------------------------------------------------------------------------
// Copy the MPI handle
void MonaCommunicator::CopyFrom(MonaCommunicator* source)
{
    throw std::runtime_error("MonaCommunicator::CopyFrom not implemented");
}

//----------------------------------------------------------------------------
// Duplicate the MPI handle

void MonaCommunicator::Duplicate(MonaCommunicator* source)
{
    throw std::runtime_error("MonaCommunicator::Duplicate not implemented");
}

//----------------------------------------------------------------------------
char* MonaCommunicator::Allocate(size_t size)
{
  DEBUG("{}: size={}", __FUNCTION__, size);
  return new char[size];
}

//----------------------------------------------------------------------------
void MonaCommunicator::Free(char* ptr)
{
  DEBUG("{}", __FUNCTION__);
  delete[] ptr;
}

//----------------------------------------------------------------------------
int MonaCommunicator::CheckForMPIError(int err)
{
  DEBUG("{}: err={}", __FUNCTION__, err);

  if (err == MPI_SUCCESS)
  {
    return 1;
  }
  else
  {
    char* msg = MonaController::ErrorString(err);
    vtkGenericWarningMacro("MPI error occurred: " << msg);
    delete[] msg;
    return 0;
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::SendVoidArray(
  const void* data, vtkIdType length, int type, int remoteProcessId, int tag)
{
  DEBUG("{}: length={}, type={}, dest={}, tag={}",
          __FUNCTION__, length, type, remoteProcessId, tag);
  const char* byteData = static_cast<const char*>(data);
  // MPI_Datatype mpiType = MonaCommunicatorGetMPIType(type);
  int sizeOfType;
  switch (type)
  {
    vtkTemplateMacro(sizeOfType = sizeof(VTK_TT));
    default:
      vtkWarningMacro(<< "Invalid data type " << type);
      sizeOfType = 1;
      break;
  }

  int maxSend = VTK_INT_MAX;
  int status;
  while (length >= maxSend)
  {
    status = MonaCommunicatorSendData(byteData, maxSend, sizeOfType, remoteProcessId, tag,
      this->MonaComm->Handle, vtkCommunicator::UseCopy, this->UseSsend);
    if (status != 0)
    {
      // Failed to send.
      std::cerr << "failed to send void array" << std::endl;
      return 0;
    }
    byteData += maxSend * sizeOfType;
    length -= maxSend;
  }
  status = MonaCommunicatorSendData(byteData, length, sizeOfType, remoteProcessId, tag,
    this->MonaComm->Handle, vtkCommunicator::UseCopy, this->UseSsend);
  if (status == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//-----------------------------------------------------------------------------
inline vtkIdType MonaCommunicatorMin(vtkIdType a, vtkIdType b)
{
  return (a > b) ? b : a;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::ReceiveVoidArray(
  void* data, vtkIdType maxlength, int type, int remoteProcessId, int tag)
{
  DEBUG("{}: maxlength={}, type={}, src={}, tag={}",
          __FUNCTION__, maxlength, type, remoteProcessId, tag);
  this->Count = 0;
  char* byteData = static_cast<char*>(data);

  int sizeOfType;
  switch (type)
  {
    vtkTemplateMacro(sizeOfType = sizeof(VTK_TT));
    default:
      vtkWarningMacro(<< "Invalid data type " << type);
      sizeOfType = 1;
      break;
  }

  // maxReceive is the maximum size of data that can be fetched in a one atomic
  // receive. If when sending the data-length >= maxReceive, then the sender
  // splits it into multiple packets of at most maxReceive size each.  (Note
  // that when the sending exactly maxReceive length message, it is split into 2
  // packets of sizes maxReceive and 0 respectively).
  int maxReceive = VTK_INT_MAX;

  // TODO, use the communicator info when it is necessary
  // there are not enough type and status information in mona currently
  // MonaCommunicatorReceiveDataInfo info;

  while (this->ReceiveDataInternal(byteData, MonaCommunicatorMin(maxlength, maxReceive), sizeOfType,
           remoteProcessId, tag, this->MonaComm->Handle, vtkCommunicator::UseCopy,
           this->LastSenderId) == 0)
  {
    remoteProcessId = this->LastSenderId;
    // TODO, add check, assume all words are recieved
    int words_received = MonaCommunicatorMin(maxlength, maxReceive);
    // if (CheckForMPIError(
    //        MPI_Get_count(&info.Status, mpiType, &words_received)) == 0)
    //{
    // Failed.
    //  return 0;
    //}
    this->Count += words_received;
    byteData += words_received * sizeOfType;
    maxlength -= words_received;
    if (words_received < maxReceive)
    {
      // words_received in this packet is exactly equal to maxReceive, then it
      // means that the sender is sending at least one more packet for this
      // message. Otherwise, we have received all the packets for this message
      // and we no longer need to iterate.
      return 1;
    }
  }
  return 0;
}

//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const int* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const unsigned long* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const char* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const unsigned char* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const float* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const double* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
#ifdef VTK_USE_64BIT_IDS
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockSend(
  const vtkIdType* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockSend not implemented");
    return 0;
}
#endif

//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  int* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  unsigned long* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  char* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  unsigned char* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  float* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  double* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
#ifdef VTK_USE_64BIT_IDS
//----------------------------------------------------------------------------
int MonaCommunicator::NoBlockReceive(
  vtkIdType* data, int length, int remoteProcessId, int tag, Request& req)
{
    throw std::runtime_error("MonaCommunicator::NoBlockReceive not implemented");
    return 0;
}
#endif

//----------------------------------------------------------------------------
MonaCommunicator::Request::Request()
{
  DEBUG("{}", __FUNCTION__);
  this->Req = new MonaCommunicatorOpaqueRequest;
}

//----------------------------------------------------------------------------
MonaCommunicator::Request::Request(const MonaCommunicator::Request& src)
{
  DEBUG("{}", __FUNCTION__);
  this->Req = new MonaCommunicatorOpaqueRequest;
  this->Req->Handle = src.Req->Handle;
}

//----------------------------------------------------------------------------
MonaCommunicator::Request& MonaCommunicator::Request::operator=(
  const MonaCommunicator::Request& src)
{
  DEBUG("{}", __FUNCTION__);
  if (this == &src)
  {
    return *this;
  }
  this->Req->Handle = src.Req->Handle;
  return *this;
}

//----------------------------------------------------------------------------
MonaCommunicator::Request::~Request()
{
  DEBUG("{}", __FUNCTION__);
  delete this->Req;
}

//----------------------------------------------------------------------------
int MonaCommunicator::Request::Test()
{
  DEBUG("{}", __FUNCTION__);
  int retVal;
  int err = mona_test(this->Req->Handle, &retVal);

  if (err == NA_SUCCESS)
  {
    return retVal;
  }
  else
  {
    vtkGenericWarningMacro("MoNA error occurred in mona_test: " << err);
    return 0;
  }
}

//----------------------------------------------------------------------------
void MonaCommunicator::Request::Wait()
{
  DEBUG("{}", __FUNCTION__);

  int err = mona_wait(this->Req->Handle);

  if (err != NA_SUCCESS)
  {
    vtkGenericWarningMacro("MoNA error occurred in mona_wait: " << err);
  }
}

//----------------------------------------------------------------------------
void MonaCommunicator::Request::Cancel()
{
  DEBUG("{}", __FUNCTION__);
    vtkGenericWarningMacro("Cancelling a MoNA request is not supported");
}

//-----------------------------------------------------------------------------
void MonaCommunicator::Barrier()
{
  DEBUG("{}", __FUNCTION__);
  mona_comm_barrier(this->MonaComm->Handle, 0);
}

//----------------------------------------------------------------------------
int MonaCommunicator::BroadcastVoidArray(void* data, vtkIdType length, int type, int root)
{
  DEBUG("{}: length={}, type={}, root={}",
          __FUNCTION__, length, type, root);
  if (!MonaCommunicatorCheckSize(length))
    return 0;

  // check the type length
  int sizeOfType;
  switch (type)
  {
    vtkTemplateMacro(sizeOfType = sizeof(VTK_TT));
    default:
      vtkWarningMacro(<< "Invalid data type " << type);
      sizeOfType = 1;
      break;
  }
  // barrier
  auto mona_comm = this->MonaComm->GetHandle();

  size_t dataSize = sizeOfType * length;
  // it only works when we add log here and open the debug
  na_return_t status = mona_comm_bcast(mona_comm, data, dataSize, root, MONA_COMM_BCAST_TAG);
  if (status == NA_SUCCESS)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::GatherVoidArray(
  const void* sendBuffer, void* recvBuffer, vtkIdType length, int type, int destProcessId)
{
  DEBUG("{}: length={}, type={}, root={}",
          __FUNCTION__, length, type, destProcessId);

  // check the type length
  int sizeOfType;
  switch (type)
  {
    vtkTemplateMacro(sizeOfType = sizeof(VTK_TT));
    default:
      vtkWarningMacro(<< "Invalid data type " << type);
      sizeOfType = 1;
      break;
  }

  size_t dataSize = sizeOfType * length;
  auto mona_comm = this->MonaComm->GetHandle();
  na_return_t status = mona_comm_gather(
    mona_comm, sendBuffer, dataSize, recvBuffer, destProcessId, MONA_COMM_GATHER_TAG);
  return (status == NA_SUCCESS);
}

//-----------------------------------------------------------------------------
int MonaCommunicator::GatherVVoidArray(const void* sendBuffer, void* recvBuffer,
  vtkIdType sendLength, vtkIdType* recvLengths, vtkIdType* offsets, int type, int destProcessId)
{
    DEBUG("{}: sendLength={}, type={}, destProcessId={}",
          __FUNCTION__, sendLength, type, destProcessId);
  if (this->LocalProcessId == destProcessId)
  {
    int result = 1;
    char* dest = reinterpret_cast<char*>(recvBuffer);
    int typeSize = 1;
    switch (type)
    {
      vtkTemplateMacro(typeSize = sizeof(VTK_TT));
    }
    // Copy local data first in case buffers are the same.
    memmove(dest + offsets[this->LocalProcessId] * typeSize, sendBuffer, sendLength * typeSize);
    // Receive everything else.
    for (int i = 0; i < this->NumberOfProcesses; i++)
    {
      if (this->LocalProcessId != i)
      {
        result &= this->ReceiveVoidArray(
          dest + offsets[i] * typeSize, recvLengths[i], type, i, GATHERV_TAG);
      }
    }
    return result;
  }
  else
  {
    return this->SendVoidArray(sendBuffer, sendLength, type, destProcessId, GATHERV_TAG);
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::ScatterVoidArray(
  const void* sendBuffer, void* recvBuffer, vtkIdType length, int type, int srcProcessId)
{
    throw std::runtime_error("MonaCommunicator::ScatterVoidArray not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::ScatterVVoidArray(const void* sendBuffer, void* recvBuffer,
  vtkIdType* sendLengths, vtkIdType* offsets, vtkIdType recvLength, int type, int srcProcessId)
{
    throw std::runtime_error("MonaCommunicator::ScatterVVoidArray not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::AllGatherVoidArray(
  const void* sendBuffer, void* recvBuffer, vtkIdType length, int type)
{   
    //this is called by the deepwater example
    DEBUG("{}: length={}, type={}", __FUNCTION__, length, type);
    int result = 1;
    result &= this->GatherVoidArray(sendBuffer, recvBuffer, length, type, 0);
    result &= this->BroadcastVoidArray(recvBuffer, length * this->NumberOfProcesses, type, 0);
    return result;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::AllGatherVVoidArray(const void* sendBuffer, void* recvBuffer,
  vtkIdType sendLength, vtkIdType* recvLengths, vtkIdType* offsets, int type)
{
  DEBUG("{}: sendlength={}, type={}", __FUNCTION__, sendLength, type);
  int result = 1;
  result &=
    this->GatherVVoidArray(sendBuffer, recvBuffer, sendLength, recvLengths, offsets, type, 0);
  // Find the maximum place in the array that contains data.
  vtkIdType maxIndex = 0;
  for (int i = 0; i < this->NumberOfProcesses; i++)
  {
    vtkIdType index = recvLengths[i] + offsets[i];
    maxIndex = (maxIndex < index) ? index : maxIndex;
  }
  result &= this->BroadcastVoidArray(recvBuffer, maxIndex, type, 0);
  return result;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::ReduceVoidArray(const void* sendBuffer, void* recvBuffer, vtkIdType length,
  int type, int operation, int destProcessId)
{
  DEBUG("{}: length={}, type={}, operation={}, root={}",
          __FUNCTION__, length, type, operation, destProcessId);
  auto mona_comm = this->MonaComm->GetHandle();

  // get mona operation
  /*
  vtk type: 6 operation: 0
  vtk type: 3 operation: 0
  vtk type: 11 operation: 0
  vtk type: 17 vtk operation: 2
  vtk type: 11 vtk operation: 1
  vtk type: 11 vtk operation: 0

  #define VTK_INT 6
  #define VTK_UNSIGNED_CHAR 3
  #define VTK_DOUBLE 11
  #define VTK_UNSIGNED_LONG_LONG 17

  operation
  0 MAX_OP
  1 MIN_OP
  2 SUM_OP
  */
  // TODO use more flexible type transfer instead of hardcode it in future
  size_t typesize;
  mona_op_t monaop;
  switch (operation)
  {
    case MAX_OP:
      if (type == VTK_INT)
      {
        monaop = mona_op_max_i32;
        typesize = sizeof(int32_t);
      }
      else if (type == VTK_UNSIGNED_CHAR)
      {
        monaop = mona_op_max_u8;
        typesize = sizeof(unsigned char);
      }
      else if (type == VTK_DOUBLE)
      {
        monaop = mona_op_max_f64;
        typesize = sizeof(double);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    case MIN_OP:
      if (type == VTK_DOUBLE)
      {
        monaop = mona_op_min_f64;
        typesize = sizeof(double);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    case SUM_OP:
      if (type == VTK_UNSIGNED_LONG_LONG)
      {
        monaop = mona_op_sum_u64;
        typesize = sizeof(uint64_t);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    default:
      vtkWarningMacro(<< "Operation number " << operation << " not supported.");
      return 0;
  }

  na_return_t status = mona_comm_reduce(mona_comm, sendBuffer, recvBuffer, typesize, length, monaop,
    NULL, destProcessId, MONA_COMM_REDUCE_TAG);

  if (status == NA_SUCCESS)
  {
    return true;
  }
  else
  {
    std::cerr << "failed for reduce with status " << status << std::endl;
    return false;
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::ReduceVoidArray(const void* sendBuffer, void* recvBuffer, vtkIdType length,
  int type, Operation* operation, int destProcessId)
{
    throw std::runtime_error("MonaCommunicator::ReduceVoidArray not implemented");
  return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::AllReduceVoidArray(
  const void* sendBuffer, void* recvBuffer, vtkIdType length, int type, int operation)
{
  DEBUG("{}: length={}, type={}, operation={}",
          __FUNCTION__, length, type, operation);
  // get comm
  auto mona_comm = this->MonaComm->GetHandle();

  //check info here
  // get size and rank
  /*
  int size, rank;
  int ret = mona_comm_size(mona_comm, &size);
  std::cout << "debug size for allreduce " << size << std::endl;
  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona size");
    return -1;
  }

  ret = mona_comm_rank(mona_comm, &rank);
  std::cout << "debug rank for allreduce " << rank << std::endl;

  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona rank");
    return -1;
  }
  */


  // get typesize and operator
  na_size_t typesize;
  mona_op_t monaop;
  switch (operation)
  {
    case MAX_OP:
      if (type == VTK_INT)
      {
        monaop = mona_op_max_i32;
        typesize = sizeof(int32_t);
      }
      else if (type == VTK_UNSIGNED_CHAR)
      {
        monaop = mona_op_max_u8;
        typesize = sizeof(unsigned char);
      }
      else if (type == VTK_DOUBLE)
      {
        monaop = mona_op_max_f64;
        typesize = sizeof(double);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    case MIN_OP:
      if (type == VTK_DOUBLE)
      {
        monaop = mona_op_min_f64;
        typesize = sizeof(double);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    case SUM_OP:
      if (type == VTK_UNSIGNED_LONG_LONG)
      {
        monaop = mona_op_sum_u64;
        typesize = sizeof(uint64_t);
      }
      else
      {
        vtkWarningMacro(<< "operation number " << operation << " and type number " << type
                        << " not supported.");
      }
      break;
    default:
      vtkWarningMacro(<< "Operation number " << operation << " not supported.");
      return 0;
  }

  na_return_t status = mona_comm_allreduce(
    mona_comm, sendBuffer, recvBuffer, typesize, length, monaop, NULL, MONA_COMM_ALLREDUCE_TAG);

  // the source code may use true or false to adjust if it is ok
  if (status == NA_SUCCESS)
  {
    return true;
  }
  else
  {
    std::cerr << "failed for allreduce" << std::endl;
    return false;
  }
}

//-----------------------------------------------------------------------------
int MonaCommunicator::AllReduceVoidArray(
  const void* sendBuffer, void* recvBuffer, vtkIdType length, int type, Operation* operation)
{
    throw std::runtime_error("MonaCommunicator::AllReduceVoidArray is not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::WaitAll(const int count, Request requests[])
{
    std::runtime_error("MonaCommunicator::WaitAll not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::WaitAny(const int count, Request requests[], int& idx)
{
    std::runtime_error("MonaCommunicator::WaitAll not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::WaitSome(const int count, Request requests[], int& NCompleted, int* completed)
{
    throw std::runtime_error("MonaCommunicator::WaitSome not implemented");
}

//-----------------------------------------------------------------------------
int MonaCommunicator::TestAll(const int count, MonaCommunicator::Request requests[], int& flag)
{
    throw std::runtime_error("MonaCommunicator::TestAll not implemented");
}

//-----------------------------------------------------------------------------
int MonaCommunicator::TestAny(
  const int count, MonaCommunicator::Request requests[], int& idx, int& flag)
{
    throw std::runtime_error("MonaCommunicator::TestAny not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::TestSome(const int count, Request requests[], int& NCompleted, int* completed)
{
    throw std::runtime_error("MonaCommunicator::TestSome not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(int source, int tag, int* flag, int* actualSource)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(
  int source, int tag, int* flag, int* actualSource, int* vtkNotUsed(type), int* size)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(
  int source, int tag, int* flag, int* actualSource, unsigned long* vtkNotUsed(type), int* size)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(
  int source, int tag, int* flag, int* actualSource, const char* vtkNotUsed(type), int* size)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(
  int source, int tag, int* flag, int* actualSource, float* vtkNotUsed(type), int* size)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

//-----------------------------------------------------------------------------
int MonaCommunicator::Iprobe(
  int source, int tag, int* flag, int* actualSource, double* vtkNotUsed(type), int* size)
{
    throw std::runtime_error("MonaCommunicator::Iprobe not implemented");
    return 0;
}

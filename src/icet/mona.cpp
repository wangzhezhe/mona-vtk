#include <IceT.h>
#include <IceTDevDiagnostics.h>
#include <iostream>
#include <mona-coll.h>
#include <mona.h>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define ICET_MONA_BARRIER_TAG 11111
#define ICET_MONA_GATHER_TAG 22222
#define ICET_MONA_GATHERV_TAG 33333
#define ICET_MONA_ALLGATHER_TAG 44444
#define ICET_MONA_ALLTOALL_TAG 55555

#define ICET_MONA_REQUEST_MAGIC_NUMBER ((IceTEnum)0x636f6c7a)

#define ICET_MONA_TEMP_BUFFER_0 (ICET_COMMUNICATION_LAYER_START | (IceTEnum)0x00)

#define ICET_IN_PLACE_COLLECT ((void*)(-1))

static IceTCommunicator MonaDuplicate(IceTCommunicator self);
static IceTCommunicator MonaSubset(IceTCommunicator self, int count, const IceTInt32* ranks);
static void MonaDestroy(IceTCommunicator self);
static void MonaBarrier(IceTCommunicator self);
static void MonaSend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag);
static void MonaRecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag);
static void MonaSendrecv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum sendtype, int dest, int sendtag, void* recvbuf, int recvcount, IceTEnum recvtype,
  int src, int recvtag);
static void MonaGather(IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype,
  void* recvbuf, int root);
static void MonaGatherv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, const int* recvcounts, const int* recvoffsets, int root);
static void MonaAllgather(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf);
static void MonaAlltoall(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf);
static IceTCommRequest MonaIsend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag);
static IceTCommRequest MonaIrecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag);
static void MonaWaitone(IceTCommunicator self, IceTCommRequest* request);
static int MonaWaitany(IceTCommunicator self, int count, IceTCommRequest* array_of_requests);
static int MonaComm_size(IceTCommunicator self);
static int MonaComm_rank(IceTCommunicator self);

typedef struct IceTMonaCommRequestInternalsStruct
{
  mona_request_t request;
} * IceTMonaCommRequestInternals;

static mona_request_t getMonaRequest(IceTCommRequest icet_request)
{
  if (icet_request == ICET_COMM_REQUEST_NULL)
  {
    return MONA_REQUEST_NULL;
  }
  if (icet_request->magic_number != ICET_MONA_REQUEST_MAGIC_NUMBER)
  {
    std::cout << "---icet debug magic_number: " << icet_request->magic_number << std::endl;
    icetRaiseError(ICET_INVALID_VALUE, "Request object is not from the Mona communicator.");
    return MONA_REQUEST_NULL;
  }

  return (((IceTMonaCommRequestInternals)icet_request->internals)->request);
}

static void setMonaRequest(IceTCommRequest icet_request, const mona_request_t mona_request)
{
  if (icet_request == ICET_COMM_REQUEST_NULL)
  {
    icetRaiseError(ICET_SANITY_CHECK_FAIL, "Cannot set Mona request in null request.");
    return;
  }
  if (icet_request->magic_number != ICET_MONA_REQUEST_MAGIC_NUMBER)
  {
    icetRaiseError(ICET_SANITY_CHECK_FAIL, "Request object is not from the Mona communicator.");
    return;
  }
  (((IceTMonaCommRequestInternals)icet_request->internals)->request) = mona_request;
}

static IceTCommRequest create_request(void)
{
  IceTCommRequest request;
  request = (IceTCommRequest)malloc(sizeof(struct IceTCommRequestStruct));
  if (!request)
  {
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommRequest");
    return nullptr;
  }

  request->magic_number = ICET_MONA_REQUEST_MAGIC_NUMBER;
  request->internals = new IceTMonaCommRequestInternalsStruct();

  if (!request->internals)
  {
    free(request);
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommRequest");
    return nullptr;
  }
  return request;
}

static void destroy_request(IceTCommRequest request)
{
  mona_request_t mona_request = getMonaRequest(request);
  delete (IceTMonaCommRequestInternalsStruct*)(request->internals);
  free(request);
}

IceTCommunicator icetCreateMonaCommunicator(const mona_comm_t mona_comm)
{
  IceTCommunicator comm;

  if (!mona_comm)
  {
    return nullptr;
  }

  comm = static_cast<IceTCommunicatorStruct*>(malloc(sizeof(IceTCommunicatorStruct)));
  if (!comm)
  {
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommunicator.");
    return nullptr;
  }

  comm->Duplicate = MonaDuplicate;
  comm->Subset = MonaSubset;
  comm->Destroy = MonaDestroy;
  comm->Barrier = MonaBarrier;
  comm->Send = MonaSend;
  comm->Recv = MonaRecv;
  comm->Sendrecv = MonaSendrecv;
  comm->Gather = MonaGather;
  comm->Gatherv = MonaGatherv;
  comm->Allgather = MonaAllgather;
  comm->Alltoall = MonaAlltoall;
  comm->Isend = MonaIsend;
  comm->Irecv = MonaIrecv;
  comm->Wait = MonaWaitone;
  comm->Waitany = MonaWaitany;
  comm->Comm_size = MonaComm_size;
  comm->Comm_rank = MonaComm_rank;

  comm->data = mona_comm;
  if (!comm->data)
  {
    free(comm);
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommunicator.");
    return NULL;
  }
  return comm;
}

void icetDestroyMonaCommunicator(IceTCommunicator comm)
{
  if (comm)
  {
    comm->Destroy(comm);
  }
}

#define MONA_COMM ((mona_comm_t)(self->data))

static IceTCommunicator MonaDuplicate(IceTCommunicator self)
{

  if (self != ICET_COMM_NULL)
  {
    auto comm = MONA_COMM;
    decltype(comm) dup;
    mona_comm_dup(comm, &dup);
    return icetCreateMonaCommunicator(dup);
  }
  else
  {
    return ICET_COMM_NULL;
  }
}

static IceTCommunicator MonaSubset(IceTCommunicator self, int count, const IceTInt32* ranks)
{

  IceTCommunicator result;
  auto comm = MONA_COMM;
  decltype(comm) subset_comm;
  mona_comm_subset(comm, ranks, count, &subset_comm);

  return icetCreateMonaCommunicator(subset_comm);
}

static void MonaDestroy(IceTCommunicator self)
{

  auto comm = MONA_COMM;
  mona_comm_free(comm);
  free(self);
}

static void MonaBarrier(IceTCommunicator self)
{

  auto comm = MONA_COMM;
  mona_comm_barrier(comm, ICET_MONA_BARRIER_TAG);
}

#define GET_DATATYPE_SIZE(icet_type, var)                                                          \
  switch (icet_type)                                                                               \
  {                                                                                                \
    case ICET_BOOLEAN:                                                                             \
      var = 1;                                                                                     \
      break;                                                                                       \
    case ICET_BYTE:                                                                                \
      var = 1;                                                                                     \
      break;                                                                                       \
    case ICET_SHORT:                                                                               \
      var = 2;                                                                                     \
      break;                                                                                       \
    case ICET_INT:                                                                                 \
      var = 4;                                                                                     \
      break;                                                                                       \
    case ICET_FLOAT:                                                                               \
      var = 4;                                                                                     \
      break;                                                                                       \
    case ICET_DOUBLE:                                                                              \
      var = 8;                                                                                     \
      break;                                                                                       \
    default:                                                                                       \
      icetRaiseError(                                                                              \
        ICET_INVALID_ENUM, "Mona Communicator received bad data type 0x%X.", icet_type);           \
      var = 1;                                                                                     \
      break;                                                                                       \
  }

static void MonaSend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag)
{

  auto comm = MONA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (buf == nullptr)
  {
    throw std::runtime_error("send should not be null");
    return;
  }
  na_return_t ret = mona_comm_send(comm, (void*)buf, count * typesize, dest, tag);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for MonaSend");
  }
}

static void MonaRecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag)
{

  auto comm = MONA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (buf == nullptr)
  {
    throw std::runtime_error("recv should not be null");
    return;
  }
  na_return_t ret = mona_comm_recv(comm, (void*)buf, count * typesize, src, tag, NULL, NULL, NULL);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for MonaSend");
  }
}

static void MonaSendrecv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum sendtype, int dest, int sendtag, void* recvbuf, int recvcount, IceTEnum recvtype,
  int src, int recvtag)
{

  auto comm = MONA_COMM;

  size_t sendtypesize;
  size_t recvtypesize;
  GET_DATATYPE_SIZE(sendtype, sendtypesize);
  GET_DATATYPE_SIZE(recvtype, recvtypesize);
  if (sendbuf == nullptr || recvbuf == NULL)
  {
    throw std::runtime_error("send/recv should not be null");
    return;
  }

  mona_comm_sendrecv(comm, (void*)sendbuf, sendcount * sendtypesize, dest, sendtag, recvbuf,
    recvcount * recvtypesize, src, recvtag, NULL, NULL, NULL);
}

static void MonaGather(IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype,
  void* recvbuf, int root)
{

  auto comm = MONA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = MONA_IN_PLACE;
  }
  mona_comm_gather(comm, sendbuf, sendcount * typesize, recvbuf, root, ICET_MONA_GATHER_TAG);
}

/*
special icet_t test case
rank 0 call gatherv ICET_IN_PLACE_COLLECT root 0
rank 0 0 recvsize_sizet 3145728 recvoffsets 0
rank 0 1 recvsize_sizet 0 recvoffsets 0
rank 0 2 recvsize_sizet 0 recvoffsets 0
rank 0 3 recvsize_sizet 0 recvoffsets 0
rank 0 sendsize 3145728
*/
static void MonaGatherv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, const int* recvcounts, const int* recvoffsets, int root)
{

  auto comm = MONA_COMM;
  int size, rank;

  mona_comm_size(comm, &size);
  mona_comm_rank(comm, &rank);

  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = MONA_IN_PLACE;
  }

  std::vector<size_t> recvsize_sizet(rank == root ? size : 0);
  std::vector<size_t> recvoffsets_sizet(rank == root ? size : 0);

  if (rank == root)
  {
    for (unsigned i = 0; i < size; i++)
    {
      recvsize_sizet[i] = recvcounts[i]*typesize;
      recvoffsets_sizet[i] = recvoffsets[i];
    }
  }

  mona_comm_gatherv(comm, sendbuf, sendcount * typesize, recvbuf, recvsize_sizet.data(),
    recvoffsets_sizet.data(), root, ICET_MONA_GATHERV_TAG);
}

static void MonaAllgather(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf)
{

  auto comm = MONA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = MONA_IN_PLACE;
  }
  na_return_t ret =
    mona_comm_allgather(comm, sendbuf, sendcount * typesize, recvbuf, ICET_MONA_ALLGATHER_TAG);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for mona_comm_allgather");
    return;
  }
}

static void MonaAlltoall(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf)
{

  auto comm = MONA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  if (sendbuf == nullptr)
  {
    throw std::runtime_error("alltoall should not be null");
    return;
  }
  na_return_t ret =
    mona_comm_alltoall(comm, sendbuf, sendcount * typesize, recvbuf, ICET_MONA_ALLTOALL_TAG);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for mona_comm_alltoall");
    return;
  }
}

static IceTCommRequest MonaIsend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag)
{

  auto comm = MONA_COMM;
  IceTCommRequest icet_request;
  mona_request_t req;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  if (buf == nullptr)
  {
    throw std::runtime_error("isend should not be null");
    return icet_request;
  }
  na_return_t ret = mona_comm_isend(comm, buf, count * typesize, dest, tag, &req);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for mona_comm_isend");
    return icet_request;
  }
  icet_request = create_request();
  setMonaRequest(icet_request, req);

  return icet_request;
}

static IceTCommRequest MonaIrecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag)
{

  auto comm = MONA_COMM;
  IceTCommRequest icet_request;
  mona_request_t req;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  na_return_t ret = mona_comm_irecv(comm, buf, count * typesize, src, tag, NULL, NULL, NULL, &req);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for mona_comm_irecv");
    return icet_request;
  }
  icet_request = create_request();
  if (buf == nullptr)
  {
    throw std::runtime_error("irecv should not be null");
    return icet_request;
  }
  setMonaRequest(icet_request, req);

  return icet_request;
}

static void MonaWaitone(IceTCommunicator self, IceTCommRequest* icet_request)
{

  mona_request_t req;
  auto comm = MONA_COMM;

  if (*icet_request == ICET_COMM_REQUEST_NULL)
    return;

  req = getMonaRequest(*icet_request);

  mona_wait(req);

  destroy_request(*icet_request);
  *icet_request = ICET_COMM_REQUEST_NULL;
}

static int MonaWaitany(IceTCommunicator self, int count, IceTCommRequest* array_of_requests)
{
  std::vector<mona_request_t> reqs(count);
  int idx;
  auto comm = MONA_COMM;

  for (idx = 0; idx < count; idx++)
  {
    reqs[idx] = getMonaRequest(array_of_requests[idx]);
  }

  size_t index;
  na_return_t ret = mona_wait_any(count, reqs.data(), &index);

  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("not success for wait any");
  }

  destroy_request(array_of_requests[index]);
  array_of_requests[index] = ICET_COMM_REQUEST_NULL;

  return index;
}

static int MonaComm_size(IceTCommunicator self)
{

  auto comm = MONA_COMM;
  int size;
  mona_comm_size(comm, &size);

  return size;
}

static int MonaComm_rank(IceTCommunicator self)
{

  auto comm = MONA_COMM;
  int rank;
  mona_comm_rank(comm, &rank);

  return rank;
}


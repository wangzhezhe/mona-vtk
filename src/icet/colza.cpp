#include <IceT.h>
#include <IceTDevDiagnostics.h>
#include <colza/communicator.hpp>
#include <colza/request.hpp>
#include <colza/types.hpp>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>

#define ICET_COLZA_REQUEST_MAGIC_NUMBER ((IceTEnum)0x636f6c7a)

#define ICET_COLZA_TEMP_BUFFER_0 (ICET_COMMUNICATION_LAYER_START | (IceTEnum)0x00)

#define ICET_IN_PLACE_COLLECT ((void*)(-1))

static IceTCommunicator ColzaDuplicate(IceTCommunicator self);
static IceTCommunicator ColzaSubset(IceTCommunicator self, int count, const IceTInt32* ranks);
static void ColzaDestroy(IceTCommunicator self);
static void ColzaBarrier(IceTCommunicator self);
static void ColzaSend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag);
static void ColzaRecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag);
static void ColzaSendrecv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum sendtype, int dest, int sendtag, void* recvbuf, int recvcount, IceTEnum recvtype,
  int src, int recvtag);
static void ColzaGather(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, int root);
static void ColzaGatherv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, const int* recvcounts, const int* recvoffsets, int root);
static void ColzaAllgather(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf);
static void ColzaAlltoall(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf);
static IceTCommRequest ColzaIsend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag);
static IceTCommRequest ColzaIrecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag);
static void ColzaWaitone(IceTCommunicator self, IceTCommRequest* request);
static int ColzaWaitany(IceTCommunicator self, int count, IceTCommRequest* array_of_requests);
static int ColzaComm_size(IceTCommunicator self);
static int ColzaComm_rank(IceTCommunicator self);

typedef struct IceTColzaCommRequestInternalsStruct
{
  colza::request request;
} * IceTColzaCommRequestInternals;

static colza::request getColzaRequest(IceTCommRequest icet_request)
{
  if (icet_request == ICET_COMM_REQUEST_NULL)
  {
    return colza::request();
  }
  if (icet_request->magic_number != ICET_COLZA_REQUEST_MAGIC_NUMBER)
  {
    icetRaiseError(ICET_INVALID_VALUE, "Request object is not from the MPI communicator.");
    return colza::request();
  }

  return (((IceTColzaCommRequestInternals)icet_request->internals)->request);
}

static void setColzaRequest(IceTCommRequest icet_request, const colza::request& colza_request)
{
  if (icet_request == ICET_COMM_REQUEST_NULL)
  {
    icetRaiseError(ICET_SANITY_CHECK_FAIL, "Cannot set Colza request in null request.");
    return;
  }
  if (icet_request->magic_number != ICET_COLZA_REQUEST_MAGIC_NUMBER)
  {
    icetRaiseError(ICET_SANITY_CHECK_FAIL, "Request object is not from the Colza communicator.");
    return;
  }
  (((IceTColzaCommRequestInternals)icet_request->internals)->request) = colza_request;
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

  request->magic_number = ICET_COLZA_REQUEST_MAGIC_NUMBER;
  request->internals = new IceTColzaCommRequestInternalsStruct();

  if (!request->internals)
  {
    free(request);
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommRequest");
    return NULL;
  }
  return request;
}

static void destroy_request(IceTCommRequest request)
{
  colza::request colza_request = getColzaRequest(request);
  delete (IceTColzaCommRequestInternalsStruct*)(request->internals);
  free(request);
}

IceTCommunicator icetCreateColzaCommunicator(const std::shared_ptr<colza::communicator>& colza_comm)
{
  IceTCommunicator comm;

  if (!colza_comm)
  {
    return nullptr;
  }

  comm = static_cast<IceTCommunicatorStruct*>(malloc(sizeof(IceTCommunicatorStruct)));
  if (!comm)
  {
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommunicator.");
    return nullptr;
  }

  comm->Duplicate = ColzaDuplicate;
  comm->Subset = ColzaSubset;
  comm->Destroy = ColzaDestroy;
  comm->Barrier = ColzaBarrier;
  comm->Send = ColzaSend;
  comm->Recv = ColzaRecv;
  comm->Sendrecv = ColzaSendrecv;
  comm->Gather = ColzaGather;
  comm->Gatherv = ColzaGatherv;
  comm->Allgather = ColzaAllgather;
  comm->Alltoall = ColzaAlltoall;
  comm->Isend = ColzaIsend;
  comm->Irecv = ColzaIrecv;
  comm->Wait = ColzaWaitone;
  comm->Waitany = ColzaWaitany;
  comm->Comm_size = ColzaComm_size;
  comm->Comm_rank = ColzaComm_rank;

  comm->data = static_cast<void*>(new std::shared_ptr<colza::communicator>(colza_comm));
  if (!comm->data)
  {
    free(comm);
    icetRaiseError(ICET_OUT_OF_MEMORY, "Could not allocate memory for IceTCommunicator.");
    return NULL;
  }
  return comm;
}

void icetDestroyColzaCommunicator(IceTCommunicator comm)
{
  if (!comm)
  {
    comm->Destroy(comm);
  }
}

#define COLZA_COMM (*((std::shared_ptr<colza::communicator>*)self->data))

static IceTCommunicator ColzaDuplicate(IceTCommunicator self)
{

  if (self != ICET_COMM_NULL)
  {
    auto comm = COLZA_COMM;
    decltype(comm) dup;
    comm->duplicate(&dup);
    return icetCreateColzaCommunicator(dup);
  }
  else
  {
    return ICET_COMM_NULL;
  }
}

static IceTCommunicator ColzaSubset(IceTCommunicator self, int count, const IceTInt32* ranks)
{
  IceTCommunicator result;
  auto comm = COLZA_COMM;
  decltype(comm) subset_comm;
  comm->subset(&subset_comm, count, ranks);
  return icetCreateColzaCommunicator(subset_comm);
}

static void ColzaDestroy(IceTCommunicator self)
{
  auto& comm = COLZA_COMM;
  // TODO uncomment when this function is implemented
  // comm->destroy();
  comm.reset();
  delete static_cast<std::shared_ptr<colza::communicator>*>(self->data);
  free(self);
}

static void ColzaBarrier(IceTCommunicator self)
{
  auto& comm = COLZA_COMM;
  comm->barrier();
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
        ICET_INVALID_ENUM, "Colza Communicator received bad data type 0x%X.", icet_type);          \
      var = 1;                                                                                     \
      break;                                                                                       \
  }

static void ColzaSend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (buf == nullptr)
  {
    throw std::runtime_error("send should not be null");
    return;
  }
  comm->send((void*)buf, count * typesize, dest, tag);
}

static void ColzaRecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (buf == nullptr)
  {
    throw std::runtime_error("recv should not be null");
    return;
  }
  comm->recv((void*)buf, count * typesize, src, tag);
}

static void ColzaSendrecv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum sendtype, int dest, int sendtag, void* recvbuf, int recvcount, IceTEnum recvtype,
  int src, int recvtag)
{
  auto& comm = COLZA_COMM;

  size_t sendtypesize;
  size_t recvtypesize;
  GET_DATATYPE_SIZE(sendtype, sendtypesize);
  GET_DATATYPE_SIZE(recvtype, recvtypesize);
  if (sendbuf == nullptr || recvbuf == NULL)
  {
    throw std::runtime_error("send/recv should not be null");
    return;
  }
  comm->sendrecv((void*)sendbuf, sendcount * sendtypesize, dest, sendtag, recvbuf,
    recvcount * recvtypesize, src, recvtag);
}

static void ColzaGather(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, int root)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = COLZA_IN_PLACE;
  }

  comm->gather(sendbuf, sendcount * typesize, recvbuf, root);
}

static void ColzaGatherv(IceTCommunicator self, const void* sendbuf, int sendcount,
  IceTEnum datatype, void* recvbuf, const int* recvcounts, const int* recvoffsets, int root)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = COLZA_IN_PLACE;
  }

  std::vector<size_t> recvcounts_sizet(comm->rank() == root ? comm->size() : 0);
  std::vector<size_t> recvoffsets_sizet(comm->rank() == root ? comm->size() : 0);

  if (comm->rank() == root)
  {
    for (unsigned i = 0; i < comm->size(); i++)
    {
      recvcounts_sizet[i] = recvcounts[i];
      recvoffsets_sizet[i] = recvoffsets[i];
    }
  }
  bool ifinplace = (sendbuf == COLZA_IN_PLACE);

  comm->gatherv(
    sendbuf, recvbuf, sendcount, recvcounts_sizet.data(), recvoffsets_sizet.data(), typesize, root);
}

static void ColzaAllgather(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);

  if (sendbuf == ICET_IN_PLACE_COLLECT)
  {
    sendbuf = COLZA_IN_PLACE;
  }

  comm->allgather(sendbuf, recvbuf, sendcount * typesize);
}

static void ColzaAlltoall(
  IceTCommunicator self, const void* sendbuf, int sendcount, IceTEnum datatype, void* recvbuf)
{
  auto& comm = COLZA_COMM;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  if (sendbuf == nullptr)
  {
    throw std::runtime_error("alltoall should not be null");
    return;
  }
  comm->alltoall((void*)sendbuf, sendcount * typesize, recvbuf, sendcount * typesize);
}

static IceTCommRequest ColzaIsend(
  IceTCommunicator self, const void* buf, int count, IceTEnum datatype, int dest, int tag)
{
  auto& comm = COLZA_COMM;
  IceTCommRequest icet_request;
  colza::request req;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  if (buf == nullptr)
  {
    throw std::runtime_error("isend should not be null");
    return icet_request;
  }
  comm->isend(buf, count * typesize, dest, tag, req);
  icet_request = create_request();
  setColzaRequest(icet_request, req);

  return icet_request;
}

static IceTCommRequest ColzaIrecv(
  IceTCommunicator self, void* buf, int count, IceTEnum datatype, int src, int tag)
{
  auto& comm = COLZA_COMM;
  IceTCommRequest icet_request;
  colza::request req;
  size_t typesize;
  GET_DATATYPE_SIZE(datatype, typesize);
  comm->irecv(buf, count * typesize, src, tag, req);
  icet_request = create_request();
  if (buf == nullptr)
  {
    throw std::runtime_error("irecv should not be null");
    return icet_request;
  }
  setColzaRequest(icet_request, req);

  return icet_request;
}

static void ColzaWaitone(IceTCommunicator self, IceTCommRequest* icet_request)
{
  colza::request req;
  auto& comm = COLZA_COMM;

  if (*icet_request == ICET_COMM_REQUEST_NULL)
    return;

  req = getColzaRequest(*icet_request);
  comm->wait(req);

  destroy_request(*icet_request);
  *icet_request = ICET_COMM_REQUEST_NULL;

}

static int ColzaWaitany(IceTCommunicator self, int count, IceTCommRequest* array_of_requests)
{
  std::vector<colza::request> reqs(count);
  int idx;
  auto& comm = COLZA_COMM;

  for (idx = 0; idx < count; idx++)
  {
    reqs[idx] = getColzaRequest(array_of_requests[idx]);
  }

  idx = comm->waitAny(count, reqs.data());

  destroy_request(array_of_requests[idx]);
  array_of_requests[idx] = ICET_COMM_REQUEST_NULL;

  return idx;
}

static int ColzaComm_size(IceTCommunicator self)
{
  auto comm = COLZA_COMM;
  return comm->size();
}

static int ColzaComm_rank(IceTCommunicator self)
{
  auto comm = COLZA_COMM;
  return comm->rank();
}

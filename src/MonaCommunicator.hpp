#ifndef MonaCommunicator_h
#define MonaCommunicator_h

#include <vtkParallelMPIModule.h> // For export macro
#include <vtkCommunicator.h>
#include "Mona.hpp"

class MonaController;
class vtkProcessGroup;

class MonaCommunicatorOpaqueComm;
class MonaCommunicatorOpaqueRequest;
class MonaCommunicatorReceiveDataInfo;

class VTKPARALLELMPI_EXPORT MonaCommunicator : public vtkCommunicator
{
public:

  class VTKPARALLELMPI_EXPORT Request
  {
  public:
    Request();
    Request( const Request& );
    ~Request();
    Request& operator = ( const Request& );
    int Test();
    void Cancel();
    void Wait();
    MonaCommunicatorOpaqueRequest* Req;
  };

  vtkTypeMacro( MonaCommunicator,vtkCommunicator);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Creates an empty communicator.
   */
  static MonaCommunicator* New();

  /**
   * Returns the singleton which behaves as the global
   * communicator
   */
  static MonaCommunicator* GetWorldCommunicator();

  static MonaCommunicator* GetWorldCommunicatorByMona(mona_comm_t mona_comm);

  /**
   * Used to initialize the communicator (i.e. create the underlying MPI_Comm).
   * The group must be associated with a valid MonaCommunicator.
   */
  int Initialize(vtkProcessGroup *group);

  /**
   * Used to initialize the communicator (i.e. create the underlying MPI_Comm)
   * using MPI_Comm_split on the given communicator. Return values are 1 for success
   * and 0 otherwise.
   */
  int SplitInitialize(vtkCommunicator *oldcomm, int color, int key);

  //@{
  /**
   * Performs the actual communication.  You will usually use the convenience
   * Send functions defined in the superclass. Return values are 1 for success
   * and 0 otherwise.
   */
  virtual int SendVoidArray(const void *data, vtkIdType length, int type,
                            int remoteProcessId, int tag) override;
  virtual int ReceiveVoidArray(void *data, vtkIdType length, int type,
                               int remoteProcessId, int tag) override;
  //@}

  //@{
  /**
   * This method sends data to another process (non-blocking).
   * Tag eliminates ambiguity when multiple sends or receives
   * exist in the same process. The last argument,
   * MonaCommunicator::Request& req can later be used (with
   * req.Test() ) to test the success of the message. Return values are 1
   * for success and 0 otherwise.
   */
  int NoBlockSend(const int* data, int length, int remoteProcessId, int tag,
                  Request& req);
  int NoBlockSend(const unsigned long* data, int length, int remoteProcessId,
                  int tag, Request& req);
  int NoBlockSend(const char* data, int length, int remoteProcessId,
                  int tag, Request& req);
  int NoBlockSend(const unsigned char* data, int length, int remoteProcessId,
                    int tag, Request& req);
  int NoBlockSend(const float* data, int length, int remoteProcessId,
                  int tag, Request& req);
  int NoBlockSend(const double* data, int length, int remoteProcessId,
                  int tag, Request& req);
#ifdef VTK_USE_64BIT_IDS
  int NoBlockSend(const vtkIdType* data, int length, int remoteProcessId,
                  int tag, Request& req);
#endif
  //@}

  //@{
  /**
   * This method receives data from a corresponding send (non-blocking).
   * The last argument,
   * MonaCommunicator::Request& req can later be used (with
   * req.Test() ) to test the success of the message. Return values
   * are 1 for success and 0 otherwise.
   */
  int NoBlockReceive(int* data, int length, int remoteProcessId,
                     int tag, Request& req);
  int NoBlockReceive(unsigned long* data, int length,
                     int remoteProcessId, int tag, Request& req);
  int NoBlockReceive(char* data, int length, int remoteProcessId,
                     int tag, Request& req);
  int NoBlockReceive(unsigned char* data, int length, int remoteProcessId,
                     int tag, Request& req);
  int NoBlockReceive(float* data, int length, int remoteProcessId,
                     int tag, Request& req);
  int NoBlockReceive(double* data, int length, int remoteProcessId,
                     int tag, Request& req);
#ifdef VTK_USE_64BIT_IDS
  int NoBlockReceive(vtkIdType* data, int length, int remoteProcessId,
                     int tag, Request& req);
#endif
  //@}


  //@{
  /**
   * More efficient implementations of collective operations that use
   * the equivalent MPI commands. Return values are 1 for success
   * and 0 otherwise.
   */
  virtual void Barrier() override;
  virtual int BroadcastVoidArray(void *data, vtkIdType length, int type,
                                 int srcProcessId) override;
  virtual int GatherVoidArray(const void *sendBuffer, void *recvBuffer,
                              vtkIdType length, int type, int destProcessId) override;
  virtual int GatherVVoidArray(const void *sendBuffer, void *recvBuffer,
                               vtkIdType sendLength, vtkIdType *recvLengths,
                               vtkIdType *offsets, int type, int destProcessId) override;
  virtual int ScatterVoidArray(const void *sendBuffer, void *recvBuffer,
                               vtkIdType length, int type, int srcProcessId) override;
  virtual int ScatterVVoidArray(const void *sendBuffer, void *recvBuffer,
                                vtkIdType *sendLengths, vtkIdType *offsets,
                                vtkIdType recvLength, int type,
                                int srcProcessId) override;
  virtual int AllGatherVoidArray(const void *sendBuffer, void *recvBuffer,
                                 vtkIdType length, int type) override;
  virtual int AllGatherVVoidArray(const void *sendBuffer, void *recvBuffer,
                                  vtkIdType sendLength, vtkIdType *recvLengths,
                                  vtkIdType *offsets, int type) override;
  virtual int ReduceVoidArray(const void *sendBuffer, void *recvBuffer,
                              vtkIdType length, int type,
                              int operation, int destProcessId) override;
  virtual int ReduceVoidArray(const void *sendBuffer, void *recvBuffer,
                              vtkIdType length, int type,
                              Operation *operation, int destProcessId) override;
  virtual int AllReduceVoidArray(const void *sendBuffer, void *recvBuffer,
                                 vtkIdType length, int type,
                                 int operation) override;
  virtual int AllReduceVoidArray(const void *sendBuffer, void *recvBuffer,
                                 vtkIdType length, int type,
                                 Operation *operation) override;
  //@}

  //@{
  /**
   * Nonblocking test for a message.  Inputs are: source -- the source rank
   * or ANY_SOURCE; tag -- the tag value.  Outputs are:
   * flag -- True if a message matches; actualSource -- the rank
   * sending the message (useful if ANY_SOURCE is used) if flag is True
   * and actualSource isn't nullptr; size -- the length of the message in
   * bytes if flag is true (only set if size isn't nullptr). The return
   * value is 1 for success and 0 otherwise.
   */
  int Iprobe(int source, int tag, int* flag, int* actualSource);
  int Iprobe(int source, int tag, int* flag, int* actualSource,
             int* type, int* size);
  int Iprobe(int source, int tag, int* flag, int* actualSource,
             unsigned long* type, int* size);
  int Iprobe(int source, int tag, int* flag, int* actualSource,
             const char* type, int* size);
  int Iprobe(int source, int tag, int* flag, int* actualSource,
             float* type, int* size);
  int Iprobe(int source, int tag, int* flag, int* actualSource,
             double* type, int* size);
  //@}

  /**
   * Given the request objects of a set of non-blocking operations
   * (send and/or receive) this method blocks until all requests are complete.
   */
  int WaitAll(const int count, Request requests[]);

  /**
   * Blocks until *one* of the specified requests in the given request array
   * completes. Upon return, the index in the array of the completed request
   * object is returned through the argument list.
   */
  int WaitAny(const int count, Request requests[], int& idx) VTK_SIZEHINT(requests, count);

  /**
   * Blocks until *one or more* of the specified requests in the given request
   * request array completes. Upon return, the list of handles that have
   * completed is stored in the completed vtkIntArray.
   */
  int WaitSome(
      const int count, Request requests[], int &NCompleted, int *completed ) VTK_SIZEHINT(requests, count);

  /**
   * Checks if the given communication request objects are complete. Upon
   * return, flag evaluates to true iff *all* of the communication request
   * objects are complete.
   */
  int TestAll( const int count, Request requests[], int& flag ) VTK_SIZEHINT(requests, count);

  /**
   * Check if at least *one* of the specified requests has completed.
   */
  int TestAny(const int count, Request requests[], int &idx, int &flag ) VTK_SIZEHINT(requests, count);

  /**
   * Checks the status of *all* the given request communication object handles.
   * Upon return, NCompleted holds the count of requests that have completed
   * and the indices of the completed requests, w.r.t. the requests array is
   * given the by the pre-allocated completed array.
   */
  int TestSome(const int count,Request requests[],
               int& NCompleted,int *completed) VTK_SIZEHINT(requests, count);

  friend class MonaController;

  MonaCommunicatorOpaqueComm *GetMonaComm()
  {
    return this->MonaComm;
  }

  int InitializeExternal(MonaCommunicatorOpaqueComm *comm);

  static char* Allocate(size_t size);
  static void Free(char* ptr);


  //@{
  /**
   * When set to 1, all MPI_Send calls are replaced by MPI_Ssend calls.
   * Default is 0.
   */
  vtkSetClampMacro(UseSsend, int, 0, 1);
  vtkGetMacro(UseSsend, int);
  vtkBooleanMacro(UseSsend, int);
  //@}

  /**
   * Copies all the attributes of source, deleting previously
   * stored data. The MPI communicator handle is also copied.
   * Normally, this should not be needed. It is used during
   * the construction of a new communicator for copying the
   * world communicator, keeping the same context.
   */
  void CopyFrom(MonaCommunicator* source);
  
protected:
  MonaCommunicator();
  ~MonaCommunicator();

  // Obtain size and rank setting NumberOfProcesses and LocalProcessId Should
  // not be called if the current communicator does not include this process
  int InitializeNumberOfProcesses();

  //@{
  /**
   * KeepHandle is normally off. This means that the MPI
   * communicator handle will be freed at the destruction
   * of the object. However, if the handle was copied from
   * another object (via CopyFrom() not Duplicate()), this
   * has to be turned on otherwise the handle will be freed
   * multiple times causing MPI failure. The alternative to
   * this is using reference counting but it is unnecessarily
   * complicated for this case.
   */
  vtkSetMacro(KeepHandle, int);
  vtkBooleanMacro(KeepHandle, int);
  //@}


  static MonaCommunicator* WorldCommunicator;

  void InitializeCopy(MonaCommunicator* source);

  /**
   * Copies all the attributes of source, deleting previously
   * stored data EXCEPT the MPI communicator handle which is
   * duplicated with MPI_Comm_dup(). Therefore, although the
   * processes in the communicator remain the same, a new context
   * is created. This prevents the two communicators from
   * intefering with each other during message send/receives even
   * if the tags are the same.
   */
  void Duplicate(MonaCommunicator* newcomm);

  /**
   * Implementation for receive data.
   */
  //virtual int ReceiveDataInternal(
  //  char *data, int length, int sizeoftype, int remoteProcessId, int tag,
  //  MonaCommunicatorReceiveDataInfo *info, int useCopy, int &senderId);
  // TODO, use the MonaCommunicatorReceiveDataInfo to wrap the mona comm 
  // when there is complete support about the mona type and status
  virtual int ReceiveDataInternal(
    char *data, int length, int sizeoftype, int remoteProcessId, int tag,
    mona_comm_t monacomm, int useCopy, int &senderId);


  MonaCommunicatorOpaqueComm* MonaComm;

  int Initialized;
  int KeepHandle;

  int LastSenderId;
  int UseSsend;
  static int CheckForMPIError(int err);

private:
  MonaCommunicator(const MonaCommunicator&) = delete;
  void operator=(const MonaCommunicator&) = delete;
};

#endif

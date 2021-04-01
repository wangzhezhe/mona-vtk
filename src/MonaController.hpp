#ifndef MonaController_h
#define MonaController_h

#include <vtkParallelMPIModule.h> // For export macro
#include <vtkMultiProcessController.h>
// Do not remove this header file. This class contains methods
// which take arguments defined in  MonaCommunicator.h by
// reference.
#include "MonaCommunicator.hpp" // Needed for direct access to communicator

class vtkIntArray;

class /*VTKPARALLELMPI_EXPORT*/ MonaController : public vtkMultiProcessController
{

public:
  static MonaController *New();
  vtkTypeMacro(MonaController, vtkMultiProcessController);
  void PrintSelf(ostream &os, vtkIndent indent) override;

  /**
   * This method is for setting up the processes.
   * It needs to be called only once during program execution.
   * Calling it more than once will have no effect. Controllers
   * created after this call will be initialized automatically
   * (i.e. they will have the proper LocalProcessId and NumberOfProcesses).
   * The addresses of argc and argv should be passed to this method
   * otherwise command line arguments will not be correct (because
   * usually MPI implementations add their own arguments during
   * startup).
   */
  virtual void Initialize(int *argc, char ***argv) override
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    this->Initialize(argc, argv, 0);
  }

  virtual void Initialize(int *vtkNotUsed(argc), char ***vtkNotUsed(argv),
                          int vtkNotUsed(initializedExternally)) override;

  void Initialize(mona_comm_t mona_comm);
  /**
   * Same as Initialize(0, 0, 1). Mainly for calling from wrapped languages.
   */
  virtual void Initialize() {
      Initialize(0, 0, 1);
  }

  /**
   * This method is for cleaning up and has to be called before
   * the end of the program if MPI was initialized with
   * Initialize()
   */
  virtual void Finalize() override;

  virtual void Finalize(int) override { Finalize(); }

  /**
   * Execute the SingleMethod (as define by SetSingleMethod) using
   * this->NumberOfProcesses processes.
   */
  virtual void SingleMethodExecute() override;

  /**
   * Execute the MultipleMethods (as define by calling SetMultipleMethod
   * for each of the required this->NumberOfProcesses methods) using
   * this->NumberOfProcesses processes.
   */
  virtual void MultipleMethodExecute() override;

  /**
   * This method can be used to tell the controller to create
   * a special output window in which all messages are preceded
   * by the process id.
   */
  virtual void CreateOutputWindow() override;

  /**
   * Given an MPI error code, return a string which contains
   * an error message. This string has to be freed by the user.
   */
  static char *ErrorString(int err);

  /**
   * MPIController uses this communicator in all sends and
   * receives. By default, MPI_COMM_WORLD is used.
   * THIS SHOULD ONLY BE CALLED ON THE PROCESSES INCLUDED
   * IN THE COMMUNICATOR. FOR EXAMPLE, IF THE COMMUNICATOR
   * CONTAINS PROCESSES 0 AND 1, INVOKING THIS METHOD ON
   * ANY OTHER PROCESS WILL CAUSE AN MPI ERROR AND POSSIBLY
   * LEAD TO A CRASH.
   */
  void SetCommunicator(MonaCommunicator *comm);

  virtual MonaController *CreateSubController(vtkProcessGroup *group) override;

  virtual MonaController *PartitionController(int localColor, int localKey) override;

  /**
   * This method sends data to another process (non-blocking).
   * Tag eliminates ambiguity when multiple sends or receives
   * exist in the same process. The last argument,
   * MonaCommunicator::Request& req can later be used (with
   * req.Test() ) to test the success of the message. Return values
   * are 1 for success and 0 otherwise.
   * Note: These methods delegate to the communicator
   */
  int NoBlockSend(const int *data, int length, int remoteProcessId, int tag,
                  MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
  int NoBlockSend(const unsigned long *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
  int NoBlockSend(const char *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
  int NoBlockSend(const unsigned char *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
  int NoBlockSend(const float *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
  int NoBlockSend(const double *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
#ifdef VTK_USE_64BIT_IDS
  int NoBlockSend(const vtkIdType *data, int length, int remoteProcessId,
                  int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockSend(data, length, remoteProcessId, tag, req);
  }
#endif

  /**
   * This method receives data from a corresponding send (non-blocking).
   * The last argument,
   * MonaCommunicator::Request& req can later be used (with
   * req.Test() ) to test the success of the message. Return values are
   * 1 for success and 0 otherwise.
   * Note: These methods delegate to the communicator
   */
  int NoBlockReceive(int *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
  int NoBlockReceive(unsigned long *data, int length,
                     int remoteProcessId, int tag,
                     MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
  int NoBlockReceive(char *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
  int NoBlockReceive(unsigned char *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
  int NoBlockReceive(float *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
  int NoBlockReceive(double *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
#ifdef VTK_USE_64BIT_IDS
  int NoBlockReceive(vtkIdType *data, int length, int remoteProcessId,
                     int tag, MonaCommunicator::Request &req)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->NoBlockReceive(data, length, remoteProcessId, tag, req);
  }
#endif

  /**
   * Nonblocking test for a message.  Inputs are: source -- the source rank
   * or ANY_SOURCE; tag -- the tag value.  Outputs are:
   * flag -- True if a message matches; actualSource -- the rank
   * sending the message (useful if ANY_SOURCE is used) if flag is True
   * and actualSource isn't nullptr; size -- the length of the message in
   * bytes if flag is true (only set if size isn't nullptr). The return
   * value is 1 for success and 0 otherwise.
   * Note: These methods delegate to the communicator
   */
  int Iprobe(int source, int tag, int *flag, int *actualSource)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource);
  }
  int Iprobe(int source, int tag, int *flag, int *actualSource,
             int *type, int *size)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource, type, size);
  }
  int Iprobe(int source, int tag, int *flag, int *actualSource,
             unsigned long *type, int *size)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource, type, size);
  }
  int Iprobe(int source, int tag, int *flag, int *actualSource,
             const char *type, int *size)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource, type, size);
  }
  int Iprobe(int source, int tag, int *flag, int *actualSource,
             float *type, int *size)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource, type, size);
  }
  int Iprobe(int source, int tag, int *flag, int *actualSource,
             double *type, int *size)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->Iprobe(source, tag, flag, actualSource, type, size);
  }

  /**
   * Given the request objects of a set of non-blocking operations
   * (send and/or receive) this method blocks until all requests are complete.
   * Note: This method delegates to the communicator
   */
  int WaitAll(const int count, MonaCommunicator::Request requests[])
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->WaitAll(count, requests);
  }

  /**
   * Blocks until *one* of the specified requests in the given request array
   * completes. Upon return, the index in the array of the completed request
   * object is returned through the argument list.
   * Note: this method delegates to the communicator
   */
  int WaitAny(const int count, MonaCommunicator::Request requests[], int &idx) VTK_SIZEHINT(requests, count)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return ((MonaCommunicator *)this->Communicator)->WaitAny(count, requests, idx);
  }

  /**
   * Blocks until *one or more* of the specified requests in the given request
   * request array completes. Upon return, the list of handles that have
   * completed is stored in the completed vtkIntArray.
   */
  int WaitSome(
      const int count, MonaCommunicator::Request requests[],
      vtkIntArray *completed) VTK_SIZEHINT(requests, count);

  /**
   * Returns true iff *all* of the communication request objects are complete.
   */
  bool TestAll(const int count, MonaCommunicator::Request requests[]);

  /**
   * Returns true iff at least *one* of the communication request objects is
   * complete. The index of the completed request, w.r.t. the requests array, is
   * reflected in the out parameter idx. Otherwise, if none of the communication
   * requests are complete false is returned.
   */
  bool TestAny(const int count, MonaCommunicator::Request requests[], int &idx) VTK_SIZEHINT(requests, count);

  /**
   * Return true iff *one or more* of the communicator request objects is
   * complete. The indices of the completed requests, w.r.t. the requests array,
   * are given in the completed user-supplied vtkIntArray.
   */
  bool TestSome(const int count, MonaCommunicator::Request requests[],
                vtkIntArray *completed) VTK_SIZEHINT(requests, count);

  static const char *GetProcessorName();

  /**
   * When set to 1, TriggerRMI uses Ssend() instead of Send() calls.
   * Off (0) by default.
   */
  static void SetUseSsendForRMI(int use_send)
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    MonaController::UseSsendForRMI = (use_send != 0) ? 1 : 0;
  }
  static int GetUseSsendForRMI()
  {
    std::cout << "monaContorller call function: " << __FUNCTION__ << std::endl;
    return MonaController::UseSsendForRMI;
  }

protected:
  MonaController();
  ~MonaController();

  // Set the communicator to comm and call InitializeNumberOfProcesses()
  void InitializeCommunicator(MonaCommunicator *comm);

  // Duplicate the current communicator, creating RMICommunicator
  void InitializeRMICommunicator();

  /**
   * Implementation for TriggerRMI() provides subclasses an opportunity to
   * modify the behaviour eg. MPIController provides ability to use Ssend
   * instead of Send.
   */
  virtual void TriggerRMIInternal(int remoteProcessId,
                                  void *arg, int argLength, int rmiTag, bool propagate) override;

  // MPI communicator created when Initialize() called.
  // This is a copy of MPI_COMM_WORLD but uses a new
  // context, i.e. even if the tags are the same, the
  // RMI messages will not interfere with user level
  // messages.
  static MonaCommunicator *WorldRMICommunicator;

  friend class MonaOutputWindow;

  // Initialize only once.
  static int Initialized;

  static char ProcessorName[];

  /**
   * When set, TriggerRMI uses Ssend instead of Send.
   */
  static int UseSsendForRMI;

private:
  MonaController(const MonaController &) = delete;
  void operator=(const MonaController &) = delete;
};

#endif

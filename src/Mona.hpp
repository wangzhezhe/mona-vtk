#ifndef Mona_h
#define Mona_h
#ifndef __VTK_WRAP__

#ifndef USE_STDARG
 #define USE_STDARG
#include "vtkParallelMPIModule.h" // For export macro
 #include "mpi.h"
 #undef USE_STDARG
#else
 #include "mpi.h"
#endif

#include <mona.h>
#include <mona-coll.h>

#include "vtkSystemIncludes.h"



class /* VTKPARALLELMPI_EXPORT */ MPICommunicatorOpaqueComm
{
public:
  MPICommunicatorOpaqueComm(MPI_Comm* handle = 0);

  MPI_Comm* GetHandle();

  friend class MonaCommunicator;
  friend class MonaController;

protected:
  MPI_Comm* Handle;
};


class /* VTKPARALLELMPI_EXPORT */ MonaCommunicatorOpaqueComm
{
public:
  MonaCommunicatorOpaqueComm(mona_comm_t handle = 0);

  mona_comm_t GetHandle();

  friend class MonaCommunicator;
  friend class MonaController;

protected:
  mona_comm_t Handle;
};

class /*VTKPARALLELMPI_EXPORT */ MPICommunicatorReceiveDataInfo
{
public:
  MPICommunicatorReceiveDataInfo()
  {
    this->Handle=0;
  }
  MPI_Datatype DataType;
  MPI_Status Status;
  MPI_Comm* Handle;
};

class /* VTKPARALLELMPI_EXPORT */ MonaOpaqueFileHandle
{
public:
  MonaOpaqueFileHandle() : Handle(MPI_FILE_NULL) { }
  MPI_File Handle;
};


//-----------------------------------------------------------------------------
class MonaCommunicatorOpaqueRequest
{
public:
  MPI_Request Handle;
};


#endif
#endif // Mona_h

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

//-----------------------------------------------------------------------------
class MonaCommunicatorOpaqueRequest
{
public:
  mona_request_t Handle;
};


#endif
#endif // Mona_h

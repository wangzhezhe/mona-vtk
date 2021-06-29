#ifndef __MPI_IN_SITU_ADAPTOR_HEADER
#define __MPI_IN_SITU_ADAPTOR_HEADER

#include <mpi.h>
#include <string>
#include <vector>
#include <queue>
#include <memory>

class Mandelbulb;

namespace InSitu
{

void MPIInitialize(const std::string& script, MPI_Comm comm);

void Finalize();

void MPICoProcessDynamic(const std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep);

}// namespace InSitu

#endif
#ifndef __IN_SITU_ADAPTOR_HEADER
#define __IN_SITU_ADAPTOR_HEADER

#include <mpi.h>
#include <string>
#include <vector>
#include <queue>
#include <memory>

#include <icet/mona.hpp>

class Mandelbulb;

namespace InSitu
{

void MPIInitialize(const std::string& script);

void MonaInitialize(const std::string& script);

void MonaInitialize(const std::string& script, mona_comm_t mona_comm);

void Finalize();

void MPICoProcess(Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep);

void MPICoProcessDynamic(MPI_Comm subcomm, std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep);

void MonaCoProcess(
  Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep);

void MonaCoProcessDynamic(mona_comm_t mona_comm, std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep);

}// namespace InSitu

#endif

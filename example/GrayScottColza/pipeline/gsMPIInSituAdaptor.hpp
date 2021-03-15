#ifndef __GS_IN_SITU_MPI_ADAPTOR_HEADER
#define __GS_IN_SITU_MPI_ADAPTOR_HEADER

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "gsMPIBackend.hpp"

namespace InSitu
{

void MPIInitialize(const std::string& script, MPI_Comm mpi_comm);

void Finalize();

// this only works when there is one block for one process
// void MonaCoProcess(DataBlock& db, int nprocs, int rank, double time, unsigned int timeStep);

// void MPIUpdateController(MPI_Comm mpi_comm);

void MPICoProcessList(
  std::vector<std::shared_ptr<DataBlock> > dataBlockList, double time, unsigned int timeStep);

//for the dynmaic version, we need to update the communicator every time

} // namespace InSitu

#endif
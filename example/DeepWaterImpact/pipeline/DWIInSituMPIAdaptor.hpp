#ifndef __DWI_IN_SITU_MPI_ADAPTOR_HEADER
#define __DWI_IN_SITU_MPI_ADAPTOR_HEADER

#include "DataBlock.hpp"
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include "mpi.h"

//first index is the blockid
//second index is the name of particular data array
using DataBlockMap = std::map<uint64_t, std::map<std::string, std::shared_ptr<DataBlock> > >;

namespace InSitu
{

void MPIInitialize(const std::string& script, MPI_Comm mpi_comm);

void Finalize();

// the datablock map contains all blocks on one process for one iteration
// this coprocess operation should be put into a lock
void MPICoProcess(DataBlockMap& dataBlocks, double time, unsigned int timeStep);

} // namespace InSitu

#endif
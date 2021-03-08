#ifndef __GS_IN_SITU_ADAPTOR_HEADER
#define __GS_IN_SITU_ADAPTOR_HEADER

#include <icet/mona.hpp>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "gsMonaBackend.hpp"

namespace InSitu
{

void MonaInitialize(const std::string& script, mona_comm_t mona_comm);

void Finalize();

// this only works when there is one block for one process
// void MonaCoProcess(DataBlock& db, int nprocs, int rank, double time, unsigned int timeStep);

void MonaUpdateController(mona_comm_t mona_comm);

void outPutVTIFile(DataBlock& dataBlock, std::string fileName);

void outPutFigure(std::shared_ptr<DataBlock> dataBlock, std::string fileName);

void MonaCoProcessList(
  std::vector<std::shared_ptr<DataBlock> > dataBlockList, double time, unsigned int timeStep);

//for the dynmaic version, we need to update the communicator every time

} // namespace InSitu

#endif
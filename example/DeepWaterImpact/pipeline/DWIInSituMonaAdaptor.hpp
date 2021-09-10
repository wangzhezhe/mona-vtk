#ifndef __DWI_IN_SITU_MONA_ADAPTOR_HEADER
#define __DWI_IN_SITU_MONA_ADAPTOR_HEADER

#include "DataBlock.hpp"
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <icet/mona.hpp>

//first index is the blockid
//second index is the name of particular data array
using DataBlockMap = std::map<uint64_t, std::map<std::string, std::shared_ptr<DataBlock> > >;

namespace InSitu
{
void MonaInitialize(const std::string& script, mona_comm_t mona_comm);

void Finalize();

void MonaUpdateController(mona_comm_t mona_comm);

void MonaCoProcessDynamic(DataBlockMap& dataBlocks, double time, unsigned int timeStep);

} // namespace InSitu

#endif
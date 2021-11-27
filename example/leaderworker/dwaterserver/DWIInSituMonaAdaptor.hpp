#ifndef __DWI_IN_SITU_MONA_ADAPTOR_HEADER
#define __DWI_IN_SITU_MONA_ADAPTOR_HEADER

#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <icet/mona.hpp>


#include "../pipeline/MonaBackend.hpp"

//first index is the blockid for the same iteration 
//the value is the dataBlock that contains data array
using DataBlockMap = std::map<uint64_t, DataBlock>;

namespace InSituDW
{
    
void DWMonaInitialize(const std::string& script, mona_comm_t mona_comm);

void DWFinalize();

void DWMonaUpdateController(mona_comm_t mona_comm);

void DWMonaCoProcessDynamic(DataBlockMap& dataBlocks, double time, unsigned int timeStep);

} // namespace InSituDW

#endif
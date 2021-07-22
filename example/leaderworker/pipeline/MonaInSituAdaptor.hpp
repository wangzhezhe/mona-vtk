#ifndef __MONA_IN_SITU_ADAPTOR_HEADER
#define __MONA_IN_SITU_ADAPTOR_HEADER

#include <string>
#include <vector>
#include <queue>
#include <memory>

#include <icet/mona.hpp>
#include "MonaBackend.hpp"

class Mandelbulb;

namespace InSitu
{

void MonaInitialize(const std::string& script, mona_comm_t mona_comm);

void Finalize();

void MonaCoProcess(
  Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep);

void MonaUpdateController(mona_comm_t mona_comm);

void MonaCoProcessDynamic(std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep);

void MBOutPutVTIFile(Mandelbulb& dataBlock, std::string fileName);


}// namespace InSitu

#endif

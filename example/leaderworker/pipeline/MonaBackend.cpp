/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "MonaBackend.hpp"
#include "../mb.hpp"
#include "MonaInSituAdaptor.hpp"
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <unistd.h>

#define MONA_BACKEND_BARRIER_TAG 2051
#define MONA_BACKEND_ALLREDUCE_TAG 2052

// update the data, try to add the visulization operations
// when this is called, we can not update the comm

int MonaBackendPipeline::execute(uint64_t iteration, mona_comm_t m_mona_comm)
{
  spdlog::trace("{}: Executing iteration {}", __FUNCTION__, iteration);
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  // still use mona comm lock to make sure we use the latest value

  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::trace("iteration {}: rank={}, size={}", iteration, procRank, procSize);

  if (m_script_name == "")
  {
    throw std::runtime_error("Empty script name");
  }

  // this may takes long time for first step
  // make sure all servers do same things
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG);
  spdlog::trace("{}: After barrier", iteration);

  if (m_first_init)
  {
    spdlog::trace("{}: First init, requires initialization with mona_comm_self", __FUNCTION__);
    InSitu::MonaInitialize(m_script_name, m_mona_comm_self);
    spdlog::trace("{}: Done initializing with mona_comm_self", __FUNCTION__);
  }

  if (m_first_init)
  {
    spdlog::trace("{}: Updating MoNA controller", __FUNCTION__);
    InSitu::MonaUpdateController(m_mona_comm);
    spdlog::trace("{}: Done updating MoNA controller", __FUNCTION__);
  }

  m_first_init = false;

  // redistribute the process
  // get the suitable workload (mandelbulb instance list) based on current data staging services

  // output all key in current map
  // extract data list from current map

  // get the total block number
  // the largest key+1 is the total block number
  // it might be convenient to get the info by API

  spdlog::trace("{}: Updating data for iteration {}", __FUNCTION__, iteration);
  size_t maxID = 0;
  std::vector<Mandelbulb> MandelbulbList;
  int totalBlock = 0;
  int localBlocks = m_datasets[iteration]["mydata"].size();
  // std::cout << "local blocks is " << localBlocks << std::endl;
  mona_comm_allreduce(
    m_mona_comm, &localBlocks, &totalBlock, sizeof(int), 1,
    [](const void* in, void* out, na_size_t, na_size_t, void*) {
      const int* a = static_cast<const int*>(in);
      int* b = static_cast<int*>(out);
      *b += *a;
    },
    nullptr, MONA_BACKEND_ALLREDUCE_TAG);
  spdlog::trace(
    "{}: After AllReduce, localBlocks={}, totalBlocks={}", __FUNCTION__, localBlocks, totalBlock);

  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    // std::cout << "iteration " << iteration << " procRank " << procRank << " key ";
    for (auto& t : m_datasets[iteration]["mydata"])
    {
      size_t blockID = t.first;
      auto depth = t.second.dimensions[0] - 1;
      auto height = t.second.dimensions[1];
      auto width = t.second.dimensions[2];
      // std::cout << blockID << ",";
      size_t blockOffset = blockID * depth;
      // reconstruct the MandelbulbList
      Mandelbulb mb(width, height, depth, blockOffset, 1.2, totalBlock);
      mb.SetData(t.second.data);
      MandelbulbList.push_back(mb);
    }
    // std::cout << std::endl;
  }
  spdlog::trace(
    "{}: About to call InSitu::MonaCoProcessDynamic with iteration={}", __FUNCTION__, iteration);
  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG);
  // InSitu::MonaCoProcessDynamic(MandelbulbList, totalBlock, iteration, iteration);

  spdlog::trace("{}: Done with InSitu::MonaCoProcessDynamic", __FUNCTION__);

  return 0;
}

int MonaBackendPipeline::cleanup(uint64_t iteration)
{
  spdlog::trace("{}: Calling cleanup for iteration {}", __FUNCTION__, iteration);
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    m_datasets.erase(iteration);
  }

  spdlog::trace("{}: Done cleaning up iteration {}", __FUNCTION__, iteration);
  return 0;
}

int MonaBackendPipeline::stage(const std::string& sender_addr, const std::string& dataset_name,
  uint64_t iteration, uint64_t block_id, const std::vector<size_t>& dimensions,
  const std::vector<int64_t>& offsets, const Type& type, const thallium::bulk& data)
{
  spdlog::debug("satge data set {} iteration {} blockid {}", dataset_name, iteration, block_id);
  // double serverStage1 = tl::timer::wtime();
  int result = 0;
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    if (m_datasets.count(iteration) != 0 && m_datasets[iteration].count(dataset_name) != 0 &&
      m_datasets[iteration][dataset_name].count(block_id) != 0)
    {
      result = 1;
      throw std::runtime_error("Block already exists for provided iteration, name, and id");
      return result;
    }
  }
  DataBlock block;
  block.dimensions = dimensions;
  block.offsets = offsets;
  block.type = type;
  block.data.resize(data.size());

  // double serverStage2 = tl::timer::wtime();

  try
  {
    std::vector<std::pair<void*, size_t> > segments = { std::make_pair<void*, size_t>(
      block.data.data(), block.data.size()) };
    auto local_bulk = m_engine.expose(segments, tl::bulk_mode::write_only);
    auto origin_ep = m_engine.lookup(sender_addr);
    data.on(origin_ep) >> local_bulk;
  }
  catch (const std::exception& ex)
  {
    result = 1;
  }
  // double serverStage3 = tl::timer::wtime();

  if (result == 0)
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    m_datasets[iteration][dataset_name][block_id] = std::move(block);
  }
  // double serverStage4 = tl::timer::wtime();

  // std::cout << "iteration " << iteration << " server stage1 " << serverStage2 - serverStage1
  //          << " stage2 " << serverStage3 - serverStage2 << " stage3 "
  //          << serverStage4 - serverStage3 << std::endl;

  return result;
}

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
#include <math.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#define MONA_BACKEND_BARRIER_TAG1 2051
#define MONA_BACKEND_BARRIER_TAG2 2052

#define MONA_BACKEND_ALLREDUCE_TAG 2053

// this synthetic application is between the
// pure synthetic one and actual one, we use mona reduce here
int MonaBackendPipeline::executesynthetic2(
  uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm)
{
  auto executeStart = tl::timer::wtime();

  spdlog::debug("{}: Executing iteration {}", __FUNCTION__, iteration);
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  // still use mona comm lock to make sure we use the latest value

  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::debug("iteration {}: rank={}, size={}", iteration, procRank, procSize);

  std::vector<Mandelbulb> MandelbulbList;
  int totalBlock = 0;
  int localBlocks = m_datasets[iteration][dataset_name].size();
  // std::cout << "local blocks is " << localBlocks << std::endl;
  // the reduce function is a lambda expression here
  mona_comm_allreduce(
    m_mona_comm, &localBlocks, &totalBlock, sizeof(int), 1,
    [](const void* in, void* out, na_size_t, na_size_t, void*) {
      const int* a = static_cast<const int*>(in);
      int* b = static_cast<int*>(out);
      *b += *a;
    },
    nullptr, MONA_BACKEND_ALLREDUCE_TAG);
  spdlog::debug(
    "{}: After AllReduce, localBlocks={}, totalBlocks={}", __FUNCTION__, localBlocks, totalBlock);

  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG2);
  spdlog::debug("after barrier, iteration={} commsize {}", iteration, procSize);

  // reduce the block data to one process and do some operations
  // reduce data reduce dimentions reduce offsets average
  int workload = 20;
  for (int k = 0; k < workload; k++)
  {
    // give staging some burdern for execution
    for (auto& t : m_datasets[iteration][dataset_name])
    {
      size_t blockID = t.first;
      auto depth = t.second.dimensions[0] - 1;
      auto height = t.second.dimensions[1];
      auto width = t.second.dimensions[2];
      // std::cout << blockID << ",";
      size_t blockOffset = blockID * depth;
      // reconstruct the MandelbulbList
      Mandelbulb mb(width, height, depth, blockOffset, 1.2, blockID, totalBlock);
      mb.SetData(t.second.data);
      MandelbulbList.push_back(mb);
      double* origin = mb.GetOrigin();
      int* extents = mb.GetExtents();

      // spdlog::debug("check upstream data id {} depth {} height {} width {} ,check generated info
      // "
      //              "origin {} {} {} extents{} {} {}",
      //  blockID, depth, height, width, origin[0], origin[1], origin[2], extents[1], extents[3],
      //  extents[5]);

      // for testing the data
      std::string fileName =
        "./mbtestdata/mb" + std::to_string(iteration) + "_" + std::to_string(blockID) + ".vti";

      // try to do multiple writs
      InSitu::MBOutPutVTIFile(mb, fileName);
    }
  }

  auto executeEnd = tl::timer::wtime();
  spdlog::debug(
    "iteration {}: Done with executesynthetic2 time {}", iteration, executeEnd - executeStart);

  return 0;
}

// update the data, try to add the visulization operations
// when this is called, we can not update the comm
int MonaBackendPipeline::executesynthetic(
  uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm)
{
  auto executeStart = tl::timer::wtime();

  spdlog::debug("{}: Executing iteration {}", __FUNCTION__, iteration);
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  // still use mona comm lock to make sure we use the latest value

  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::debug("iteration {}: rank={}, size={}", iteration, procRank, procSize);

  std::vector<Mandelbulb> MandelbulbList;
  int totalBlock = 0;
  int localBlocks = m_datasets[iteration][dataset_name].size();
  // std::cout << "local blocks is " << localBlocks << std::endl;
  mona_comm_allreduce(
    m_mona_comm, &localBlocks, &totalBlock, sizeof(int), 1,
    [](const void* in, void* out, na_size_t, na_size_t, void*) {
      const int* a = static_cast<const int*>(in);
      int* b = static_cast<int*>(out);
      *b += *a;
    },
    nullptr, MONA_BACKEND_ALLREDUCE_TAG);
  spdlog::debug(
    "{}: After AllReduce, localBlocks={}, totalBlocks={}", __FUNCTION__, localBlocks, totalBlock);

  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG2);
  spdlog::debug("after barrier, iteration={} commsize {}", iteration, procSize);

  double syntheticT = 18.0 * pow(procSize, -0.6);
  usleep(syntheticT * 1000000);

  auto executeEnd = tl::timer::wtime();
  spdlog::debug("iteration {}: Done with synthetic time {}", iteration, executeEnd - executeStart);

  return 0;
}

int MonaBackendPipeline::execute(
  uint64_t& iteration, std::string& dataset_name, mona_comm_t m_mona_comm)
{
  auto executeStart = tl::timer::wtime();

  spdlog::debug("{}: Executing iteration {}", __FUNCTION__, iteration);
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  // still use mona comm lock to make sure we use the latest value

  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::debug("iteration {}: rank={}, size={}", iteration, procRank, procSize);

  if (m_script_name == "")
  {
    throw std::runtime_error("Empty script name");
  }

  // this may takes long time for first step
  // make sure all servers do same things
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG1);
  spdlog::debug("{}: After barrier", iteration);

  // there is an extra bcast operation for first added process, do this thing every time
  if (m_first_init)
  {
    // self is created when we init the backend
    spdlog::debug("{}: First init, requires initialization with mona_comm_self", __FUNCTION__);
    InSitu::MonaInitialize(m_script_name, m_mona_comm_self);
    spdlog::debug("{}: Done initializing with mona_comm_self", __FUNCTION__);
  }

  // update it anyway
  // when the new server added, this should be updated

  spdlog::debug("{}: Updating MoNA controller", __FUNCTION__);
  InSitu::MonaUpdateController(m_mona_comm);
  spdlog::debug("{}: Done updating MoNA controller", __FUNCTION__);

  m_first_init = false;

  // redistribute the process
  // get the suitable workload (mandelbulb instance list) based on current data staging services

  // output all key in current map
  // extract data list from current map

  // get the total block number
  // the largest key+1 is the total block number
  // it might be convenient to get the info by API

  spdlog::debug("{}: Updating data for iteration {}", __FUNCTION__, iteration);
  size_t maxID = 0;
  std::vector<Mandelbulb> MandelbulbList;
  int totalBlock = 0;
  int localBlocks = m_datasets[iteration][dataset_name].size();
  // std::cout << "local blocks is " << localBlocks << std::endl;
  mona_comm_allreduce(
    m_mona_comm, &localBlocks, &totalBlock, sizeof(int), 1,
    [](const void* in, void* out, na_size_t, na_size_t, void*) {
      const int* a = static_cast<const int*>(in);
      int* b = static_cast<int*>(out);
      *b += *a;
    },
    nullptr, MONA_BACKEND_ALLREDUCE_TAG);
  spdlog::debug(
    "{}: After AllReduce, localBlocks={}, totalBlocks={}", __FUNCTION__, localBlocks, totalBlock);

  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    // std::cout << "iteration " << iteration << " procRank " << procRank << " key ";
    for (auto& t : m_datasets[iteration][dataset_name])
    {
      size_t blockID = t.first;
      auto depth = t.second.dimensions[0] - 1;
      auto height = t.second.dimensions[1];
      auto width = t.second.dimensions[2];
      // std::cout << blockID << ",";
      size_t blockOffset = blockID * depth;
      // reconstruct the MandelbulbList
      Mandelbulb mb(width, height, depth, blockOffset, 1.2, blockID, totalBlock);
      mb.SetData(t.second.data);
      MandelbulbList.push_back(mb);
      double* origin = mb.GetOrigin();
      int* extents = mb.GetExtents();

      // spdlog::debug("check upstream data id {} depth {} height {} width {} ,check generated info
      // "
      //              "origin {} {} {} extents{} {} {}",
      //  blockID, depth, height, width, origin[0], origin[1], origin[2], extents[1], extents[3],
      //  extents[5]);

      // for testing the data
      // std::string fileName =
      //  "./mbtestdata" + std::to_string(iteration) + "_" + std::to_string(blockID) + ".vti";
      // InSitu::MBOutPutVTIFile(t.second, fileName);
    }
    // std::cout << std::endl;
  }
  spdlog::debug("{}: About to call InSitu::MonaCoProcessDynamic with iteration={} commsize {}",
    __FUNCTION__, iteration, procSize);
  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG2);
  spdlog::debug("after barrier, iteration={} commsize {}", iteration, procSize);

  double syntheticT = 18 * pow(procSize, -0.6);
  usleep(syntheticT * 1000000);
  InSitu::MonaCoProcessDynamic(MandelbulbList, totalBlock, iteration, iteration);
  // use 15*x^(-0.9)

  auto executeEnd = tl::timer::wtime();
  spdlog::debug("iteration {}: Done with InSitu::MonaCoProcessDynamic time {}", iteration,
    executeEnd - executeStart);

  return 0;
}

int MonaBackendPipeline::cleanup(std::string& dataset_name, uint64_t iteration)
{
  spdlog::debug(
    "{}: Calling cleanup for iteration {} for datastet {}", __FUNCTION__, iteration, dataset_name);
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    m_datasets[iteration].erase(dataset_name);
  }

  spdlog::debug(
    "{}: Done cleaning up iteration {} for datastet {}", __FUNCTION__, iteration, dataset_name);
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
    throw ex.what();
    return result;
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

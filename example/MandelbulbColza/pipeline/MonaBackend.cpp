/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "MonaBackend.hpp"
#include "../MonaInSituAdaptor.hpp"
#include "../mb.hpp"
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <spdlog/spdlog.h>

COLZA_REGISTER_BACKEND(monabackend, MonaBackendPipeline);

#define MONA_BACKEND_BARRIER_TAG 2051
#define MONA_BACKEND_ALLREDUCE_TAG 2052

// this function is called by colza framework when the pipeline is created
// this function is also called when there is join or leave of the processes
void MonaBackendPipeline::updateMonaAddresses(
  mona_instance_t mona, const std::vector<na_addr_t>& addresses)
{
  // this function is called when server is started first time
  // or when there is process join and leave
  // std::cout << "updateMonaAddresses is called" << std::endl;

  spdlog::trace("{}: called", __FUNCTION__);
  {
    std::lock_guard<tl::mutex> g_comm(this->m_mona_comm_mtx);
    m_mona = mona;
    m_member_addrs = addresses;
    m_need_reset = true;
  }
  spdlog::trace("{}: number of addresses is now {}", __FUNCTION__, addresses.size());
}

colza::RequestResult<int32_t> MonaBackendPipeline::start(uint64_t iteration)
{
  spdlog::trace("{}: Starting iteration {}", __FUNCTION__, iteration);

  std::lock_guard<tl::mutex> g_comm(m_mona_comm_mtx);
  if(m_need_reset || (m_mona_comm == nullptr)) {
      spdlog::trace("{}: Need to create a MoNA communicator", __FUNCTION__);
      if(m_mona_comm) {
        mona_comm_free(m_mona_comm);
      }
      na_return_t ret =
          mona_comm_create(m_mona, m_member_addrs.size(), m_member_addrs.data(), &(m_mona_comm));
      if (ret != 0)
      {
          spdlog::trace("{}: MoNA communicator creation failed", __FUNCTION__);
          throw std::runtime_error("failed to init mona communicator");
      }
      spdlog::trace("{}: MoNA communicator creation succeeded", __FUNCTION__);
  }

  spdlog::trace("{}: Start complete", __FUNCTION__);

  colza::RequestResult<int32_t> result;
  result.success() = true;
  result.value() = 0;
  return result;
}

void MonaBackendPipeline::abort(uint64_t iteration)
{
  spdlog::trace("{}: Abort call for iteration {}", __FUNCTION__, iteration);
  // free the communicator
  {
    std::lock_guard<tl::mutex> g_comm(m_mona_comm_mtx);
    mona_comm_free(m_mona_comm);
    m_mona_comm = nullptr;
    m_need_reset = true;
  }
  spdlog::trace("{}: Abort complete", __FUNCTION__);
}

// update the data, try to add the visulization operations
colza::RequestResult<int32_t> MonaBackendPipeline::execute(uint64_t iteration)
{
  spdlog::trace("{}: Executing iteration {}", __FUNCTION__, iteration);
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step

  int totalBlock = 0;
  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::trace("{}: rank={}, size={}", __FUNCTION__, procRank, procSize);

  if(m_script_name == "") {
    throw std::runtime_error("Empty script name");
  }

  // this may takes long time for first step
  // make sure all servers do same things
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG);
  spdlog::trace("{}: After barrier", __FUNCTION__);

  if (m_need_reset && !m_first_init) {
    spdlog::trace("{}: Need to reset,finalizing InSitu first", __FUNCTION__);
    InSitu::Finalize();
    spdlog::trace("{}: Done finalizing InSitu for reset", __FUNCTION__);
  }
  if (m_first_init || m_need_reset) {
    spdlog::trace("{}: first_init={}, need_reset={}, calling MonaInitialize",
            __FUNCTION__, m_first_init, m_need_reset);
    InSitu::MonaInitialize(m_script_name, m_mona_comm);
    spdlog::trace("{}: Done initializing");
  }
#if 0
  if (m_first_init) {
    spdlog::trace("{}: First time, calling InSitu::MonaInitialize", __FUNCTION__);
    InSitu::MonaInitialize(m_script_name, m_mona_comm);
    spdlog::trace("{}: Done calling InSitu::MonaInitialize", __FUNCTION__);
  } else if (m_need_reset) {

    InSitu::MonaUpdateController(m_mona_comm);
  }
#endif

  m_need_reset = false;
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

  int localBlocks = m_datasets[iteration]["mydata"].size();
  std::cout << "local blocks is " << localBlocks << std::endl;
  mona_comm_allreduce(m_mona_comm, &localBlocks, &totalBlock, sizeof(int), 1,
          [](const void* in, void* out, na_size_t, na_size_t, void*) {
                const int* a = static_cast<const int*>(in);
                int* b = static_cast<int*>(out);
                *b += *a;
          }, nullptr, MONA_BACKEND_ALLREDUCE_TAG);
  spdlog::trace("{}: After AllReduce, localBlocks={}, totalBlocks={}",
          __FUNCTION__, localBlocks, totalBlock);

  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    // std::cout << "iteration " << iteration << " procRank " << procRank << " key ";
    for (auto& t : m_datasets[iteration]["mydata"])
    {
      size_t blockID = t.first;
      auto width = t.second.dimensions[0]-1;
      auto height = t.second.dimensions[1];
      auto depth = t.second.dimensions[2];
      // std::cout << blockID << ",";
      size_t blockOffset = blockID * depth;
      // reconstruct the MandelbulbList
      Mandelbulb mb(width, height, depth, blockOffset, 1.2, totalBlock);
      mb.SetData(t.second.data);
      MandelbulbList.push_back(mb);
    }
    // std::cout << std::endl;
  }
  spdlog::trace("{}: About to call InSitu::MonaCoProcessDynamic with iteration={}", iteration);
  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  InSitu::MonaCoProcessDynamic(MandelbulbList, totalBlock, iteration, iteration);

  spdlog::trace("{}: Done with InSitu::MonaCoProcessDynamic");

  // try to execute the in-situ function that render the data
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  return result;
}

colza::RequestResult<int32_t> MonaBackendPipeline::cleanup(uint64_t iteration)
{
  spdlog::trace("{}: Calling cleanup for iteration {}", __FUNCTION__, iteration);
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    m_datasets.erase(iteration);
  }
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  spdlog::trace("{}: Done cleaning up iteration {}", __FUNCTION__, iteration);
  return result;
}

colza::RequestResult<int32_t> MonaBackendPipeline::stage(const std::string& sender_addr,
  const std::string& dataset_name, uint64_t iteration, uint64_t block_id,
  const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets,
  const colza::Type& type, const thallium::bulk& data)
{
  colza::RequestResult<int32_t> result;
  result.value() = 0;
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    if (m_datasets.count(iteration) != 0 && m_datasets[iteration].count(dataset_name) != 0 &&
      m_datasets[iteration][dataset_name].count(block_id) != 0)
    {
      result.error() = "Block already exists for provided iteration, name, and id";
      result.success() = false;
      return result;
    }
  }
  DataBlock block;
  block.dimensions = dimensions;
  block.offsets = offsets;
  block.type = type;
  block.data.resize(data.size());

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
    result.success() = false;
    result.error() = ex.what();
  }

  if (result.success())
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    m_datasets[iteration][dataset_name][block_id] = std::move(block);
  }
  return result;
}

colza::RequestResult<int32_t> MonaBackendPipeline::destroy()
{
  colza::RequestResult<int32_t> result;
  result.value() = true;
  return result;
}

std::unique_ptr<colza::Backend> MonaBackendPipeline::create(const colza::PipelineFactoryArgs& args)
{
  return std::unique_ptr<colza::Backend>(new MonaBackendPipeline(args));
}

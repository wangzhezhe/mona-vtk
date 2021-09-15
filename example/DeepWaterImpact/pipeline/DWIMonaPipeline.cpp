/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "DWIMonaPipeline.hpp"
#include "DWIInSituMonaAdaptor.hpp"
#include <spdlog/spdlog.h>

COLZA_REGISTER_BACKEND(dwi_mona_backend, DWIMONAPipeline);
#define MONA_BACKEND_BARRIER_TAG 2051
#define MONA_BACKEND_ALLREDUCE_TAG 2052

void DWIMONAPipeline::updateMonaAddresses(
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

    if (m_mona_comm_self == nullptr)
    {
      na_addr_t self_addr = NA_ADDR_NULL;
      na_return_t ret = mona_addr_self(m_mona, &self_addr);
      if (ret != NA_SUCCESS)
      {
        spdlog::critical("{}: mona_addr_self returned {}", __FUNCTION__, ret);
        throw std::runtime_error("mona_addr_self failed");
      }
      ret = mona_comm_create(m_mona, 1, &self_addr, &m_mona_comm_self);
      if (ret != NA_SUCCESS)
      {
        spdlog::critical("{}: mona_comm_create returned {}", __FUNCTION__, ret);
        throw std::runtime_error("mona_comm_create failed");
      }
      mona_addr_free(m_mona, self_addr);
    }
  }
  spdlog::trace("{}: number of addresses is now {}", __FUNCTION__, addresses.size());
}

colza::RequestResult<int32_t> DWIMONAPipeline::start(uint64_t iteration)
{
  spdlog::trace("{}: Starting iteration {}", __FUNCTION__, iteration);

  std::lock_guard<tl::mutex> g_comm(m_mona_comm_mtx);
  if (m_need_reset || m_mona_comm == nullptr)
  {
    spdlog::trace("{}: Need to create a MoNA communicator", __FUNCTION__);
    if (m_mona_comm)
    {
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

void DWIMONAPipeline::abort(uint64_t iteration)
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

colza::RequestResult<int32_t> DWIMONAPipeline::execute(uint64_t iteration)
{

  spdlog::trace("Iteration {} executing", iteration);

  double t1 = tl::timer::wtime();

  int totalBlock = 0;
  int procSize, procRank;
  mona_comm_size(m_mona_comm, &procSize);
  mona_comm_rank(m_mona_comm, &procRank);
  spdlog::info("{}: Iteration {}, rank={}, size={}", __FUNCTION__, iteration, procRank, procSize);

  // this may takes long time for first step
  // make sure all servers do same things
  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG);
  spdlog::trace("{}: After barrier", __FUNCTION__);

  std::string scriptname = "render_dwi.py";

  if (m_first_init)
  {
    spdlog::trace("{}: First init, requires initialization with mona_comm_self", __FUNCTION__);
    InSitu::MonaInitialize(scriptname, m_mona_comm_self);
    spdlog::trace("{}: Done initializing with mona_comm_self", __FUNCTION__);
  }

  spdlog::trace("{}: Updating MoNA controller", __FUNCTION__);
  InSitu::MonaUpdateController(m_mona_comm);
  spdlog::trace("{}: Done updating MoNA controller", __FUNCTION__);

  m_first_init = false;
  m_need_reset = false;


  mona_comm_barrier(m_mona_comm, MONA_BACKEND_BARRIER_TAG);
  InSitu::MonaCoProcessDynamic(this->m_datasets[iteration], iteration, iteration);

  spdlog::trace("Iteration {} finish executing", iteration);
  
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  return result;
}

colza::RequestResult<int32_t> DWIMONAPipeline::cleanup(uint64_t iteration)
{
  spdlog::trace("Iteration {} cleaned up", iteration);
  std::lock_guard<tl::mutex> g(m_datasets_mtx);
  m_datasets.erase(iteration);
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;

  return result;
}

colza::RequestResult<int32_t> DWIMONAPipeline::stage(const std::string& sender_addr,
  const std::string& dataset_name, uint64_t iteration, uint64_t block_id,
  const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets,
  const colza::Type& type, const thallium::bulk& data)
{
  colza::RequestResult<int32_t> result;
  result.value() = 0;
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    if (m_datasets.count(iteration) != 0 && m_datasets[iteration].count(block_id) != 0 &&
      m_datasets[iteration][block_id].count(dataset_name) != 0)
    {
      result.error() = "Block already exists for provided iteration, name, and id " +
        std::to_string(iteration) + dataset_name + std::to_string(block_id);
      result.success() = false;
      return result;
    }
  }
  std::shared_ptr<DataBlock> blockptr = std::make_shared<DataBlock>();

  blockptr->dimensions = dimensions;
  blockptr->offsets = offsets;
  blockptr->type = type;
  blockptr->data.resize(data.size());

  try
  {
    std::vector<std::pair<void*, size_t> > segments = { std::make_pair<void*, size_t>(
      blockptr->data.data(), blockptr->data.size()) };
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
    m_datasets[iteration][block_id][dataset_name] = blockptr;
  }

  // spdlog::trace(
  //  "stage ok iteration {}, blockid {}, datasetname {}", iteration, block_id, dataset_name);

  return result;
}

colza::RequestResult<int32_t> DWIMONAPipeline::destroy()
{
  colza::RequestResult<int32_t> result;
  result.value() = true;
  // TODO
  return result;
}

std::unique_ptr<colza::Backend> DWIMONAPipeline::create(const colza::PipelineFactoryArgs& args)
{
  return std::unique_ptr<colza::Backend>(new DWIMONAPipeline(args));
}

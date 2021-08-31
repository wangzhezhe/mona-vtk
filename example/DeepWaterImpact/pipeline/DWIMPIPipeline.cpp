/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "DWIMPIPipeline.hpp"
#include "DWIInSituMPIAdaptor.hpp"
#include <spdlog/spdlog.h>

COLZA_REGISTER_BACKEND(dwi_mpi_backend, DWIMPIPipeline);

void DWIMPIPipeline::updateMonaAddresses(
  mona_instance_t mona, const std::vector<na_addr_t>& addresses)
{
  spdlog::trace("Mona addresses have been updated, group size is now {}", addresses.size());
  (void)addresses;
  (void)mona;
  // this function is called when server is started first time
  // or when there is process join and leave
  std::cout << "updateMonaAddresses mpi version is called" << std::endl;
  if (this->m_first_init)
  {
    this->m_mpi_comm = MPI_COMM_WORLD;
    MPI_Comm_rank(this->m_mpi_comm, &(this->m_init_rank));
    MPI_Comm_size(this->m_mpi_comm, &(this->m_init_proc));
  }
  else
  {
    int rank_new;
    int proc_new;
    MPI_Comm_rank(this->m_mpi_comm, &rank_new);
    MPI_Comm_size(this->m_mpi_comm, &proc_new);
    if (rank_new != m_init_rank || proc_new != m_init_proc)
    {
      throw std::runtime_error("the mpi comm group change");
    }
  }
}

colza::RequestResult<int32_t> DWIMPIPipeline::start(uint64_t iteration)
{
  spdlog::trace("Iteration {} starting", iteration);
  colza::RequestResult<int32_t> result;
  result.success() = true;
  result.value() = 0;
  return result;
}

void DWIMPIPipeline::abort(uint64_t iteration)
{
  spdlog::trace("Iteration {} aborted", iteration);
}

colza::RequestResult<int32_t> DWIMPIPipeline::execute(uint64_t iteration)
{

  spdlog::trace("Iteration {} executing", iteration);

  if (this->m_first_init)
  {
    // init the mochi communicator and register the pipeline
    // this is supposed to be called once
    // TODO set this from the client or server parameters?
    // std::string scriptname =
    //  "/global/homes/z/zw241/cworkspace/src/mona-vtk/example/GrayScottColza/pipeline/render.py";
    // copy the render_dwi into current dir at scripts
    std::string scriptname = "./render_dwi.py";
    InSitu::MPIInitialize(scriptname, this->m_mpi_comm);
    this->m_first_init = false;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  
  InSitu::MPICoProcess(this->m_datasets[iteration], iteration, iteration);

  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  return result;
}

colza::RequestResult<int32_t> DWIMPIPipeline::cleanup(uint64_t iteration)
{
  spdlog::trace("Iteration {} cleaned up", iteration);
  std::lock_guard<tl::mutex> g(m_datasets_mtx);
  m_datasets.erase(iteration);
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;

  return result;
}

colza::RequestResult<int32_t> DWIMPIPipeline::stage(const std::string& sender_addr,
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

  spdlog::trace(
    "stage ok iteration {}, blockid {}, datasetname {}", iteration, block_id, dataset_name);

  return result;
}

colza::RequestResult<int32_t> DWIMPIPipeline::destroy()
{
  colza::RequestResult<int32_t> result;
  result.value() = true;
  // TODO
  return result;
}

std::unique_ptr<colza::Backend> DWIMPIPipeline::create(const colza::PipelineFactoryArgs& args)
{
  return std::unique_ptr<colza::Backend>(new DWIMPIPipeline(args));
}

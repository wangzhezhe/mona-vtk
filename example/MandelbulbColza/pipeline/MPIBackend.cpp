/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "MPIBackend.hpp"
#include "../MPIInSituAdaptor.hpp"
#include "../mb.hpp"
#include <iostream>

// this is only for testing
int global_rank = 0;
int global_proc = 0;

COLZA_REGISTER_BACKEND(mpibackend, MPIBackendPipeline);

// this function is called by colza framework when the pipeline is created
// this function is also called when there is join or leave of the processes
void MPIBackendPipeline::updateMonaAddresses(
  mona_instance_t mona, const std::vector<na_addr_t>& addresses)
{
  // this function is called when server is started first time
  // or when there is process join and leave
  std::cout << "updateMonaAddresses is called, do not reset communicator for MPI backend"
            << std::endl;
  if (this->m_first_init)
  {
    this->m_mpi_comm = MPI_COMM_WORLD;
    MPI_Comm_rank(this->m_mpi_comm, &global_rank);
    MPI_Comm_size(this->m_mpi_comm, &global_proc);
  }
  else
  {
    int rank_new;
    int proc_new;
    MPI_Comm_rank(this->m_mpi_comm, &rank_new);
    MPI_Comm_size(this->m_mpi_comm, &proc_new);
    if (rank_new != global_rank || proc_new != global_proc)
    {
      throw std::runtime_error("the mpi comm group change");
    }
  }
}

colza::RequestResult<int32_t> MPIBackendPipeline::start(uint64_t iteration)
{
  std::cerr << "Iteration " << iteration << " starting" << std::endl;
  colza::RequestResult<int32_t> result;
  result.success() = true;
  result.value() = 0;
  return result;
}

void MPIBackendPipeline::abort(uint64_t iteration)
{
  std::cerr << "Client aborted iteration " << iteration << std::endl;
}

// update the data, try to add the visulization operations
colza::RequestResult<int32_t> MPIBackendPipeline::execute(uint64_t iteration)
{

  double t1 = tl::timer::wtime();
  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  if (this->m_first_init)
  {
    // init the mochi communicator and register the pipeline
    // this is supposed to be called once
    // TODO set this from the client or server parameters?
    if (m_script_name == "")
    {
      throw std::runtime_error("Empty script name");
    }

    InSitu::MPIInitialize(m_script_name);
    this->m_first_init = false;
  }

  // output all key in current map
  // extract data list from current map

  // get the total block number
  // the largest key+1 is the total block number
  // it might be convenient to get the info by API
  size_t maxID = 0;
  std::vector<Mandelbulb> MandelbulbList;
  int totalBlock = 0;
  // get block num by collective operation
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    int localBlocks = m_datasets[iteration]["mydata"].size();

    MPI_Allreduce(&localBlocks, &totalBlock, 1, MPI_INT, MPI_SUM, this->m_mpi_comm);
    // std::cout << "debug totalBlock is " << totalBlock << std::endl;
    // std::cout << "iteration " << iteration << " procRank " << procRank << " key ";
    for (auto& t : m_datasets[iteration]["mydata"])
    {
      size_t blockID = t.first;
      // std::cout << blockID << ",";
      auto depth = t.second.dimensions[0] - 1;
      auto height = t.second.dimensions[1];
      auto width = t.second.dimensions[2];

      size_t blockOffset = blockID * depth;
      // reconstruct the MandelbulbList
      // std::cout << "debug parameters " << width << "," << height << "," << depth << ","
      //          << blockOffset << std::endl;
      Mandelbulb mb(width, height, depth, blockOffset, 1.2, totalBlock);
      mb.SetData(t.second.data);
      MandelbulbList.push_back(mb);
    }
  }

  MPI_Barrier(this->m_mpi_comm);
  // process the insitu function for the MandelbulbList
  // the controller is updated in the MonaUpdateController
  InSitu::MPICoProcessDynamic(this->m_mpi_comm, MandelbulbList, totalBlock, iteration, iteration);

  // try to execute the in-situ function that render the data
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  int procRank;
  MPI_Comm_rank(MPI_COMM_WORLD, &procRank);
  double t2 = tl::timer::wtime();
    std::cout << "Rank " << procRank << " completed execution in " << (t2-t1) << " sec" << std::endl;
  return result;
}

colza::RequestResult<int32_t> MPIBackendPipeline::cleanup(uint64_t iteration)
{
  std::lock_guard<tl::mutex> g(m_datasets_mtx);
  m_datasets.erase(iteration);
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  return result;
}

colza::RequestResult<int32_t> MPIBackendPipeline::stage(const std::string& sender_addr,
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

  /*
  std::cout << "iteration " << iteration << " block_id " << block_id << " dimensions "
            << dimensions[0] << "," << dimensions[1] << "," << dimensions[2] << "offsets "
            << offsets[0] << "," << offsets[1] << "," << offsets[2] << " datasize " << data.size()
            << std::endl;
  */

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

colza::RequestResult<int32_t> MPIBackendPipeline::destroy()
{
  colza::RequestResult<int32_t> result;
  result.value() = true;
  return result;
}

std::unique_ptr<colza::Backend> MPIBackendPipeline::create(const colza::PipelineFactoryArgs& args)
{
  return std::unique_ptr<colza::Backend>(new MPIBackendPipeline(args));
}

/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "gsMonaBackend.hpp"
#include "gsMonaInSituAdaptor.hpp"
#include <iostream>
#include <memory> // We need to include this for shared_ptr

COLZA_REGISTER_BACKEND(gsmonabackend, MonaBackendPipeline);

// this function is called by colza framework when the pipeline is created
// this function is also called when there is join or leave of the processes
void MonaBackendPipeline::updateMonaAddresses(
  mona_instance_t mona, const std::vector<na_addr_t>& addresses)
{
  // this function is called when server is started first time
  // or when there is process join and leave
  std::cout << "updateMonaAddresses is called" << std::endl;
  this->m_need_reset = true;

  // create the mona communicator
  // there are seg fault here if we create themm multiple times without the condition of
  na_return_t ret =
    mona_comm_create(mona, addresses.size(), addresses.data(), &(this->m_mona_comm));
  if (ret != 0)
  {
    throw std::runtime_error("failed to init mona");
  }

  int procSize;
  int procRank;
  mona_comm_size(this->m_mona_comm, &procSize);
  mona_comm_rank(this->m_mona_comm, &procRank);
  std::cout << "Init mona, mona addresses have been updated, size is " << procSize << " rank is "
            << procRank << std::endl;
}

colza::RequestResult<int32_t> MonaBackendPipeline::start(uint64_t iteration)
{
  std::cerr << "Iteration " << iteration << " starting" << std::endl;
  colza::RequestResult<int32_t> result;
  result.success() = true;
  result.value() = 0;
  return result;
}

void MonaBackendPipeline::abort(uint64_t iteration)
{
  std::cerr << "Client aborted iteration " << iteration << std::endl;
}

// update the data, try to add the visulization operations
colza::RequestResult<int32_t> MonaBackendPipeline::execute(uint64_t iteration)
{
  std::cout << " gs pipeline execute iteration " << iteration << std::endl;

  // when the mona is updated, init and reset
  // otherwise, do not reset
  // it might need some time for the fir step
  if (this->m_first_init)
  {
    // init the mochi communicator and register the pipeline
    // this is supposed to be called once
    // TODO set this from the client or server parameters?
    // std::string scriptname =
    //  "/global/homes/z/zw241/cworkspace/src/mona-vtk/example/GrayScottColza/pipeline/render.py";
    std::string SRCDIR = getenv("SRCDIR");
    std::string scriptname = SRCDIR + "/example/GrayScottColza/pipeline/gsrender_multiclip.py";

    InSitu::MonaInitialize(scriptname, this->m_mona_comm);
    this->m_first_init = false;
  }
  else
  {
    if (this->m_need_reset)
    {
      // when communicator is updated after the first initilization
      // the global communicator will be replaced
      // there are still some issues here
      // icet contect is updated automatically in paraveiw patch
      InSitu::MonaUpdateController(this->m_mona_comm);
      this->m_need_reset = false;
    }
  }

  // redistribute the process
  // get the suitable workload (mandelbulb instance list) based on current data staging services
  int procSize;
  int procRank;
  mona_comm_size(this->m_mona_comm, &procSize);
  mona_comm_rank(this->m_mona_comm, &procRank);

  // output all key in current map
  // extract data list from current map
  std::vector<std::shared_ptr<DataBlock> > dataBlockList;
  // process the data blocks
  {
    std::lock_guard<tl::mutex> g(m_datasets_mtx);
    // std::cout << "iteration " << iteration << " procRank " << procRank << " key ";
    for (auto& t : m_datasets[iteration]["grayscottu"])
    {
      size_t blockID = t.first;
      // process the insitu function for the MandelbulbList
      // the controller is updated in the MonaUpdateController
      // std::cout << "debug blockID " << t.first << " size " << t.second->data.size() << " dim "
      //          << t.second->dimensions[0] << "," << t.second->dimensions[1] << ","
      //          << t.second->dimensions[2] << " offset " << t.second->offsets[0] << ","
      //          << t.second->offsets[1] << "," << t.second->offsets[2] << std::endl;

      dataBlockList.push_back(t.second);
      // std::string fileName =
      // "gsdata/var_" + std::to_string(iteration) + "_" + std::to_string(blockID);
      // InSitu::outPutVTIFile(t.second, fileName);
      // InSitu::outPutFigure(t.second, fileName);
    }
    // std::cout << std::endl;
  }
  if (procRank == 0)
  {
    std::cout << "iteration " << iteration << " ok to put data, start coprocess" << std::endl;
  }
  // get the block number from env
  // we may not need the total blocks value here
  InSitu::MonaCoProcessList(dataBlockList, iteration, iteration);

  // try to execute the in-situ function that render the data
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
  return result;
}

colza::RequestResult<int32_t> MonaBackendPipeline::cleanup(uint64_t iteration)
{
  std::lock_guard<tl::mutex> g(m_datasets_mtx);
  m_datasets.erase(iteration);
  auto result = colza::RequestResult<int32_t>();
  result.value() = 0;
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
    m_datasets[iteration][dataset_name][block_id] = blockptr;
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

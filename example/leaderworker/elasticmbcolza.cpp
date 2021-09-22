

/*
 dynamic sim + dynamic staging based on colza
 */
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include "Controller.hpp"
#include "MonaClientCommunicator.hpp"
#include "mb.hpp"
#include <colza/Client.hpp>
#include <fstream>
#include <iostream>
#include <ssg-mpi.h>
#include <ssg.h>
#include <vector>

#ifdef USE_GNI
extern "C"
{
#include <rdmacred.h>
}
#include <margo.h>
#include <mercury.h>
#define DIE_IF(cond_expr, err_fmt, ...)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (cond_expr)                                                                                 \
    {                                                                                              \
      fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " err_fmt "\n", __FILE__, __LINE__,        \
        ##__VA_ARGS__);                                                                            \
      exit(1);                                                                                     \
    }                                                                                              \
  } while (0)
#endif

size_t int2size_t(int val)
{
  return (val < 0) ? __SIZE_MAX__ : (size_t)((unsigned)val);
}

namespace tl = thallium;

static std::string g_address = "na+sm";
static int g_num_threads = 0;
static std::string g_log_level = "info";
static std::string g_leader_addr_file = "dynamic_leader.config";
static std::string g_drc_file = "dynamic_drc.config";
static bool g_join = false;
static unsigned g_swim_period_ms = 1000;
static int64_t g_drc_credential = -1;
static uint64_t g_num_iterations = 10;
// for initial stage, how many process is joint by separate srun
// instead of using the MPI
static uint64_t g_num_initial_join = 0;

static uint64_t g_total_block_number = 256;
static uint64_t g_block_width = 64;
static uint64_t g_block_height = 64;
static uint64_t g_block_depth = 64;
static std::string g_ssg_file = "ssgfile";
// this is specilized exmaple for the monabackend
static std::string g_pipeline = "monabackend";

// block_offset, 1.2, g_total_block_number

static void parse_command_line(int argc, char** argv, int rank);

std::unique_ptr<tl::engine> globalServerEnginePtr;

#define MONA_TESTSIM_BARRIER_TAG 2021
#define MONA_TESTSIM_BCAST_TAG 2022

int main(int argc, char** argv)
{

  // Initialize MPI
  MPI_Init(&argc, &argv);

  ssg_init();

  int rank;
  int procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &procs);

  parse_command_line(argc, argv, rank);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  // make sure the leader or worker
  bool leader = false;
  // only the first created one is the leader
  // the new joined one is not the leader
  if (rank == 0 && (g_join == false))
  {
    leader = true;
  }

  // for the initial processes
  // the rank 0 process create drc things
  // other process get this by MPI
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));

// all processes can be viewd at g_join type in this case
// since the drc is started by the colza server
#ifdef USE_GNI
  // get the drc id from the shared file
  std::ifstream infile(g_drc_file);
  std::string cred_id;
  std::getline(infile, cred_id);
  if (rank == 0)
  {
    std::cout << "load cred_id: " << cred_id << std::endl;
  }

  char drc_key_str[256] = { 0 };
  uint32_t drc_cookie;
  uint32_t drc_credential_id;
  drc_info_handle_t drc_credential_info;
  int ret;
  drc_credential_id = (uint32_t)atoi(cred_id.c_str());

  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access %u", drc_credential_id);
  drc_cookie = drc_get_first_cookie(drc_credential_info);

  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  globalServerEnginePtr = std::make_unique<tl::engine>(
    tl::engine(g_address, THALLIUM_SERVER_MODE, true, g_num_threads, &hii));

#else
  // tl::engine engine(g_address, THALLIUM_SERVER_MODE);
  // globalServerEnginePtr =
  //  std::make_unique<tl::engine>(tl::engine(g_address, THALLIUM_SERVER_MODE));
  throw std::runtime_error("gni is supposed to use");
#endif

  if (leader)
  {
    // write the leader thallium addr into the file
    std::ofstream leaderFile;
    leaderFile.open(g_leader_addr_file);
    leaderFile << std::string(globalServerEnginePtr->self()) << "\n";
    leaderFile.close();
    spdlog::info(
      "ok to write the leader addr into file {} ", std::string(globalServerEnginePtr->self()));
  }

  MPI_Barrier(MPI_COMM_WORLD);

  // Create Mona instance
  mona_instance_t mona = mona_init_thread(g_address.c_str(), NA_TRUE, &hii.na_init_info, NA_TRUE);

  // Print MoNA address for information
  na_addr_t mona_addr;
  mona_addr_self(mona, &mona_addr);
  std::vector<char> mona_addr_buf(256);
  na_size_t mona_addr_size = 256;
  mona_addr_to_string(mona, mona_addr_buf.data(), &mona_addr_size, mona_addr);
  spdlog::debug("MoNA address is {}", mona_addr_buf.data());
  mona_addr_free(mona, mona_addr);

  // provider should start initilzted firstly before the client
  ControllerProvider controllerProvider(*globalServerEnginePtr, 0, leader);

  MPI_Barrier(MPI_COMM_WORLD);
  // create the controller
  // the rank 0 is not added into the worker
  // the worker size is procs-1 for init stage
  Controller controller(globalServerEnginePtr.get(), std::string(mona_addr_buf.data()), 0,
    procs + g_num_initial_join, controllerProvider.m_leader_meta.get(),
    controllerProvider.m_common_meta.get(), g_leader_addr_file, rank);

  // start the colza part that can interact with the staging service
  // Initialize a Client
  colza::Client colzaClient(*globalServerEnginePtr);
  // this is changed when the client used for sim is changed
  colza::DistributedPipelineHandle colzaPipeline;

  // if the g_join is true the procs is 1
  spdlog::debug("rank {} create the controller ok", rank);
  std::vector<Mandelbulb> MandelbulbList;
  int step = 0;
  bool ifInit = true;

  while (step < g_num_iterations)
  {
    // send the rescale command by the rank 0 process
    // then every process execute sync

    auto syncStart = tl::timer::wtime();

    spdlog::info("start sync for iteration {} ", step);
    // TODO return the mona comm and get the step we should process
    // get the step that needs to be processes after the sync operation
    // the step should be syncronized from the leader
    // we can get this message based on synced mona
    controller.m_controller_client->sync(step, leader);

    auto syncEnd1 = tl::timer::wtime();

    // the controller.m_controller_client->m_mona_comm is updated here
    controller.m_controller_client->getMonaComm(mona);

    // mona comm check size and procs
    int procSize, procRank;
    mona_comm_size(controller.m_controller_client->m_mona_comm, &procSize);
    mona_comm_rank(controller.m_controller_client->m_mona_comm, &procRank);
    spdlog::debug("iteration {} : rank={}, size={}", step, procRank, procSize);

    // the mona comm should be updated after the actual comm is updated
    // this part can be svaed further, and only update it when it is necessary
    colza::MonaClientCommunicator colzaMonaComm =
      colza::MonaClientCommunicator(controller.m_controller_client->m_mona_comm);

    // for the first iteration, this is true

    // create mona comm
    // this should be in the global region
    // otherwise, it will be deleted after this scope
    // use another provider id, since the controller one is 0
    // maybe do not create whole colza pipeline every time, just update comm
    if (ifInit)
    {
      colzaPipeline =
        colzaClient.makeDistributedPipelineHandle(&colzaMonaComm, g_ssg_file, 1, g_pipeline);
      ifInit = false;
    }
    else
    {
      // we only update the communicator in it
      colzaPipeline.updateComm(&colzaMonaComm);
    }

    // this may takes long time for first step
    // make sure all servers do same things
    auto syncEnd2 = tl::timer::wtime();

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG);

    // mona comm bcast
    auto syncEnd3 = tl::timer::wtime();
    // the barrier takes long time no sure the reason?
    spdlog::info("iteration {} sync time is {} and {} and {}", step, syncEnd1 - syncStart,
      syncEnd2 - syncEnd1, syncEnd3 - syncEnd2);

    // to do some actual work based on current info
    // for the mb case, caculate how many data blocks should be maintaind in current size
    unsigned reminder = 0;
    if (g_total_block_number % procSize != 0)
    {
      // the last process will process the reminder
      reminder = (g_total_block_number) % unsigned(procSize);
    }

    unsigned nblocks_per_proc = g_total_block_number / procSize;
    if (procRank < reminder)
    {
      // some process need to procee more than one
      nblocks_per_proc = nblocks_per_proc + 1;
    }
    // this value will vary when there is process join/leave
    // compare the nblocks_per_proc to see if it change
    MandelbulbList.clear();

    // caculate the order
    double order = 4.0 + ((double)step) * 8.0 / 100.0;

    // update the data list
    // we may need to do some data marshal here, when the i is small, the time is shor
    // when the i is large, the time is long, there is unbalance here
    // the index should be 0, n-1, 1, n-2, ...
    // the block id base may also need to be updated?
    int rank_offset = procSize;
    int blockid = procRank;
    for (int i = 0; i < nblocks_per_proc; i++)
    {
      if (blockid >= g_total_block_number)
      {
        continue;
      }
      int block_offset = blockid * g_block_depth;
      // the block offset need to be recaculated, we can not use the resize function
      MandelbulbList.push_back(Mandelbulb(g_block_width, g_block_height, g_block_depth,
        block_offset, 1.2, blockid, g_total_block_number));
      MandelbulbList[i].compute(order);
      blockid = blockid + rank_offset;
    }

    auto caculateEnd = tl::timer::wtime();

    // if (procRank == 0)
    //{
    spdlog::info("iteration {} compute time is {} nblocks_per_proc {} ", step,
      caculateEnd - syncEnd3, nblocks_per_proc);
    //}

    // calculate the execution time

    // extract the bounds and stage the data here

    // start the execution

    // if the new data block size is differnet with the old data block size
    // try to resize the data block list
    // when we compute the data, we do not depend on the previous data value

    // sleep(5);

    // do the rescale operation
    // add process here
    // if (leader)
    //{
    //  controller.naiveJoin(step);
    //}

    // set the colza execute wait here
    // which will call the clean up

    // colza pipeline start
    colzaPipeline.start(step);

    // colza stage

    for (int i = 0; i < MandelbulbList.size(); i++)
    {

      // stage the data at current iteration

      int32_t result;

      int* extents = MandelbulbList[i].GetExtents();
      // the extends value is from 0 to 29
      // the dimension value should be extend value +1
      // the sequence is depth, height, width
      std::vector<size_t> dimensions = { int2size_t(*(extents + 1)) + 1,
        int2size_t(*(extents + 3)) + 1, int2size_t(*(extents + 5)) + 1 };
      std::vector<int64_t> offsets = { 0, 0, MandelbulbList[i].GetZoffset() };

      auto type = colza::Type::INT32;
      colzaPipeline.stage("mydata", step, MandelbulbList[i].GetBlockID(), dimensions, offsets, type,
        MandelbulbList[i].GetData(), &result);
      /*
      std::cout << "step " << step << " blockid " << blockid << " dimentions " << dimensions[0]
                << "," << dimensions[1] << "," << dimensions[2] << " offsets " << offsets[0]
                << "," << offsets[1] << "," << offsets[2] << " listvalueSize "
                << MandelbulbList[i].DataSize() << std::endl;
      */
      if (result != 0)
      {
        throw std::runtime_error(
          "failed to stage " + std::to_string(step) + " return status " + std::to_string(result));
      }
    }

    // colza execute
    colzaPipeline.execute(step);

    // colza cleanup
    colzaPipeline.cleanup(step);

    // remove process here
    bool leave = controller.naiveLeave2(step, 1, procSize, procRank);
    if (leave)
    {
      // write a file and the colza server can start
      static std::string leaveFileName = "clientleave.config";
      std::ofstream leaveFile;
      leaveFile.open(leaveFileName);
      leaveFile << "test" << "\n";
      leaveFile.close();
      break;
    }

    if (rank == 0)
    {
      step++;
    }

    // sync the step
    auto syncStep = tl::timer::wtime();

    na_return_t ret = mona_comm_bcast(
      controller.m_controller_client->m_mona_comm, &step, sizeof(step), 0, MONA_TESTSIM_BCAST_TAG);

    auto syncStepEnd = tl::timer::wtime();

    // this time is trival
    spdlog::debug("get the step value {} from the master takes {}", step, syncStepEnd - syncStep);

    if (ret != NA_SUCCESS)
    {
      throw std::runtime_error("failed to execute bcast to get step value");
    }
  }

  spdlog::debug("Engine finalized, now finalizing MoNA...");
  mona_finalize(mona);

  // there are finalize error when we call it, not sure the reason
  // spdlog::debug("Finalizing SSG");
  // ssg_finalize();

  spdlog::debug("MoNA finalized");

  // colza finalize
  // maybe wait the engine things finish here?
  globalServerEnginePtr->finalize();

  MPI_Finalize();

  return 0;
}

void parse_command_line(int argc, char** argv, int rank)
{
  try
  {
    TCLAP::CmdLine cmd("Spawns a Colza daemon", ' ', "0.1");
    TCLAP::ValueArg<std::string> addressArg(
      "a", "address", "Address or protocol (e.g. ofi+tcp)", true, "", "string");
    TCLAP::ValueArg<int> numThreads(
      "t", "num-threads", "Number of threads for RPC handlers", false, 0, "int");
    TCLAP::ValueArg<std::string> logLevel("v", "verbose",
      "Log level (trace, debug, info, warning, error, critical, off)", false, "info", "string");
    TCLAP::ValueArg<std::string> ssgFile("s", "ssg-file", "SSG file name", false, "", "string");
    TCLAP::SwitchArg joinGroup("j", "join", "Join an existing group rather than create it", false);
    TCLAP::ValueArg<int64_t> drc(
      "d", "drc-credential-id", "DRC credential ID, if already setup", false, -1, "int");
    TCLAP::ValueArg<unsigned> numIterations(
      "i", "iterations", "Number of iterations", false, 10, "int");
    TCLAP::ValueArg<unsigned> numInitJoin(
      "x", "initjoin", "Number of process joined by srun for initilization stage", false, 0, "int");

    TCLAP::ValueArg<int> totalBlockNumArg(
      "b", "total-block-number", "Total block number", true, 0, "int");
    TCLAP::ValueArg<int> widthArg("w", "width", "Width of data block", true, 64, "int");
    TCLAP::ValueArg<int> depthArg("p", "depth", "Depth of data block", true, 64, "int");
    TCLAP::ValueArg<int> heightArg("e", "height", "Height of data block", true, 64, "int");

    cmd.add(addressArg);
    cmd.add(numThreads);
    cmd.add(logLevel);
    cmd.add(ssgFile);
    // in this test, if the jointgroup is true, it can switch dynamically
    cmd.add(joinGroup);
    cmd.add(drc);
    cmd.add(numIterations);
    cmd.add(numInitJoin);
    cmd.add(totalBlockNumArg);
    cmd.add(widthArg);
    cmd.add(depthArg);
    cmd.add(heightArg);

    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_num_threads = numThreads.getValue();
    g_log_level = logLevel.getValue();
    g_join = joinGroup.getValue();
    g_num_iterations = numIterations.getValue();
    g_num_initial_join = numInitJoin.getValue();

    g_total_block_number = totalBlockNumArg.getValue();
    g_block_width = widthArg.getValue();
    g_block_depth = depthArg.getValue();
    g_block_height = heightArg.getValue();

    if (rank == 0)
    {
      std::cout << "------configuraitons------" << std::endl;
      std::cout << "g_address: " << g_address << std::endl;
      std::cout << "g_num_threads: " << g_num_threads << std::endl;
      std::cout << "g_log_level: " << g_log_level << std::endl;
      std::cout << "g_join: " << g_join << std::endl;
      std::cout << "g_num_iterations: " << g_num_iterations << std::endl;
      std::cout << "g_num_initial_join: " << g_num_initial_join << std::endl;
      std::cout << "g_total_block_number: " << g_total_block_number << std::endl;
      std::cout << "g_block_width: " << g_block_width << std::endl;
      std::cout << "g_block_depth: " << g_block_depth << std::endl;
      std::cout << "g_block_height: " << g_block_height << std::endl;
      std::cout << "--------------------------" << std::endl;
    }
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

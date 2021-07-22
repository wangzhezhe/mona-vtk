

/*
 only testing the dynamic sim
 */
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include "Controller.hpp"
#include "mb.hpp"
#include <fstream>
#include <iostream>
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
static std::string g_pipeline = "";

// block_offset, 1.2, g_total_block_number

static void parse_command_line(int argc, char** argv, int rank);

std::unique_ptr<tl::engine> globalServerEnginePtr;

#define MONA_TESTSIM_BARRIER_TAG 2021
#define MONA_TESTSIM_BCAST_TAG 2022

int main(int argc, char** argv)
{

  // Initialize MPI
  MPI_Init(&argc, &argv);
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

  if (!g_join)
  {
#ifdef USE_GNI
    uint32_t drc_credential_id = 0;
    drc_info_handle_t drc_credential_info;
    uint32_t drc_cookie;
    char drc_key_str[256] = { 0 };
    int ret;

    if (rank == 0)
    {
      ret = drc_acquire(&drc_credential_id, DRC_FLAGS_FLEX_CREDENTIAL);
      DIE_IF(ret != DRC_SUCCESS, "drc_acquire");

      ret = drc_access(drc_credential_id, 0, &drc_credential_info);
      DIE_IF(ret != DRC_SUCCESS, "drc_access");
      drc_cookie = drc_get_first_cookie(drc_credential_info);
      sprintf(drc_key_str, "%u", drc_cookie);
      hii.na_init_info.auth_key = drc_key_str;

      ret = drc_grant(drc_credential_id, drc_get_wlm_id(), DRC_FLAGS_TARGET_WLM);
      DIE_IF(ret != DRC_SUCCESS, "drc_grant");

      spdlog::debug("grant the drc_credential_id: {}", drc_credential_id);
      spdlog::debug("use the drc_key_str {}", std::string(drc_key_str));
      for (int dest = 1; dest < procs; dest++)
      {
        // dest tag communicator
        MPI_Send(&drc_credential_id, 1, MPI_UINT32_T, dest, 0, MPI_COMM_WORLD);
      }

      // write this cred_id into file that can be shared by clients
      // output the credential id into the config files
      std::ofstream credFile;
      credFile.open(g_drc_file);
      credFile << drc_credential_id << "\n";
      credFile.close();
    }
    else
    {
      // send rcv is the block call
      // gather the id from the rank 0
      // source tag communicator
      MPI_Recv(&drc_credential_id, 1, MPI_UINT32_T, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      spdlog::debug("rank {} recieve cred key {}", rank, drc_credential_id);

      if (drc_credential_id == 0)
      {
        throw std::runtime_error("failed to rcv drc_credential_id");
      }
      ret = drc_access(drc_credential_id, 0, &drc_credential_info);
      DIE_IF(ret != DRC_SUCCESS, "drc_access %u", drc_credential_id);
      drc_cookie = drc_get_first_cookie(drc_credential_info);

      sprintf(drc_key_str, "%u", drc_cookie);
      hii.na_init_info.auth_key = drc_key_str;
    }

    globalServerEnginePtr = std::make_unique<tl::engine>(
      tl::engine(g_address, THALLIUM_SERVER_MODE, true, g_num_threads, &hii));

#else

    // tl::engine engine(g_address, THALLIUM_SERVER_MODE);
    // globalServerEnginePtr =
    //  std::make_unique<tl::engine>(tl::engine(g_address, THALLIUM_SERVER_MODE));
    // if (rank == 0)
    //{
    //  spdlog::trace("use the protocol other than gni: {}", g_address);
    // write the leader addr
    //}
    throw std::runtime_error("gni is supposed to use");

#endif
  }
  else
  {
// for new started processes
// other process get it by the file
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
  }

  if (leader)
  {
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

  // if the g_join is true the procs is 1
  spdlog::debug("rank {} create the controller ok", rank);
  std::vector<Mandelbulb> MandelbulbList;
  int step = 0;
  bool ifupdated = true;
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

    if (step == 5)
    {
      // for testing
      // this should be get from the leader
      ifupdated = true;
    }
    if (ifupdated)
    {
      // only update mona when there are addr updates
      controller.m_controller_client->getMonaComm(mona);
      ifupdated = false;
    }

    // mona comm check size and procs
    int procSize, procRank;
    mona_comm_size(controller.m_controller_client->m_mona_comm, &procSize);
    mona_comm_rank(controller.m_controller_client->m_mona_comm, &procRank);
    spdlog::debug("iteration {} : rank={}, size={}", step, procRank, procSize);

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
    int rank_offset = 40;
    int blockid = procRank;
    for (int i = 0; i < nblocks_per_proc; i++)
    {
      if (blockid >= g_total_block_number)
      {
        continue;
      }
      int block_offset = blockid * g_block_depth;
      // the block offset need to be recaculated, we can not use the resize function
      MandelbulbList.push_back(Mandelbulb(
        g_block_width, g_block_height, g_block_depth, block_offset, 1.2, blockid, g_total_block_number));
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

    // remove process here
    bool leave = controller.naiveLeave(step, 1, procRank);
    if (leave)
    {
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

   // try to write data out for testing
  }

  spdlog::debug("Engine finalized, now finalizing MoNA...");
  mona_finalize(mona);
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

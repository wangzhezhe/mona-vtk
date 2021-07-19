
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include "../StagingClient.hpp"
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
static std::string g_leader_addr_file = "dynamic_sim_leader.config";
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

  // create the staging client
  std::string stagingLeaderFile = "dynamic_server_leader.config";
  StagingClient stagingClient(globalServerEnginePtr.get(), stagingLeaderFile);

  spdlog::info("ok to create StagingClient");

  for (int step = 0; step < g_num_iterations; step++)
  {
    // do the sync operation

    if (leader)
    {
      spdlog::info("start sync for step {}", step);

      stagingClient.leadersync(step);
    }

    stagingClient.workerSync(leader, step, rank);

    // the leader is responsible for sync and bcast
    // use the mona for the dynamic case
    MPI_Barrier(MPI_COMM_WORLD);

    spdlog::info("ok sync for step {}", step);

    // the controller start a new process each iteration
    // for testing
    if (step == 2)
    {
      // set the expected value at the server end firstly
      // before doing the actual join or leave
      if (leader)
      {
        // only the leader is responsible for trigger things
        stagingClient.updateExpectedProcess("join", 1);

        std::string startStagingCommand = "/bin/bash ./addprocess.sh";
        std::string command = startStagingCommand + " " + std::to_string(1);
        // use systemcall to start ith server
        spdlog::info("Add server by command: {}", command);
        std::system(command.c_str());
      }
    }

    if (step == 5)
    {
      // remove the server process
      if (leader)
      {
        stagingClient.updateExpectedProcess("leave", 1);
        // do not shutdown the leader
        int size = stagingClient.m_stagingView.size();
        if (size == 0)
        {
          throw std::runtime_error("the staging view is not supposed to be 0");
        }
        stagingClient.removeProcess(stagingClient.m_stagingView[size - 1]);
      }
    }

    sleep(5);
  }

  spdlog::debug("Engine finalized, now finalizing MoNA...");
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
      "b", "total-block-number", "Total block number", false, 0, "int");
    TCLAP::ValueArg<int> widthArg("w", "width", "Width of data block", false, 64, "int");
    TCLAP::ValueArg<int> depthArg("p", "depth", "Depth of data block", false, 64, "int");
    TCLAP::ValueArg<int> heightArg("e", "height", "Height of data block", false, 64, "int");

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

// the client to test if it is ok to close the server
// removetely and adding it back
// this is based on the leader worker client without the
// data staging operation

/*
 dynamic sim + dynamic staging based on leader worker mechanism
 */
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include "../../Controller.hpp"
#include "../../TypeSizes.hpp"
#include "../../mb.hpp"
#include "../StagingClient.hpp"
#include <fstream>
#include <iostream>
#include <thallium.hpp>
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
static std::string g_sim_leader_file = "dynamic_leader.config";
static std::string g_drc_file = "dynamic_drc.config";
static std::string g_server_leader_config = "dynamic_server_leader.config";
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
    leaderFile.open(g_sim_leader_file);
    leaderFile << std::string(globalServerEnginePtr->self()) << "\n";
    leaderFile.close();
    spdlog::info(
      "ok to write the sim leader addr into file {} ", std::string(globalServerEnginePtr->self()));
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
  // this is for the elasticity of the sim program
  ControllerProvider controllerProvider(*globalServerEnginePtr, 0, leader);

  MPI_Barrier(MPI_COMM_WORLD);
  // create the controller
  // the rank 0 is not added into the worker
  // the worker size is procs-1 for init stage
  Controller controller(globalServerEnginePtr.get(), std::string(mona_addr_buf.data()), 0,
    procs + g_num_initial_join, controllerProvider.m_leader_meta.get(),
    controllerProvider.m_common_meta.get(), g_sim_leader_file, rank);

  // the client used for sync staging view
  StagingClient stagingClient(globalServerEnginePtr.get(), g_server_leader_config);

  // if the g_join is true the procs is 1
  spdlog::debug("rank {} create the controller and staging client ok", rank);

  // the data generated by the sim
  std::vector<Mandelbulb> MandelbulbList;
  int step = 0;
  std::vector<tl::async_response> asyncResponses;
  int procSize, procRank; // these values are updated at the second iteration
  double lastComputeSubmit =
    tl::timer::wtime(); // this value will be updated when the compute is submitted next time
  int avalibleDynamicProcess = g_num_initial_join;
  bool leave = false;
  double lastWaitTime = 0;
  bool needadd = false;
  while (step < g_num_iterations)
  {
    bool exitok = false;
    // try to close the server one by one
    // do not close the master
    if (step != 0)
    {
      if (step % 2 == 1)
      {
        // remove two servers
        // call the exit api
        exitok = stagingClient.exit(2, leader);
        // update expected server number
        if (leader && exitok)
        {
          // only reset the expected number to stage
          // once by the leader rank
          stagingClient.updateExpectedProcess("leave", 2);
          needadd = true;
        }
      }
      else
      {
        // add two servers back if we exit it before
        // just write the configure files
        if (needadd && leader)
        {
          // add servers back
          spdlog::info("try to add one process at step {}", step);
          stagingClient.updateExpectedProcess("join", 1);

          // it looks using the system call does not trigger things (not sure the reason)
          // even if the existing process exist, so we still use the config file

          // std::string startStagingCommand = "/bin/bash ./addprocess.sh";
          // std::string command = startStagingCommand + " " + std::to_string(1);
          //  use systemcall to start ith server
          // spdlog::info("Add server by command: {}", command);
          // std::system(command.c_str());

          // get the env about the leaveconfig path
          std::string leaveConfigPath = getenv("LEAVECONFIGPATH");
          // the LEAVECONFIGPATH contains the
          static std::string leaveFileName =
            leaveConfigPath + "clientleave.config" + std::to_string(procRank);
          // std::cout << "leaveFileName is " << leaveFileName << std::endl;
          std::ofstream leaveFile;
          leaveFile.open(leaveFileName);
          leaveFile << "test\n";
          leaveFile.close();
          // trigger a service then break
          spdlog::info("send leave signal step {} rank {}", step, procRank);
          
          // set it back after add
          needadd = false;
        }
      }
    }

    // try to add the server back to the data staging

    // syncSimOperation

    // for step 0, the sim is synced before the while loop
    auto syncSimStart = tl::timer::wtime();
    spdlog::info("start sync for iteration {} ", step);
    // TODO return the mona comm and get the step we should process
    // get the step that needs to be processes after the sync operation
    // the step should be syncronized from the leader
    // we can get this message based on synced mona
    controller.m_controller_client->sync(step, leader,procRank);
    // ok to sync the sim program

    // the controller.m_controller_client->m_mona_comm is updated here
    controller.m_controller_client->getMonaComm(mona);

    // mona comm check size and procs
    mona_comm_size(controller.m_controller_client->m_mona_comm, &procSize);
    mona_comm_rank(controller.m_controller_client->m_mona_comm, &procRank);

    auto syncSimEnd = tl::timer::wtime();

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG);

    if (leader)
    {
      spdlog::info("iteration {} : rank={}, size={} simsynctime {}", step, procRank, procSize,
        syncSimEnd - syncSimStart);
    }

    // compute part
    // to do some actual work based on current info
    // for the mb case, caculate how many data blocks should be maintaind in current size
    auto caculateStart = tl::timer::wtime();

    // synthetic compurte
    sleep(3);
    auto caculateEnd2 = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} compute time is {}", step, caculateEnd2 - caculateStart);
    }

    // waitTime
    auto waitSart = tl::timer::wtime();

    for (int i = 0; i < asyncResponses.size(); i++)
    {
      // wait the execution finish
      int ret = asyncResponses[i].wait();
      if (ret != 0)
      {
        throw std::runtime_error("failed for execution");
      }
    }

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG);
    auto waitEend = tl::timer::wtime();

    // all process should record the waitTime, this is different compared with previous evaluation
    lastWaitTime = waitEend - waitSart;
    std::cout << "step " << step << " debug lastWaitTime " << lastWaitTime << std::endl;
    if (leader)
    {
      double stageComputation = waitEend - lastComputeSubmit;
      spdlog::info("wait time for iteration {}: {} total stage computation {} ", step,
        waitEend - waitSart, stageComputation);

      // TODO try to record the data for model using
      // only record it from the second iteration, there is no staging operation at the first one
      if (step > 0)
      {
        spdlog::info("iteration {} recordData {}, {}", step, stagingClient.m_stagingView.size(),
          stageComputation);

        // the size is still the last recorded size of staging part
        // this is used for the model part to collect the historical data
        controller.m_dmpc.recordData(
          "staging", stageComputation, stagingClient.m_stagingView.size());
      }
    }

    // we can start rescale and syncstage when execute finish
    // start to sync the staging service
    auto syncStageStart = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("start syncstage for step {}", step);

      stagingClient.leadersync(step);
    }
    // use the new updated client comm
    stagingClient.workerSyncMona(leader, step, rank, controller.m_controller_client->m_mona_comm);

    // this may takes long time for first step
    // make sure all servers do same things

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG);

    // mona comm bcast
    auto syncStageEnd = tl::timer::wtime();
    if (leader)
    {
      spdlog::info("iteration {} syncstage time is {} ", step, syncStageEnd - syncStageStart);
    }

    // colza stage
    auto stageStart = tl::timer::wtime();

    // synthetic data transfer operation
    sleep(1);

    // the execute can be async and we wait the execute finish before the next sync
    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG);

    auto stageEnd = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} stage time {}", step, stageEnd - stageStart);
    }
    // start the execute
    // asyncResponses = stagingClient.execute(step, dataSetName, leader);
    lastComputeSubmit = tl::timer::wtime();

    // synthetic execution operation

    if (rank == 0)
    {
      step++;
    }

    // sync the step
    auto syncStep = tl::timer::wtime();

    ret = mona_comm_bcast(
      controller.m_controller_client->m_mona_comm, &step, sizeof(step), 0, MONA_TESTSIM_BCAST_TAG);

    auto syncStepEnd = tl::timer::wtime();

    // this time is trival
    spdlog::debug("get the step value {} from the master takes {}", step, syncStepEnd - syncStep);

    if (ret != NA_SUCCESS)
    {
      throw std::runtime_error("failed to execute bcast to get step value");
    }
  }

  // wait the last execution after the remove operation
  for (int i = 0; i < asyncResponses.size(); i++)
  {
    // wait the execution finish
    int ret = asyncResponses[i].wait();
    if (ret != 0)
    {
      throw std::runtime_error("failed for execution wait for last iteration");
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

  spdlog::debug("all things are finalized");
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
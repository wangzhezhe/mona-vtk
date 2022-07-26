
/*
 dynamic sim + dynamic staging based on leader worker mechanism
 */
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include "Controller.hpp"
#include "TypeSizes.hpp"
#include "mb.hpp"
#include "pipeline/StagingClient.hpp"
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

#define MONA_TESTSIM_BARRIER_TAG1 2016
#define MONA_TESTSIM_BARRIER_TAG2 2017
#define MONA_TESTSIM_BARRIER_TAG3 2018
#define MONA_TESTSIM_BARRIER_TAG4 2019
#define MONA_TESTSIM_BARRIER_TAG5 2020
#define MONA_TESTSIM_BARRIER_TAG6 2021


#define MONA_TESTSIM_BCAST_TAG1 2022
#define MONA_TESTSIM_BCAST_TAG2 2023
#define MONA_TESTSIM_BCAST_TAG3 2024


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
  // the rank of new joined one can also be zero
  // so we use two condition to make sure there is unique leader
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

  // TODO maybe get some information from the controller provide
  // the expected process info should be required from the leader process

  MPI_Barrier(MPI_COMM_WORLD);

  // create the controller
  // the rank 0 is not added into the worker
  // the worker size is procs-1 for init stage
  // the procs+g_num_initial_join only works for leader
  Controller controller(globalServerEnginePtr.get(), std::string(mona_addr_buf.data()), 0,
    procs + g_num_initial_join, controllerProvider.m_leader_meta.get(),
    controllerProvider.m_common_meta.get(), g_sim_leader_file, rank);

  if(leader){
    // for the leader process, store its m_leader_mona_addr
    controller.m_controller_client->m_common_meta->m_leader_mona_addr = std::string(mona_addr_buf.data());
  }


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
  double lastExecuteTime = 0;
  double lastComputeTime = 0;
  while (step < g_num_iterations)
  {
    // switch the process functionality
    // we do nothing at the first step here
    // remove process here (this should leave after the execution) of previous step
    // avaliable slot also decrease, since at the first two step, we let it to be the 1
    // the upper bound here for dynamic anticipation should be g_num_initial_join-2
    // spdlog::info("start dynamicLeave for iteration {} ", step);

    /* elasticity opertaion*/
    if (step != 0)
    {

      // if (leader)
      //{
      spdlog::info(
        "step {} lastComputeTime {} lastExecuteTime {}", step, lastComputeTime, lastExecuteTime);
      //}

      // the case to add new staging services
      if (step >= 1 && ((lastExecuteTime - lastComputeTime) > 1.0))
      {
        spdlog::info("step {} consider sim leave", step);
        // the leader node do not leave forever
        // only the new joined node might be leave
        // after the leave opeartion of the simulation process
        // it sends the signal to add a new coresponding staging server
        // this is the static way

        // TODO we need a better way
        // to set an bound here for remsining process that can leave
        if (procSize > 32)
        {
          leave = controller.naiveLeave2(step, 1, procSize, procRank);
        }

        // leave = controller.dynamicLeave(
        //   step, stagingClient.m_stagingView.size(), procSize, procRank, avalibleDynamicProcess);
        //  remember add the bcast back when using the dynamic leave

        // bcast the updated avalibleDynamicProcess to all other processes
        // for next round to do the dynamicLeave, there are same view about the
        // avalibleDynamicProcess
        if (step > 0)
        {
          mona_comm_bcast(controller.m_controller_client->m_mona_comm, &avalibleDynamicProcess,
            sizeof(avalibleDynamicProcess), 0, MONA_TESTSIM_BCAST_TAG1);
        }

        // exit one sim process and start one staging service
        if (leave)
        {
          // write a file and the colza server can start
          // trigger another server before leave
          spdlog::info("leave sim operation at step {} rank {}", step, procRank);
          stagingClient.updateExpectedProcess("join", 1);
          // TODO, also need to notify the sim program that the expected number
          // decrease one here

          // it looks using the system call does not trigger things (not sure the reason)
          // even if the existing process exist, so we still use the config file

          // std::string startStagingCommand = "/bin/bash ./addprocess.sh";
          // std::string command = startStagingCommand + " " + std::to_string(1);
          //  use systemcall to start ith server
          // spdlog::info("Add server by command: {}", command);
          // std::system(command.c_str());

          // get the env about the leaveconfig path
          std::string leaveConfigPath = getenv("CONFIGPATH");
          // the LEAVECONFIGPATH contains the
          static std::string leaveFileName =
            leaveConfigPath + "clientleave.config" + std::to_string(procRank);
          std::cout << "leaveFileName is " << leaveFileName << std::endl;
          std::ofstream leaveFile;
          leaveFile.open(leaveFileName);
          // leaveFile << "test\n";
          leaveFile.close();
          // trigger a service then break
          spdlog::info("send leave signal step {} rank {}", step, procRank);
          break;
        }
      }

      // the case to remove the staging services
      // computation time is longer than processing time
      // how to compute the staging processing time
      // in the async call
      // maybe return the time it consumed in async call
      // there is one mismatch between leader and worker for step here
      // the better condition should be that both lastComputeTime and lastExecuteTime is larger than
      // zero
      if (leader && step >= 2 && ((lastComputeTime - lastExecuteTime) > 1.0))
      {
        // free one process
        // exit one data staging service
        // and then give it back to the simulation program
        /*static exit staging service*/
        // add a bound here
        spdlog::info("add sim operation at step {} ", step);

        int stagingSize = stagingClient.m_stagingView.size();

        // make the lower bound as the 2 for staging service
        if (stagingSize > 2)
        {
          int addnum = 1;
          bool exitok = stagingClient.exit(addnum, leader);
          // update expected server number
          if (leader && exitok)
          {
            // only reset the expected number to stage
            // once by the leader rank
            // the expected data staging process
            // need to decrease one for the staging service
            stagingClient.updateExpectedProcess("leave", 1);
            // need to add one for the sim for metadata, there is one pending for sim
            controller.m_controller_client->expectedUpdatingProcess(1);

            // send a siganal to start a new sim process
            std::string addConfigPath = getenv("CONFIGPATH");
            // the LEAVECONFIGPATH contains the
            for (int i = 0; i < addnum; i++)
            {
              static std::string addFileName =
                addConfigPath + "clientadd.config" + std::to_string(i);
              std::cout << "addFileName is " << addFileName << std::endl;
              std::ofstream addFile;
              addFile.open(addFileName);
              // addFile << "testadd\n";
              addFile.close();
              // trigger a service then break
              spdlog::info("send add sim signal at step {}", step);
            }
          }
        }
      }
    }

    // syncSimOperation
    // for step 0, the sim is synced before the while loop
    auto syncSimStart = tl::timer::wtime();
    spdlog::info("start sync for iteration {} ", step);
    // TODO return the mona comm and get the step we should process
    // get the step that needs to be processes after the sync operation
    // the step should be syncronized from the leader
    // we can get this message based on synced mona
    controller.m_controller_client->sync(step, leader, procRank);
    // ok to sync the sim program
    spdlog::debug("sync ok for iteration {} ", step);

    // the controller.m_controller_client->m_mona_comm is updated here
    controller.m_controller_client->getMonaComm(mona);
    spdlog::debug("getMonaComm ok for iteration {} ", step);

    // mona comm check size and procs
    mona_comm_size(controller.m_controller_client->m_mona_comm, &procSize);
    mona_comm_rank(controller.m_controller_client->m_mona_comm, &procRank);

    auto syncSimEnd = tl::timer::wtime();

    spdlog::debug("iteration {} : newrank={}, size={} ", step, procRank, procSize);

    //it hangs there sometimes, there are issues in getting Mona Comm?
    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG1);

    if (leader)
    {
      //the rank of the leader may change after adding the new process
      //it is not always the 0
      //how does other ranks knows the leader rank?
      spdlog::info("iteration {} : newrank={}, size={} simsynctime {}", step, procRank, procSize,
        syncSimEnd - syncSimStart);
    }

    // sync the step firstly before the computation
    // there might be new added sim processes
    auto syncStep = tl::timer::wtime();
    spdlog::debug("current step value {} ", step);

    ret = mona_comm_bcast(
      controller.m_controller_client->m_mona_comm, &step, sizeof(step), 0, MONA_TESTSIM_BCAST_TAG2);
    if (ret != NA_SUCCESS)
    {
      throw std::runtime_error("failed to execute bcast to get step value");
    }

    auto syncStepEnd = tl::timer::wtime();

    // this time is trival
    spdlog::debug("get the step value {} from the master takes {}", step, syncStepEnd - syncStep);

    // compute part
    // to do some actual work based on current info
    // for the mb case, caculate how many data blocks should be maintaind in current size
    auto caculateStart = tl::timer::wtime();
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
    // init the list
    // there some issues about using the procRank here
    // we use the mona rank

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
      blockid = blockid + rank_offset;
    }
    auto caculateEnd1 = tl::timer::wtime();

    // compute list (this is time consuming part)
    std::vector<tl::managed<tl::thread> > ths;
    for (int i = 0; i < nblocks_per_proc; i++)
    {
      // tl::managed<tl::thread> th = globalServerEnginePtr->get_handler_pool().make_thread(
      // [&]() { MandelbulbList[i].compute(order); });
      // ths.push_back(std::move(th));
      MandelbulbList[i].compute(order);
    }
    // how to wait the finish of this thread?
    // for (auto& mth : ths)
    //{
    //  mth->join();
    //}

    // add synthetic time to adjust the simulation computation time
    //if (step >= 5 && step <= 9)
    if (step >= 4 && step <= 5)
    {
      sleep(8);
    }

    // wait finish
    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG2);
    auto caculateEnd2 = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} nblocks_per_proc {} compute time is {} and {}, sum {}", step,
        nblocks_per_proc, caculateEnd2 - caculateEnd1, caculateEnd1 - caculateStart,
        caculateEnd2 - caculateStart);
    }

    lastComputeTime = caculateEnd2 - caculateStart;

    // waitTime
    auto waitSart = tl::timer::wtime();

    lastExecuteTime = 0;
    for (int i = 0; i < asyncResponses.size(); i++)
    {
      // wait the execution finish
      // record the max execute time
      double tempExectime = asyncResponses[i].wait();
      lastExecuteTime = std::max(lastExecuteTime, tempExectime);
    }

    // only the leader have the correct lastExecuteTime
    mona_comm_bcast(controller.m_controller_client->m_mona_comm, &lastExecuteTime,
      sizeof(lastExecuteTime), 0, MONA_TESTSIM_BCAST_TAG3);

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG3);
    auto waitEend = tl::timer::wtime();

    // all process should record the waitTime, this is different compared with previous evaluation
    lastWaitTime = waitEend - waitSart;
    // std::cout << "step " << step << " debug lastWaitTime " << lastWaitTime << std::endl;
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

    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG4);

    // mona comm bcast
    auto syncStageEnd = tl::timer::wtime();
    if (leader)
    {
      spdlog::info("iteration {} syncstage time is {} ", step, syncStageEnd - syncStageStart);
    }

    // colza stage
    auto stageStart = tl::timer::wtime();

    std::string dataSetName = "mydata";

    for (int i = 0; i < MandelbulbList.size(); i++)
    {

      // stage the data at current iteration
      int* extents = MandelbulbList[i].GetExtents();
      // the extends value is from 0 to 29
      // the dimension value should be extend value +1
      // the sequence is depth, height, width
      // there is +1 in the depth direaction
      // the actual data is 1 larger for data allocation
      std::vector<size_t> dimensions = { int2size_t(*(extents + 1)) + 1,
        int2size_t(*(extents + 3)) + 1, int2size_t(*(extents + 5)) + 1 };
      std::vector<int64_t> offsets = { MandelbulbList[i].GetZoffset(), 0, 0 };

      auto type = Type::INT32;
      stagingClient.stage(dataSetName, step, MandelbulbList[i].GetBlockID(), dimensions, offsets,
        type, MandelbulbList[i].GetData(), procRank);
      /*
      std::cout << "step " << step << " blockid " << blockid << " dimentions " << dimensions[0]
                << "," << dimensions[1] << "," << dimensions[2] << " offsets " << offsets[0]
                << "," << offsets[1] << "," << offsets[2] << " listvalueSize "
                << MandelbulbList[i].DataSize() << std::endl;
      */
    }

    // the execute can be async and we wait the execute finish before the next sync
    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG5);

    auto stageEnd = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} stage time {}", step, stageEnd - stageStart);
    }
    // start the execute
    // the responses here is a vector
    asyncResponses = stagingClient.execute(step, dataSetName, leader);

    lastComputeSubmit = tl::timer::wtime();

    // colza execute
    // stagingClient.execute(step);

    // colza cleanup
    // stagingClient.cleanup(step);

    // sync the step

    if (leader)
    {
      spdlog::debug("current step {} newstep {}", step, step + 1);
      step = step + 1;
    }

    // make sure the master step is same for all proc
    mona_comm_barrier(controller.m_controller_client->m_mona_comm, MONA_TESTSIM_BARRIER_TAG6);
  }

  // wait the last execution after the remove operation
  for (int i = 0; i < asyncResponses.size(); i++)
  {
    // wait the execution finish
    asyncResponses[i].wait();
    // int ret = asyncResponses[i].wait();
    // if (ret != 0)
    //{
    //   throw std::runtime_error("failed for execution wait for last iteration");
    // }
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

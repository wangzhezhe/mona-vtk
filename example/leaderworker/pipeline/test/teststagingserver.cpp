/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "../StagingProvider.hpp"

#include <margo.h>
#include <mercury.h>

#ifdef USE_GNI
extern "C"
{
#include <rdmacred.h>
}
#endif

namespace tl = thallium;

static std::string g_address = "na+sm";
static int g_num_threads = 0;
static std::string g_log_level = "info";
static std::string g_config_file = "";
static bool g_join = false;
static int g_join_num = 0;

static unsigned g_swim_period_ms = 1000;
static int64_t g_drc_credential = -1;
static std::string g_drc_file = "dynamic_drc.config";
static std::string g_server_leader_config = "dynamic_server_leader.config";

static void parse_command_line(int argc, char** argv);
static int64_t setup_credentials();
static uint32_t get_credential_cookie(int64_t credential_id);

int main(int argc, char** argv)
{
  parse_command_line(argc, argv);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  // Initialize MPI
  MPI_Init(&argc, &argv);
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  uint32_t cookie = 0;
  if (!g_join)
  {
    g_drc_credential = g_drc_credential == -1 ? setup_credentials() : g_drc_credential;
    spdlog::trace("Credential id is {}", g_drc_credential);
    cookie = get_credential_cookie(g_drc_credential);
    if (rank == 0)
    {
      // also write the g_drc_file for sim using
      g_drc_file = "dynamic_drc.config";
      std::ofstream credFile;
      credFile.open(g_drc_file);
      credFile << g_drc_credential << "\n";
      credFile.close();
    }
  }
  // add barrier to wait the file write finish
  MPI_Barrier(MPI_COMM_WORLD);
  if (g_join || rank != 0)
  {
    // load the cred from the file
    std::ifstream infile(g_drc_file);
    std::string cred_id;
    std::getline(infile, cred_id);

    std::cout << "load cred_id: " << cred_id << std::endl;
    g_drc_credential = (uint32_t)atoi(cred_id.c_str());
    spdlog::trace("Credential id read from config file: {}", g_drc_credential);
    if (g_drc_credential != -1)
      cookie = get_credential_cookie(g_drc_credential);
  }

  hg_init_info hii;
  memset(&hii, 0, sizeof(hii));
  std::string cookie_str = std::to_string(cookie);
  if (g_drc_credential != -1)
    hii.na_init_info.auth_key = cookie_str.c_str();

  // the is HG_INVALID_ARG if we set thread number as value that is larger than 1...
  tl::engine engine(g_address, THALLIUM_SERVER_MODE, true, g_num_threads, &hii);
  engine.enable_remote_shutdown();

  bool leader = false;

  if (g_join == false && rank == 0)
  {
    spdlog::debug("rank {} is leader", rank);
    leader = true;
    // record the leader addr
    std::ofstream leaderFile;
    leaderFile.open(g_server_leader_config);
    // write the thallium engine into the file
    leaderFile << (std::string)engine.self() << "\n";
    leaderFile.close();
  }

  MPI_Barrier(MPI_COMM_WORLD);

  // Create Mona instance
  mona_instance_t mona = mona_init_thread(g_address.c_str(), NA_TRUE, &hii.na_init_info, NA_TRUE);

  // add call back after initilization
  engine.push_prefinalize_callback([mona]() {
    spdlog::info("Engine finalized, now finalizing MoNA...");
    mona_finalize(mona);
    spdlog::info("MoNA finalized");
  });

  // Print MoNA address for information
  na_addr_t mona_addr;
  mona_addr_self(mona, &mona_addr);
  std::vector<char> mona_addr_buf(256);
  na_size_t mona_addr_size = 256;
  mona_addr_to_string(mona, mona_addr_buf.data(), &mona_addr_size, mona_addr);
  spdlog::debug("MoNA address is {}", mona_addr_buf.data());
  mona_addr_free(mona, mona_addr);

  // Read config file
  /*
  std::string config;
  if (!g_config_file.empty())
  {
    std::ifstream t(g_config_file.c_str());
    config = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
  }
  */

  // the client will use the default provider if it did not call server based on provider handle
  std::string stagingScript = "./mbrender_64_iso.py";
  // std::string stagingScript = "./gridwriter.py";
  StagingProvider stageprovider(engine, 0, leader, mona, std::string(mona_addr_buf.data()),
    g_server_leader_config, stagingScript);

  // set the expected addr, this is important
  if (leader)
  {
    stageprovider.setExpectedNum(size + g_join_num);
    // the leader add into to map direactly
    stageprovider.addLeaderAddr((std::string)engine.self(), std::string(mona_addr_buf.data()));
  }
  MPI_Barrier(MPI_COMM_WORLD);
  if (leader == false)
  {
    // the leader do not send things to itself, it is supposed to recieve n-1 addr
    stageprovider.addrRegister();
  }

  spdlog::info("Server running at address {}", (std::string)engine.self());

  engine.wait_for_finalize();

  MPI_Finalize();

  return 0;
}

void parse_command_line(int argc, char** argv)
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
    TCLAP::ValueArg<std::string> configFile("c", "config", "config file name", false, "", "string");
    TCLAP::SwitchArg joinGroup("j", "join", "Join an existing group rather than create it", false);
    TCLAP::ValueArg<unsigned> swimPeriod(
      "p", "swim-period-length", "Length of the SWIM period in milliseconds", false, 1000, "int");

    TCLAP::ValueArg<unsigned> gjoinNum(
      "g", "gjoin_number", "gjoin number for testing for initilization", false, 0, "int");

    cmd.add(addressArg);
    cmd.add(numThreads);
    cmd.add(logLevel);
    cmd.add(configFile);
    cmd.add(joinGroup);
    cmd.add(swimPeriod);
    cmd.add(gjoinNum);

    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_num_threads = numThreads.getValue();
    g_log_level = logLevel.getValue();
    g_config_file = configFile.getValue();
    g_join = joinGroup.getValue();
    g_swim_period_ms = swimPeriod.getValue();
    g_join_num = gjoinNum.getValue();
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

int64_t setup_credentials()
{
  uint32_t drc_credential_id = -1;
#ifdef USE_GNI
  if (g_address.find("gni") == std::string::npos)
    return -1;

  int rank;
  int ret;

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0)
  {
    ret = drc_acquire(&drc_credential_id, DRC_FLAGS_FLEX_CREDENTIAL);
    if (ret != DRC_SUCCESS)
    {
      spdlog::critical("drc_acquire failed (ret = {})", ret);
      exit(-1);
    }
  }

  MPI_Bcast(&drc_credential_id, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);

  if (rank == 0)
  {
    ret = drc_grant(drc_credential_id, drc_get_wlm_id(), DRC_FLAGS_TARGET_WLM);
    if (ret != DRC_SUCCESS)
    {
      spdlog::critical("drc_grant failed (ret = {})", ret);
      exit(-1);
    }
    spdlog::info("DRC credential id: {}", drc_credential_id);
  }

#endif
  return drc_credential_id;
}

uint32_t get_credential_cookie(int64_t credential_id)
{
  uint32_t drc_cookie = 0;

  if (credential_id < 0)
    return drc_cookie;
  if (g_address.find("gni") == std::string::npos)
    return drc_cookie;

#ifdef USE_GNI

  drc_info_handle_t drc_credential_info;
  int ret;

  ret = drc_access(credential_id, 0, &drc_credential_info);
  if (ret != DRC_SUCCESS)
  {
    spdlog::critical("drc_access failed (ret = {})", ret);
    exit(-1);
  }

  drc_cookie = drc_get_first_cookie(drc_credential_info);

#endif
  return drc_cookie;
}
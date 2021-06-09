/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <colza/Provider.hpp>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <ssg-mpi.h>
#include <tclap/CmdLine.h>
#include <vector>

namespace tl = thallium;

static std::string g_address = "na+sm";
static int g_num_threads = 0;
static std::string g_log_level = "info";
static std::string g_ssg_file = "";
static std::string g_config_file = "";
static bool g_join = false;
// const std::string credFileName = "global_cred_conf";

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
      exit(EXIT_FAILURE);                                                                          \
    }                                                                                              \
  } while (0)
#endif

static void parse_command_line(int argc, char** argv);

int main(int argc, char** argv)
{
  parse_command_line(argc, argv);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  // Initialize MPI
  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Initialize SSG
  // ssg handles the gni case
  int ret = ssg_init();
  if (ret != SSG_SUCCESS)
  {
    std::cerr << "Could not initialize SSG" << std::endl;
    exit(-1);
  }

#ifdef USE_GNI
  uint32_t drc_credential_id;
  drc_info_handle_t drc_credential_info;
  uint32_t drc_cookie;
  char drc_key_str[256] = { 0 };
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));

  // init the DRC
  /* acquire DRC cred on MPI rank 0 */
  if (rank == 0)
  {
    std::cout << "use protocol " << g_address << std::endl;
    ret = drc_acquire(&drc_credential_id, DRC_FLAGS_FLEX_CREDENTIAL);
    DIE_IF(ret != DRC_SUCCESS, "drc_acquire");
    std::cout << "get drc_credential_id " << drc_credential_id << std::endl;
  }
  MPI_Bcast(&drc_credential_id, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);

  /* access credential on all ranks and convert to string for use by mercury */
  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access");
  drc_cookie = drc_get_first_cookie(drc_credential_info);
  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  /* rank 0 grants access to the credential, allowing other jobs to use it */
  if (rank == 0)
  {
    ret = drc_grant(drc_credential_id, drc_get_wlm_id(), DRC_FLAGS_TARGET_WLM);
    DIE_IF(ret != DRC_SUCCESS, "drc_grant");

    // try to use an separate file to write the drc
    // std::ofstream credFile;
    // credFile.open(credFileName);
    // credFile << drc_credential_id << "\n";
    // credFile.close();
  }

  /* init margo */
  /* use the main xstream to drive progress & run handlers */

  tl::engine engine("ofi+gni", THALLIUM_SERVER_MODE, true, g_num_threads, &hii);
  engine.enable_remote_shutdown();
#else
  if (rank == 0)
  {
    std::cout << "use protocol " << g_address << std::endl;
  }

  tl::engine engine(g_address, THALLIUM_SERVER_MODE, true, g_num_threads);
  engine.enable_remote_shutdown();
#endif

  ssg_group_id_t gid;
  if (!g_join)
  {
    // Create SSG group using MPI
    ssg_group_config_t group_config = SSG_GROUP_CONFIG_INITIALIZER;
    group_config.swim_period_length_ms = 1000;
    // group_config.swim_period_length_ms = 10000;
    group_config.swim_suspect_timeout_periods = 3;
    group_config.swim_subgroup_member_count = 1;
    // set the credential information
#ifdef USE_GNI
    group_config.ssg_credential = (int64_t)drc_credential_id; /* DRC credential for group */
#endif

    ssg_group_create_mpi(
      engine.get_margo_instance(), "mygroup", MPI_COMM_WORLD, &group_config, nullptr, nullptr, &gid);
  }
  else
  {
    int num_addrs = SSG_ALL_MEMBERS;
    ret = ssg_group_id_load(g_ssg_file.c_str(), &num_addrs, &gid);
    if (ret != SSG_SUCCESS)
    {
      std::cerr << "Could not load group id from file" << std::endl;
      exit(-1);
    }
    ret = ssg_group_join(engine.get_margo_instance(), gid, nullptr, nullptr);
    if (ret != SSG_SUCCESS)
    {
      std::cerr << "Could not join SSG group" << std::endl;
      exit(-1);
    }
  }
  engine.push_prefinalize_callback([]() { ssg_finalize(); });

  // Write SSG file
  if (rank == 0 && !g_ssg_file.empty() && !g_join)
  {
    int ret = ssg_group_id_store(g_ssg_file.c_str(), gid, SSG_ALL_MEMBERS);
    if (ret != SSG_SUCCESS)
    {
      spdlog::critical("Could not store SSG file {}", g_ssg_file);
      exit(-1);
    }
  }

  // Create Mona instance
  mona_instance_t mona = mona_init(g_address.c_str(), NA_TRUE, NULL);

  // Print MoNA address for information
  na_addr_t mona_addr;
  mona_addr_self(mona, &mona_addr);
  std::vector<char> mona_addr_buf(256);
  na_size_t mona_addr_size = 256;
  mona_addr_to_string(mona, mona_addr_buf.data(), &mona_addr_size, mona_addr);
  spdlog::debug("MoNA address is {}", mona_addr_buf.data());
  mona_addr_free(mona, mona_addr);

  // Read config file
  std::string config;
  if (!g_config_file.empty())
  {
    std::ifstream t(g_config_file.c_str());
    config = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
  }

  // create ESs
  std::vector<tl::managed<tl::xstream> > colza_xstreams;
  tl::managed<tl::pool> managed_colza_pool = tl::pool::create(tl::pool::access::mpmc);
  for (int i = 0; i < g_num_threads; i++)
  {
    colza_xstreams.push_back(
      tl::xstream::create(tl::scheduler::predef::basic_wait, *managed_colza_pool));
  }
  tl::pool colza_pool;
  if (g_num_threads == 0)
  {
    colza_pool = *managed_colza_pool;
  }
  else
  {
    colza_pool = tl::xstream::self().get_main_pools(1)[0];
  }
  engine.push_prefinalize_callback([&colza_xstreams]() {
    for (auto& es : colza_xstreams)
    {
      es->join();
    }
    colza_xstreams.clear();
  });
  // use dedicated colza pool for the provider
  colza::Provider provider(engine, gid, g_join, mona, 0, config, colza_pool);

  spdlog::info("Server running at address {}", (std::string)engine.self());
  engine.wait_for_finalize();

  mona_finalize(mona);

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
    TCLAP::ValueArg<std::string> ssgFile("s", "ssg-file", "SSG file name", false, "", "string");
    TCLAP::ValueArg<std::string> configFile("c", "config", "config file name", false, "", "string");
    TCLAP::SwitchArg joinGroup("j", "join", "Join an existing group rather than create it", false);
    cmd.add(addressArg);
    cmd.add(numThreads);
    cmd.add(logLevel);
    cmd.add(ssgFile);
    cmd.add(configFile);
    cmd.add(joinGroup);
    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_num_threads = numThreads.getValue();
    g_log_level = logLevel.getValue();
    g_ssg_file = ssgFile.getValue();
    g_config_file = configFile.getValue();
    g_join = joinGroup.getValue();

    if (g_num_threads != 1)
    {
      throw std::runtime_error("set the thread number to 1 to avoid the VTK multithread issue");
    }
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <colza/Admin.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <ssg.h>
#include <sstream>
#include <tclap/CmdLine.h>
#include <vector>
#include <fstream>

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

static std::string g_address;
static std::string g_protocol;
static std::string g_pipeline;
static std::string g_type;
static std::string g_config;
static std::string g_library;
static std::string g_token;
static std::string g_operation;
static std::string g_log_level = "info";
static std::string g_ssg_file;

static void parse_command_line(int argc, char** argv);

int main(int argc, char** argv)
{
  parse_command_line(argc, argv);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  ssg_init();

#ifdef USE_GNI

  const std::string credFileName = "global_cred_conf";
  std::ifstream infile(credFileName);
  std::string cred_id;
  std::getline(infile, cred_id);
  std::cout << "load cred_id: " << cred_id << std::endl;

  drc_info_handle_t drc_credential_info;
  uint32_t drc_cookie;
  char drc_key_str[256] = { 0 };
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));

  /*
    std::cout << "g_ssg_file: " << g_ssg_file << std::endl;
    int ret = ssg_group_id_load(g_ssg_file.c_str(), &num_addrs, &g_id);
    DIE_IF(ret != SSG_SUCCESS, "ssg_group_id_load");

    std::cout << "get g_id: " << g_id << std::endl;
    std::cout << "get num_addrs: " << num_addrs << std::endl;

    int64_t ssg_cred = ssg_group_id_get_cred(g_id);
    DIE_IF(ssg_cred == -1, "ssg_group_id_get_cred");
  */

  int ret;
  uint32_t drc_credential_id = (uint32_t)atoi(cred_id.c_str());
  /* access credential and covert to string for use by mercury */
  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access %u %ld", drc_credential_id, cred_id);
  drc_cookie = drc_get_first_cookie(drc_credential_info);
  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  margo_instance_id mid = margo_init_opt(g_protocol.c_str(), MARGO_CLIENT_MODE, &hii, true, 2);
  tl::engine engine(mid);

#else
  // Initialize the thallium server
  tl::engine engine(g_protocol, THALLIUM_SERVER_MODE);

#endif

  try
  {

    // Initialize an Admin
    colza::Admin admin(engine);

    if (g_operation == "create")
    {
      admin.createDistributedPipeline(
        g_ssg_file, 0, g_pipeline, g_type, g_config, g_library, g_token);
      spdlog::info("Created pipeline {}", g_pipeline);
    }
    else if (g_operation == "destroy")
    {
      admin.destroyDistributedPipeline(g_ssg_file, 0, g_pipeline, g_token);
      spdlog::info("Destroyed pipeline {}", g_pipeline);
    }

    // Any of the above functions may throw a colza::Exception
  }
  catch (const colza::Exception& ex)
  {
    std::cerr << ex.what() << std::endl;
    exit(-1);
  }

  engine.finalize();
  ssg_finalize();

  return 0;
}

void parse_command_line(int argc, char** argv)
{
  try
  {
    TCLAP::CmdLine cmd("Colza admin", ' ', "0.1");
    TCLAP::ValueArg<std::string> addressArg(
      "a", "address", "Address or protocol", true, "", "string");
    TCLAP::ValueArg<std::string> tokenArg("q", "token", "Security token", false, "", "string");
    TCLAP::ValueArg<std::string> typeArg(
      "t", "type", "Pipeline type", false, "simple_stager", "string");
    TCLAP::ValueArg<std::string> libraryArg(
      "l", "library", "Dynamic library containing pipeline definition", false, "", "string");
    TCLAP::ValueArg<std::string> pipelineArg("n", "pipeline", "Pipeline name", false, "", "string");
    TCLAP::ValueArg<std::string> configArg(
      "c", "config", "Pipeline configuration", false, "", "string");
    TCLAP::ValueArg<std::string> logLevel("v", "verbose",
      "Log level (trace, debug, info, warning, error, critical, off)", false, "info", "string");
    TCLAP::ValueArg<std::string> ssgFileArg("s", "ssg-file", "SSG file name", true, "", "string");
    std::vector<std::string> options = { "create", "destroy" };
    TCLAP::ValuesConstraint<std::string> allowedOptions(options);
    TCLAP::ValueArg<std::string> operationArg(
      "x", "exec", "Operation to execute", true, "create", &allowedOptions);
    cmd.add(addressArg);
    cmd.add(typeArg);
    cmd.add(tokenArg);
    cmd.add(configArg);
    cmd.add(pipelineArg);
    cmd.add(logLevel);
    cmd.add(libraryArg);
    cmd.add(operationArg);
    cmd.add(ssgFileArg);
    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_library = libraryArg.getValue();
    g_token = tokenArg.getValue();
    g_config = configArg.getValue();
    g_type = typeArg.getValue();
    g_pipeline = pipelineArg.getValue();
    g_operation = operationArg.getValue();
    g_log_level = logLevel.getValue();
    g_protocol = g_address.substr(0, g_address.find(":"));
    g_ssg_file = ssgFileArg.getValue();
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

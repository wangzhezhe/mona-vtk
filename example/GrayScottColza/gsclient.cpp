/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include "gray-scott.h"
#include "settings.h"
#include <colza/Client.hpp>
#include <colza/MPIClientCommunicator.hpp>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <ssg-mpi.h>
#include <ssg.h>

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

static std::string g_pipeline;

static void parse_command_line(int argc, char** argv);

// refer to https://stackoverflow.com/questions/27490762/how-can-i-convert-to-size-t-from-int-safely
size_t int2size_t(int val)
{
  return (val < 0) ? __SIZE_MAX__ : (size_t)((unsigned)val);
}

void print_settings(const Settings& s)
{
  std::cout << "grid:             " << s.L << "x" << s.L << "x" << s.L << std::endl;
  std::cout << "steps:            " << s.steps << std::endl;
  std::cout << "F:                " << s.F << std::endl;
  std::cout << "k:                " << s.k << std::endl;
  std::cout << "dt:               " << s.dt << std::endl;
  std::cout << "Du:               " << s.Du << std::endl;
  std::cout << "Dv:               " << s.Dv << std::endl;
  std::cout << "noise:            " << s.noise << std::endl;
  std::cout << "ssgfile:          " << s.ssgfile << std::endl;
  std::cout << "loglevel:         " << s.loglevel << std::endl;
  std::cout << "protocol:         " << s.protocol << std::endl;
  std::cout << "pipelinename:     " << s.pipelinename << std::endl;
}

void print_simulator_settings(const GrayScott& s)
{
  std::cout << "process layout:   " << s.npx << "x" << s.npy << "x" << s.npz << std::endl;
  std::cout << "local grid size:  " << s.size_x << "x" << s.size_y << "x" << s.size_z << std::endl;
}

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cout << "<binary> <setting.json>" << std::endl;
    exit(0);
  }
  // init settings
  Settings settings = Settings::from_json(argv[1]);
  MPI_Init(&argc, &argv);
  ssg_init();

  MPI_Comm gscomm(MPI_COMM_WORLD);

  int rank, nprocs;

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  GrayScott sim(settings, gscomm);
  sim.init();

  if (rank == 0)
  {
    print_settings(settings);
    print_simulator_settings(sim);
    std::cout << "========================================" << std::endl;
  }

  spdlog::set_level(spdlog::level::from_str(settings.loglevel));

#ifdef USE_GNI

  drc_info_handle_t drc_credential_info;
  uint32_t drc_cookie;
  char drc_key_str[256] = { 0 };
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));
  int64_t drc_credential_id;
  /*
  if (rank == 0)
  {
    std::ifstream infile(credFileName);
    std::string cred_id;
    std::getline(infile, cred_id);
    std::cout << "load cred_id: " << cred_id << std::endl;
    drc_credential_id = (uint32_t)atoi(cred_id.c_str());
  }
  MPI_Bcast(&drc_credential_id, 1, MPI_UINT32_T, 0, MPI_COMM_WORLD);
  */

  // get the cred id by ssg
  int num_addrs = SSG_ALL_MEMBERS;
  ssg_group_id_t g_id;
  int ret = ssg_group_id_load(settings.ssgfile.c_str(), &num_addrs, &g_id);
  DIE_IF(ret != SSG_SUCCESS, "ssg_group_id_load");
  if (rank == 0)
  {
    std::cout << "get num_addrs: " << num_addrs << std::endl;
  }

  ret = ssg_group_id_get_cred(g_id, &drc_credential_id);
  DIE_IF(ret != SSG_SUCCESS, "ssg_group_id_get_cred");
  if (rank == 0)
  {
    std::cout << "get drc_credential_id: " << drc_credential_id << std::endl;
  }
  /* access credential and covert to string for use by mercury */
  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access %u %ld", ret, drc_credential_id);
  drc_cookie = drc_get_first_cookie(drc_credential_info);
  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  if (settings.protocol != "gni")
  {
    throw std::runtime_error("the gni should be used");
  }

  tl::engine engine("ofi+gni", THALLIUM_CLIENT_MODE, true, 2, &hii);

#else
  // Initialize the thallium client
  if (settings.protocol == "gni")
  {
    throw std::runtime_error("the USE_GNI shoule be enabled for compiling");
  }
  tl::engine engine(settings.protocol, THALLIUM_CLIENT_MODE);

#endif

  try
  {
    colza::MPIClientCommunicator colzacomm(MPI_COMM_WORLD);
    // use another ColzaClient
    // Initialize a Client
    colza::Client client(engine);
    // Open distributed pipeline from provider 0
    colza::DistributedPipelineHandle pipeline =
      client.makeDistributedPipelineHandle(&colzacomm, settings.ssgfile, 0, settings.pipelinename);

    for (int step = 0; step < settings.steps; step++)
    {
      // start iteration
      // compute stage
      sim.iterate();

      // the join and leave may happens here
      // this should be called after the compute process
      pipeline.start(step);

      // generate the datablock and put the data
      spdlog::trace("Calling stage {}", step);

      // make sure the pipeline start is called by every process
      // before the stage call
      MPI_Barrier(MPI_COMM_WORLD);

      double stageStart = tl::timer::wtime();
      
      // the sim.npx  sim.npy  sim.npz labels how many process at each dimention not the actual cell dims
      std::vector<size_t> dimensions = { sim.size_x, sim.size_y, sim.size_z };

      // the offset is used to label the current position in global domain
      // the offset means the lower bound of the current grid
      std::vector<int64_t> offsets = { (int64_t)sim.offset_x, (int64_t)sim.offset_y, (int64_t)sim.offset_z };

      // for this simulation, the global domain is fixed
      // if we have one rank, the whole domain is processed by this one rank
      // if we have two ranks, every process generates half of the data
      int blockid = rank;

      auto type = colza::Type::FLOAT64;

      int32_t result;
      pipeline.stage(
        "grayscottu", step, blockid, dimensions, offsets, type, sim.u_noghost().data(), &result);

      if (result != 0)
      {
        throw std::runtime_error(
          "failed to stage " + std::to_string(step) + " return status " + std::to_string(result));
      }

      double stageEnd = tl::timer::wtime();
      if (rank == 0)
      {
        std::cout << "rank " << rank << " stage time " << stageEnd - stageStart << std::endl;
      }
      MPI_Barrier(MPI_COMM_WORLD);

      spdlog::trace("Calling execute {}", step);

      double exeStart = tl::timer::wtime();
      // execute the pipeline
      // TODO double check this part, if multiple clients call this, the execute function at the server end
      // is called only one time???
      pipeline.execute(step);
      double exeEnd = tl::timer::wtime();
      if (rank == 0)
      {
        // only care about the rank0
        std::cout << "rank " << rank << " execution time " << exeEnd - exeStart << std::endl;
      }

      MPI_Barrier(MPI_COMM_WORLD);
      spdlog::trace("Calling cleanup {}", step);

      // clean up the data for every time step?
      // cleanup the pipeline
      // the clean up operation is decided by the backend
      pipeline.cleanup(step);
    }

    spdlog::trace("Done");
  }
  catch (const colza::Exception& ex)
  {
    std::cerr << ex.what() << std::endl;
    exit(-1);
  }
  spdlog::trace("Finalizing engine");

  engine.finalize();

  spdlog::trace("Finalizing SSG");
  ssg_finalize();

  spdlog::trace("Finalizing MPI");
  MPI_Finalize();

  return 0;
}

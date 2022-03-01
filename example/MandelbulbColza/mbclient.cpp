/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include "mb.hpp"
#include <colza/Client.hpp>
#include <colza/MPIClientCommunicator.hpp>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <ssg-mpi.h>
#include <ssg.h>
#include <tclap/CmdLine.h>

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
static std::string g_pipeline;
static std::string g_log_level = "info";
static std::string g_ssg_file;
static int g_total_block_number;
static int g_total_step;
static int g_block_width;
static int g_block_depth;
static int g_block_height;

static void parse_command_line(int argc, char** argv);
static uint32_t get_credentials_from_ssg_file();

size_t int2size_t(int val)
{
  return (val < 0) ? __SIZE_MAX__ : (size_t)((unsigned)val);
}

int main(int argc, char** argv)
{
  parse_command_line(argc, argv);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  MPI_Init(&argc, &argv);

  ssg_init();

  colza::MPIClientCommunicator comm(MPI_COMM_WORLD);
  int rank = comm.rank();
  int nprocs = comm.size();

  if (rank == 0)
  {
    std::cout << "-------key varaibles--------" << std::endl;
    std::cout << "g_total_block_number:" << g_total_block_number << std::endl;
    std::cout << "g_total_step:" << g_total_step << std::endl;
    std::cout << "g_block_width:" << g_block_width << std::endl;
    std::cout << "g_block_depth:" << g_block_depth << std::endl;
    std::cout << "g_block_height:" << g_block_height << std::endl;
    std::cout << "g_pipeline:" << g_pipeline << std::endl;
    std::cout << "g_address:" << g_address << std::endl;
    std::cout << "----------------------------" << std::endl;
  }

  uint32_t cookie = get_credentials_from_ssg_file();

  hg_init_info hii;
  memset(&hii, 0, sizeof(hii));
  std::string cookie_str = std::to_string(cookie);
  if (cookie != 0)
    hii.na_init_info.auth_key = cookie_str.c_str();

  // Initialize the thallium server
  tl::engine engine(g_address, THALLIUM_SERVER_MODE, false, 0, &hii);

  // create the mandelbulb list
  if (g_total_step == 0 || g_total_block_number == 0)
  {
    throw std::runtime_error("failed to init g_total_step and g_total_block_number");
  }

  unsigned reminder = 0;
  if (g_total_block_number % nprocs != 0 && rank == (nprocs - 1))
  {
    // the last process will process the reminder
    reminder = (g_total_block_number) % unsigned(nprocs);
  }
  // this value will vary when there is process join/leave
  const unsigned nblocks_per_proc = reminder + g_total_block_number / nprocs;

  int blockid_base = rank * nblocks_per_proc;
  std::vector<Mandelbulb> MandelbulbList;
  // std::cout << "g_total_block_number " << g_total_block_number << std::endl;

  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * g_block_depth;
    // std::cout << "push blockid " << blockid << std::endl;
    MandelbulbList.push_back(Mandelbulb(
      g_block_width, g_block_height, g_block_depth, block_offset, 1.2, g_total_block_number));
  }

  try
  {
    // Initialize a Client
    colza::Client client(engine);
    // Open distributed pipeline from provider 0
    colza::DistributedPipelineHandle pipeline =
      client.makeDistributedPipelineHandle(&comm, g_ssg_file, 0, g_pipeline);

    for (int step = 0; step < g_total_step; step++)
    {
      // start iteration
      // compute stage
      double order = 4.0 + ((double)step) * 8.0 / 100.0;
      for (int i = 0; i < MandelbulbList.size(); i++)
      {
        // update data value
        MandelbulbList[i].compute(order);
      }

      // the join and leave may happens here
      // this should be called after the compute process

      spdlog::trace("prepare start {}", step);
      MPI_Barrier(MPI_COMM_WORLD);

      auto startStart = tl::timer::wtime();
      pipeline.start(step);
      auto startEnd = tl::timer::wtime();

      if (rank == 0)
      {
        // only care about the rank0
        std::cout << "rank " << rank << " start time " << startEnd - startStart << std::endl;
      }

      // make sure the pipeline start is called by every process
      // before the stage call
      MPI_Barrier(MPI_COMM_WORLD);
      // generate the datablock and put the data
      spdlog::trace("start finish, prepare to call stage {}", step);

      uint64_t blockid;
      // use another iteration to do the stage
      // for every block

      double stageStart = tl::timer::wtime();

      for (int i = 0; i < MandelbulbList.size(); i++)
      {
        // double innerstageStart = tl::timer::wtime();

        // stage the data at current iteration
        blockid = blockid_base + i;
        int32_t result;

        int* extents = MandelbulbList[i].GetExtents();
        // the extends value is from 0 to 29
        // the dimension value should be extend value +1
        // the sequence is depth, height, width
        std::vector<size_t> dimensions = { int2size_t(*(extents + 1)) + 1,
          int2size_t(*(extents + 3)) + 1, int2size_t(*(extents + 5)) + 1 };
        std::vector<int64_t> offsets = { 0, 0, MandelbulbList[i].GetZoffset() };

        // TODO test
        // output the data to detect data offline

        auto type = colza::Type::INT32;
        pipeline.stage(
          "mydata", step, blockid, dimensions, offsets, type, MandelbulbList[i].GetData(), &result);
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
        // double innerstageEnd = tl::timer::wtime();
        // std::cout << "rank " << rank << " blockid " << i << " step " << step << " inner stage
        // time "
        //          << innerstageEnd - innerstageStart << std::endl;
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
      double cleanupStart = tl::timer::wtime();

      pipeline.cleanup(step);

      MPI_Barrier(MPI_COMM_WORLD);
      double cleanupEnd = tl::timer::wtime();

      if (rank == 0)
      {
        // only care about the rank0
        std::cout << "rank " << rank << " cleanup time " << cleanupEnd - cleanupStart << std::endl;
      }
      sleep(2);
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

void parse_command_line(int argc, char** argv)
{
  try
  {
    TCLAP::CmdLine cmd("Colza client", ' ', "0.1");
    TCLAP::ValueArg<std::string> addressArg(
      "a", "address", "Address or protocol", true, "", "string");
    TCLAP::ValueArg<std::string> pipelineArg("p", "pipeline", "Pipeline name", true, "", "string");
    TCLAP::ValueArg<std::string> logLevel("v", "verbose",
      "Log level (trace, debug, info, warning, error, critical, off)", false, "info", "string");
    TCLAP::ValueArg<std::string> ssgFileArg("s", "ssg-file", "SSG file name", true, "", "string");
    TCLAP::ValueArg<int> totalBlockNumArg(
      "b", "total-block-number", "Total block number", true, 0, "int");
    TCLAP::ValueArg<int> totalStepArg("t", "total-time-step", "Total time step", true, 0, "int");
    TCLAP::ValueArg<int> widthArg("w", "width", "Width of data block", true, 64, "int");
    TCLAP::ValueArg<int> depthArg("d", "depth", "Depth of data block", true, 64, "int");
    TCLAP::ValueArg<int> heightArg("e", "height", "Height of data block", true, 64, "int");

    cmd.add(addressArg);
    cmd.add(pipelineArg);
    cmd.add(logLevel);
    cmd.add(ssgFileArg);
    cmd.add(totalBlockNumArg);
    cmd.add(totalStepArg);
    cmd.add(widthArg);
    cmd.add(depthArg);
    cmd.add(heightArg);

    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_pipeline = pipelineArg.getValue();
    g_log_level = logLevel.getValue();
    g_ssg_file = ssgFileArg.getValue();
    g_total_block_number = totalBlockNumArg.getValue();
    g_total_step = totalStepArg.getValue();
    g_block_width = widthArg.getValue();
    g_block_depth = depthArg.getValue();
    g_block_height = heightArg.getValue();
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

uint32_t get_credentials_from_ssg_file()
{
  uint32_t cookie = 0;
#ifdef USE_GNI

  int64_t credential_id = -1;
  int ret = ssg_get_group_cred_from_file(g_ssg_file.c_str(), &credential_id);
  if (credential_id == -1)
    return cookie;

  drc_info_handle_t drc_credential_info;

  ret = drc_access(credential_id, 0, &drc_credential_info);
  if (ret != DRC_SUCCESS)
  {
    spdlog::critical("drc_access failed (ret = {})", ret);
    exit(-1);
  }

  cookie = drc_get_first_cookie(drc_credential_info);
#endif
  return cookie;
}

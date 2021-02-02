/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include "mb.hpp"
#include <colza/Client.hpp>
#include <colza/MPIClientCommunicator.hpp>
#include <iostream>
#include <mpi.h>
#include <spdlog/spdlog.h>
#include <ssg.h>
#include <tclap/CmdLine.h>

namespace tl = thallium;

static std::string g_address;
static std::string g_pipeline;
static std::string g_log_level = "info";
static std::string g_ssg_file;
static int g_total_block_number;
static int g_total_step;

static void parse_command_line(int argc, char** argv);

// refer to https://stackoverflow.com/questions/27490762/how-can-i-convert-to-size-t-from-int-safely
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

  // Initialize the thallium server
  tl::engine engine(g_address, THALLIUM_SERVER_MODE);

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
  //std::cout << "g_total_block_number " << g_total_block_number << std::endl;

  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * DEPTH;
    //std::cout << "push blockid " << blockid << std::endl;
    MandelbulbList.push_back(
      Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset, 1.2, g_total_block_number));
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

      MPI_Barrier(MPI_COMM_WORLD);

      // the join and leave may happens here
      // this should be called after the compute process
      pipeline.start(step);
      // generate the datablock and put the data
      spdlog::trace("Calling stage {}", step);
      uint64_t blockid;
      // use another iteration to do the stage
      // for every block
      for (int i = 0; i < MandelbulbList.size(); i++)
      {
        // stage the data at current iteration
        blockid = blockid_base + i;
        int32_t result;

        int* extents = MandelbulbList[i].GetExtents();
        // the extends value is from 0 to 29
        // the dimension value should be extend value +1
        std::vector<size_t> dimensions = { int2size_t(*(extents + 1)) + 1,
          int2size_t(*(extents + 3)) + 1, int2size_t(*(extents + 5)) + 1 };
        std::vector<int64_t> offsets = { 0, 0, MandelbulbList[i].GetZoffset() };

        auto type = colza::Type::INT32;
        pipeline.stage(
          "mydata", step, blockid, dimensions, offsets, type, MandelbulbList[i].GetData(), &result);

        if (result != 0)
        {
          throw std::runtime_error(
            "failed to stage " + std::to_string(step) + " return status " + std::to_string(result));
        }
      }

      MPI_Barrier(MPI_COMM_WORLD);
      spdlog::trace("Calling execute {}", step);

      // execute the pipeline
      pipeline.execute(step);

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

    cmd.add(addressArg);
    cmd.add(pipelineArg);
    cmd.add(logLevel);
    cmd.add(ssgFileArg);
    cmd.add(totalBlockNumArg);
    cmd.add(totalStepArg);

    cmd.parse(argc, argv);
    g_address = addressArg.getValue();
    g_pipeline = pipelineArg.getValue();
    g_log_level = logLevel.getValue();
    g_ssg_file = ssgFileArg.getValue();
    g_total_block_number = totalBlockNumArg.getValue();
    g_total_step = totalStepArg.getValue();
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

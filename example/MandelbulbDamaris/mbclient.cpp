/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include "mb.hpp"
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <tclap/CmdLine.h>
#include <Damaris.h>
#include <damaris/data/VariableManager.hpp>

static std::string g_script;
static std::string g_log_level = "info";
static std::string g_damaris_config;
static int g_total_block_number;
static int g_total_step;
static int g_block_width;
static int g_block_depth;
static int g_block_height;

static void parse_command_line(int argc, char** argv);
static uint32_t get_credentials_from_ssg_file();

int main(int argc, char** argv)
{
  parse_command_line(argc, argv);
  spdlog::set_level(spdlog::level::from_str(g_log_level));

  MPI_Init(&argc, &argv);

  int ret = damaris_initialize(g_damaris_config.c_str(), MPI_COMM_WORLD);
  if(ret != DAMARIS_OK) {
    spdlog::critical("damaris_initialize failed with error code {}", ret);
    exit(-1);
  }
  spdlog::info("Damaris initialized");

  int is_client;
  ret = damaris_start(&is_client);
  if(ret != DAMARIS_OK && ret != DAMARIS_NO_SERVER) {
    spdlog::critical("damaris_start failed with error code {}", ret);
    exit(-1);
  }
  if(!is_client) {
    damaris_finalize();
    MPI_Finalize();
    exit(0);
  }
  spdlog::info("Damaris client starting");

  MPI_Comm MPI_COMM_CLIENTS;
  damaris_client_comm_get(&MPI_COMM_CLIENTS);
  int rank;
  int nprocs;
  MPI_Comm_rank(MPI_COMM_CLIENTS, &rank);
  MPI_Comm_size(MPI_COMM_CLIENTS, &nprocs);

  damaris_parameter_get("WIDTH",  &g_block_width,        sizeof(g_block_width));
  damaris_parameter_get("HEIGHT", &g_block_height,       sizeof(g_block_height));
  damaris_parameter_get("DEPTH",  &g_block_depth,        sizeof(g_block_depth));
  damaris_parameter_get("BLOCKS", &g_total_block_number, sizeof(g_total_block_number));

  if (rank == 0)
  {
    std::cout << "-------key varaibles--------" << std::endl;
    std::cout << "g_total_block_number:" << g_total_block_number << std::endl;
    std::cout << "g_total_step:" << g_total_step << std::endl;
    std::cout << "g_block_width:" << g_block_width << std::endl;
    std::cout << "g_block_depth:" << g_block_depth << std::endl;
    std::cout << "g_block_height:" << g_block_height << std::endl;
    std::cout << "----------------------------" << std::endl;
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

  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * g_block_depth;
    MandelbulbList.push_back(Mandelbulb(
      g_block_width, g_block_height, g_block_depth,
      block_offset, 1.2, g_total_block_number));
    std::array<int64_t,3> position = {
        MandelbulbList[i].GetExtents()[0],
        MandelbulbList[i].GetExtents()[2],
        MandelbulbList[i].GetExtents()[4]
    };
    damaris_set_block_position("mandelbulb", i, position.data());
  }

  try
  {
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

      MPI_Barrier(MPI_COMM_CLIENTS);

      // use another iteration to do the stage
      // for every block

      for (int i = 0; i < MandelbulbList.size(); i++)
      {
          damaris_write_block("mandelbulb", i, MandelbulbList[i].GetData());
      }
      MPI_Barrier(MPI_COMM_CLIENTS);

      if(step == 0) {
          std::string script_name = g_script;
          script_name.resize(1024, '\0');
          damaris_write("script", script_name.data());
      }

      double exeStart = MPI_Wtime();
      damaris_signal("render");

      double exeEnd = MPI_Wtime();
      if (rank == 0)
      {
        // only care about the rank0
        std::cout << "rank " << rank << " execution time " << exeEnd - exeStart << std::endl;
      }

      MPI_Barrier(MPI_COMM_CLIENTS);
      damaris_end_iteration();

      sleep(2);
    }

    spdlog::trace("Done");
  }
  catch (const std::exception& ex)
  {
    std::cerr << ex.what() << std::endl;
    exit(-1);
  }
  spdlog::trace("Stopping Damaris servers");
  damaris_stop();
  spdlog::trace("Finalizing Damaris");
  damaris_finalize();
  spdlog::trace("Finalizing MPI");
  MPI_Finalize();

  return 0;
}

void parse_command_line(int argc, char** argv)
{
  try
  {
    TCLAP::CmdLine cmd("Mandelbulb client", ' ', "0.1");
    TCLAP::ValueArg<std::string> configArg(
      "c", "config", "Damaris configuration file", true, "", "string");
    TCLAP::ValueArg<std::string> scriptArg(
      "s", "script", "Python script for in situ pipeline", true, "", "string");
    TCLAP::ValueArg<std::string> logLevel("v", "verbose",
      "Log level (trace, debug, info, warning, error, critical, off)", false, "info", "string");
    TCLAP::ValueArg<int> totalStepArg("t", "total-time-step", "Total time step", true, 0, "int");

    cmd.add(configArg);
    cmd.add(scriptArg);
    cmd.add(logLevel);
    cmd.add(totalStepArg);

    cmd.parse(argc, argv);
    g_damaris_config = configArg.getValue();
    g_script = scriptArg.getValue();
    g_log_level = logLevel.getValue();
    g_total_step = totalStepArg.getValue();
  }
  catch (TCLAP::ArgException& e)
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
    exit(-1);
  }
}

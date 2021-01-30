#include "Mandelbulb_dynamic.hpp"

#include <mpi.h>

#include "InSituAdaptor.hpp"
#include "ProcessController.hpp"
#include <future>
#include <iostream>

#include <chrono>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define BILLION 1000000000L

mona_comm_t initMonaComm()
{
  ABT_init(0, NULL);
  mona_instance_t mona = mona_init("ofi+tcp", NA_TRUE, NULL);

  // caculate num_procs and other addr
  // mona need init all the communicators based on MPI
  int ret;
  na_addr_t self_addr;
  ret = mona_addr_self(mona, &self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona self addr");
    return 0;
  }
  char self_addr_str[128];
  na_size_t self_addr_size = 128;
  ret = mona_addr_to_string(mona, self_addr_str, &self_addr_size, self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to execute mona_addr_to_string");
    return 0;
  }
  int num_proc;
  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);

  char* other_addr_str = (char*)malloc(128 * num_proc);

  MPI_Allgather(self_addr_str, 128, MPI_BYTE, other_addr_str, 128, MPI_BYTE, MPI_COMM_WORLD);

  na_addr_t* other_addr = (na_addr_t*)malloc(num_proc * sizeof(*other_addr));

  int i;
  for (i = 0; i < num_proc; i++)
  {
    ret = mona_addr_lookup(mona, other_addr_str + 128 * i, other_addr + i);
    if (ret != 0)
    {
      throw std::runtime_error("failed to execute mona_addr_lookup");
      return 0;
    }
  }
  free(other_addr_str);

  // create the mona_comm based on the other_addr
  mona_comm_t mona_comm;
  ret = mona_comm_create(mona, num_proc, other_addr, &mona_comm);
  if (ret != 0)
  {
    throw std::runtime_error("failed to init mona");
    return 0;
  }
  return mona_comm;
}

void fixMochiCommSizeTest(std::string scriptname, int total_block_number, int totalstep)
{
  int rank, nprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

  // create the mona comm for all procs
  // init mona comm
  ABT_init(0, NULL);
  mona_instance_t mona = mona_init("ofi+tcp", NA_TRUE, NULL);

  // caculate num_procs and other addr
  // mona need init all the communicators based on MPI
  int ret;
  na_addr_t self_addr;
  ret = mona_addr_self(mona, &self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to get mona self addr");
  }
  char self_addr_str[128];
  na_size_t self_addr_size = 128;
  ret = mona_addr_to_string(mona, self_addr_str, &self_addr_size, self_addr);
  if (ret != 0)
  {
    throw std::runtime_error("failed to execute mona_addr_to_string");
  }

  int num_procs;
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

  char* other_addr_str = (char*)malloc(128 * num_procs);

  MPI_Allgather(self_addr_str, 128, MPI_BYTE, other_addr_str, 128, MPI_BYTE, MPI_COMM_WORLD);

  na_addr_t* other_addr = (na_addr_t*)malloc(num_procs * sizeof(*other_addr));

  int i;
  for (i = 0; i < num_procs; i++)
  {
    ret = mona_addr_lookup(mona, other_addr_str + 128 * i, other_addr + i);
    if (ret != 0)
    {
      throw std::runtime_error("failed to execute mona_addr_lookup");
    }
  }
  free(other_addr_str);

  // create the mona_comm based on the other_addr
  mona_comm_t mona_comm;
  ret = mona_comm_create(mona, num_procs, other_addr, &mona_comm);
  if (ret != 0)
  {
    throw std::runtime_error("failed to init mona");
  }

  // create the catalyst pipeline and register comm
  InSitu::MonaInitialize(scriptname, mona_comm);

  if (nprocs - 0 <= 0)
  {
    throw std::runtime_error("nprocs should large than 2");
  }

  // it runs ok when the dummy value is 0
  // it is ok for the dummy node that conains the zero data block
  size_t dummyValue = 2;
  size_t actualProcWithData = nprocs - dummyValue;

  // InSitu::MPIInitialize(scriptname);

  unsigned reminder = 0;
  if (total_block_number % actualProcWithData != 0 && rank == (actualProcWithData - 1))
  {
    // the last process will process the reminder
    reminder = (total_block_number) % unsigned(actualProcWithData);
  }
  // this value will vary when there is process join/leave
  unsigned nblocks_per_proc = reminder + total_block_number / actualProcWithData;
  if (rank >= actualProcWithData)
  {
    nblocks_per_proc = 0;
  }

  DEBUG(nblocks_per_proc << " blocks for rank " << rank);
  // do work
  int blockid_base = rank * nblocks_per_proc;
  std::vector<Mandelbulb> MandelbulbList;
  if (rank < actualProcWithData)
  {
    // the mandelbulb list is zero for the dummy node
    for (int i = 0; i < nblocks_per_proc; i++)
    {
      int blockid = blockid_base + i;
      int block_offset = blockid * DEPTH;
      MandelbulbList.push_back(
        Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset, 1.2, total_block_number));
    }
  }

  for (int i = 0; i < totalstep; i++)
  {
    double t_start, t_end;
    {
      double order = 4.0 + ((double)i) * 8.0 / 100.0;
      // MPI_Barrier(MPI_COMM_WORLD);
      // t_start = MPI_Wtime();
      for (auto& mandelbulb : MandelbulbList)
      {
        mandelbulb.compute(order);
      }
      // MPI_Barrier(MPI_COMM_WORLD);
      // t_end = MPI_Wtime();
    }

    if (rank == 0)
    {
      std::cout << "Computation " << i << " completed in " << (t_end - t_start) << " seconds."
                << std::endl;
    }

    {
      // MPI_Barrier(MPI_COMM_WORLD);
      // t_start = MPI_Wtime();
      // if there is updates for the mona_comm
      // reinit it
      // mona_comm_t mona_comm = initMonaComm();
      // the mona comm is created in the init process of the insitu Init in default
      // use the mona created outside of the InSitu scope
      InSitu::MonaCoProcessDynamic(mona_comm, MandelbulbList, total_block_number, i, i);
      // InSitu::MonaCoProcessDynamic(NULL, MandelbulbList, total_block_number, i, i);
      // InSitu::MPICoProcessDynamic(MPI_COMM_WORLD, MandelbulbList, total_block_number, i, i);
      // MPI_Barrier(MPI_COMM_WORLD);
      // t_end = MPI_Wtime();
    }
    if (rank == 0)
    {
      std::cout << "InSitu " << i << " completed in " << (t_end - t_start) << " seconds."
                << std::endl;
    }
  }

  std::cout << " proc " << rank << " waiting" << std::endl;

  // set a barrier
  mona_comm_barrier(mona_comm, 6666);
  std::cout << " proc " << rank << " finish in situ step" << std::endl;

  // There are still some issues to call the paraview finalize
  // InSitu::Finalize();
}

void fixMochiCommSize(std::string scriptname, int total_block_number, int totalstep)
{
  int rank, nprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  InSitu::MonaInitialize(scriptname);
  // InSitu::MPIInitialize(scriptname);
  unsigned reminder = 0;
  if (total_block_number % nprocs != 0 && rank == (nprocs - 1))
  {
    // the last process will process the reminder
    reminder = (total_block_number) % unsigned(nprocs);
  }
  // this value will vary when there is process join/leave
  const unsigned nblocks_per_proc = reminder + total_block_number / nprocs;

  int blockid_base = rank * nblocks_per_proc;
  std::vector<Mandelbulb> MandelbulbList;
  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * DEPTH;
    MandelbulbList.push_back(
      Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset, 1.2, total_block_number));
  }

  for (int i = 0; i < totalstep; i++)
  {
    double t_start, t_end;
    {
      double order = 4.0 + ((double)i) * 8.0 / 100.0;
      MPI_Barrier(MPI_COMM_WORLD);
      t_start = MPI_Wtime();
      for (auto& mandelbulb : MandelbulbList)
      {
        mandelbulb.compute(order);
      }
      MPI_Barrier(MPI_COMM_WORLD);
      t_end = MPI_Wtime();
    }

    if (rank == 0)
    {
      std::cout << "Computation " << i << " completed in " << (t_end - t_start) << " seconds."
                << std::endl;
    }

    {
      MPI_Barrier(MPI_COMM_WORLD);
      t_start = MPI_Wtime();
      // if there is updates for the mona_comm
      // reinit it
      // mona_comm_t mona_comm = initMonaComm();
      // the mona comm is created in the init process of the insitu Init in default
      InSitu::MonaCoProcessDynamic(NULL, MandelbulbList, total_block_number, i, i);
      // InSitu::MPICoProcessDynamic(MPI_COMM_WORLD, MandelbulbList, total_block_number, i, i);
      MPI_Barrier(MPI_COMM_WORLD);
      t_end = MPI_Wtime();
    }
    if (rank == 0)
    {
      std::cout << "InSitu " << i << " completed in " << (t_end - t_start) << " seconds."
                << std::endl;
    }
  }

  InSitu::Finalize();
}

// start with the full MPI size communicator
// decrease it to one for sub-communicator and then increase it back to the full MPI communicator
void variedMPICommSize(
  std::string scriptname, int total_block_number, int process_granularity, int totalstep)
{

  int globalrank, globalnprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &globalrank);
  MPI_Comm_size(MPI_COMM_WORLD, &globalnprocs);
  unsigned reminder = 0;
  unsigned extra_block_per_proc = 0;

  if (total_block_number < globalnprocs)
  {
    throw std::runtime_error("the total block number should large or equal to the process number");
    exit(0);
  }

  reminder = (total_block_number) % unsigned(globalnprocs);

  // caculate the blocks number per process as the initial states
  if (total_block_number % globalnprocs != 0 && globalrank < reminder)
  {
    extra_block_per_proc = 1;
  }
  unsigned nblocks_per_proc = extra_block_per_proc + (total_block_number / globalnprocs);
  int blockid_base = globalrank * nblocks_per_proc;
  std::vector<Mandelbulb> MandelbulbList;
  // init the color (non-neg or MPI_UNDEFINED)
  int color = 0;
  MPI_Comm subcomm = MPI_COMM_WORLD;

  // init the mandulebulb list
  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * DEPTH;
    MandelbulbList.push_back(
      Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset, 1.2, total_block_number));
  }

  // the increase or decrease label will be controlled by rank 0 proc
  // since it exists for a long time
  bool ifDecrease = true;
  bool ifIncrease = false;
  int localrank = globalrank;
  int localnprocs = globalnprocs;

  InSitu::MPIInitialize(scriptname);
  for (int step = 0; step < totalstep; step++)
  {
    MPI_Barrier(MPI_COMM_WORLD);

    // assume the synthetic migragation is ok
    // start to iterate
    // get new comm group based on updated color
    MPI_Comm_split(MPI_COMM_WORLD, color, globalrank, &subcomm);
    if (subcomm != MPI_COMM_NULL)
    {
      MPI_Comm_rank(subcomm, &localrank);
      MPI_Comm_size(subcomm, &localnprocs);

      // if (globalrank == 0)
      //{
      //  std::cout << "sub rank " << localrank << " sub procs " << localnprocs << std::endl;
      //}

      double t_start, t_end;
      {
        double order = 4.0 + ((double)step) * 8.0 / 100.0;
        MPI_Barrier(subcomm);
        t_start = MPI_Wtime();
        for (auto& mandelbulb : MandelbulbList)
        {
          // std::cout << "local rank " << localrank << " iterate " << step << " offset "
          //          << mandelbulb.GetZoffset() << std::endl;
          mandelbulb.compute(order);
        }
        MPI_Barrier(subcomm);
        t_end = MPI_Wtime();
      }

      if (globalrank == 0)
      {
        std::cout << "Computation " << step << " localnprocs " << localnprocs << " completed in "
                  << (t_end - t_start) << " seconds." << std::endl;
      }

      {
        MPI_Barrier(subcomm);
        t_start = MPI_Wtime();
        InSitu::MPICoProcessDynamic(subcomm, MandelbulbList, total_block_number, step, step);
        // InSitu::StoreBlocks(step, MandelbulbList);
        MPI_Barrier(subcomm);
        t_end = MPI_Wtime();
      }
      if (globalrank == 0)
      {
        std::cout << "InSitu " << step << " localnprocs " << localnprocs << " completed in "
                  << (t_end - t_start) << " seconds." << std::endl;
        std::cout << std::endl;
      }
    }

    if (globalrank == 0)
    {
      // control the increase or decrease label
      if (localnprocs == 1)
      {
        // start to increase if there is one proc
        ifDecrease = false;
        ifIncrease = true;
      }
      if (localnprocs == globalnprocs)
      {
        // start to decrease if the group size is full
        ifDecrease = true;
        ifIncrease = false;
      }
    }
    // bcast the updated control label to all procs
    MPI_Bcast(&ifDecrease, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
    MPI_Bcast(&ifIncrease, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);

    // update the color value gradually and do the synthetic data migragation, except the first
    // step
    if (ifDecrease && subcomm != MPI_COMM_NULL)
    {
      if (localnprocs < (process_granularity + 1))
      {
        // if there is not enough remaining procs, the number of decreased procs will be
        // localnprocs-1
        PController::processLeave(subcomm, localrank, localnprocs, color, total_block_number,
          MandelbulbList, localnprocs - 1, step % globalnprocs);
      }
      else
      {
        PController::processLeave(subcomm, localrank, localnprocs, color, total_block_number,
          MandelbulbList, process_granularity, step % globalnprocs);
      }
    }
    if (ifIncrease)
    {
      // all procs in the global world need to call join function
      // since new one will be joined into the original comm group
      // local procs number need to be updated by zero id proc, since other procs may leave the
      // comm grop some times and there coresponding value is not updated in-time
      // we only support one procs join each time currently
      MPI_Bcast(&localnprocs, 1, MPI_INT, 0, MPI_COMM_WORLD);
      PController::processJoin(
        subcomm, globalrank, localnprocs, color, total_block_number, MandelbulbList);
    }
  }
  InSitu::Finalize();
}

void monaSplit(mona_comm_t mona_comm_bootstrap, int color, size_t globalrank, mona_comm_t* subcomm)
{
  // get the color list by all gather
  int globalnproc;
  mona_comm_size(mona_comm_bootstrap, &globalnproc);

  std::vector<int> colorlist(globalnproc);
  mona_comm_allgather(
    mona_comm_bootstrap, &color, sizeof(int), colorlist.data(), MANDELBULB_ALLGATHER_TAG);

  // check the color
  // std::cout << "rank " << globalrank << " debug color list" << std::endl;
  // for (int i = 0; i < colorlist.size(); i++)
  //{
  //  std::cout << "," << colorlist[i];
  //}
  // std::cout << std::endl;

  // get the rank list in subset
  std::vector<int> ranklist;
  for (int i = 0; i < globalnproc; i++)
  {
    // reuse the mpi undefined label here
    // we do not need to update the process join and leave function in this case
    if (colorlist[i] != MONA_UNDEFINED)
    {
      ranklist.push_back(i);
    }
  }

  // create the new comm based on rank list
  if (color == 0)
  {
    mona_comm_subset(mona_comm_bootstrap, ranklist.data(), ranklist.size(), subcomm);
  }
  else
  {
    // assign null to comm if the current color is not defined
    *subcomm = NULL;
  }
  return;
}

void variedMonaCommSize(
  std::string scriptname, int total_block_number, int process_granularity, int totalstep)
{

  // init the bootstrap comm
  mona_comm_t mona_comm_bootstrap = initMonaComm();

  int globalrank, globalnprocs;

  mona_comm_rank(mona_comm_bootstrap, &globalrank);
  mona_comm_size(mona_comm_bootstrap, &globalnprocs);

  DEBUG("debug bootstrap comm rank " << globalrank << " size " << globalnprocs);

  // caculate how many blocks in each ranks
  unsigned reminder = 0;
  unsigned extra_block_per_proc = 0;

  if (total_block_number < globalnprocs)
  {
    throw std::runtime_error("the total block number should large or equal to the process number");
    exit(0);
  }

  reminder = (total_block_number) % unsigned(globalnprocs);

  // caculate the blocks number per process as the initial states
  if (total_block_number % globalnprocs != 0 && globalrank < reminder)
  {
    extra_block_per_proc = 1;
  }
  unsigned nblocks_per_proc = extra_block_per_proc + (total_block_number / globalnprocs);
  int blockid_base = globalrank * nblocks_per_proc;

  std::vector<Mandelbulb> MandelbulbList;
  mona_comm_t subcomm = NULL;
  int color = 0;
  // init the mandulebulb list
  for (int i = 0; i < nblocks_per_proc; i++)
  {
    int blockid = blockid_base + i;
    int block_offset = blockid * DEPTH;
    MandelbulbList.push_back(
      Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset, 1.2, total_block_number));
  }

  // the increase or decrease label will be controlled by rank 0 proc
  // since it exists for a long time
  bool ifDecrease = true;
  bool ifIncrease = false;
  int localrank;
  int localnprocs;

  InSitu::MonaInitialize(scriptname);
  for (int step = 0; step < totalstep; step++)
  {
    mona_comm_barrier(mona_comm_bootstrap, MANDELBULB_BARRIER_TAG);
    // create a new mona communicator here
    // based on the varied address list
    // if (subcomm != NULL)
    //{
    // delete previous subcomm
    //  mona_comm_free(subcomm);
    //}
    monaSplit(mona_comm_bootstrap, color, globalrank, &subcomm);

    if (subcomm != NULL)
    {

      mona_comm_rank(subcomm, &localrank);
      mona_comm_size(subcomm, &localnprocs);

      DEBUG("debug sub rank " << localrank << " sub procs " << localnprocs);

      double t_start, t_end;
      {
        double order = 4.0 + ((double)step) * 8.0 / 100.0;
        mona_comm_barrier(subcomm, MANDELBULB_BARRIER_TAG);
        t_start = MPI_Wtime();
        for (auto& mandelbulb : MandelbulbList)
        {
          // std::cout << "local rank " << localrank << " iterate " << step << " offset "
          //          << mandelbulb.GetZoffset() << std::endl;
          mandelbulb.compute(order);
        }
        mona_comm_barrier(subcomm, MANDELBULB_BARRIER_TAG);
        t_end = MPI_Wtime();
      }

      if (globalrank == 0)
      {
        std::cout << "Computation " << step << " block size " << MandelbulbList.size()
                  << " localnprocs " << localnprocs << " completed in " << (t_end - t_start)
                  << " seconds." << std::endl;
      }

      {
        mona_comm_barrier(subcomm, MANDELBULB_BARRIER_TAG);
        t_start = MPI_Wtime();
        InSitu::MonaCoProcessDynamic(subcomm, MandelbulbList, total_block_number, step, step);
        mona_comm_barrier(subcomm, MANDELBULB_BARRIER_TAG);
        t_end = MPI_Wtime();
      }
      if (globalrank == 0)
      {
        std::cout << "InSitu " << step << " localnprocs " << localnprocs << " completed in "
                  << (t_end - t_start) << " seconds." << std::endl;
        std::cout << std::endl;
      }
    }

    if (globalrank == 0)
    {
      // control the increase or decrease label
      if (localnprocs == 1)
      {
        // start to increase if there is one proc
        ifDecrease = false;
        ifIncrease = true;
      }
      if (localnprocs == globalnprocs)
      {
        // start to decrease if the group size is full
        ifDecrease = true;
        ifIncrease = false;
      }
    }
    // bcast the updated control label to all procs
    mona_comm_bcast(mona_comm_bootstrap, &ifDecrease, sizeof(bool), 0, MANDELBULB_BCAST_TAG);
    mona_comm_bcast(mona_comm_bootstrap, &ifIncrease, sizeof(bool), 0, MANDELBULB_BCAST_TAG);

    // update the color value gradually and do the synthetic data migragation, except the first
    // step
    if (ifDecrease && subcomm != NULL)
    {
      if (localnprocs < (process_granularity + 1))
      {
        // if there is not enough remaining procs, the number of decreased procs will be
        // localnprocs-1
        PController::processLeave(subcomm, localrank, localnprocs, color, total_block_number,
          MandelbulbList, localnprocs - 1, step % globalnprocs);
      }
      else
      {
        PController::processLeave(subcomm, localrank, localnprocs, color, total_block_number,
          MandelbulbList, process_granularity, step % globalnprocs);
      }
    }
    if (ifIncrease)
    {
      // all procs in the global world need to call join function
      // since new one will be joined into the original comm group
      // local procs number need to be updated by zero id proc, since other procs may leave the
      // comm grop some times and there coresponding value is not updated in-time
      // we only support one procs join each time currently

      mona_comm_bcast(mona_comm_bootstrap, &localnprocs, sizeof(int), 0, MANDELBULB_BCAST_TAG);

      // bootstrap comm is necessary here, since we need to join another new process into the group
      PController::processJoin(subcomm, mona_comm_bootstrap, globalrank, localnprocs, color,
        total_block_number, MandelbulbList);
    }
  }
  InSitu::Finalize();
}

int main(int argc, char** argv)
{

  MPI_Init(&argc, &argv);
  int globalrank;
  MPI_Comm_rank(MPI_COMM_WORLD, &globalrank);

  if (argc != 5)
  {
    std::cerr
      << "Usage: " << argv[0]
      << " <script.py> <total_blocks_num> <granularity of process to join or leave> <totalstep>"
      << std::endl;
    exit(0);
  }

  std::string scriptname = argv[1];
  int total_block_number = std::stoi(argv[2]);
  int process_granularity = std::stoi(argv[3]);
  int totalstep = std::stoi(argv[4]);

  if (process_granularity == 0)
  {
    fixMochiCommSizeTest(scriptname, total_block_number, totalstep);
  }
  else
  {
    // variedMPICommSize(scriptname, total_block_number, process_granularity, totalstep);
    variedMonaCommSize(scriptname, total_block_number, process_granularity, totalstep);
  }

  MPI_Finalize();

  return 0;
}

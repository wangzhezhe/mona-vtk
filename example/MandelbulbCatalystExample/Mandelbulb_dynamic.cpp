#include "Mandelbulb_dynamic.hpp"

#include <mpi.h>

#include "InSituAdaptor.hpp"
#include <future>
#include <iostream>

#include <chrono>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define BILLION 1000000000L

unsigned WIDTH = 30;
unsigned HEIGHT = 30;
unsigned DEPTH = 30;
unsigned Globalpid = 0;


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

void fixMochiCommSize(std::string scriptname, int total_block_number, int totalstep)
{
  int rank, nprocs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  InSitu::MonaInitialize(scriptname);
  //InSitu::MPIInitialize(scriptname);
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
      //InSitu::MPICoProcessDynamic(MPI_COMM_WORLD, MandelbulbList, total_block_number, i, i);
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
// this is only executed by the procs in subcomm
// the offset might realt with the iteration
// we want to aoid the case that there is too many blocks on zero one rank
void processLeave(MPI_Comm subcomm, int localrank, int localnprocs, int& color,
  int total_block_number, std::vector<Mandelbulb>& MandelbulbList, int process_leave_num,
  int baseoffset)
{
  int tag_block_num = 111;
  int tag_block_offsetlist = 222;
  // the procs id in this range will leave the group
  if (localrank >= localnprocs - process_leave_num && localrank <= localnprocs - 1)
  {
    // update the color
    // and this process will leave
    color = MPI_UNDEFINED;
    // decide how to distribute the instance on current process to others
    // send to all other processes about the blocknumber and offset (meatadata)
    unsigned total_migragate_block_size = MandelbulbList.size();
    int new_comm_size = localnprocs - process_leave_num;
    // the zero rank always stay
    int migragate_index = 0;
    // make sure there is at least one block
    // if there are 3 blocks 2 procs, one block might miss without ceil
    unsigned avg_migragate_block_per_proc =
      ceil((1.0 * total_migragate_block_size) / (1.0 * new_comm_size));
    if (avg_migragate_block_per_proc == 0)
    {
      avg_migragate_block_per_proc = 1;
    }
    // std::cout << "debug rank " << localrank << " total_migragate_block_size "
    //          << total_migragate_block_size << " new_comm_size " << new_comm_size << std::endl;
    // the baseoffset is realted with the first position, if we do not set baseoffset
    // all the blocks will be moved into the rank zero proc
    // this offset is to control the start position of every procs in the leaving group
    int offset = (baseoffset + localrank - new_comm_size) % new_comm_size;
    int id = offset;
    int count = new_comm_size;
    while (count > 0)
    {
      unsigned migragate_block_per_proc =
        (total_migragate_block_size >= avg_migragate_block_per_proc ? avg_migragate_block_per_proc
                                                                    : total_migragate_block_size);
      // if there is only one count
      // give all remaining blocks to it
      if (count == 1)
      {
        migragate_block_per_proc = total_migragate_block_size;
      }
      total_migragate_block_size = total_migragate_block_size - migragate_block_per_proc;

      // send to every other procs, the number of migrated blocks and the offset
      // std::cout << "debug rank " << localrank << " migragate_block_per_proc "
      //          << migragate_block_per_proc << " to " << id << std::endl;

      MPI_Send(&migragate_block_per_proc, 1, MPI_UNSIGNED, id, tag_block_num, subcomm);

      if (migragate_block_per_proc > 0)
      {
        std::vector<unsigned> offsetList;
        for (int k = 0; k < migragate_block_per_proc; k++)
        {
          offsetList.push_back(MandelbulbList[migragate_index].GetZoffset());
          migragate_index++;
        }

        // send offsetlist to dedicated procs
        // std::cout << "debug rank " << localrank << " send offset number "
        //          << migragate_block_per_proc << " to " << id << std::endl;
        MPI_Send(offsetList.data(), migragate_block_per_proc, MPI_UNSIGNED, id,
          tag_block_offsetlist, subcomm);
      }
      id = (id + 1) % new_comm_size;
      count--;
    }
  }
  else
  {
    // the meta and actual data should be splited as two steps both of them are used nonblocking
    // send/recv the group that still exist
    // for existing procs, they accept the metadata and generate the new mandelbulb instances
    // every existing procs recv the metadata (block number) from the leaving group
    for (int sendid = localnprocs - process_leave_num; sendid <= localnprocs - 1; sendid++)
    {
      int offset = sendid - (localnprocs - process_leave_num);
      // std::cout << "rank localrank " << localrank << " sendid " << sendid << " offset " << offset
      //          << std::endl;
      unsigned recv_block_num = 0;
      MPI_Recv(&recv_block_num, 1, MPI_UNSIGNED, sendid, tag_block_num, subcomm, MPI_STATUS_IGNORE);

      if (recv_block_num > 0)
      {
        std::vector<unsigned> block_offset_list(recv_block_num);

        MPI_Recv(block_offset_list.data(), recv_block_num, MPI_UNSIGNED, sendid,
          tag_block_offsetlist, subcomm, MPI_STATUS_IGNORE);
        // std::cout << "debug rank localrank " << localrank << " recv offset num " <<
        // recv_block_num
        //          << " from " << sendid << std::endl;
        // generate new existance based on the blocks offset
        for (int j = 0; j < recv_block_num; j++)
        {
          // push mandelbulb into the new list
          MandelbulbList.push_back(
            Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset_list[j], 1.2, total_block_number));
        }
      }
    }
  }
}

// this is executed by procs in comm world
// existing procs will distribute there work load to the new added procs
// only support one procs to be added each time currently
// it is complicated to decide which procs send to which one if there are multiple new procs
void processJoin(MPI_Comm subcomm, int globalrank, int localnprocs, int& color,
  int total_block_number, std::vector<Mandelbulb>& MandelbulbList)
{
  int tag_block_num = 333;
  int tag_block_offsetlist = 444;
  // add one process back into the pool
  // the proc with id that equals to localnprocs will be added
  if (globalrank == localnprocs)
  {
    // this is the new proc that will join into the subcomm
    color = 0;
    // clear existing mandelbulb list
    MandelbulbList.clear();
    // this proc will recv block metadata and offset list from other procs
    // the global id should be the 0 to localnprocs-1 for other procs
    unsigned recv_block_num = 0;
    // current procs may recv some blocks from procs in the original existing group
    for (int sendid = 0; sendid < localnprocs; sendid++)
    {
      MPI_Recv(&recv_block_num, 1, MPI_UNSIGNED, sendid, tag_block_num, MPI_COMM_WORLD,
        MPI_STATUSES_IGNORE);
      if (recv_block_num != 0)
      {
        std::vector<unsigned> block_offset_list(recv_block_num);
        // recv offset list if it recv nonzero blocks from other procs
        MPI_Recv(block_offset_list.data(), recv_block_num, MPI_UNSIGNED, sendid,
          tag_block_offsetlist, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);

        // insert the mandelbulb instance into current procs
        for (int i = 0; i < recv_block_num; i++)
        {
          // push mandelbulb into the new list
          MandelbulbList.push_back(
            Mandelbulb(WIDTH, HEIGHT, DEPTH, block_offset_list[i], 1.2, total_block_number));
        }
      }
    }
  }
  // current existing group
  if (subcomm != MPI_COMM_NULL)
  {

    unsigned newgroupsize = localnprocs + 1;
    // caculate the new block number at each proc after new procs join
    unsigned new_block_number_base = (total_block_number / newgroupsize);
    unsigned new_block_number_reminder = (total_block_number % newgroupsize);
    unsigned local_reminder = 0;
    if (new_block_number_reminder != 0)
    {
      // there is reminder
      // the procs id from 0 to new_block_number_reminder-1 will contain reminder
      if (globalrank >= 0 && globalrank <= new_block_number_reminder - 1)
      {
        local_reminder = 1;
      }
    }

    unsigned new_block_number = new_block_number_base + local_reminder;
    unsigned send_block_num = MandelbulbList.size() - new_block_number;
    unsigned new_proc_global_id = localnprocs;

    // std::cout << "debug rank "<< globalrank << " send " << total_send_block_num << " to id " <<
    // new_proc_global_id << std::endl; send the metadata to dedicated procs, the newcoming id is
    // new_proc_global_id
    MPI_Send(&send_block_num, 1, MPI_UNSIGNED, new_proc_global_id, tag_block_num, MPI_COMM_WORLD);

    if (send_block_num > 0)
    {
      std::vector<unsigned> send_offset_list;
      for (int i = 0; i < send_block_num; i++)
      {
        unsigned offset = MandelbulbList[i].GetZoffset();
        send_offset_list.push_back(offset);
      }
      MPI_Send(send_offset_list.data(), send_block_num, MPI_UNSIGNED, new_proc_global_id,
        tag_block_offsetlist, MPI_COMM_WORLD);
      // remove the first send_block_num elements
      // TODO since they are constructed in other procs
      // if use an array to store the mandeulbulb element, it may requres the element movement
      // when erase and the assignment operator need to be used(which is deleted in this
      // example)
      MandelbulbList.erase(MandelbulbList.begin(), MandelbulbList.begin() + send_block_num);
    }
  }
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
        processLeave(subcomm, localrank, localnprocs, color, total_block_number, MandelbulbList,
          localnprocs - 1, step % globalnprocs);
      }
      else
      {
        processLeave(subcomm, localrank, localnprocs, color, total_block_number, MandelbulbList,
          process_granularity, step % globalnprocs);
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
      processJoin(subcomm, globalrank, localnprocs, color, total_block_number, MandelbulbList);
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
    fixMochiCommSize(scriptname, total_block_number, totalstep);
  }
  else
  {
    throw std::runtime_error("only support fixed comm size currently");
    variedMPICommSize(scriptname, total_block_number, process_granularity, totalstep);
  }

  MPI_Finalize();

  return 0;
}

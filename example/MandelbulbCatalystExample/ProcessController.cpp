#include "ProcessController.hpp"
#include "Mandelbulb_dynamic.hpp"
#include <unistd.h>

namespace PController
{

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

void processLeave(mona_comm_t subcomm, int localrank, int localnprocs, int& color,
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
    color = MONA_UNDEFINED;
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
      DEBUG("debug rank " << localrank << " send migragate_block_per_proc "
                          << migragate_block_per_proc << " to " << id);

      mona_comm_send(subcomm, &migragate_block_per_proc, sizeof(unsigned), id, tag_block_num);
      if (migragate_block_per_proc > 0)
      {
        std::vector<unsigned> offsetList;
        for (int k = 0; k < migragate_block_per_proc; k++)
        {
          offsetList.push_back(MandelbulbList[migragate_index].GetZoffset());
          migragate_index++;
        }

        // send offsetlist to dedicated procs
        DEBUG("debug rank " << localrank << " send offset number " << migragate_block_per_proc
                            << " to " << id);

        mona_comm_send(subcomm, offsetList.data(), sizeof(unsigned) * migragate_block_per_proc, id,
          tag_block_offsetlist);
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

      unsigned recv_block_num = 0;

      mona_comm_recv(
        subcomm, &recv_block_num, sizeof(unsigned), sendid, tag_block_num, NULL, NULL, NULL);

      DEBUG("rank localrank " << localrank << " sendid " << sendid << " recv_block_num "
                              << recv_block_num);

      if (recv_block_num > 0)
      {
        std::vector<unsigned> block_offset_list(recv_block_num);

        mona_comm_recv(subcomm, block_offset_list.data(), sizeof(unsigned) * recv_block_num, sendid,
          tag_block_offsetlist, NULL, NULL, NULL);

        DEBUG("debug rank localrank " << localrank << " recv offset num " << recv_block_num
                                      << " from send id " << sendid);

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

void processJoin(mona_comm_t subcomm, mona_comm_t boostrapcomm, int globalrank, int localnprocs,
  int& color, int total_block_number, std::vector<Mandelbulb>& MandelbulbList)
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

      mona_comm_recv(
        boostrapcomm, &recv_block_num, sizeof(unsigned), sendid, tag_block_num, NULL, NULL, NULL);

      if (recv_block_num != 0)
      {
        DEBUG("rank " << globalrank << " recv " << recv_block_num << " from " << sendid
                      << " for process join");
        std::vector<unsigned> block_offset_list(recv_block_num);
        // recv offset list if it recv nonzero blocks from other procs
        // MPI_Recv(block_offset_list.data(), recv_block_num, MPI_UNSIGNED, sendid,
        //  tag_block_offsetlist, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
        mona_comm_recv(boostrapcomm, block_offset_list.data(), sizeof(unsigned) * recv_block_num,
          sendid, tag_block_offsetlist, NULL, NULL, NULL);
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
  if (subcomm != NULL)
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

    DEBUG(
      "debug rank " << globalrank << " send " << send_block_num << " to id " << new_proc_global_id);
    // send the metadata to dedicated procs, the newcoming id is new_proc_global_id
    mona_comm_send(
      boostrapcomm, &send_block_num, sizeof(unsigned), new_proc_global_id, tag_block_num);
    if (send_block_num > 0)
    {
      std::vector<unsigned> send_offset_list;
      for (int i = 0; i < send_block_num; i++)
      {
        unsigned offset = MandelbulbList[i].GetZoffset();
        send_offset_list.push_back(offset);
      }

      mona_comm_send(boostrapcomm, send_offset_list.data(), sizeof(unsigned) * send_block_num,
        new_proc_global_id, tag_block_offsetlist);

      // remove the first send_block_num elements
      // TODO since they are constructed in other procs
      // if use an array to store the mandeulbulb element, it may requres the element movement
      // when erase and the assignment operator need to be used(which is deleted in this
      // example)
      MandelbulbList.erase(MandelbulbList.begin(), MandelbulbList.begin() + send_block_num);
    }
  }
}

}
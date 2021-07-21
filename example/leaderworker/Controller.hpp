#ifndef __CONTROLLER_HPP
#define __CONTROLLER_HPP

#include "ControllerClient.hpp"
#include "ControllerProvider.hpp"
#include "common.hpp"

namespace tl = thallium;

#define MONA_TESTLEAVE_BARRIER_TAG 2023

struct Controller
{

  Controller(tl::engine* engineptr, std::string mona_addr, uint16_t provider_id, int procs,
    LeaderMeta* leader_meta, CommonMeta* common_meta, std::string leaderAddrConfig, int rank)
  {

    // create the client
    // the new joined process need to load the leader info from the config
    m_controller_client = std::make_unique<ControllerClient>(
      ControllerClient(engineptr, provider_id, leader_meta, common_meta, leaderAddrConfig));

    if (leader_meta != nullptr)
    {
      // for the master case
      // set the initial process number that we need to sync
      // this is supposed to called once by leader process
      m_controller_client->expectedUpdatingProcess(procs);
      spdlog::info("rank {} create master controller, update expected process", rank);
    }

    // make sure the pending number is set for the inital processes then do subsequent works
    MPI_Barrier(MPI_COMM_WORLD);

    // if the controller is created first time
    // this is initial part, we need to register process to server
    // if this is new added, the expected process number have been set
    // every one register its process, include the leader itsself
    spdlog::info("rank {} registerProcessToLeader", rank);
    m_controller_client->registerProcessToLeader(mona_addr);
    spdlog::info("rank {} registerProcessToLeader ok", rank);
  };

  ~Controller(){};

  std::unique_ptr<ControllerClient> m_controller_client;
  int m_addedServerID = 1;

  // this is supposed to be called by leader process
  void naiveJoin(int iteration)
  {
    if (iteration == 3)
    {
      // assume the server number is 1 for naive cases
      int serverNum = 1;
      // tell leader that we plan to add process
      m_controller_client->expectedUpdatingProcess(serverNum);

      // trigger new processes
      std::string startStagingCommand = "/bin/bash ./addprocess.sh";
      spdlog::info("original command: {}", startStagingCommand);
      for (int i = 0; i < serverNum; i++)
      {
        char strid[10];
        sprintf(strid, "%02d", this->m_addedServerID);
        std::string command = startStagingCommand + " " + std::string(strid);

        // use systemcall to start ith server
        spdlog::info("Add server by command: {}", command);
        std::system(command.c_str());
        this->m_addedServerID++;
      }
    }
  }
  // the pending process num at the leader must be udpated firstly

  bool naiveLeave(int iteration, int leaveNum, int monaRank)
  {
    bool leave = false;
    if (iteration == 4)
    {
      if (monaRank == 0)
      {
        // tell leader that we plan to add process
        m_controller_client->expectedUpdatingProcess(leaveNum);
      }
      mona_comm_barrier(m_controller_client->m_mona_comm, MONA_TESTLEAVE_BARRIER_TAG);

      // do the leave operation
      if (monaRank >= 1 && monaRank <= leaveNum)
      {
        // call the leave RPC, send the req to the leader to deregister the addr

        spdlog::debug("mona rank {} call the process leave", monaRank);
        m_controller_client->removeProcess();
        leave = true;
      }
    }
    return leave;
  }

  bool naiveLeave2(int iteration, int leaveNum, int procs, int monaRank)
  {
    bool leave = false;
    if (iteration == 5)
    {
      if (monaRank == 0)
      {
        // tell leader that we plan to add process
        m_controller_client->expectedUpdatingProcess(leaveNum);
      }
      mona_comm_barrier(m_controller_client->m_mona_comm, MONA_TESTLEAVE_BARRIER_TAG);

      // do the leave operation
      if (monaRank >= procs - leaveNum)
      {
        // call the leave RPC, send the req to the leader to deregister the addr

        spdlog::debug("mona rank {} call the process leave", monaRank);
        m_controller_client->removeProcess();
        leave = true;
      }
    }
    return leave;
  }

  // how to let process itsself know it should leave?
  // all get the gjoin list and rank id, then the first one in gjoin list should move
  // or for testing, from last to the first
};

#endif
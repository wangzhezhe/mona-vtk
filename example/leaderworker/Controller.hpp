// this is the controller for process sync, not model caculation things
// it is more like the rescaller plus the mechnism to sync the simulation process
#ifndef __CONTROLLER_HPP
#define __CONTROLLER_HPP

#include "ControllerClient.hpp"
#include "ControllerProvider.hpp"
#include "DynamicProcessController.hpp"
#include "common.hpp"

namespace tl = thallium;

#define MONA_TESTLEAVE_BARRIER_TAG 2023
#define MONA_TESTLEAVE_BCAST_TAG 2024

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

  // varaibles in the class
  std::unique_ptr<ControllerClient> m_controller_client;
  int m_addedServerID = 1;
  DynamicProcessController m_dmpc;

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
    //if (iteration % 2 == 0 && iteration != 0)
    if (iteration != 0)
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
  // different rank have differnet operations to control the process leaving
  bool dynamicLeave(
    int iteration, int stagingNum, int simNum, int simRank, int& avalibleDynamicProcess)
  {
    if (iteration == 0 || avalibleDynamicProcess == 0)
    {
      // we do nothing at the first iteration since the proc value is not initilized
      return false;
    }

    // decide leavenumber for master and bcast to all clients
    bool leave = false;
    int leaveNum = 0;
    if (simRank == 0)
    {
      // decide the leave number
      // not enough data return 1, else return
      // TODO, the 5.0 should sent from the caller in future
      // current testing, the normal computation time is around 6.0, we set the target as 5.0
      leaveNum = this->m_dmpc.dynamicAddProcessToStaging("default", stagingNum, 5.0);

      if (leaveNum > avalibleDynamicProcess)
      {
        leaveNum = avalibleDynamicProcess;
        avalibleDynamicProcess = 0;
      }
      else
      {
        avalibleDynamicProcess -= leaveNum;
      }

      spdlog::info(
        "iteration {} we caculate the process leave number {}, avalibleDynamicProcess {}",
        iteration, leaveNum, avalibleDynamicProcess);

      // tell leader that we plan to add process to data staging service
      m_controller_client->expectedUpdatingProcess(leaveNum);
    }

    // mona_comm_barrier(m_controller_client->m_mona_comm, MONA_TESTLEAVE_BARRIER_TAG);
    mona_comm_bcast(this->m_controller_client->m_mona_comm, &leaveNum, sizeof(leaveNum), 0,
      MONA_TESTLEAVE_BCAST_TAG);

    // do the leave operation for non zero process
    if (simRank >= simNum - leaveNum)
    {
      // call the leave RPC, send the req to the leader to deregister the addr
      spdlog::debug(
        "mona rank {} call the process leave, simNum {} leaveNum {}", simRank, simNum, leaveNum);
      m_controller_client->removeProcess();
      leave = true;
    }
    return leave;
  }
};
#endif
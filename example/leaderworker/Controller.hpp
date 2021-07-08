#include "ControllerClient.hpp"
#include "ControllerProvider.hpp"
#include "common.hpp"

namespace tl = thallium;

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
      // set the initial process number that we need to sync
      // this is supposed to called once by leader process
      m_controller_client->expectedUpdatingProcess(procs);
      spdlog::info("rank {} create master controller, update expected process", rank);
    }

    // make sure the pending number is set then do subsequent works
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
  void naive(int iteration)
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
};

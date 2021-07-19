// the provider for the data staging service

#ifndef __STAGING_CLIENT_H
#define __STAGING_CLIENT_H

#include <dlfcn.h>
#include <mona-coll.h>
#include <mona.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <tuple>
#include <vector>

using namespace std::string_literals;
namespace tl = thallium;

#define MONA_SYNCVIEW_BCAST_TAG1 2025
#define MONA_SYNCVIEW_BCAST_TAG2 2026

// TODO add pipeline things in future
class StagingClient
{

public:
  std::map<std::string, tl::endpoint> m_addrToEndpoints;
  tl::engine* m_engineptr;
  std::string m_stagingLeader;
  tl::endpoint m_stagingLeaderEndpoint;
  std::vector<std::string> m_stagingView;
  int m_viewUpdated = 0;

  std::string loadLeaderAddr(std::string leaderAddrConfig)
  {

    std::ifstream infile(leaderAddrConfig);
    std::string content = "";
    std::getline(infile, content);
    if (content.compare("") == 0)
    {
      std::getline(infile, content);
      if (content.compare("") == 0)
      {
        throw std::runtime_error("failed to load the master server\n");
      }
    }
    return content;
  }

  StagingClient(tl::engine* engineptr, std::string stagingLeaderAddrConfig)
  {
    m_engineptr = engineptr;
    m_stagingLeader = loadLeaderAddr(stagingLeaderAddrConfig);
    spdlog::debug("load staging leader {}", m_stagingLeader);
    m_stagingLeaderEndpoint = this->m_engineptr->lookup(m_stagingLeader);
  }

  ~StagingClient() {}

  void leadersync(int iteration)
  {
    tl::remote_procedure syncRPC = this->m_engineptr->define("syncview");
    // tl::remote_procedure helloRPC = this->m_engineptr->define("helloRPC");
    // helloRPC.on(ph)();

    tl::provider_handle ph(m_stagingLeaderEndpoint, 0);
    int addrSize = this->m_stagingView.size();

    std::vector<std::string> returnAddrs = syncRPC.on(ph)(iteration, addrSize);
    spdlog::debug("sent sync rpc");

    if (returnAddrs.size() > 0)
    {
      spdlog::info(
        "staging view is updated from {} to {}", this->m_stagingView.size(), returnAddrs.size());
      this->m_stagingView = returnAddrs;
      this->m_viewUpdated = 1;
    }
  }

  void workerSync(bool leader, int iteration, int monarank)
  {
    if (leader)
    {
      // bcast updated
      if (this->m_viewUpdated == 0)
      {
        // na_return_t ret = mona_comm_bcast(controller.m_controller_client->m_mona_comm,
        //  &this->m_viewUpdated, sizeof(int), monarank, MONA_SYNCVIEW_BCAST_TAG1);
        MPI_Bcast(&this->m_viewUpdated, 1, MPI_INT, 0, MPI_COMM_WORLD);
      }
      else if (this->m_viewUpdated == 1)
      {
        int group_size = this->m_stagingView.size();
        spdlog::info("iteration {} groupsize {}", iteration, group_size);
        // we tell the new group size to the worker direactly in this case
        // na_return_t ret = mona_comm_bcast(controller.m_controller_client->m_mona_comm,
        // &group_size,
        // sizeof(int), 0, MONA_SYNCVIEW_BCAST_TAG1);
        // this is code that assume client do not change, otherwise, we use the mona or wrapper for
        // it
        MPI_Bcast(&group_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        // use the packet addr
        std::vector<char> packed_addresses(group_size * 256);
        for (int i = 0; i < group_size; i++)
        {
          spdlog::info("iteration {} pack addr {}", iteration, this->m_stagingView[i]);
          strcpy(packed_addresses.data() + i * 256, this->m_stagingView[i].c_str());
        }
        // na_return_t ret = mona_comm_bcast(controller.m_controller_client->m_mona_comm,
        //  packed_addresses.data(), packed_addresses.size(), monarank, MONA_SYNCVIEW_BCAST_TAG2);

        MPI_Bcast(packed_addresses.data(), packed_addresses.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
      }
      else
      {
        throw std::runtime_error("wrong this->m_viewUpdated value");
      }

      // reset indicator to 0
      this->m_viewUpdated = 0;
    }
    else
    {
      // bcast updated
      int viewUpdated = 0;
      // na_return_t ret = mona_comm_bcast(controller.m_controller_client->m_mona_comm,
      // &viewUpdated,
      //  sizeof(int), monarank, MONA_SYNCVIEW_BCAST_TAG1);
      MPI_Bcast(&viewUpdated, 1, MPI_INT, 0, MPI_COMM_WORLD);

      // if not updated return
      if (viewUpdated != 0)
      {
        int group_size = viewUpdated;
        // use the packet addr
        std::vector<char> packed_addresses(group_size * 256);

        // na_return_t ret = mona_comm_bcast(controller.m_controller_client->m_mona_comm,
        //  packed_addresses.data(), packed_addresses.size(), monarank, MONA_SYNCVIEW_BCAST_TAG2);

        MPI_Bcast(packed_addresses.data(), packed_addresses.size(), MPI_BYTE, 0, MPI_COMM_WORLD);

        // check the results
        for (int i = 0; i < group_size; i++)
        {
          char* addr = packed_addresses.data() + i * 256;
          spdlog::debug(
            "iteration {} rank {} check addr {}", iteration, monarank, std::string(addr));
        }
      }
    }
  }

  void updateExpectedProcess(std::string operation, int num)
  {

    tl::remote_procedure updateExpectedProcessNumRPC =
      this->m_engineptr->define("updateExpectedProcessNum");
    int result = updateExpectedProcessNumRPC.on(this->m_stagingLeaderEndpoint)(operation, num);
    if (result != 0)
    {
      throw std::runtime_error("failed to updateExpectedProcess");
    }
  }

  void removeProcess(std::string thalliumAddr)
  {

    // deregister the thallium server from the leader
    tl::remote_procedure removeMonaAddr = this->m_engineptr->define("removeProcess");

    tl::provider_handle ph(m_stagingLeaderEndpoint, 0);
    int result = removeMonaAddr.on(ph)(thalliumAddr);
    if (result != 0)
    {
      throw std::runtime_error("failed to call removeProcess");
    }
    spdlog::debug("ok to call removeMonaAddr {}", thalliumAddr);

    // shut down that server remotely
    tl::remote_procedure shutdownRPC =
      this->m_engineptr->define("shutdownprocess").disable_response();
    auto endpoint = this->m_engineptr->lookup(thalliumAddr);
    tl::provider_handle ph2(endpoint, 0);

    shutdownRPC.on(ph2)();
    spdlog::debug("ok to call shutdownprocess");

    return;
  }
};

#endif

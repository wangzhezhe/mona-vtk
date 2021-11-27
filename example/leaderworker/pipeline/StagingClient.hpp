// the provider for the data staging service

#ifndef __STAGING_CLIENT_HPP
#define __STAGING_CLIENT_HPP

#include <dlfcn.h>
#include <mona-coll.h>
#include <mona.h>
#include <spdlog/spdlog.h>

#include "../TypeSizes.hpp"
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

  // for backend
  tl::remote_procedure m_stage;
  tl::remote_procedure m_execute;
  tl::remote_procedure m_cleanup;

  // for management
  tl::remote_procedure m_syncview;
  tl::remote_procedure m_updateExpectedProcessNum;
  tl::remote_procedure m_removeProcess;
  tl::remote_procedure m_shutdownprocess;

  // define lookup

  tl::endpoint lookup(const std::string& address)
  {
    auto it = m_addrToEndpoints.find(address);
    if (it == m_addrToEndpoints.end())
    {
      // do not lookup here to avoid the potential mercury race condition
      // throw std::runtime_error("failed to find addr, cache the endpoint at the constructor\n");
      auto endpoint = this->m_engineptr->lookup(address);
      std::string tempAddr = address;
      this->m_addrToEndpoints[tempAddr] = endpoint;
      return endpoint;
    }
    return it->second;
  }

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
    : m_stage(engineptr->define("stage"))
    , m_execute(engineptr->define("execute"))
    , m_cleanup(engineptr->define("cleanup"))
    , m_syncview(engineptr->define("syncview"))
    , m_updateExpectedProcessNum(engineptr->define("updateExpectedProcessNum"))
    , m_removeProcess(engineptr->define("removeProcess"))
    , m_shutdownprocess(engineptr->define("shutdownprocess").disable_response())

  {
    m_engineptr = engineptr;
    m_stagingLeader = loadLeaderAddr(stagingLeaderAddrConfig);
    spdlog::debug("load staging leader {}", m_stagingLeader);
    m_stagingLeaderEndpoint = this->lookup(m_stagingLeader);
  }

  ~StagingClient() {}

  void leadersync(int iteration)
  {
    // tl::remote_procedure syncRPC = this->m_engineptr->define("syncview");
    // tl::remote_procedure helloRPC = this->m_engineptr->define("helloRPC");
    // helloRPC.on(ph)();

    tl::provider_handle ph(m_stagingLeaderEndpoint, 0);
    int addrSize = this->m_stagingView.size();

    std::vector<std::string> returnAddrs = m_syncview.on(ph)(iteration, addrSize);
    spdlog::debug("sent sync rpc");

    if (returnAddrs.size() > 0)
    {
      spdlog::info(
        "iteration {} staging view is updated from {} to {}", iteration, this->m_stagingView.size(), returnAddrs.size());
      this->m_stagingView = returnAddrs;
      this->m_viewUpdated = 1;
    }
  }

  void workerSync(bool leader, int iteration, int rank)
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
        this->m_stagingView.clear();
        for (int i = 0; i < group_size; i++)
        {
          char* addr = packed_addresses.data() + i * 256;
          // spdlog::debug("iteration {} rank {} check addr {}", iteration, rank,
          // std::string(addr)); updte current stageView
          this->m_stagingView.push_back(std::string(addr));
        }
      }
    }
  }

  void workerSyncMona(bool leader, int iteration, int monarank, mona_comm_t mona_comm)
  {
    if (leader)
    {
      // bcast updated
      if (this->m_viewUpdated == 0)
      {
        na_return_t ret = mona_comm_bcast(
          mona_comm, &this->m_viewUpdated, sizeof(int), 0, MONA_SYNCVIEW_BCAST_TAG1);
        // MPI_Bcast(&this->m_viewUpdated, 1, MPI_INT, 0, MPI_COMM_WORLD);
      }
      else if (this->m_viewUpdated == 1)
      {
        int group_size = this->m_stagingView.size();
        spdlog::info("iteration {} groupsize {}", iteration, group_size);
        // we tell the new group size to the worker direactly in this case
        na_return_t ret =
          mona_comm_bcast(mona_comm, &group_size, sizeof(int), 0, MONA_SYNCVIEW_BCAST_TAG1);
        // MPI_Bcast(&group_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        // use the packet addr
        std::vector<char> packed_addresses(group_size * 256);
        for (int i = 0; i < group_size; i++)
        {
          spdlog::info("iteration {} pack addr {}", iteration, this->m_stagingView[i]);
          strcpy(packed_addresses.data() + i * 256, this->m_stagingView[i].c_str());
        }
        ret = mona_comm_bcast(
          mona_comm, packed_addresses.data(), packed_addresses.size(), 0, MONA_SYNCVIEW_BCAST_TAG2);
        // MPI_Bcast(packed_addresses.data(), packed_addresses.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
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
      na_return_t ret =
        mona_comm_bcast(mona_comm, &viewUpdated, sizeof(int), 0, MONA_SYNCVIEW_BCAST_TAG1);
      // MPI_Bcast(&viewUpdated, 1, MPI_INT, 0, MPI_COMM_WORLD);

      // if not updated return
      if (viewUpdated != 0)
      {
        int group_size = viewUpdated;
        // use the packet addr
        std::vector<char> packed_addresses(group_size * 256);

        ret = mona_comm_bcast(
          mona_comm, packed_addresses.data(), packed_addresses.size(), 0, MONA_SYNCVIEW_BCAST_TAG2);

        // MPI_Bcast(packed_addresses.data(), packed_addresses.size(), MPI_BYTE, 0, MPI_COMM_WORLD);

        // check and updates results
        this->m_stagingView.clear();
        for (int i = 0; i < group_size; i++)
        {
          char* addr = packed_addresses.data() + i * 256;
          // spdlog::debug(
          //  "iteration {} rank {} check addr {}", iteration, monarank, std::string(addr));
          this->m_stagingView.push_back(std::string(addr));
        }
      }
    }
  }

  void updateExpectedProcess(std::string operation, int num)
  {

    // tl::remote_procedure updateExpectedProcessNumRPC =
    //  this->m_engineptr->define("updateExpectedProcessNum");
    int result = m_updateExpectedProcessNum.on(this->m_stagingLeaderEndpoint)(operation, num);
    if (result != 0)
    {
      throw std::runtime_error("failed to updateExpectedProcess");
    }
  }

  void removeProcess(std::string thalliumAddr)
  {

    // deregister the thallium server from the leader
    // tl::remote_procedure removeMonaAddr = this->m_engineptr->define("removeProcess");

    tl::provider_handle ph(m_stagingLeaderEndpoint, 0);
    int result = m_removeProcess.on(ph)(thalliumAddr);
    if (result != 0)
    {
      throw std::runtime_error("failed to call removeProcess");
    }
    spdlog::debug("ok to call removeMonaAddr {}", thalliumAddr);

    // shut down that server remotely
    // tl::remote_procedure shutdownRPC =
    //  this->m_engineptr->define("shutdownprocess").disable_response();
    auto endpoint = this->lookup(thalliumAddr);
    tl::provider_handle ph2(endpoint, 0);

    m_shutdownprocess.on(ph2)();
    spdlog::debug("ok to call shutdownprocess");

    return;
  }

  // only call one exeucte for each iteration for each process
  std::vector<tl::async_response> execute(
    uint64_t iteration, std::string& dataset_name, bool leader)
  {
    std::vector<tl::async_response> asyncResponses;

    if (leader == false)
    {
      return asyncResponses;
    }
    // for the leader
    // send execute rpc to every one in the stageView
    for (auto& addr : this->m_stagingView)
    {
      auto endpoint = this->lookup(addr);
      tl::provider_handle ph(endpoint, 0);
      auto response = m_execute.on(ph).async(dataset_name, iteration);
      asyncResponses.push_back(std::move(response));
    }

    return asyncResponses;
  }

  void cleanup(int monarank, std::string& dataset_name, uint64_t iteration)
  {
    // choose a suitable server to create ph
    int stagingSize = this->m_stagingView.size();
    if (stagingSize == 0)
    {
      throw std::runtime_error(
        "stage api, stagingSize is not supposed to be zero for rank " + std::to_string(monarank));
    }
    spdlog::debug("start iteration {} cleanup for monarank {}", iteration, stagingSize, monarank);
    int serverid = monarank % stagingSize;
    auto ep = this->lookup(this->m_stagingView[serverid]);

    tl::provider_handle ph(ep, 0);
    auto sender_addr = static_cast<std::string>(this->m_engineptr->self());

    // send the RPC
    int result = this->m_cleanup.on(ph)(dataset_name, iteration);
    if (result != 0)
    {
      throw std::runtime_error("failed to cleanup iteration rank id :" + std::to_string(iteration) +
        "," + std::to_string(monarank) + "," + dataset_name);
    }

    return;
  }

  void stage(const std::string& dataset_name, uint64_t iteration, uint64_t block_id,
    const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets, const Type& type,
    const void* data, int monarank)
  {

    // choose a suitable server to create ph
    int stagingSize = this->m_stagingView.size();
    if (stagingSize == 0)
    {
      throw std::runtime_error(
        "stage api, stagingSize is not supposed to be zero for rank " + std::to_string(monarank));
    }
    spdlog::debug(
      "start iteration {} stage view size {} monarank {}", iteration, stagingSize, monarank);
    int serverid = monarank % stagingSize;
    auto ep = this->lookup(this->m_stagingView[serverid]);

    tl::provider_handle ph(ep, 0);
    auto sender_addr = static_cast<std::string>(this->m_engineptr->self());

    std::vector<std::pair<void*, size_t> > segment(1);
    segment[0].first = const_cast<void*>(data);
    segment[0].second = ComputeDataSize(dimensions, type);
    auto bulk = this->m_engineptr->expose(segment, tl::bulk_mode::read_only);

    // synchronous call
    // sender addr is used for rpc call to pull the data
    int result = this->m_stage.on(ph)(
      sender_addr, dataset_name, iteration, block_id, dimensions, offsets, type, bulk);
    if (result != 0)
    {
      throw std::runtime_error("failed to stage iteration rank id :" + std::to_string(iteration) +
        "," + std::to_string(monarank) + "," + std::to_string(block_id));
    }

    // spdlog::debug(
    //  "ok for start iteration {} stage view size {} monarank {}", iteration, stagingSize,
    //  monarank);

    return;
  }

  tl::async_response asyncstage(const std::string& dataset_name, uint64_t iteration,
    uint64_t block_id, const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets,
    const Type& dataType, const void* data, int monarank)
  {

    // choose a suitable server to create ph
    int stagingSize = this->m_stagingView.size();
    if (stagingSize == 0)
    {
      throw std::runtime_error(
        "stage api, stagingSize is not supposed to be zero for rank " + std::to_string(monarank));
    }
    spdlog::debug(
      "start iteration {} stage view size {} monarank {}", iteration, stagingSize, monarank);
    int serverid = monarank % stagingSize;
    auto ep = this->lookup(this->m_stagingView[serverid]);

    tl::provider_handle ph(ep, 0);
    auto sender_addr = static_cast<std::string>(this->m_engineptr->self());

    std::vector<std::pair<void*, size_t> > segment(1);
    segment[0].first = const_cast<void*>(data);
    segment[0].second = ComputeDataSize(dimensions, dataType);
    auto bulk = this->m_engineptr->expose(segment, tl::bulk_mode::read_only);

    // synchronous call
    // sender addr is used for rpc call to pull the data
    auto response = this->m_stage.on(ph).async(
      sender_addr, dataset_name, iteration, block_id, dimensions, offsets, dataType, bulk);


    // spdlog::debug(
    //  "ok for start iteration {} stage view size {} monarank {}", iteration, stagingSize,
    //  monarank);
    return response;
  }
};

#endif

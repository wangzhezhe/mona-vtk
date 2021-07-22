// the provider for the data staging service

#ifndef __STAGING_PROVIDER_H
#define __STAGING_PROVIDER_H

#include <dlfcn.h>
#include <mona-coll.h>
#include <mona.h>
#include <spdlog/spdlog.h>

#include "StagingMeta.hpp"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <tuple>
#include <vector>

#include "MonaBackend.hpp"

using namespace std::string_literals;
namespace tl = thallium;

// TODO add pipeline things in future
class StagingProvider : public tl::provider<StagingProvider>
{
  auto id() const { return get_provider_id(); } // for convenience

  using json = nlohmann::json;

public:
  // control client to access necessary info
  // this info should binded with provider since the provider needed to start earlier than client
  std::unique_ptr<StagingLeaderMeta> m_stageleader_meta;
  std::unique_ptr<StagingCommonMeta> m_stagecommon_meta;
  std::map<std::string, tl::endpoint> m_addrToEndpoints;

  StagingProvider(const tl::engine& engine, uint16_t provider_id, bool leader, mona_instance_t mona,
    std::string mona_addr, std::string leaderAddrConfig, std::string stagingScriptFile)
    : tl::provider<StagingProvider>(engine, provider_id)
  {
    // for consistency management
    define("addProcess", &StagingProvider::addProcess);
    define("removeProcess", &StagingProvider::removeProcess);
    // reset the expected process number into k, only for leader
    define("resetExpectedProcessNum", &StagingProvider::resetExpectedProcessNum);
    // add k or remove k for expected process number, only for leader
    define("updateExpectedProcessNum", &StagingProvider::updateExpectedProcessNum);
    // the leader update the addr list of the worker
    define("updateMonaAddrList", &StagingProvider::updateMonaAddrList);

    // call pipeline stage and execution
    define("stage", &StagingProvider::stage);
    define("execute", &StagingProvider::execute);

    // expose to sim, this is only exposed by the leader process
    // the sim process will call these api to change the status of the pipeline
    define("syncview", &StagingProvider::syncview);
    // define("activate", &StagingProvider::activate);
    // define("deactivate", &StagingProvider::deactivate);
    define("shutdownprocess", &StagingProvider::shutdownprocess).disable_response();

    // this is for simple testing
    define("helloRPC", &StagingProvider::hello);

    this->m_stagecommon_meta = std::make_unique<StagingCommonMeta>(StagingCommonMeta());
    this->m_stagecommon_meta->m_thallium_self_addr = (std::string)(this->get_engine().self());
    this->m_stagecommon_meta->m_mona = mona;
    this->m_stagecommon_meta->m_mona_self_addr = mona_addr;

    // create the pipeline
    // TODO make this more dynamic use factory method
    this->m_stagecommon_meta->m_pipeline =
      std::make_shared<MonaBackendPipeline>(engine, stagingScriptFile, mona);

    // load the master addr
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
    // record the leader addr
    this->m_stagecommon_meta->m_thallium_leader_addr = content;
    spdlog::debug("leader thallium addr {}", this->m_stagecommon_meta->m_thallium_leader_addr);

    if (leader)
    {
      // only leader contains the leader meta
      this->m_stageleader_meta = std::make_unique<StagingLeaderMeta>(StagingLeaderMeta());
      this->m_stagecommon_meta->m_ifleader = true;
    }
  }

  ~StagingProvider()
  {
    spdlog::trace("[provider:{}] Deregistering provider", id());
    spdlog::trace("[provider:{}]    => done!", id());
  }

  // this is supposed to be called by leader service itsself
  void setExpectedNum(int num)
  {
    std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_workernum_mtx);
    this->m_stageleader_meta->m_expected_worker_num = num;
  }

  void addLeaderAddr(std::string leaderThallimAddr, std::string leaderMonaAddr)
  {
    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);
      this->m_stageleader_meta->m_mona_addresses_map[leaderThallimAddr] = leaderMonaAddr;
      this->m_stageleader_meta->m_added_list.push_back(leaderMonaAddr);
    }
  }

  tl::endpoint lookup(const std::string& address)
  {
    auto it = m_addrToEndpoints.find(address);
    if (it == m_addrToEndpoints.end())
    {
      // do not lookup here to avoid the potential mercury race condition
      // throw std::runtime_error("failed to find addr, cache the endpoint at the constructor\n");
      auto endpoint = this->get_engine().lookup(address);
      std::string tempAddr = address;
      this->m_addrToEndpoints[tempAddr] = endpoint;
      return endpoint;
    }
    return it->second;
  }

  // for non leader, try to register to leader
  void addrRegister()
  {
    // send self mona to the leader
    // use the default provider 0 in this case
    tl::remote_procedure addProcess = this->get_engine().define("addProcess");

    auto leaderEndpoint = this->lookup(this->m_stagecommon_meta->m_thallium_leader_addr);

    tl::provider_handle ph(leaderEndpoint, 0);
    int result = addProcess.on(ph)(
      this->m_stagecommon_meta->m_thallium_self_addr, this->m_stagecommon_meta->m_mona_self_addr);
    if (result != 0)
    {
      throw std::runtime_error("failt to register to leader");
    }
    return;
  }

  // this is supposed to be exposed by leader
  void resetExpectedProcessNum(const tl::request& req, int num)
  {
    std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_workernum_mtx);
    this->m_stageleader_meta->m_expected_worker_num = num;
    req.respond(0);
  }

  void updateExpectedProcessNum(const tl::request& req, std::string operaiton, int num)
  {
    std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_workernum_mtx);
    if (operaiton.compare("join") == 0)
    {
      this->m_stageleader_meta->m_expected_worker_num += num;
    }
    else if (operaiton.compare("leave") == 0)
    {
      this->m_stageleader_meta->m_expected_worker_num -= num;
    }
    else
    {
      throw std::runtime_error("unsupported operation for updateExpectedProcessNum");
    }

    spdlog::debug("m_expected_worker_num is updated to {} map size {}",
      this->m_stageleader_meta->m_expected_worker_num,
      this->m_stageleader_meta->m_mona_addresses_map.size());
    req.respond(0);
  }
  // leader process expose this RPC, the sim call this rpc to deregister
  // and then shut down the remote server, this is different with the server add logic
  // since for the server add logic, the new server register to the leader automatically
  void removeProcess(const tl::request& req, std::string& thalliumAddr)
  {
    spdlog::debug("RPC removeMonaAddr is called to deregister {}", thalliumAddr);
    // if this is old one , this is existing one, we find its addr by its id
    // no response
    // check member id, if not exist, throw error
    // if exist, remove
    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);
      if (this->m_stageleader_meta->m_mona_addresses_map.find(thalliumAddr) ==
        this->m_stageleader_meta->m_mona_addresses_map.end())
      {
        req.respond(1);
        throw std::runtime_error("the clientThalliumAddr " + thalliumAddr + "does not exist");
      }
      else
      {
        // remove list records which one need to be removed
        // then we send this message to all the workers during the sync process
        this->m_stageleader_meta->m_removed_list.push_back(
          this->m_stageleader_meta->m_mona_addresses_map[thalliumAddr]);

        // remove the addr from the addr map, which represents the actual registered process
        this->m_stageleader_meta->m_mona_addresses_map.erase(thalliumAddr);
      }
    }
    req.respond(0);
  }

  void shutdownprocess(const tl::request& req)
  {
    spdlog::info("rpc shutdown is called");
    this->get_engine().finalize();
    return;
  }

  // leader process expose this RPC
  // Attention! do not use the req.get_endpoint
  // the sender addr and server addr may not same
  // we need the server addr
  void addProcess(const tl::request& req, std::string thalliumAddr, std::string monaAddr)
  {
    // if added, this is a new process, we create a new id, return its id
    // create uuid, put it into the map, return the id
    spdlog::debug("RPC addProcess for server {}", thalliumAddr);

    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);

      // if the current addr is not stored into the map
      if (this->m_stageleader_meta->m_mona_addresses_map.find(thalliumAddr) ==
        this->m_stageleader_meta->m_mona_addresses_map.end())
      {
        // this addr is not exist in the map
        // put the thallium addr into it, this record the thallium addr instead of mona addr
        // the client will check this set, and update all mona addrs insted of the modified addrs
        // when the thallium addr is added in the m_first_added_set
        this->m_stageleader_meta->m_first_added_set.insert(thalliumAddr);
      }

      this->m_stageleader_meta->m_mona_addresses_map[thalliumAddr] = monaAddr;
      this->m_stageleader_meta->m_added_list.push_back(monaAddr);
    }

    req.respond(0);
  }

  // non-leader process expose this RPC
  // we need to decide the process is first added or it have been exist
  // if it have been exist, we need send
  void updateMonaAddrList(const tl::request& req, UpdatedMonaList& updatedMonaList)
  {

    for (int i = 0; i < updatedMonaList.m_mona_added_list.size(); i++)
    {
      this->m_stagecommon_meta->m_monaaddr_set.insert(updatedMonaList.m_mona_added_list[i]);
    }

    for (int i = 0; i < updatedMonaList.m_mona_remove_list.size(); i++)
    {
      this->m_stagecommon_meta->m_monaaddr_set.erase(updatedMonaList.m_mona_remove_list[i]);
    }

    if (updatedMonaList.m_mona_added_list.size() != 0 ||
      updatedMonaList.m_mona_remove_list.size() != 0)
    {
      // update the mona_comm
      // mona things are actually updated
      std::vector<na_addr_t> m_member_addrs;
      for (auto& p : this->m_stagecommon_meta->m_monaaddr_set)
      {

        na_addr_t addr = NA_ADDR_NULL;
        na_return_t ret = mona_addr_lookup(this->m_stagecommon_meta->m_mona, p.c_str(), &addr);
        if (ret != NA_SUCCESS)
        {
          throw std::runtime_error("failed for mona_addr_lookup");
        }

        m_member_addrs.push_back(addr);
      }

      na_return_t ret = mona_comm_create(this->m_stagecommon_meta->m_mona, m_member_addrs.size(),
        m_member_addrs.data(), &(this->m_stagecommon_meta->m_mona_comm));
      if (ret != 0)
      {
        spdlog::debug("{}: MoNA communicator creation failed", __FUNCTION__);
        throw std::runtime_error("failed to init mona communicator");
      }
      spdlog::debug("recreate the mona_comm, addr size {}", m_member_addrs.size());
    }

    // this is a disable response api
    req.respond(0);
  }

  // this is supposed to exposed by the leader
  // leader wait the registration
  // and then send updates to all workers
  // the sync is supposed to be called after the rescale operation
  void syncview(const tl::request& req, int& iteration, int& clientProcessNum)
  {
    spdlog::debug("sync is called for iteration {}", iteration);

    if (this->m_stagecommon_meta->m_ifleader == false)
    {
      throw std::runtime_error("sync is supposed to be called for the leader process");
    }

    // client and server already sync, do nothing
    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);
      // we should use the expected client process number
      // instead of the current process number
      // if there is updates for the client, this value should also updated
      // one benifits of using the old process number is that
      // we only do the actual sync when server is actaully loaded, it might need long time to load
      // if (clientProcessNum == this->m_stageleader_meta->m_mona_addresses_map.size())
      // compared with the expected number not the actual registered number
      // by this way, we wait wait the addr to be synced for next iteration
      if (clientProcessNum == this->m_stageleader_meta->m_expected_worker_num)
      {
        spdlog::debug("iteration {} client process number equals to server", iteration);

        // return empty thallium addr
        std::vector<std::string> thalliumAddrs;
        req.respond(thalliumAddrs);
        return;
      }
    }

    // if the current one is the leader process
    // difference between actual and worker
    while (this->m_stageleader_meta->addrDiff())
    {
      // there is still process that do not update its addr

      spdlog::debug("wait, current expected process {} addr map size {}",
        this->m_stageleader_meta->m_expected_worker_num,
        this->m_stageleader_meta->m_mona_addresses_map.size());
      usleep(100000);
    }

    // if expected process equals to the actual one, the leader owns the latest view, it propagate
    // this view to all members range the map and call the update rpc
    // is it ok to call the RPC in the existing one?
    // if it is not ok, we let the dedicated clients to do this
    tl::remote_procedure updateMonaAddrListRPC =
      // send req to workers
      this->get_engine().define("updateMonaAddrList");
    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);
      // maybe just create a snap shot of the current addr instead of using a large critical
      // region
      // TODO, if the new joined and it is the first time
      spdlog::info("iteration {} m_added_list size {} m_removed_list size {}", iteration,
        this->m_stageleader_meta->m_added_list.size(),
        this->m_stageleader_meta->m_removed_list.size());
      UpdatedMonaList updatedMonaList(
        this->m_stageleader_meta->m_added_list, this->m_stageleader_meta->m_removed_list);
      std::unique_ptr<UpdatedMonaList> updatedMonaListAll;
      // When there are process that are added firstly
      // we set the updatedmonalist as all existing mona addrs
      // otherwise, this list is nullptr
      if (this->m_stageleader_meta->m_first_added_set.size() > 0)
      {
        spdlog::debug("debug iteration {} m_first_added_set {}", iteration,
          this->m_stageleader_meta->m_first_added_set.size());
        std::vector<std::string> added;
        // this is empty
        std::vector<std::string> removed;

        for (auto& p : this->m_stageleader_meta->m_mona_addresses_map)
        {
          // put all mona addr into this
          added.push_back(p.second);
        }
        // there is new joined process here
        updatedMonaListAll = std::make_unique<UpdatedMonaList>(UpdatedMonaList(added, removed));
      }

      // the key is the thallium addr which we should call based on rpc
      for (auto& p : this->m_stageleader_meta->m_mona_addresses_map)
      {
        // if not self
        if (this->m_stagecommon_meta->m_thallium_self_addr.compare(p.first) == 0)
        {
          // do not updates to itsself
          continue;
        }

        spdlog::debug("iteration {} leader send updated list to {} ", iteration, p.first);

        tl::endpoint workerEndpoint = this->lookup(p.first);

        // TODO if it belongs to the m_first_added_set, then use all the list addr
        // otherwise, use the common monaList

        if (this->m_stageleader_meta->m_first_added_set.find(p.first) !=
          this->m_stageleader_meta->m_first_added_set.end())
        {
          // just checking
          // when current addr is not in the added set
          // it should not be the monaListA
          if (updatedMonaListAll.get() != nullptr)
          {
            // use the MonaListAll in this case
            int result = updateMonaAddrListRPC.on(workerEndpoint)(*(updatedMonaListAll.get()));
            if (result != 0)
            {
              throw std::runtime_error("failed to notify to worker " + p.first);
            }
            spdlog::debug("iteration {} leader sent updatedMonaListAll ok", iteration);
          }
          else
          {
            throw std::runtime_error("updatedMonaListAll is not supposed to be empty");
          }
        }
        else
        {
          // TODO use async call here
          int result = updateMonaAddrListRPC.on(workerEndpoint)(updatedMonaList);
          if (result != 0)
          {
            throw std::runtime_error("failed to notify to worker " + p.first);
          }
          spdlog::debug("iteration {} leader sent updatedMonaList ok", iteration);
        }
      }

      // also update things to itself's common data
      for (int i = 0; i < updatedMonaList.m_mona_added_list.size(); i++)
      {
        this->m_stagecommon_meta->m_monaaddr_set.insert(updatedMonaList.m_mona_added_list[i]);
      }

      for (int i = 0; i < updatedMonaList.m_mona_remove_list.size(); i++)
      {
        this->m_stagecommon_meta->m_monaaddr_set.erase(updatedMonaList.m_mona_remove_list[i]);
      }

      // recreate the mona comm when it is necessary
      if (this->m_stageleader_meta->m_first_added_set.size() > 0 ||
        updatedMonaList.m_mona_added_list.size() > 0 ||
        updatedMonaList.m_mona_remove_list.size() > 0)
      {
        // mona things are actually updated
        std::vector<na_addr_t> m_member_addrs;
        for (auto& p : this->m_stagecommon_meta->m_monaaddr_set)
        {

          na_addr_t addr = NA_ADDR_NULL;
          na_return_t ret = mona_addr_lookup(this->m_stagecommon_meta->m_mona, p.c_str(), &addr);
          if (ret != NA_SUCCESS)
          {
            throw std::runtime_error("failed for mona_addr_lookup");
          }

          m_member_addrs.push_back(addr);
        }

        na_return_t ret = mona_comm_create(this->m_stagecommon_meta->m_mona, m_member_addrs.size(),
          m_member_addrs.data(), &(this->m_stagecommon_meta->m_mona_comm));
        if (ret != 0)
        {
          spdlog::debug("{}: MoNA communicator creation failed", __FUNCTION__);
          throw std::runtime_error("failed to init mona communicator");
        }
        spdlog::debug("recreate the mona_comm, addr size {}", m_member_addrs.size());
      }

      this->m_stageleader_meta->m_added_list.clear();
      this->m_stageleader_meta->m_removed_list.clear();
      // clean the first added vector
      // after this point, there is no first added processes
      this->m_stageleader_meta->m_first_added_set.clear();
    }

    // return current thallium addrs
    std::vector<std::string> thalliumAddrs;

    {
      std::lock_guard<tl::mutex> lock(this->m_stageleader_meta->m_monaAddrmap_mtx);
      for (auto& p : this->m_stageleader_meta->m_mona_addresses_map)
      {
        // put current thallium addr into the vector and return it to the client
        thalliumAddrs.push_back(p.first);
      }
    }
    req.respond(thalliumAddrs);
  }

  // for the pipeline operation

  void stage(const tl::request& req, const std::string& sender_addr,
    const std::string& dataset_name, uint64_t iteration, uint64_t block_id,
    const std::vector<size_t>& dimensions, const std::vector<int64_t>& offsets, const Type& type,
    const thallium::bulk& data)
  {
    // TODO check the status
    // execute when pipeline is in activate status
    int result = this->m_stagecommon_meta->m_pipeline->stage(
      sender_addr, dataset_name, iteration, block_id, dimensions, offsets, type, data);

    req.respond(result);
  }

  void execute(const tl::request& req, uint64_t iteration)
  {
    int result = this->m_stagecommon_meta->m_pipeline->execute(
      iteration, this->m_stagecommon_meta->m_mona_comm);
    req.respond(result);
  }

  void hello(const tl::request& req)
  {
    std::cout << "------RPC call recieved for hello for testing----" << std::endl;
    req.respond(0);
  }
};

#endif

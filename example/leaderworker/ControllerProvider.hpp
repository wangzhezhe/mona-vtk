/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __CONTROLLER_PROVIDER_H
#define __CONTROLLER_PROVIDER_H

#include <dlfcn.h>
#include <mona.h>
#include <spdlog/spdlog.h>

#include "ControllerClient.hpp"
#include "common.hpp"
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

// TODO add pipeline things in future
class ControllerProvider : public tl::provider<ControllerProvider>
{
  auto id() const { return get_provider_id(); } // for convenience

  using json = nlohmann::json;

public:
  // control client to access necessary info
  // this info should binded with provider since the provider needed to start earlier than client
  std::unique_ptr<LeaderMeta> m_leader_meta;
  std::unique_ptr<CommonMeta> m_common_meta;

  ControllerProvider(const tl::engine& engine, uint16_t provider_id, bool leader)
    : tl::provider<ControllerProvider>(engine, provider_id)
  {
    define("colza_addMonaAddr", &ControllerProvider::addMonaAddr);
    define("colza_removeMonaAddr", &ControllerProvider::removeMonaAddr);
    define("colza_updateMonaAddrList", &ControllerProvider::updateMonaAddrList);
    define("colza_helloRPC", &ControllerProvider::hello);
    if (leader)
    {
      this->m_leader_meta = std::make_unique<LeaderMeta>(LeaderMeta());
    }
    this->m_common_meta = std::make_unique<CommonMeta>(CommonMeta());
  }

  ~ControllerProvider()
  {
    spdlog::trace("[provider:{}] Deregistering provider", id());
    spdlog::trace("[provider:{}]    => done!", id());
  }

  // leader process expose this RPC
  void removeMonaAddr(const tl::request& req)
  {
    std::string clientThalliumAddr = std::string(req.get_endpoint());
    spdlog::debug("RPC removeMonaAddr is called by {}", clientThalliumAddr);
    // if this is old one , this is existing one, we find its addr by its id
    // no response
    // check member id, if not exist, throw error
    // if exist, remove
    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_monaAddrmap_mtx);
      if (this->m_leader_meta->m_mona_addresses_map.find(clientThalliumAddr) ==
        this->m_leader_meta->m_mona_addresses_map.end())
      {
        throw std::runtime_error("the clientThalliumAddr " + clientThalliumAddr + "does not exist");
      }
      else
      {
        this->m_leader_meta->m_removed_list.push_back(
          this->m_leader_meta->m_mona_addresses_map[clientThalliumAddr]);
        this->m_leader_meta->m_mona_addresses_map.erase(clientThalliumAddr);
      }
    }
    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
      this->m_leader_meta->pendingProcessNum = this->m_leader_meta->pendingProcessNum - 1;
    }
    req.respond(0);
  }

  // leader process expose this RPC
  void addMonaAddr(const tl::request& req, std::string monaAddr)
  {
    // if added, this is a new process, we create a new id, return its id
    // create uuid, put it into the map, return the id
    std::string clientThalliumAddr = std::string(req.get_endpoint());
    spdlog::debug("RPC addMonaAddr is called by {}", clientThalliumAddr);

    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_monaAddrmap_mtx);

      // if the current addr is not stored into the map
      if (this->m_leader_meta->m_mona_addresses_map.find(clientThalliumAddr) ==
        this->m_leader_meta->m_mona_addresses_map.end())
      {
        // this addr is not exist in the map
        // put the thallium addr into it, this record the thallium addr instead of mona addr
        // the client will check this set, and update all mona addrs insted of the modified addrs
        // when the thallium addr is added in the m_first_added_set
        this->m_leader_meta->m_first_added_set.insert(clientThalliumAddr);
      }

      this->m_leader_meta->m_mona_addresses_map[clientThalliumAddr] = monaAddr;
      this->m_leader_meta->m_added_list.push_back(monaAddr);
    }

    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
      this->m_leader_meta->pendingProcessNum = this->m_leader_meta->pendingProcessNum - 1;
    }

    req.respond(0);
  }

  void hello(const tl::request& req)
  {
    std::cout << "------RPC call recieved for hello for testing" << std::endl;
    req.respond(0);
  }

  // non-leader process expose this RPC
  // we need to decide the process is first added or it have been exist
  // if it have been exist, we need send
  void updateMonaAddrList(const tl::request& req, UpdatedMonaList& updatedMonaList)
  {
    // when every client recieve the RPC
    // they merge the list into the current mona_addr list
    // two pointer
    // just push
    std::string clientThalliumAddr = std::string(req.get_endpoint());
    spdlog::debug("RPC updateMonaAddrList is called by {}", clientThalliumAddr);

    for (int i = 0; i < updatedMonaList.m_mona_added_list.size(); i++)
    {
      this->m_common_meta->m_monaaddr_set.insert(updatedMonaList.m_mona_added_list[i]);
    }

    for (int i = 0; i < updatedMonaList.m_mona_remove_list.size(); i++)
    {
      this->m_common_meta->m_monaaddr_set.erase(updatedMonaList.m_mona_remove_list[i]);
    }

    // update its m_mona_addrlist_updated
    // the sim process will read this variable
    {
      std::lock_guard<tl::mutex> lock(this->m_common_meta->m_addrlist_updated_mtx);
      this->m_common_meta->m_mona_addrlist_updated = true;
    }
    req.respond(0);
  }
};

#endif

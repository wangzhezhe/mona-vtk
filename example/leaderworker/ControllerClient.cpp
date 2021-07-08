

#include "ControllerClient.hpp"
#include "common.hpp"

// this is called by every one
// if the leader
// if the non-leader, wait for the m_mona_addrlist_updated updated

int ControllerClient::getPendingProcess()
{
  std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
  return this->m_leader_meta->pendingProcessNum;
}

void ControllerClient::sync(int iteration, bool leader)
{
  if (leader)
  {
    // propagate its addr information to all the workers
    // leader process know the pending process

    while (this->getPendingProcess() > 0)
    {
      // there is still process that do not update its addr
      usleep(1000000);
      spdlog::debug("current pending processnum {} addr map size {}",
        this->m_leader_meta->pendingProcessNum, this->m_leader_meta->m_mona_addresses_map.size());
    }

    // if all pending processes is updated, the leader owns the latest view, it propagate this view
    // to all members range the map and call the update rpc
    tl::remote_procedure updateMonaAddrListRPC =
      this->m_clientengine_ptr->define("colza_updateMonaAddrList");
    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_monaAddrmap_mtx);
      // maybe just create a snap shot of the current addr instead of using a large critical region
      // TODO, if the new joined and it is the first time
      UpdatedMonaList updatedMonaList(
        this->m_leader_meta->m_added_list, this->m_leader_meta->m_removed_list);
      std::unique_ptr<UpdatedMonaList> updatedMonaListAll;
      // When there are process that are added firstly
      // we set the updatedmonalist as all existing mona addrs
      if (this->m_leader_meta->m_first_added_set.size() > 0)
      {
        spdlog::debug("debug iteration {} m_first_added_set {}", iteration,
          this->m_leader_meta->m_first_added_set.size());
        std::vector<std::string> added;
        // this is empty
        std::vector<std::string> removed;

        for (auto& p : this->m_leader_meta->m_mona_addresses_map)
        {
          // put all mona addr into this
          added.push_back(p.second);
        }
        // there is new joined process here
        updatedMonaListAll = std::make_unique<UpdatedMonaList>(UpdatedMonaList(added, removed));
      }

      // the key is the thallium addr which we should call based on rpc
      for (auto& p : this->m_leader_meta->m_mona_addresses_map)
      {
        // if not self
        if (this->m_self_addr.compare(p.first) == 0)
        {
          // do not updates to itsself
          continue;
        }

        // TODO we need to check if there is new added addr
        // if there is new added addr, we send all info to it

        // TODO use cache here
        // TODO define it in the constructor
        spdlog::debug("leader sent updated list to {} ", p.first);
        tl::endpoint workerEndpoint = this->m_clientengine_ptr->lookup(p.first);

        // TODO if it belongs to the m_first_added_set, then use all the list addr
        if (this->m_leader_meta->m_first_added_set.find(p.first) !=
          this->m_leader_meta->m_first_added_set.end())
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
            spdlog::debug("leader sent updated list ok");
          }
          else
          {
            throw std::runtime_error("updatedMonaListAll is not supposed to be empty");
          }
        }
        else
        {
          int result = updateMonaAddrListRPC.on(workerEndpoint)(updatedMonaList);
          if (result != 0)
          {
            throw std::runtime_error("failed to notify to worker " + p.first);
          }
          spdlog::debug("leader sent updated list ok");
        }
      }

      // also update things to itself's common data

      for (int i = 0; i < updatedMonaList.m_mona_added_list.size(); i++)
      {
        this->m_common_meta->m_monaaddr_set.insert(updatedMonaList.m_mona_added_list[i]);
      }

      for (int i = 0; i < updatedMonaList.m_mona_remove_list.size(); i++)
      {
        this->m_common_meta->m_monaaddr_set.erase(updatedMonaList.m_mona_remove_list[i]);
      }

      this->m_common_meta->m_mona_addrlist_updated = true;
      // till this point, all workers should be notified, we set the mona list as empty
      this->m_leader_meta->m_added_list.clear();
      this->m_leader_meta->m_removed_list.clear();
      // clean the first added vector
      // after this point, there is no first added processes
      this->m_leader_meta->m_first_added_set.clear();
      // the client code call the other processes
    }
  }

  // for all other processes
  // wait the current mona_addr_list is updated
  spdlog::info("iteration {} wait the sync addr list to be updated", iteration);
  while (this->m_common_meta->m_mona_addrlist_updated == false)
  {
    usleep(5000);
  }

  // update it back to false for next iteration case
  {
    std::lock_guard<tl::mutex> lock(this->m_common_meta->m_addrlist_updated_mtx);
    this->m_common_meta->m_mona_addrlist_updated = false;
  }
  // extract current mona addr
  spdlog::info("iteration {} size of the mona addr {} ", iteration,
    this->m_common_meta->m_monaaddr_set.size());
  // maybe add a barrier here for future using
}

// this is called by the any process that is newly added into the group
void ControllerClient::registerProcessToLeader(std::string mona_addr)
{
  // TODO define it in the constructor
  // register its addr
  tl::remote_procedure addMonaAddr = this->m_clientengine_ptr->define("colza_addMonaAddr");
  this->m_leader_endpoint = this->m_clientengine_ptr->lookup(this->m_leader_addr);
  int result = addMonaAddr.on(this->m_leader_endpoint)(mona_addr);
  if (result != 0)
  {
    throw std::runtime_error("failed to register mona addr");
  }
  return;
}

// this is called by the leader process
void ControllerClient::removeProcess()
{
  // TODO
}

// this is called by the leader process itsself set the expected process (both join and leave)
void ControllerClient::expectedUpdatingProcess(int num)
{
  if (num < 0)
  {
    throw std::runtime_error("expected process num  to be udpated should larger than 0");
  }
  std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
  this->m_leader_meta->pendingProcessNum = num;
  return;
}

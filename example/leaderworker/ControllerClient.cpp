

#include "ControllerClient.hpp"
#include "common.hpp"

tl::endpoint ControllerClient::lookup(const std::string& address)
{
  auto it = m_addrToEndpoints.find(address);
  if (it == m_addrToEndpoints.end())
  {
    // do not lookup here to avoid the potential mercury race condition
    // throw std::runtime_error("failed to find addr, cache the endpoint at the constructor\n");
    auto endpoint = this->m_clientengine_ptr->lookup(address);
    std::string tempAddr = address;
    this->m_addrToEndpoints[tempAddr] = endpoint;
    return endpoint;
  }
  return it->second;
}

// this is called by every one
// if the leader
// if the non-leader, wait for the m_mona_addrlist_updated updated
int ControllerClient::getPendingProcess()
{
  std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
  return this->m_leader_meta->pendingProcessNum;
}

void ControllerClient::sync(int iteration, bool leader, int offset)
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
      this->m_clientengine_ptr->define("sim_updateMonaAddrList").disable_response();
    {
      std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_monaAddrmap_mtx);
      // maybe just create a snap shot of the current addr instead of using a large critical region
      // TODO, if the new joined and it is the first time
      spdlog::info("iteration {} m_added_list size {} m_removed_list size {}", iteration,
        this->m_leader_meta->m_added_list.size(), this->m_leader_meta->m_removed_list.size());
      UpdatedMonaList updatedMonaList(
        this->m_leader_meta->m_added_list, this->m_leader_meta->m_removed_list);
      std::unique_ptr<UpdatedMonaList> updatedMonaListAll;

      // update the leader's meta data, make sure its view is the latest one
      // this need to be done firstly since we need to make addr_list
      // in a consistent sequence for all members
      // the new added one need to copy its mona_addr_list direactly
      for (int i = 0; i < updatedMonaList.m_mona_added_list.size(); i++)
      {
        // this->m_common_meta->m_monaaddr_set.insert(updatedMonaList.m_mona_added_list[i]);
        this->m_common_meta->m_monaaddr_list.push_back(updatedMonaList.m_mona_added_list[i]);
      }

      for (int i = 0; i < updatedMonaList.m_mona_remove_list.size(); i++)
      {
        // this->m_common_meta->m_monaaddr_set.erase(updatedMonaList.m_mona_remove_list[i]);

        this->m_common_meta->m_monaaddr_list.erase(
          std::remove(this->m_common_meta->m_monaaddr_list.begin(),
            this->m_common_meta->m_monaaddr_list.end(), updatedMonaList.m_mona_remove_list[i]),
          this->m_common_meta->m_monaaddr_list.end());
      }

      // When there are process that are added firstly
      // we set the updatedmonalist as all existing mona addrs
      if (this->m_leader_meta->m_first_added_set.size() > 0)
      {
        spdlog::debug("debug iteration {} m_first_added_set {}", iteration,
          this->m_leader_meta->m_first_added_set.size());

        // get whole updated monaaddr_list direactly
        std::vector<std::string> added = this->m_common_meta->m_monaaddr_list;
        // this is empty
        std::vector<std::string> removed;

        updatedMonaListAll = std::make_unique<UpdatedMonaList>(UpdatedMonaList(added, removed));
      }

      // for async response
      std::vector<tl::async_response> async_responses;
      // the key is the thallium addr which we should call based on rpc
      for (auto& p : this->m_leader_meta->m_mona_addresses_map)
      {
        // if not self
        if (this->m_self_addr.compare(p.first) == 0)
        {
          // do not updates to itsself
          // its have been updated
          continue;
        }

        // TODO we need to check if there is new added addr
        // if there is new added addr, we send all info to it

        // TODO use cache here
        // TODO define it in the constructor
        spdlog::debug("leader start send updated list to {} ", p.first);
        // TODO use a cache here
        tl::endpoint workerEndpoint = this->lookup(p.first);

        // the leader process also need to tell others what is the rank 0 one
        std::string leaderMonaAddr = this->m_common_meta->m_leader_mona_addr;

        if (leaderMonaAddr == "")
        {
          throw std::runtime_error("leader mona addr is not supposed to be empty");
        }

        // if it belongs to the m_first_added_set, then use all the list addr
        if (this->m_leader_meta->m_first_added_set.find(p.first) !=
          this->m_leader_meta->m_first_added_set.end())
        {
          // just checking
          // when current addr is not in the added set
          // it should not be the monaListA
          spdlog::debug("iteration {} leader start sending updatedMonaListAll", iteration);
          if (updatedMonaListAll.get() != nullptr)
          {
            // use the MonaListAll in this case
            updateMonaAddrListRPC.on(workerEndpoint)(*(updatedMonaListAll.get()), leaderMonaAddr);
            // if (result != 0)
            //{
            //  throw std::runtime_error("failed to notify to worker " + p.first);
            //}
            spdlog::debug("iteration {} leader sent updatedMonaListAll ok", iteration);
          }
          else
          {
            throw std::runtime_error("updatedMonaListAll is not supposed to be empty");
          }
        }
        else
        {
          // for others that exist
          // TODO use async call here
          spdlog::debug("iteration {} leader sent updatedMonaList to {}", iteration,
            std::string(workerEndpoint));
          updateMonaAddrListRPC.on(workerEndpoint)(updatedMonaList, leaderMonaAddr);
          spdlog::debug("iteration {} leader sent updatedMonaList ok", iteration);
        }
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
    // maybe set a random to make sure every thread do not start at the same time
    usleep(500000);
    // tl::thread::sleep(*(this->m_clientengine_ptr), 300);
    tl::thread::yield();
  }

  // update it back to false for next iteration case
  {
    std::lock_guard<tl::mutex> lock(this->m_common_meta->m_addrlist_updated_mtx);
    this->m_common_meta->m_mona_addrlist_updated = false;
  }
  // extract current mona addr
  spdlog::info("iteration {} size of the mona addr {} ", iteration,
    this->m_common_meta->m_monaaddr_list.size());
  // maybe add a barrier here for future using
}

// get the mona_comm based on current mona addr list
// TODO only get comm when we make sure there are updates
void ControllerClient::getMonaComm(mona_instance_t mona)
{
  // get m_member_addrs from the set

  // always put the leader's mona addr as the first one
  // just use a cache to store the master's addr
  // for the first iteration, it is correct one
  std::vector<na_addr_t> m_member_addrs;

  // set the leader as the first one manually
  na_addr_t addr = NA_ADDR_NULL;
  na_return_t ret = mona_addr_lookup(mona, this->m_common_meta->m_leader_mona_addr.c_str(), &addr);
  if (ret != NA_SUCCESS)
  {
    throw std::runtime_error("failed for mona_addr_lookup for leader");
  }

  m_member_addrs.push_back(addr);

  for (auto& p : this->m_common_meta->m_monaaddr_list)
  {
    if (p == this->m_common_meta->m_leader_mona_addr)
    {
      continue;
    }
    na_addr_t addr = NA_ADDR_NULL;
    na_return_t ret = mona_addr_lookup(mona, p.c_str(), &addr);
    if (ret != NA_SUCCESS)
    {
      throw std::runtime_error("failed for mona_addr_lookup");
    }

    m_member_addrs.push_back(addr);
  }

  ret = mona_comm_create(mona, m_member_addrs.size(), m_member_addrs.data(), &(this->m_mona_comm));
  if (ret != 0)
  {
    spdlog::debug("{}: MoNA communicator creation failed", __FUNCTION__);
    throw std::runtime_error("failed to init mona communicator");
  }

  return;
}

// this is called by the any process that is newly added into the group
void ControllerClient::registerProcessToLeader(std::string mona_addr)
{
  // TODO define it in the constructor
  // register its addr
  tl::remote_procedure addMonaAddr = this->m_clientengine_ptr->define("sim_addMonaAddr");
  int result = addMonaAddr.on(this->m_leader_endpoint)(mona_addr);
  if (result != 0)
  {
    throw std::runtime_error("failed to register mona addr");
  }
  return;
}

// this is called by the process who wants to deregister its thallium addr from the leader
void ControllerClient::removeProcess()
{
  tl::remote_procedure removeMonaAddr = this->m_clientengine_ptr->define("sim_removeMonaAddr");
  int result = removeMonaAddr.on(this->m_leader_endpoint)();
  if (result != 0)
  {
    throw std::runtime_error("failed to remove mona addr");
  }
  return;
}

// this is called by the leader process itsself set the expected process (both join and leave)
void ControllerClient::expectedUpdatingProcess(int num)
{
  if (num < 0)
  {
    throw std::runtime_error("expected process num to be udpated should larger than 0");
  }
  std::lock_guard<tl::mutex> lock(this->m_leader_meta->m_pendingProcessNum_mtx);
  this->m_leader_meta->pendingProcessNum = num;
  return;
}

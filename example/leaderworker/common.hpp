#ifndef __LEADER_WORKER_COMMON_H
#define __LEADER_WORKER_COMMON_H

#include <map>
#include <set>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

namespace tl = thallium;

struct UpdatedMonaList
{
  UpdatedMonaList(){};
  UpdatedMonaList(
    std::vector<std::string> mona_added_list, std::vector<std::string> mona_remove_list)
    : m_mona_added_list(mona_added_list)
    , m_mona_remove_list(mona_remove_list){};

  std::vector<std::string> m_mona_added_list;
  std::vector<std::string> m_mona_remove_list;

  ~UpdatedMonaList(){};

  template <typename A>
  void serialize(A& ar)
  {
    ar& m_mona_added_list;
    ar& m_mona_remove_list;
  }
};

// meta information hold by leader
struct LeaderMeta
{
  // special metaData for leader process
  tl::mutex m_pendingProcessNum_mtx; // this value shows how many process need
                                     // to send to rpc to leader process
  int pendingProcessNum = 0;
  tl::mutex m_monaAddrmap_mtx;
  //the key is the thllium addr
  //the value is the mona addr
  //we do not separate hash value in this case
  //Attention! when we notiy all the worker, we need to use the thallium addr instead of mona addr
  std::map<std::string, std::string> m_mona_addresses_map;

  //TODO we also need store the thallium addr, they are used for transering key messages
  //which is onpair with the mona things
  //the added list and removed list is for mona addr
  std::vector<std::string> m_added_list;
  std::vector<std::string> m_removed_list;
  std::set<std::string> m_first_added_set;
};

// meta information hold by every process
struct CommonMeta
{
  // metadata for all process
  tl::mutex m_addrlist_updated_mtx;
  bool m_mona_addrlist_updated = false;
  std::string m_self_uuid;
  mona_instance_t m_mona;
  std::string m_mona_self_addr;
  std::set<std::string> m_monaaddr_set;
};

#endif
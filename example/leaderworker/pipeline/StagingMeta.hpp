#ifndef __STAGING_META_HPP
#define __STAGING_META_HPP

#include "./Backend.hpp"
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
struct StagingLeaderMeta
{
  // compare the expected registered worker and actual registered
  tl::mutex m_workernum_mtx;
  // the actual data is the size of the m_mona_addresses_map
  int m_expected_worker_num = 0;

  tl::mutex m_monaAddrmap_mtx;
  // the key is the thllium addr
  // the value is the mona addr
  std::map<std::string, std::string> m_mona_addresses_map;

  // sending the modification to others
  // these are used for storing the thallium addr
  tl::mutex m_modifiedAddr_mtx;
  // the added and removed list should put the mona addr
  // they are used to notify staging workers
  std::vector<std::string> m_added_list;
  std::vector<std::string> m_removed_list;
  std::set<std::string> m_first_added_set;

  bool addrDiff()
  {
    std::lock_guard<tl::mutex> lock(this->m_monaAddrmap_mtx);
    int mapsize = m_mona_addresses_map.size();
    if (mapsize == this->m_expected_worker_num)
    {
      return false;
    }
    return true;
  }

  // TODO, we do not add or remove process at the same time
  // otherwise, we need to use the hash value to label if there are updates
  // since the view can be different when there is same addr map
  // currently, if actual value do not equal to expected value
  // the view is different
};

// meta information hold by every worker process
// they also need to maintain the mona adder list
// since the in-staging execution use the mona things
struct StagingCommonMeta
{
  mona_instance_t m_mona;
  mona_comm_t m_mona_comm = nullptr;

  std::string m_thallium_self_addr;
  std::string m_thallium_leader_addr;
  std::string m_mona_self_addr;

  // store the current list for mona addr
  std::set<std::string> m_monaaddr_set;
  bool m_ifleader = false;

  // the pipeline that provide data store, stage and execution
  std::shared_ptr<Backend> m_pipeline;
};

#endif
#include "Rescaler.hpp"
#include <cstdlib>
#include <ssg.h>
#include <thallium.hpp>
#include <vector>

namespace tl = thallium;

void Rescaler::addNewServer(const int& serverNum, const std::string& startStagingCommand)
{
  if (serverNum <= 0)
  {
    throw std::runtime_error("serverNum is supposed to >0");
  }

  // start n server
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

void Rescaler::makeServersLeave(
  const std::string& ssg_file, const int& serverNum, uint16_t provider_id) const
{
  ssg_group_id_t gid;
  int num_addrs = -1;
  int ret = ssg_group_id_load(ssg_file.c_str(), &num_addrs, &gid);
  if (ret != SSG_SUCCESS)
    throw std::runtime_error("Could not open SSG file ");

  ret = ssg_group_observe(this->m_engine.get_margo_instance(), gid);
  if (ret != SSG_SUCCESS)
    throw std::runtime_error("Could not observe SSG group from " + ssg_file);

  int group_size = 0;
  ssg_get_group_size(gid, &group_size);
  for (int rank = 1; rank <= serverNum; rank++)
  {
    if (rank < 0 || rank >= group_size)
      continue;
    ssg_member_id_t member_id = SSG_MEMBER_ID_INVALID;
    ssg_get_group_member_id_from_rank(gid, rank, &member_id);
    hg_addr_t a = HG_ADDR_NULL;
    ssg_get_group_member_addr(gid, member_id, &a);
    auto ph = tl::provider_handle(this->m_engine, a, provider_id, false);
    this->m_leave.on(ph)();
    spdlog::info("make rank {} process leave", rank);
  }

  ssg_group_unobserve(gid);
}
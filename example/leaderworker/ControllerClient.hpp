/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __CONTROLLER_CLIENT_HPP
#define __CONTROLLER_CLIENT_HPP

#include "ControllerProvider.hpp"
#include "common.hpp"
#include <memory>
#include <thallium.hpp>

#include <mona-coll.h>
#include <mona.h>

namespace tl = thallium;

class ControllerProvider;

/**
 * @brief The Client object is the main object used to establish
 * a connection with a Colza service.
 */
class ControllerClient
{

  friend class ControllerProvider;

public:
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

  /**
   * @brief Constructor.
   *
   * if the leader_meta is nullptr, this is not leader
   */
  ControllerClient(tl::engine* clientEnginePtr, uint16_t providerID, LeaderMeta* leader_meta,
    CommonMeta* common_meta, std::string leaderAddrConfig)
    : m_clientengine_ptr(clientEnginePtr)
    , m_providerID(providerID)
  // TODO put the remote proceduer here
  {
    if (leader_meta != nullptr)
    {
      m_leader_meta = leader_meta;
    }
    if (common_meta == nullptr)
    {
      throw std::runtime_error("common_meta should not be nullptr");
    }
    m_common_meta = common_meta;

    // load the leader addr
    this->m_leader_addr = loadLeaderAddr(leaderAddrConfig);

    spdlog::info("load the leader addr {} ", this->m_leader_addr);

    m_leader_endpoint = this->m_clientengine_ptr->lookup(this->m_leader_addr);

    m_self_addr = std::string(this->m_clientengine_ptr->self());
  }

  /**
   * @brief Copy constructor.
   */
  ControllerClient(const ControllerClient&) = default;

  /**
   * @brief Move constructor.
   */
  ControllerClient(ControllerClient&&) = default;

  /**
   * @brief Copy-assignment operator.
   */
  ControllerClient& operator=(const ControllerClient&) = default;

  /**
   * @brief Move-assignment operator.
   */
  ControllerClient& operator=(ControllerClient&&) = default;

  /**
   * @brief Destructor.
   */
  ~ControllerClient() = default;

  /**
   * @brief Returns the thallium engine used by the client.
   */
  // const thallium::engine& engine() const;

  void sync(int iteration, bool leader);

  void registerProcessToLeader(std::string mona_addr);

  void removeProcess();

  void expectedUpdatingProcess(int num);

  int getPendingProcess();

  void getMonaComm(mona_instance_t mona);

  tl::endpoint m_leader_endpoint;
  std::string m_leader_addr;
  std::string m_self_addr;
  uint16_t m_providerID = 0;

  // the pointer to the provider and client engine
  // only access, not maintain its lifecycle
  tl::engine* m_clientengine_ptr = nullptr;

  // ptr to the control client to access necessary info
  // only access info not manage itslifecycle
  LeaderMeta* m_leader_meta = nullptr;
  CommonMeta* m_common_meta = nullptr;

  mona_comm_t m_mona_comm = nullptr;
};

#endif

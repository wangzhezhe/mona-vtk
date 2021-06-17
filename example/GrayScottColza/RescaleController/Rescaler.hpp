/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __RESCALER_HPP_
#define __RESCALER_HPP_

#include <thallium.hpp>
#include <spdlog/spdlog.h>

namespace tl = thallium;

//the actual behaviours of rescaler at the client side is only issue the commands
//such as send rpc to shut down the remote server
//or execute the system call to start the data staging service
class Rescaler
{

public:
  tl::engine m_engine;
  tl::remote_procedure m_leave;

  Rescaler(const tl::engine& engine)
    : m_engine(engine)
    , m_leave(m_engine.define("colza_leave").disable_response())
  {
  }

  void shutdownServer(const std::string& ssg_file, const int& serverNum) const;
  void addNewServer(const int& serverNum, const std::string& startStagingCommand);

  ~Rescaler() {}

  int m_addedServerID=0;
};

#endif

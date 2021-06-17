#ifndef __CONTROLLER_HPP_
#define __CONTROLLER_HPP_

#include "Rescaler.hpp"
#include <spdlog/spdlog.h>
#include <thallium.hpp>
#include <string>
namespace tl = thallium;

// the actual behaviours of rescaler at the client side is only issue the commands
// such as send rpc to shut down the remote server
// or execute the system call to start the data staging service
class Controller
{

public:
  Controller(const tl::engine& engine, const std::string& ssgfileName, const std::string& command)
    : m_rescaler(engine)
    , m_ssgfileName(ssgfileName)
    , m_commandToAddServer(command)
  {
  }

  // the naive policy that decide add or remove a process for current step
  void naivePolicy(const int& step);

  ~Controller() {}

  Rescaler m_rescaler;
  std::string m_ssgfileName;
  std::string m_commandToAddServer;
};

#endif

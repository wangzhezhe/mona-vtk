#include "Controller.hpp"

// the const function is not supposed to modify the member of the class
void Controller::naivePolicy(const int& step)
{
  if (step == 1)
  {
    // try to add new process
    this->m_rescaler.addNewServer(1, this->m_commandToAddServer);
  }
  if (step == 4)
  {
    // try to remove one process
    // use the default provider id, which is 0
    this->m_rescaler.makeServersLeave(m_ssgfileName, 1, 0);
  }
}

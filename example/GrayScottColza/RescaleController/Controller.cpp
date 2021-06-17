#include "Controller.hpp"

// the const function is not supposed to modify the member of the class
void Controller::naivePolicy(const int& step)
{
  if (step < 1)
  {
    // add new process
    this->m_rescaler.addNewServer(1, this->m_commandToAddServer);
  }
  if (step >= 10 && step < 20)
  {
    // remove one process
    this->m_rescaler.shutdownServer(m_ssgfileName, 1);
  }
}

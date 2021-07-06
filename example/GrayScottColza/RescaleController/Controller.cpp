#include "Controller.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>

// the const function is not supposed to modify the member of the class
int Controller::naivePolicy(const int& step)
{
  if (step > 0)
  {
    // try to add multiple process, the actual addserver operation is executed outside of this fun
    return 1;
  }
  /*
  if (step == 4)
  {
    // try to remove one process
    // use the default provider id, which is 0
    this->m_rescaler.makeServersLeave(m_ssgfileName, 1, 0);
    // sleep 600 ms to make sure the server is closed completely
    // and the ssg file is also updated
    // the ssg period is set to 500 ms so we sleep 600 ms here
    usleep(600);
    spdlog::info("makeServersLeave function return at step {} ", step);
  }
  */
 return 0;
}

bool Controller::enoughData()
{
  // check if data is enough avalible (we need to make sure there are two different process numbers)
  int size = this->m_historicalData.size();
  bool enoughdata = false;
  if (size >= 2 &&
    (this->m_historicalData[size - 1].m_processNum !=
      this->m_historicalData[size - 2].m_processNum))
  {
    enoughdata = true;
  }
  return enoughdata;
}

// the currentP is the current process number
// the targetExecTime is the execution time we want to achieve
int Controller::dynamicJoinProcessNum(
  const std::string& modelName, const int& currentP, const double& targetExecTime)
{
  if (this->m_models.find(modelName) == m_models.end())
  {
    // the model does not exist
    spdlog::info("model not exist");

    if (this->enoughData())
    {
      // estimate model
      // set the default upper value as 128
      int N = 128;
      int size = this->m_historicalData.size();
      this->m_models[modelName] =
        std::make_unique<Model>(this->m_historicalData[size - 2].m_processNum,
          this->m_historicalData[size - 2].m_executionTime,
          this->m_historicalData[size - 1].m_processNum,
          this->m_historicalData[size - 1].m_executionTime, N);
      spdlog::info("ok to create a {} model, data {} {} {} {}", modelName,
        this->m_historicalData[size - 2].m_processNum,
        this->m_historicalData[size - 2].m_executionTime,
        this->m_historicalData[size - 1].m_processNum,
        this->m_historicalData[size - 1].m_executionTime);
    }
    else
    {
      return 1;
    }
  }

  // only access the content without managing its lifecycle
  Model* tempmodel = this->m_models[modelName].get();
  double newProcessNum = exp((log(targetExecTime) - log(tempmodel->m_a)) / tempmodel->m_b);
  spdlog::info("newprocessnum: {} check newProcessNum estimate parameters targetExecTime {} a {} b {} ",
    newProcessNum, targetExecTime, tempmodel->m_a, tempmodel->m_b);
  if (newProcessNum < currentP*1.0)
  {
    std::cout << "error message newProcessNum: " << newProcessNum << " currentP: " << currentP << std::endl;
    throw std::runtime_error(
      "model caculation error, the new process number should larger than current num");
  }

  return ceil(newProcessNum - currentP);
}

// dynamic start process
// maybe control the process number started in one node

void Controller::recordData(double executionTime, int processNum)
{
  this->m_historicalData.push_back(HistoryData(executionTime, processNum));
  return;
}
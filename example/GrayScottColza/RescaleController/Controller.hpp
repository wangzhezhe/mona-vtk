#ifndef __CONTROLLER_HPP_
#define __CONTROLLER_HPP_

#include "Rescaler.hpp"
#include <map>
#include <math.h>
#include <string>
#include <thallium.hpp>
#include <vector>
namespace tl = thallium;

// the power law model that map the execution time into the process number
// y= a*b^x, and p is the break point between the head and tail region
// x represents the process number and y represents the exeuction time
struct Model
{
  Model(double raw_x1, double raw_y1, double raw_x2, double raw_y2, int N)
  {
    // take ln operation
    double x1 = log(raw_x1);
    double x2 = log(raw_x2);
    double y1 = log(raw_y1);
    double y2 = log(raw_y2);

    double est_a = (y1 - y2) / (x1 - x2);
    double est_b = (y2 * x1 - y1 * x2) / (x1 - x2);

    this->m_b = est_a;
    this->m_a = exp(est_b);
    this->m_p = pow((pow(N, this->m_b + 1.0) + 1.0) / 2.0, 1.0 / (this->m_b + 1.0));
  }

  double m_a;
  double m_b;
  double m_p;
};

// the history data used for expecting model
struct HistoryData
{
  HistoryData(double executionTime, int processNum)
    : m_executionTime(executionTime)
    , m_processNum(processNum){};
  double m_executionTime;
  int m_processNum;
};

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
  int naivePolicy(const int& step);

  int dynamicJoinProcessNum(
    const std::string& modelName, const int& currentP, const double& expectedDecreaseTime);

  // void dynamicLeaveProcessNum(int& currentP, double& expectedIncreaseTime);
  void recordData(double executionTime, int processNum);
  // if there is enough data for model using
  bool enoughData();

  ~Controller() {}

  Rescaler m_rescaler;
  std::string m_ssgfileName;
  std::string m_commandToAddServer;

  // the model for apecific load
  std::map<std::string, std::unique_ptr<Model> > m_models;

  // the vector to store historical data for model evaluation
  std::vector<HistoryData> m_historicalData;
};

#endif

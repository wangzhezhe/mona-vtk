#ifndef __DYNAMIC_PROCESS_CONTROLLER_HPP
#define __DYNAMIC_PROCESS_CONTROLLER_HPP

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <math.h>
#include <spdlog/spdlog.h>

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

struct DynamicProcessController
{

  DynamicProcessController(){};

  ~DynamicProcessController(){};

  std::map<std::string, std::unique_ptr<Model> > m_models;
  // the vector to store historical data for model evaluation
  std::vector<HistoryData> m_historicalDataSim;
  std::vector<HistoryData> m_historicalDataStaging;

  void recordData(std::string component, double executionTime, int processNum)
  {
    if (component.compare("sim") == 0)
    {
      this->m_historicalDataSim.push_back(HistoryData(executionTime, processNum));
    }
    else if (component.compare("staging") == 0)
    {
      this->m_historicalDataStaging.push_back(HistoryData(executionTime, processNum));
    }
    else
    {
      throw std::runtime_error("unsupported component name when record the data");
    }
    return;
  }

  bool enoughData(std::string component)
  {
    bool enoughdata = false;
    if (component.compare("sim") == 0)
    {
      int size = this->m_historicalDataSim.size();
      if (size >= 2 &&
        (this->m_historicalDataSim[size - 1].m_processNum !=
          this->m_historicalDataSim[size - 2].m_processNum))
      {
        enoughdata = true;
      }
    }
    else if (component.compare("staging") == 0)
    {
      int size = this->m_historicalDataStaging.size();
      if (size >= 2 &&
        (this->m_historicalDataStaging[size - 1].m_processNum !=
          this->m_historicalDataStaging[size - 2].m_processNum))
      {
        enoughdata = true;
      }
    }
    else
    {
      throw std::runtime_error("unsupported component name when checking enoughData");
    }
    return enoughdata;
  }
  // current P is the current data staging number
  int dynamicAddProcessToStaging(
    const std::string modelName, const int& currentP, const double& targetExecTime)
  {
    if (this->m_models.find(modelName) == m_models.end())
    {
      // the model does not exist
      spdlog::info("model not exist, create model based on existing data");
      if (this->enoughData("staging"))
      {
        // estimate model
        // set the default upper value as 128
        // we only try to adjust staging data here
        int N = 128;
        int size = this->m_historicalDataStaging.size();
        this->m_models[modelName] =
          std::make_unique<Model>(this->m_historicalDataStaging[size - 2].m_processNum,
            this->m_historicalDataStaging[size - 2].m_executionTime,
            this->m_historicalDataStaging[size - 1].m_processNum,
            this->m_historicalDataStaging[size - 1].m_executionTime, N);
        spdlog::info("ok to create a {} model, data {} {} {} {}", modelName,
          this->m_historicalDataStaging[size - 2].m_processNum,
          this->m_historicalDataStaging[size - 2].m_executionTime,
          this->m_historicalDataStaging[size - 1].m_processNum,
          this->m_historicalDataStaging[size - 1].m_executionTime);
      }
      else
      {
        spdlog::info("staging data not exist, no enough data, return 1");
        return 1;
      }
    }

    // only access the content without managing its lifecycle
    Model* tempmodel = this->m_models[modelName].get();
    double newProcessNum = exp((log(targetExecTime) - log(tempmodel->m_a)) / tempmodel->m_b);
    spdlog::info(
      "newprocessnum: {} check newProcessNum estimate parameters targetExecTime {} a {} b {} ",
      newProcessNum, targetExecTime, tempmodel->m_a, tempmodel->m_b);
    if (newProcessNum < currentP * 1.0)
    {
      std::cout << "warning: newProcessNum: " << newProcessNum << " currentP: " << currentP
                << std::endl;
      //throw std::runtime_error(
      //  "model caculation error, the new process number should larger than current num");
      //it is possible that current waitime is old and new added process did not play a row
      return 0;
    }

    return ceil(newProcessNum - currentP);
  }

  // how to let process itsself know it should leave?
  // all get the gjoin list and rank id, then the first one in gjoin list should move
  // or for testing, from last to the first
};

#endif
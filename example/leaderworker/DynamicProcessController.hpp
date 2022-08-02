#ifndef __DYNAMIC_PROCESS_CONTROLLER_HPP
#define __DYNAMIC_PROCESS_CONTROLLER_HPP

#include <iostream>
#include <map>
#include <math.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

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

struct Model2d
{
  // if we init the model with some prior knowledge, we use this constructor
  // proc num, datasize, exectime
  Model2d(double p1[3], double p2[3])
  {

    if (abs(p1[0] - p2[0]) > 0.00001)
    {
      throw std::runtime_error("the two init points used in the model should equal for x");
    }

    this->m_x1 = p1[0];

    double z11 = p1[2];
    double z12 = p2[2];

    double y1 = p1[1];
    double y2 = p2[1];

    std::cout << "z11 z12 y1 y2 " << z11 << " " << z12 << " " << y1 << " " << y2 << std::endl;
    this->m_bx1 = (z11 - z12) / (y1 - y2);
    this->m_ax1 = z11 - this->m_bx1 * y1;
  }

  // index 0 is the process number
  // index 1 is the data size
  // index 2 is the execution time
  Model2d(double p1[3], double p2[3], double p3[3])
  {
    std::cout << "model2d" << std::endl;
    std::cout << "p1 " << p1[0] << "," << p1[1] << "," << p1[2] << std::endl;
    std::cout << "p2 " << p2[0] << "," << p2[1] << "," << p2[2] << std::endl;
    std::cout << "p3 " << p3[0] << "," << p3[1] << "," << p3[2] << std::endl;

    double x1 = p1[0];
    m_x1 = x1;
    double y1 = p1[1];
    double z1 = p1[2];

    double y2 = p2[1];
    double z2 = p2[2];

    double x2 = p3[0];
    double y3 = p3[1];
    double z3 = p3[2];

    this->m_bx1 = (z1 - z2) / (y1 - y2);
    this->m_ax1 = z1 - this->m_bx1 * y1;
    std::cout << z3 << " " << z2 << " " << x2 << " " << x1 << std::endl;

    double newp2x = this->m_ax1;
    double newp2y = y3;
    double newp2z = m_ax1 + this->m_bx1 * newp2y;

    y2 = newp2y;
    z2 = newp2z;

    this->m_by3 = ((log(z3) - log(z2)) * 1.0) / ((log(x2) - log(x1)) * 1.0);
  }

  Model2d(const Model2d& other) = default;

  void addPoint(double p3[3])
  {
    if (abs(p3[0] - this->m_x1) < 0.00001)
    {
      throw std::runtime_error("added node should be different with the m_x1");
    }

    double x2 = p3[0];
    double y3 = p3[1];
    double z23 = p3[2];

    double z13 = this->m_ax1 + this->m_bx1 * y3;

    this->m_by3 = ((log(z13) - log(z23))) / ((log(this->m_x1) - log(x2)));
    // double logay3=log(z13)-by3*log(this->m_x1);
    this->m_p3added = true;
  }

  int getExpectedProcNum(double datasize, double expectedExec)
  {

    double yn = datasize;
    double zmn = expectedExec;

    double byn = this->m_by3;

    double z1n = this->m_ax1 + this->m_bx1 * yn;

    //std::cout << "debug m_x1 " << m_x1 << " m_bx1 " << m_bx1 << " m_ax1 " << m_ax1 << " m_by3 "
    //          << m_by3 << " z1n " << z1n << std::endl;
    
    //std::cout  << "z1n, byn, this->m_ax1 " << z1n << " " << byn << " " << this->m_ax1 << std::endl;
    double log_ayn = log(z1n) - byn * log(this->m_x1);
    //std::cout << "debug log_ayn" << log(z1n) << "," << log(this->m_x1) << std::endl;

    double log_xm = (log(zmn) - log_ayn) / byn;
    //std::cout <<"log_ayn, log_xm, x_m " << log_ayn << " " << log_xm << " " << exp(log_xm) << std::endl;
    return int(exp(log_xm));
  }

  double m_x1 = 0;
  double m_bx1 = 0;
  double m_ax1 = 0;
  double m_by3 = 0;
  bool m_p3added = false;
};

// the history data used for expecting model
struct HistoryData
{
  HistoryData(double executionTime, size_t processNum, double dataSize = 0)
    : m_executionTime(executionTime)
    , m_processNum(processNum)
    , m_dataSize(dataSize)
  {
  }
  double m_executionTime;
  size_t m_processNum;
  double m_dataSize;
};

struct DynamicProcessController
{

  DynamicProcessController(){};

  ~DynamicProcessController(){};
  std::map<std::string, std::unique_ptr<Model2d> > m_models2d;
  std::map<std::string, std::unique_ptr<Model> > m_models;
  // the vector to store historical data for model evaluation
  std::vector<HistoryData> m_historicalDataSim;
  std::vector<HistoryData> m_historicalDataStaging;

  void recordData(
    std::string component, double executionTime, size_t processNum, double dataSize = 0)
  {
    if (component.compare("sim") == 0)
    {
      this->m_historicalDataSim.push_back(HistoryData(executionTime, processNum, dataSize));
    }
    else if (component.compare("staging") == 0)
    {
      this->m_historicalDataStaging.push_back(HistoryData(executionTime, processNum, dataSize));
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

  // when we use the model that contains two variables, we use this to decide if there are enough
  // three elements are: [0]execution time, [1]processNum, [2]dataSize
  bool enoughDataSim2d()
  {
    bool enoughdata = false;
    // if model exist, return true
    std::string modelName = "default";
    if (this->m_models2d.find(modelName) != m_models2d.end())
    {
      std::cout << "enoughDataSim2d check, model exist" << std::endl;
      return true;
    }
    int size = this->m_historicalDataSim.size();
    if (size >= 3 &&
      (this->m_historicalDataSim[size - 1].m_processNum !=
        this->m_historicalDataSim[size - 2].m_processNum) &&
      (this->m_historicalDataSim[size - 2].m_processNum ==
        this->m_historicalDataSim[size - 3].m_processNum))
    {
      enoughdata = true;
    }

    return enoughdata;
  }

  int expectedStagingNum(const double& targetExecTime){
    double m_a = 17.3360;
    double m_b = -0.5455;
    double newProcessNum = exp((log(targetExecTime) - log(m_a)) / m_b);
    spdlog::info(
      "newprocessnum: {} check newProcessNum estimate parameters targetExecTime {} a {} b {} ",
      newProcessNum, targetExecTime, m_a, m_b);
      
    return ceil(newProcessNum);
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
      // throw std::runtime_error(
      //  "model caculation error, the new process number should larger than current num");
      // it is possible that current waitime is old and new added process did not play a row
      return 0;
    }

    return ceil(newProcessNum - currentP);
  }

  // how to let process itsself know it should leave?
  // all get the gjoin list and rank id, then the first one in gjoin list should move
  // or for testing, from last to the first

  int dynamicAddProcessToStaging2d(
    const int& currentP, double datasize, double targetedExecutionTime)
  {

    // TODO logtarget_x = (log(targetedExecutionTime) - log(ax1 + bx1*datasize) +  by2*log(x1))/by2
    std::string modelName = "default";
    Model2d* p = nullptr;
    if (this->m_models2d.find(modelName) == m_models2d.end())
    {
      // the model does not exist
      spdlog::info("m_models2d not exist, create m_models2d based on existing data");
      if (this->enoughDataSim2d())
      {
        // estimate model
        // set the default upper value as 128
        // we only try to adjust staging data here
        int size = this->m_historicalDataSim.size();

        // reorganize the data into the parameter ok for Model2d
        // for p1 p2 and p3
        // three elements are: [0]execution time, [1]processNum, [2]dataSize
        /*
        double m_executionTime;
        size_t m_processNum;
        size_t m_dataSize;
        */
        double p1[3] = { this->m_historicalDataSim[size - 3].m_processNum,
          this->m_historicalDataSim[size - 3].m_dataSize,
          this->m_historicalDataSim[size - 3].m_executionTime };

        double p2[3] = { this->m_historicalDataSim[size - 2].m_processNum,
          this->m_historicalDataSim[size - 2].m_dataSize,
          this->m_historicalDataSim[size - 2].m_executionTime };

        double p3[3] = { this->m_historicalDataSim[size - 1].m_processNum,
          this->m_historicalDataSim[size - 1].m_dataSize,
          this->m_historicalDataSim[size - 1].m_executionTime };

        this->m_models2d[modelName] = std::make_unique<Model2d>(p1, p2, p3);
        p = this->m_models2d[modelName].get();

        // compute the targeted execution time
      }
      else
      {
        spdlog::info("staging data not exist, no enough data, return 1");
        return 1;
      }
    }
    else
    {
      // if we find the model
      p = this->m_models2d[modelName].get();
    }

    if (p == nullptr)
    {
      throw std::runtime_error("the pointer p is not supposed to be empty");
    }

    double bx1 = p->m_bx1;
    double ax1 = p->m_ax1;
    double by3 = p->m_by3;
    double x1 = p->m_x1;
    double target_y = datasize;

    double k =
      (1.0 * log(targetedExecutionTime) - 1.0 * log(ax1 + bx1 * target_y) + 1.0 * by3 * log(x1)) /
      (1.0 * by3);

    double target_x = exp(k);

    std::cout << "debug model parameters " << bx1 << " " << ax1 << " " << by3 << " " << x1 << " "
              << target_y << " " << target_x << std::endl;

    if (target_x < currentP)
    {
      std::cout << " k is " << k << " do not support decreasing the process number target_x is "
                << target_x << " currentP is " << currentP << std::endl;

      return 0;
    }
    return ceil(target_x - 1.0 * currentP);
  }
};

#endif
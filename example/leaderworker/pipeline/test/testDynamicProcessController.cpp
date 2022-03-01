
#include "../../DynamicProcessController.hpp"

void testCalculation()
{
  std::cout << "------testCalculation------" << std::endl;
  DynamicProcessController dc;
  // process number
  double xlist[] = { 3, 4 };
  // execution time
  double ylist[] = { 15.79 - 3, 13.50 - 3 };
  int N = 128;

  // create model and put it into the controller
  dc.m_models["default"] = std::make_unique<Model>(xlist[0], ylist[0], xlist[1], ylist[1], N);

  // test the join calculation
  double target = 7.27;
  int increadNum = dc.dynamicAddProcessToStaging("default", 4, target);
  std::cout << "guarantee " << target << " need to increase " << increadNum << std::endl;
}

void testCalculation2d()
{
  std::cout << "------testCalculation2d------" << std::endl;
  DynamicProcessController dc;
  // execution time, process number, data size
  dc.recordData("sim", 5.7459, 4, 161.313);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  dc.recordData("sim", 5.87288, 4, 169.613);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  dc.recordData("sim", 6.13427, 4, 177.789);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  dc.recordData("sim", 7.09789, 5, 201.286);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  // dc.recordData("sim", 8.09789, 6, 250.286);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  // dc.recordData("sim", 7.0062, 5, 3001.55/16.0);
  // std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  // std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133/16.0, 13) <<
  // std::endl;
}

void testCalculation2dPriorInfo()
{
  std::cout << "---testCalculation2dPriorInfo---" << std::endl;
  double p1[3] = { 4.0, 169.613, 5.7534 };
  double p2[3] = { 4.0, 728.73883, 24.9022 };

  // create the model
  Model2d md(p1, p2);

  // check enough data
  if (md.m_p3added==true)
  {
    throw std::runtime_error("model should not be ready");
  }

  double p3[3] = { 6.0, 188.21533, 4.65162 };
  // add new point
  md.addPoint(p3);
  // check enough data
  if (md.m_p3added==false)
  {
    throw std::runtime_error("model should be ready");
  }

  // 1223.643, 21.8646
  // 1028.746, 18.7495
  // 848.8925, 16.2486
  // 728.73883, 13.2295
  // 629.4637, 10.736
  int procNum = md.getExpectedProcNum(1583.3732, 28.8258);
  std::cout << "get proc num1: " << procNum << std::endl;

  procNum = md.getExpectedProcNum(1223.643, 21.8646);
  std::cout << "get proc num2: " << procNum << std::endl;

  procNum = md.getExpectedProcNum(1028.746, 18.7495);
  std::cout << "get proc num3: " << procNum << std::endl;

  procNum = md.getExpectedProcNum(848.8925, 28.8258);
  std::cout << "get proc num4: " << procNum << std::endl;
}

int main()
{
  testCalculation();
  testCalculation2d();
  testCalculation2dPriorInfo();
  return 0;
}
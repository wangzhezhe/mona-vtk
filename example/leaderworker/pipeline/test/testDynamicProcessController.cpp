
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
  dc.recordData("sim", 6.33, 4, 2592.85/16.0);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  dc.recordData("sim", 6.61, 4, 2725.39/16.0);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133, 5) << std::endl;

  dc.recordData("sim", 7.0062, 5, 3001.55/16.0);
  std::cout << "enough data: " << dc.enoughDataSim2d() << std::endl;
  std::cout << "added value " << dc.dynamicAddProcessToStaging2d(0, 6003.1133/16.0, 13) << std::endl;
}

int main()
{
  testCalculation();
  testCalculation2d();
  return 0;
}
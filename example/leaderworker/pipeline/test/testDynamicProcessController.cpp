
#include "../../DynamicProcessController.hpp"


void testCalculation()
{
  std::cout << "------testCalculation------" << std::endl;
  DynamicProcessController dc;
  // process number
  double xlist[] = { 3, 4 };
  // execution time
  double ylist[] = { 15.79-3, 13.50-3 };
  int N = 128;

  // create model and put it into the controller
  dc.m_models["default"] = std::make_unique<Model>(xlist[0], ylist[0], xlist[1], ylist[1], N);

  // test the join calculation
  double target = 7.27;
  int increadNum = dc.dynamicAddProcessToStaging("default", 4, target);
  std::cout << "guarantee " << target << " need to increase " << increadNum << std::endl;
}

int main()
{
  testCalculation();
}
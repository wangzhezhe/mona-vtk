
#include "../Controller.hpp"

void testmodel()
{
  std::cout << "------testmodel------" << std::endl;
  double xlist[] = { 2, 4 };
  double ylist[] = { 1.83801, 0.920036 };
  int N = 128;
  Model m(xlist[0], ylist[0], xlist[1], ylist[1], N);
  // expected resutls: test1 a: 3.671900621388728 b: -0.9983823974093184
  // head_tail_break:  11.367692715084873
  std::cout << "results, a: " << m.m_a << " b: " << m.m_b << " p: " << m.m_p << std::endl;
}

void testJoinCalculation()
{

  // create controller
  tl::engine engine("tcp", THALLIUM_CLIENT_MODE, true, 2);
  // create the controller
  Controller controller(engine, "test", "test");

  double xlist[] = { 2, 4 };
  double ylist[] = { 1.83801, 0.920036 };
  int N = 128;

  // create model and put it into the controller
  controller.m_models["default"] =
    std::make_unique<Model>(xlist[0], ylist[0], xlist[1], ylist[1], N);

  // test the join calculation
  double target = 0.5;
  int increadNum = controller.dynamicJoinProcessNum("default", xlist[0], target);
  std::cout << "guarantee " << target << " increase from 2 to " << xlist[0] + increadNum
            << std::endl;

  target = 0.4;
  increadNum = controller.dynamicJoinProcessNum("default", xlist[0], target);
  std::cout << "guarantee " << target << " increase from 2 to " << xlist[0] + increadNum
            << std::endl;

  target = 0.3;
  increadNum = controller.dynamicJoinProcessNum("default", xlist[0], target);
  std::cout << "guarantee " << target << " increase from 2 to " << xlist[0] + increadNum
            << std::endl;
  target = 0.2;
  increadNum = controller.dynamicJoinProcessNum("default", xlist[0], target);
  std::cout << "guarantee " << target << " increase from 2 to " << xlist[0] + increadNum
            << std::endl;
  target = 0.1;
  increadNum = controller.dynamicJoinProcessNum("default", xlist[0], target);
  std::cout << "guarantee " << target << " increase from 2 to " << xlist[0] + increadNum
            << std::endl;
}

void testTemp()
{
  std::cout << "test testTemp" << std::endl;
  // create controller
  tl::engine engine("tcp", THALLIUM_CLIENT_MODE, true, 2);
  // create the controller
  Controller controller(engine, "test", "test");

  double xlist[] = { 4, 5 };
  double ylist[] = { 47.47, 39.74 };
  int N = 128;

  // create model and put it into the controller
  controller.m_models["default"] =
    std::make_unique<Model>(xlist[0], ylist[0], xlist[1], ylist[1], N);

  // test the join calculation
  double target = 2.0;
  int increadNum = controller.dynamicJoinProcessNum("default", 5, target);
  std::cout << "guarantee " << target << " increase from 5 to " << increadNum << std::endl;
}

int main()
{
  testmodel();
  testJoinCalculation();
  testTemp();
}
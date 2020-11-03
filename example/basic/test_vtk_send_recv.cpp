#include <vtkMPI.h>
#include <MonaController.hpp>
#include "mpi.h"

// for sphere
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkXMLPolyDataWriter.h>

// refer to https://vtk.org/Wiki/VTK/Examples/Cxx/IO/WriteVTP for write vtp
int main(int argc, char* argv[])
{
  // This is here to avoid false leak messages from vtkDebugLeaks when
  // using mpich. It appears that the root process which spawns all the
  // main processes waits in MPI_Init() and calls exit() when
  // the others are done, causing apparent memory leaks for any objects
  // created before MPI_Init().
  MPI_Init(&argc, &argv);
  int procNum;
  MPI_Comm_size(MPI_COMM_WORLD, &procNum);
  // Note that this will create a MonaCommunicator if MPI
  // is configured, vtkThreadedController otherwise.
  // init the controller
  MonaController* controller = MonaController::New();
  controller->Initialize(&argc, &argv, 1);
  std::cout << "ok to init the controller" << std::endl;
  int rank = controller->GetLocalProcessId();
  if (procNum != 2)
  {
    throw std::runtime_error("the proc number should be 2");
  }
  std::cout << "current rank is " << rank << std::endl;

  // vtkMultiProcessController::SetGlobalController(Controller);
  if (rank == 0)
  {
    // create sphere source
    vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetThetaResolution(8);
    sphereSource->SetPhiResolution(8);
    sphereSource->SetStartTheta(0.0);
    sphereSource->SetEndTheta(360.0);
    sphereSource->SetStartPhi(0.0);
    sphereSource->SetEndPhi(180.0);
    sphereSource->LatLongTessellationOff();

    sphereSource->Update();
    vtkSmartPointer<vtkPolyData> polyData = sphereSource->GetOutput();

    // send this poly data
    // vtkDataObject *data, int remoteId, int tag
    std::cout << "---generate sphere source---\n";
    polyData->PrintSelf(std::cout, vtkIndent(0));
    // write the image
    // Write the file by vtkXMLDataSetWriter
    vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    writer->SetFileName("./poly_send.vtp");
    // get the specific polydata and check the results
    writer->SetInputData(polyData);
    writer->Write();

    std::cout << "---rank0 send sphere data---\n";
    controller->Send(polyData, 1, 12345);
  }
  if (rank == 1)
  {
    // recieve the poly data
    vtkNew<vtkPolyData> rcvpData;
    int tag = 12345;
    controller->Receive(rcvpData, 0, 12345);

    // print the results
    std::cout << "---rank1 recieve sphere data---\n";
    rcvpData->PrintSelf(std::cout, vtkIndent(5));
    vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    writer->SetFileName("./poly_recv.vtp");

    // get the specific polydata and check the results
    writer->SetInputData(rcvpData);
    writer->Write();
  }

  MPI_Finalize();
  return 0;
}

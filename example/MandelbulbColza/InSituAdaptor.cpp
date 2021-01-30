#include "InSituAdaptor.hpp"

#include <mpi.h>
#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkCommunicator.h>
#include <vtkFloatArray.h>
#include <vtkIceTContext.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMPI.h>
#include <vtkMPICommunicator.h>
#include <vtkMPIController.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkMultiPieceDataSet.h>
#include <vtkMultiProcessController.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>

#include "mb.hpp"
#include <MonaController.hpp>
#include <iostream>

#ifdef DEBUG_BUILD
#define DEBUG(x) std::cout << x << std::endl;
#else
#define DEBUG(x)                                                                                   \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

namespace
{
vtkMultiProcessController* Controller = nullptr;
vtkCPProcessor* Processor = nullptr;
vtkMultiBlockDataSet* VTKGrid;

// one process generates one data object
void BuildVTKGrid(Mandelbulb& grid, int nprocs, int rank)
{
  int* extents = grid.GetExtents();
  vtkNew<vtkImageData> imageData;
  imageData->SetSpacing(1.0 / nprocs, 1, 1);
  imageData->SetExtent(extents);
  imageData->SetOrigin(
    grid.GetOrigin()); // Not necessary for (0,0,0) // the origin is different for different block
  vtkNew<vtkMultiPieceDataSet> multiPiece;
  multiPiece->SetNumberOfPieces(nprocs);
  multiPiece->SetPiece(rank, imageData.GetPointer());
  VTKGrid->SetNumberOfBlocks(1);
  VTKGrid->SetBlock(0, multiPiece.GetPointer());
}

// one process generates multiple data objects
void BuildVTKGridList(std::vector<Mandelbulb>& gridList, int global_blocks)
{
  int local_piece_num = gridList.size();
  vtkNew<vtkMultiPieceDataSet> multiPiece;
  multiPiece->SetNumberOfPieces(local_piece_num);

  for (int i = 0; i < local_piece_num; i++)
  {
    int* extents = gridList[i].GetExtents();
    vtkNew<vtkImageData> imageData;
    imageData->SetSpacing(1.0 / global_blocks, 1, 1);
    imageData->SetExtent(extents);
    imageData->SetOrigin(gridList[i].GetOrigin());
    multiPiece->SetPiece(i, imageData.GetPointer());
  }

  // one block conains one multipiece, one multipiece contains multiple actual data objects
  if (local_piece_num == 0)
  {
    // when there is no grid, and it is dummy node
    VTKGrid->SetNumberOfBlocks(0);
  }
  else
  {
    VTKGrid->SetNumberOfBlocks(1);
    VTKGrid->SetBlock(0, multiPiece.GetPointer());
  }
}

void UpdateVTKAttributes(Mandelbulb& mandelbulb, int rank, vtkCPInputDataDescription* idd)
{
  vtkMultiPieceDataSet* multiPiece = vtkMultiPieceDataSet::SafeDownCast(VTKGrid->GetBlock(0));
  if (idd->IsFieldNeeded("mandelbulb", vtkDataObject::POINT))
  {
    vtkDataSet* dataSet = vtkDataSet::SafeDownCast(multiPiece->GetPiece(rank));
    if (dataSet->GetPointData()->GetNumberOfArrays() == 0)
    {
      // pressure array
      vtkNew<vtkIntArray> data;
      data->SetName("mandelbulb");
      data->SetNumberOfComponents(1);
      dataSet->GetPointData()->AddArray(data.GetPointer());
    }
    vtkIntArray* data = vtkIntArray::SafeDownCast(dataSet->GetPointData()->GetArray("mandelbulb"));
    // The pressure array is a scalar array so we can reuse
    // memory as long as we ordered the points properly.
    int* theData = mandelbulb.GetData();
    data->SetArray(theData, static_cast<vtkIdType>(mandelbulb.GetNumberOfLocalCells()), 1);
  }
}

void UpdateVTKAttributesList(
  std::vector<Mandelbulb>& mandelbulbList, vtkCPInputDataDescription* idd)
{
  int pieceNum = mandelbulbList.size();
  if (pieceNum > 0)
  {
    //there is piece only when there is datablock
    vtkMultiPieceDataSet* multiPiece = vtkMultiPieceDataSet::SafeDownCast(VTKGrid->GetBlock(0));
    if (idd->IsFieldNeeded("mandelbulb", vtkDataObject::POINT))
    {
      for (int i = 0; i < pieceNum; i++)
      {
        vtkDataSet* dataSet = vtkDataSet::SafeDownCast(multiPiece->GetPiece(i));
        if (dataSet->GetPointData()->GetNumberOfArrays() == 0)
        {
          // pressure array
          vtkNew<vtkIntArray> data;
          data->SetName("mandelbulb");
          data->SetNumberOfComponents(1);
          dataSet->GetPointData()->AddArray(data.GetPointer());
        }
        vtkIntArray* data =
          vtkIntArray::SafeDownCast(dataSet->GetPointData()->GetArray("mandelbulb"));
        // The pressure array is a scalar array so we can reuse
        // memory as long as we ordered the points properly.
        // std::cout << "set actual value for piece " << i << std::endl;
        int* theData = mandelbulbList[i].GetData();
        data->SetArray(
          theData, static_cast<vtkIdType>(mandelbulbList[i].GetNumberOfLocalCells()), 1);
      }
    }
  }
}

void BuildVTKDataStructures(
  Mandelbulb& mandelbulb, int nprocs, int rank, vtkCPInputDataDescription* idd)
{
  if (VTKGrid == NULL)
  {
    // The grid structure isn't changing so we only build it
    // the first time it's needed. If we needed the memory
    // we could delete it and rebuild as necessary.
    VTKGrid = vtkMultiBlockDataSet::New();
    BuildVTKGrid(mandelbulb, nprocs, rank);
  }
  UpdateVTKAttributes(mandelbulb, rank, idd);
}

void BuildVTKDataStructuresList(
  std::vector<Mandelbulb>& mandelbulbList, int global_nblocks, vtkCPInputDataDescription* idd)
{
  // reset vtk grid for each call??
  // if there is memory leak here
  if (VTKGrid != NULL)
  {
    // The grid structure isn't changing so we only build it
    // the first time it's needed. If we needed the memory
    // we could delete it and rebuild as necessary.
    // delete VTKGrid;
    // refer to https://vtk.org/Wiki/VTK/Tutorials/SmartPointers
    VTKGrid->Delete();
  }

  // reset the grid each time, since the block number may change for different steps, block offset
  // may also change
  VTKGrid = vtkMultiBlockDataSet::New();
  BuildVTKGridList(mandelbulbList, global_nblocks);

  // fill in actual values
  UpdateVTKAttributesList(mandelbulbList, idd);
}
} // namespace

namespace InSitu
{

void* icetFactoryMona(vtkMultiProcessController* controller, void* args)
{
  // return the icet communicator based on colza
  DEBUG("---icetFactoryMona is called to create the icet comm");
  auto m_comm = ((MonaCommunicator*)(controller->GetCommunicator()))->GetMonaComm()->GetHandle();
  if (m_comm == nullptr)
  {
    std::cerr << "failed to get the colza communicator by icetFactoryMona" << std::endl;
    return nullptr;
  }
  return icetCreateMonaCommunicator(m_comm);
}

void MPIInitialize(const std::string& script)
{
  DEBUG("InSituAdaptor MPIInitialize Start ");
  vtkMPICommunicator* communicator = vtkMPICommunicator::New();
  vtkMPIController* controller = vtkMPIController::New();
  controller->SetCommunicator(communicator);
  controller->Initialize(nullptr, nullptr, 1);
  Controller = controller;

  if (Processor == NULL)
  {
    Processor = vtkCPProcessor::New();
    Processor->Initialize("./");
    vtkMultiProcessController::SetGlobalController(controller);
  }
  else
  {
    Processor->RemoveAllPipelines();
  }

  vtkNew<vtkCPPythonScriptPipeline> pipeline;
  pipeline->Initialize(script.c_str());

  Processor->AddPipeline(pipeline.GetPointer());
  DEBUG("InSituAdaptor MPIInitialize Finish ");
}

void MonaInitialize(const std::string& script, mona_comm_t mona_comm)
{
  DEBUG("InSituAdaptor Initialize Start ");
  MonaCommunicator* communicator = MonaCommunicator::New();
  MonaController* controller = MonaController::New();
  // controller->SetCommunicator(communicator);
  // the initilize operation will also init the communicator
  // there are segfault to call the setCommunicator then call the Init
  controller->Initialize(nullptr, nullptr, 1, mona_comm);
  Controller = controller;

  // register the icet communicator into the paraview
  // based on the paraview patch
  // https://gitlab.kitware.com/mdorier/paraview/-/commit/3423280e57778a0f8d208543caf2e01ba2524e02
  // related issue https://discourse.paraview.org/t/glgenframebuffers-errors-in-pvserver-5-8/3632/15
  // refer to this commit to check how to use different communicator for MPI example
  // https://gitlab.kitware.com/paraview/paraview/-/merge_requests/4361
  vtkIceTContext::RegisterIceTCommunicatorFactory("MonaCommunicator", icetFactoryMona, controller);

  /* to make sure no mpi barrier is used here*/
  /* this part may contains some operations that hangs the current mona logic*/
  if (Processor == NULL)
  {

    vtkMultiProcessController::SetGlobalController(controller);
    Processor = vtkCPProcessor::New();
    // the global controller is acquired during the Initialize
    // we want the mona to be used, so we set the controller before the init
    Processor->Initialize("./");
    // It is important to set the controller again to make sure to use the mochi
    // controller, the controller might be replaced during the init process
    // the processor new will set the mpi controller
  }
  else
  {
    Processor->RemoveAllPipelines();
  }

  vtkNew<vtkCPPythonScriptPipeline> pipeline;
  pipeline->Initialize(script.c_str());
  Processor->AddPipeline(pipeline.GetPointer());
  DEBUG("InSituAdaptor Initialize Finish ");
}

void Finalize()
{
  if (Processor)
  {
    Processor->Delete();
    Processor = NULL;
  }
  if (VTKGrid)
  {
    VTKGrid->Delete();
    VTKGrid = NULL;
  }
}

void MonaCoProcessDynamic(mona_comm_t mona_comm, std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep)
{
  DEBUG("---execute MonaCoProcessDynamic");
  if (mona_comm != NULL)
  {
    // reset the communicator if it is not null
    MonaCommunicatorOpaqueComm opaqueComm(mona_comm);
    vtkNew<MonaCommunicator> monaCommunicator;
    monaCommunicator->InitializeExternal(&opaqueComm);
    if (auto controller =
          MonaController::SafeDownCast(vtkMultiProcessController::GetGlobalController()))
    {
      controller->SetCommunicator(monaCommunicator);
    }
    else
    {
      throw std::runtime_error(
        "Cannot change communicator since existing global controller is not a MonaController.");
    }
  }

  // actual execution of the coprocess
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);

  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    // TODO use the blocknumber and blockid
    // std::cout << "debug list size " << mandelbulbList.size() << " for rank " << rank <<
    // std::endl;
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructuresList(mandelbulbList, global_nblocks, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  }

  return;
}

void MPICoProcessDynamic(MPI_Comm subcomm, std::vector<Mandelbulb>& mandelbulbList,
  int global_nblocks, double time, unsigned int timeStep)
{
  // set the new communicator
  vtkMPICommunicatorOpaqueComm opaqueComm(&subcomm);
  vtkNew<vtkMPICommunicator> mpiCommunicator;
  mpiCommunicator->InitializeExternal(&opaqueComm);
  if (auto controller =
        vtkMPIController::SafeDownCast(vtkMultiProcessController::GetGlobalController()))
  {
    controller->SetCommunicator(mpiCommunicator);
  }
  else
  {
    throw std::runtime_error(
      "Cannot change communicator since existing global controller is not a vtkMPIController.");
  }

  // actual execution of the coprocess
  // this is for vti
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);

  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    // TODO use the blocknumber and blockid
    // std::cout << "debug list size " << mandelbulbList.size() << " for rank " << rank <<
    // std::endl;
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructuresList(mandelbulbList, global_nblocks, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  }
}
// this paraview branch support the MPI communicator changing dynamically
// https://gitlab.kitware.com/mdorier/paraview/-/tree/dev-icet-integration
void MPICoProcess(Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep)
{
  // switch the communicator
  const int color = (rank == 0 || ((timeStep * rank) % 2 == 0)) ? 0 : MPI_UNDEFINED;
  MPI_Comm subcomm;
  MPI_Comm_split(MPI_COMM_WORLD, color, rank, &subcomm);

  if (subcomm != MPI_COMM_NULL)
  {

    int sub_rank, sub_nprocs;
    MPI_Comm_rank(subcomm, &sub_rank);
    MPI_Comm_size(subcomm, &sub_nprocs);
    if (sub_rank == 0)
    {
      printf("---timeStep=%d, subgroup nrank=%d\n", timeStep, sub_nprocs);
    }

    DEBUG("InSituAdaptor MPICoProcess Start for rank " << rank);
    // set the new communicator
    vtkMPICommunicatorOpaqueComm opaqueComm(&subcomm);
    vtkNew<vtkMPICommunicator> mpiCommunicator;
    mpiCommunicator->InitializeExternal(&opaqueComm);
    if (auto controller =
          vtkMPIController::SafeDownCast(vtkMultiProcessController::GetGlobalController()))
    {
      controller->SetCommunicator(mpiCommunicator);
    }
    else
    {
      throw std::runtime_error(
        "Cannot change communicator since existing global controller is not a vtkMPIController.");
    }

    // actual execution of the coprocess
    vtkNew<vtkCPDataDescription> dataDescription;
    dataDescription->AddInput("input");
    dataDescription->SetTimeData(time, timeStep);
    if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
    {
      vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
      BuildVTKDataStructures(mandelbulb, nprocs, rank, idd);
      idd->SetGrid(VTKGrid);
      Processor->CoProcess(dataDescription.GetPointer());
    }
    DEBUG("InSituAdaptor MPICoProcess Finish for rank " << rank);
  }
}

void MonaCoProcess(Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep)
{
  DEBUG("InSituAdaptor MonaCoProcess Start ");
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);
  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructures(mandelbulb, nprocs, rank, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  }
  DEBUG("InSituAdaptor MonaCoProcess Finish ");
}

} // namespace InSitu

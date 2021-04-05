#include "MonaInSituAdaptor.hpp"

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
#include <spdlog/spdlog.h>
#include <iostream>

#ifdef DEBUG_BUILD
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define DEBUG(...)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

namespace
{
vtkMultiProcessController* Controller = nullptr;
vtkCPProcessor*            Processor = nullptr;
vtkMultiBlockDataSet*      VTKGrid = nullptr;

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
    // there is piece only when there is datablock
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
  //there is known issue if we delete VTKGrid every time
  if (VTKGrid == NULL)
  {
    VTKGrid = vtkMultiBlockDataSet::New();
  }

  // fill in actual values
  BuildVTKGridList(mandelbulbList, global_nblocks);
  UpdateVTKAttributesList(mandelbulbList, idd);
}
} // namespace

namespace InSitu
{

void* icetFactoryMona(vtkMultiProcessController* controller, void* args)
{
  // return the icet communicator based on colza
  DEBUG("{}", __FUNCTION__);
  auto m_comm = ((MonaCommunicator*)(controller->GetCommunicator()))->GetMonaComm()->GetHandle();
  if (m_comm == nullptr)
  {
    spdlog::error("Failed to extract MonaCommunicator in {}", __FUNCTION__);
    return nullptr;
  }
  return icetCreateMonaCommunicator(m_comm);
}

void MonaInitialize(const std::string& script, mona_comm_t mona_comm)
{
  DEBUG("{}: script={}, comm={}", __FUNCTION__, script, (void*)mona_comm);
  MonaController* controller = MonaController::New();
  // MonaCommunicator* communicator = MonaCommunicator::New();
  // controller->SetCommunicator(communicator);
  // NOTE: no need for the above; the initilize operation will also init
  // the communicator internally.
  // There are segfault to call the setCommunicator then call the Init
  controller->Initialize(mona_comm);
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
  // issue, when delete, the processor is new one?
  if (Processor == NULL)
  {
    DEBUG("{}: Setting Global Controller", __FUNCTION__);
    vtkMultiProcessController::SetGlobalController(controller);
    DEBUG("{}: Creating new vtkCPProcessor", __FUNCTION__);
    Processor = vtkCPProcessor::New();
    // the global controller is acquired during the Initialize
    // we want the mona to be used, so we set the controller before the init
    DEBUG("{}: Initializing the Processor", __FUNCTION__);
    Processor->Initialize("./");
    DEBUG("{}: Done initializing Processor", __FUNCTION__);

    // It is important to set the controller again to make sure to use the mochi
    // controller, the controller might be replaced during the init process
    // the processor new will set the mpi controller
  }
  else
  {
    DEBUG("{}: Processor already initialized, removing all pipelines", __FUNCTION__);
    Processor->RemoveAllPipelines();
    DEBUG("{}: Done removing all the pipelines", __FUNCTION__);
  }

  DEBUG("{}: Initializing pipeline", __FUNCTION__);
  vtkNew<vtkCPPythonScriptPipeline> pipeline;
  pipeline->Initialize(script.c_str());
  DEBUG("{}: Done initializing pipeline", __FUNCTION__);

  Processor->AddPipeline(pipeline.GetPointer());
  DEBUG("{}: Done adding pipeline to processor", __FUNCTION__);
}

void Finalize()
{
  DEBUG("{}: Finalizing", __FUNCTION__);
  if (Processor)
  {
    DEBUG("{}: Deleting Processor", __FUNCTION__);
    Processor->Delete();
    Processor = NULL;
  }
  if (VTKGrid)
  {
    DEBUG("{}: Deleting VTKGrid", __FUNCTION__);
    VTKGrid->Delete();
    VTKGrid = NULL;
  }
}

void MonaUpdateController(mona_comm_t mona_comm)
{
  DEBUG("{}: comm={}", __FUNCTION__, (void*)mona_comm);
  if (mona_comm != NULL)
  {
    // the global communicator is updated every time
    // reset the communicator if it is not null
    MonaCommunicatorOpaqueComm opaqueComm(mona_comm);
    // this is a wrapper for the new communicator, it is ok to be an stack object
    // MonaCommunicatorOpaqueComm* opaqueComm = new MonaCommunicatorOpaqueComm(mona_comm);
    // the communicator will be deleted automatically if we use vtkNew
    //  vtkNew<MonaCommunicator> monaCommunicator;
    // old one is freed when execute set communicator
    MonaCommunicator* monaCommunicator = MonaCommunicator::New();
    monaCommunicator->InitializeExternal(&opaqueComm);
    // get the address of the global controller
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
  } else {
    throw std::runtime_error(
            "Cannot set null communicator");
  }
}

// the controller is supposed to be updated when executing this function
void MonaCoProcessDynamic(
  std::vector<Mandelbulb>& mandelbulbList, int global_nblocks, double time, unsigned int timeStep)
{
  DEBUG("{}: local_nblocks={}, total_nblocks={}, time={}, timestep={}", __FUNCTION__,
        mandelbulbList.size(), global_nblocks, time, timeStep);

  // actual execution of the coprocess
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);

  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    DEBUG("{}: Processor->RequestDataDescription return 1", __FUNCTION__);
    // TODO use the blocknumber and blockid
    // std::cout << "debug list size " << mandelbulbList.size() << " for rank " << rank <<
    // std::endl;
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructuresList(mandelbulbList, global_nblocks, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  } else {
    DEBUG("{}: Processor->RequestDataDescription(dataDescription.GetPointer()) returned 0", __FUNCTION__);
  }

  DEBUG("{}: MonaCoProcessDynamic completed", __FUNCTION__);
  return;
}

void MonaCoProcess(Mandelbulb& mandelbulb, int nprocs, int rank, double time, unsigned int timeStep)
{
  DEBUG("{}: nprocs={}, rank={}, time={}, timestep={}", __FUNCTION__, nprocs, rank, time, timeStep);
  vtkNew<vtkCPDataDescription> dataDescription;
  dataDescription->AddInput("input");
  dataDescription->SetTimeData(time, timeStep);
  if (Processor->RequestDataDescription(dataDescription.GetPointer()) != 0)
  {
    DEBUG("{}: Processor->RequestDataDescription returned 1", __FUNCTION__);
    vtkCPInputDataDescription* idd = dataDescription->GetInputDescriptionByName("input");
    BuildVTKDataStructures(mandelbulb, nprocs, rank, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  }
  DEBUG("{}: MonaCoProcess completed", __FUNCTION__);
}

} // namespace InSitu

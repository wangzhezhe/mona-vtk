#include "DWIInSituMonaAdaptor.hpp"

#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkCharArray.h>
#include <vtkCommunicator.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkTypeFloat32Array.h>
#include <vtkTypeInt32Array.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>

#include <vtkIceTContext.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkMultiPieceDataSet.h>
#include <vtkMultiProcessController.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>

#include <vtkImageImport.h>
#include <vtkXMLImageDataWriter.h>

#include <icet/mona.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetMapper.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWindowToImageFilter.h>
// this is what we implemented in src
// to aoivd the linker link to other libs installed by spack env
#include "../../../src/MonaController.hpp"

#ifdef DEBUG_BUILD
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define DEBUG(...)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

namespace InSituDW
{

vtkMultiProcessController* Controller = nullptr;
vtkCPProcessor* Processor = nullptr;
mona_comm_t mona_comm_global = nullptr;

// one can add a vtkMultiPieceDataSet as the block and then put the subdatasets as pieces in the
// vtkMultiPieceDataSet.
vtkMultiBlockDataSet* VTKGrid = nullptr;

void checkData(std::map<std::string, std::shared_ptr<DataBlock> >& datablocks, int blockid)
{
  // check the existance of point data array
  if (datablocks.count("points") == 0)
  {
    throw std::runtime_error(
      std::string("points") + " array does not exist for blockid " + std::to_string(blockid));
  }

  if (datablocks.count("cell_offsets") == 0)
  {
    throw std::runtime_error(
      std::string("cell_offsets") + " array does not exist for blockid " + std::to_string(blockid));
  }

  if (datablocks.count("cell_conn") == 0)
  {
    throw std::runtime_error(
      std::string("cell_conn") + " array does not exist for blockid " + std::to_string(blockid));
  }

  if (datablocks.count("cell_types") == 0)
  {
    throw std::runtime_error(
      std::string("cell_types") + " array does not exist for blockid " + std::to_string(blockid));
  }

  if (datablocks.count("rho") == 0)
  {
    throw std::runtime_error(
      std::string("rho") + " array does not exist for blockid " + std::to_string(blockid));
  }
  if (datablocks.count("v02") == 0)
  {
    throw std::runtime_error(
      std::string("v02") + " array does not exist for blockid " + std::to_string(blockid));
  }
}

// one process generates one data object
void BuildVTKGridList(DataBlockMap& dataBlocks, int rank)
{
  int local_piece_num = dataBlocks.size();
  // the multiple piece data is used to contain multiple data partitions in one process
  vtkNew<vtkMultiPieceDataSet> multiPiece;
  multiPiece->SetNumberOfPieces(local_piece_num);
  int pieceCount = 0;

  // TODO start to update here, the key is the blockid and the value is the marshaled data
  // we just need to unmarshal it here instead of extracting it and composing the vtk manually
  for (auto it = dataBlocks.begin(); it != dataBlocks.end(); it++)
  {
    char str[128];
    sprintf(str, "BuildVTKGridList the blockid %d\n", it->first);
    DEBUG("{}", std::string(str));
    // vtkNew<vtkUnstructuredGrid> vtkunstructuredData;

    // unmarshal the data back into the vtk

    vtkNew<vtkCharArray> recvCharArray;
    // set the save to 1 to prevent deleting the array

    if (it->first == 0)
    {
      DEBUG("char array size {}", it->second.data.size());
    }

    recvCharArray->SetArray(
      (char*)it->second.data.data(), static_cast<vtkIdType>(it->second.data.size()), 1);

    // vtkSmartPointer<vtkDataObject> recvbj = vtkCommunicator::UnMarshalDataObject(recvCharArray);
    // vtkSmartPointer<vtkUnstructuredGrid> recvbj =
    // vtkCommunicator::UnMarshalDataObject(recvCharArray); vtkUnstructuredGrid* unstructureGrid =
    // (vtkUnstructuredGrid*)recvbj;

    vtkNew<vtkUnstructuredGrid> ungrid;
    vtkCommunicator::UnMarshalDataObject(recvCharArray, ungrid);
    // if (it->first == 0)
    //{
    //  recvbj->PrintSelf(std::cout, vtkIndent(5));
    //}

    // put the data of the map into the unstructred Data
    // this piece value is local one
    multiPiece->SetPiece(pieceCount, ungrid.GetPointer());
    pieceCount++;
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

  // check out the vtkunstructuredData info
  // check the rank info, only check the first one

  // VTKGrid->PrintSelf(std::cout, vtkIndent(5));
}

void BuildVTKDataStructures(DataBlockMap& dataBlocks)
{
  // there is known render issue if we delete VTKGrid every time
  if (VTKGrid == NULL)
  {
    VTKGrid = vtkMultiBlockDataSet::New();
  }

  // fill in actual values
  // we put the filed data into the piece data direactly
  // we do not need the UpdateVTKAttributes function
  int rank;
  mona_comm_rank(mona_comm_global, &rank);
  BuildVTKGridList(dataBlocks, rank);
}

void* icetFactoryMonaDWI(vtkMultiProcessController* controller, void* args)
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

void DWMonaInitialize(const std::string& script, mona_comm_t mona_comm)
{
  DEBUG("{}", "MonaInitialize Initialize Start");
  MonaController* controller = MonaController::New();
  mona_comm_global = mona_comm;

  controller->Initialize(mona_comm);
  Controller = controller;
  vtkIceTContext::RegisterIceTCommunicatorFactory(
    "MonaCommunicator", icetFactoryMonaDWI, controller);

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
    Processor->RemoveAllPipelines();
  }

  vtkNew<vtkCPPythonScriptPipeline> pipeline;
  pipeline->Initialize(script.c_str());
  Processor->AddPipeline(pipeline.GetPointer());
  DEBUG("{}", "MonaInitialize Initialize Finish");
}

void DWFinalize()
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

void DWMonaUpdateController(mona_comm_t mona_comm)
{
  DEBUG("{}: comm={}", __FUNCTION__, (void*)mona_comm);
  if (mona_comm != NULL)
  {
    mona_comm_global = mona_comm;
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
  }
  else
  {
    throw std::runtime_error("Cannot set null communicator");
  }
}

void DWMonaCoProcessDynamic(DataBlockMap& dataBlocks, double time, unsigned int timeStep)
{
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

    BuildVTKDataStructures(dataBlocks);
    // when all process built its data structure, then do coprocessing
    idd->SetGrid(VTKGrid);
    DEBUG("{}", "Processor CoProcess Start");
    Processor->CoProcess(dataDescription.GetPointer());
  }
}

} // namespace InSituDW

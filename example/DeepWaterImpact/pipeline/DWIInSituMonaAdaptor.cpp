#include "DWIInSituMonaAdaptor.hpp"

#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkCommunicator.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkTypeFloat32Array.h>
#include <vtkTypeInt32Array.h>
#include <vtkUnsignedCharArray.h>

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

#include <iostream>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetMapper.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWindowToImageFilter.h>
#include <icet/mona.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>
//this is what we implemented in src
//to aoivd the linker link to other libs installed by spack env
#include "../../../src/MonaController.hpp"

#ifdef DEBUG_BUILD
#define DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define DEBUG(...)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

namespace InSitu
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
  for (auto it = dataBlocks.begin(); it != dataBlocks.end(); it++)
  {
    char str[128];
    sprintf(str, "BuildVTKGridList the blockid %d\n", it->first);
    DEBUG("{}", std::string(str));
    vtkNew<vtkUnstructuredGrid> vtkunstructuredData;

    // check the existance of point data array
    checkData(it->second, it->first);

    // the raw type of all kinds of arrays are
    // rho type float32 v02 type float32 points array type float32
    // cell_types np.ubyte cell_offsets int cell_conn int

    // create the points information
    vtkNew<vtkTypeFloat32Array> pointArray;
    pointArray->SetNumberOfComponents(3);
    // get pointsData, assume it is the double
    int pointsArraySize = it->second["points"]->data.size() / sizeof(vtkTypeFloat32);
    pointArray->SetArray(
      (float*)it->second["points"]->data.data(), static_cast<vtkIdType>(pointsArraySize), 1);

    vtkNew<vtkPoints> points;
    points->SetData(pointArray);
    vtkunstructuredData->SetPoints(points);

    // set cells based on setdata
    vtkNew<vtkCellArray> cellArray;
    vtkNew<vtkTypeInt64Array> offsetArray;
    offsetArray->SetNumberOfComponents(1);
    vtkNew<vtkTypeInt64Array> connectivityArray;
    connectivityArray->SetNumberOfComponents(1);
    vtkNew<vtkUnsignedCharArray> celltypeArray;
    celltypeArray->SetNumberOfComponents(1);

    int offsetArraySize = it->second["cell_offsets"]->data.size() / sizeof(vtkTypeInt64);
    offsetArray->SetArray((vtkTypeInt64*)it->second["cell_offsets"]->data.data(),
      static_cast<vtkIdType>(offsetArraySize), 1);

    int connectivityArraySize = it->second["cell_conn"]->data.size() / sizeof(vtkTypeInt64);
    connectivityArray->SetArray((vtkTypeInt64*)it->second["cell_conn"]->data.data(),
      static_cast<vtkIdType>(connectivityArraySize), 1);

    int celltypeArraySize =
      it->second["cell_types"]->data.size() / sizeof(VTK_TYPE_NAME_UNSIGNED_CHAR);
    celltypeArray->SetArray((VTK_TYPE_NAME_UNSIGNED_CHAR*)it->second["cell_types"]->data.data(),
      static_cast<vtkIdType>(celltypeArraySize), 1);

    cellArray->SetData(offsetArray, connectivityArray);

    if (it->first == 0)
    {
      //DEBUG("offsetArraySize " << offsetArraySize << " connectivityArraySize "
      //                         << connectivityArraySize << " celltypeArraySize "
      //                         << celltypeArraySize);
      //DEBUG("---cellArray---");
      //cellArray->PrintSelf(std::cout, vtkIndent(0));

      //DEBUG("---celltypeArray---");
      //celltypeArray->PrintSelf(std::cout, vtkIndent(0));
    }

    vtkunstructuredData->SetCells(celltypeArray, cellArray);

    // set the actual data
    vtkDataSet* dataSet = vtkDataSet::SafeDownCast(vtkunstructuredData);

    // set the actual rho data
    vtkNew<vtkTypeFloat32Array> fielddatarho;
    fielddatarho->SetName("rho");
    fielddatarho->SetNumberOfComponents(1);

    int rhoArraysize = it->second["rho"]->data.size() / sizeof(vtkTypeFloat32);
    fielddatarho->SetArray(
      (float*)it->second["rho"]->data.data(), static_cast<vtkIdType>(rhoArraysize), 1);
    dataSet->GetCellData()->AddArray(fielddatarho.GetPointer());

    // set the actual v02 data
    vtkNew<vtkTypeFloat32Array> fielddatav02;
    fielddatav02->SetName("v02");
    fielddatav02->SetNumberOfComponents(1);

    int v02Arraysize = it->second["v02"]->data.size() / sizeof(vtkTypeFloat32);
    fielddatav02->SetArray(
      (float*)it->second["v02"]->data.data(), static_cast<vtkIdType>(v02Arraysize), 1);
    dataSet->GetCellData()->AddArray(fielddatav02.GetPointer());

    // put the data of the map into the unstructred Data
    // this piece value is local one
    multiPiece->SetPiece(pieceCount, vtkunstructuredData.GetPointer());
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

  //VTKGrid->PrintSelf(std::cout, vtkIndent(5));
  
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
  DEBUG("{}", "MonaInitialize Initialize Start");
  MonaController* controller = MonaController::New();
  mona_comm_global = mona_comm;

  controller->Initialize(mona_comm);
  Controller = controller;
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
    Processor->RemoveAllPipelines();
  }

  vtkNew<vtkCPPythonScriptPipeline> pipeline;
  pipeline->Initialize(script.c_str());
  Processor->AddPipeline(pipeline.GetPointer());
  DEBUG("{}","MonaInitialize Initialize Finish");
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

void MonaUpdateController(mona_comm_t mona_comm)
{
  DEBUG("{}: comm={}", __FUNCTION__, (void*)mona_comm);
  if (mona_comm != NULL)
  {
    mona_comm_global=mona_comm;
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

void MonaCoProcessDynamic(DataBlockMap& dataBlocks, double time, unsigned int timeStep)
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

} // namespace InSitu

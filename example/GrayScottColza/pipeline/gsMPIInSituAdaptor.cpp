#include "gsMPIInSituAdaptor.hpp"

#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkCommunicator.h>
#include <vtkDoubleArray.h>
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

#include <vtkImageImport.h>
#include <vtkXMLImageDataWriter.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetMapper.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>

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
void BuildVTKGridList(std::vector<std::shared_ptr<DataBlock> >& dataBlockList)
{

  int local_piece_num = dataBlockList.size();
  vtkNew<vtkMultiPieceDataSet> multiPiece;
  multiPiece->SetNumberOfPieces(local_piece_num);

  for (int i = 0; i < local_piece_num; i++)
  {
    std::array<int, 3> indexlb = { { (int)dataBlockList[i]->offsets[0],
      (int)dataBlockList[i]->offsets[1], (int)dataBlockList[i]->offsets[2] } };
    std::array<int, 3> indexub = {
      { (int)(dataBlockList[i]->offsets[0] + dataBlockList[i]->dimensions[0] - 1),
        (int)(dataBlockList[i]->offsets[1] + dataBlockList[i]->dimensions[1] - 1),
        (int)(dataBlockList[i]->offsets[2] + dataBlockList[i]->dimensions[2] - 1) }
    };

    vtkNew<vtkImageData> imageData;
    imageData->SetSpacing(1, 1, 1);
    imageData->SetExtent(indexlb[0], indexub[0], indexlb[1], indexub[1], indexlb[2], indexub[2]);
    // imageData->SetOrigin(indexlb[0], indexlb[1], indexlb[2]);
    imageData->SetOrigin(0, 0, 0);
    // this piece value is local one
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

void UpdateVTKAttributesList(
  std::vector<std::shared_ptr<DataBlock> >& dataBlockList, vtkCPInputDataDescription* idd)
{

  int pieceNum = dataBlockList.size();
  if (pieceNum > 0)
  {
    vtkMultiPieceDataSet* multiPiece = vtkMultiPieceDataSet::SafeDownCast(VTKGrid->GetBlock(0));
    if (idd->IsFieldNeeded("grayscottu", vtkDataObject::POINT))
    {
      for (int i = 0; i < pieceNum; i++)
      {
        vtkDataSet* dataSet = vtkDataSet::SafeDownCast(multiPiece->GetPiece(i));
        if (dataSet->GetPointData()->GetNumberOfArrays() == 0)
        {
          //  array to store the grayscottu
          vtkNew<vtkDoubleArray> data;
          data->SetName("grayscottu");
          data->SetNumberOfComponents(1);
          dataSet->GetPointData()->AddArray(data.GetPointer());
        }
        vtkDoubleArray* data =
          vtkDoubleArray::SafeDownCast(dataSet->GetPointData()->GetArray("grayscottu"));
        // The pressure array is a scalar array so we can reuse
        // memory as long as we ordered the points properly.
        // std::cout << "set actual value for piece " << i << std::endl;
        double* theData = (double*)dataBlockList[i]->data.data();
        int elementNum = dataBlockList[i]->dimensions[0] * dataBlockList[i]->dimensions[1] *
          dataBlockList[i]->dimensions[2];
        data->SetArray(theData, static_cast<vtkIdType>(elementNum), 1);
      }
    }
  }
}

void BuildVTKDataStructuresList(
  std::vector<std::shared_ptr<DataBlock> >& dataBlockList, vtkCPInputDataDescription* idd)
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
  BuildVTKGridList(dataBlockList);

  // fill in actual values
  UpdateVTKAttributesList(dataBlockList, idd);
}

} // namespace

namespace InSitu
{

void MPIInitialize(const std::string& script, MPI_Comm mpi_comm)
{
  DEBUG("MPIInitialize Initialize Start ");
  vtkMPICommunicator* communicator = vtkMPICommunicator::New();
  vtkMPIController* controller = vtkMPIController::New();
  // controller->SetCommunicator(communicator);
  // the initilize operation will also init the communicator
  // there are segfault to call the setCommunicator then call the Init
  controller->Initialize(nullptr, nullptr, 1);
  Controller = controller;

  if (Processor == NULL)
  {
    vtkMultiProcessController::SetGlobalController(controller);
    Processor = vtkCPProcessor::New();
    // the global controller is acquired during the Initialize
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
  DEBUG("MPIInitialize Initialize Finish ");
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

void MPICoProcessList(
  std::vector<std::shared_ptr<DataBlock> > dataBlockList, double time, unsigned int timeStep)
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
    BuildVTKDataStructuresList(dataBlockList, idd);
    idd->SetGrid(VTKGrid);
    Processor->CoProcess(dataDescription.GetPointer());
  }
}

} // namespace InSitu

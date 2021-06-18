#include "gsMonaInSituAdaptor.hpp"

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

#include <MonaController.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

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
vtkCPProcessor* Processor = nullptr;
vtkMultiBlockDataSet* VTKGrid = nullptr;

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
  // there is known issue if we delete VTKGrid every time
  if (VTKGrid == NULL)
  {
    VTKGrid = vtkMultiBlockDataSet::New();
  }

  // fill in actual values
  BuildVTKGridList(dataBlockList);
  UpdateVTKAttributesList(dataBlockList, idd);
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

void MonaInitialize(const std::string& script, mona_comm_t mona_comm)
{
  DEBUG("InSituAdaptor Initialize Start ");
  MonaCommunicator* communicator = MonaCommunicator::New();
  MonaController* controller = MonaController::New();
  // controller->SetCommunicator(communicator);
  // the initilize operation will also init the communicator
  // there are segfault to call the setCommunicator then call the Init
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

void MonaUpdateController(mona_comm_t mona_comm)
{
  DEBUG("---execute MonaUpdateController");
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
  }
  else
  {
    throw std::runtime_error("Cannot set null communicator");
  }
}

void MonaCoProcessList(
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

void outPutVTIFile(DataBlock& dataBlock, std::string fileName)
{

  std::array<int, 3> indexlb = { { (int)dataBlock.offsets[0], (int)dataBlock.offsets[1],
    (int)dataBlock.offsets[2] } };
  std::array<int, 3> indexub = { { (int)(dataBlock.offsets[0] + dataBlock.dimensions[0] - 1),
    (int)(dataBlock.offsets[1] + dataBlock.dimensions[1] - 1),
    (int)(dataBlock.offsets[2] + dataBlock.dimensions[2] - 1) } };

  auto importer = vtkSmartPointer<vtkImageImport>::New();
  importer->SetDataSpacing(1, 1, 1);
  importer->SetDataOrigin(0, 0, 0);
  // from 0 to the shape -1 or from lb to the ub??
  importer->SetWholeExtent(indexlb[0], indexub[0], indexlb[1], indexub[1], indexlb[2], indexub[2]);
  importer->SetDataExtentToWholeExtent();
  importer->SetDataScalarTypeToDouble();
  importer->SetNumberOfScalarComponents(1);
  importer->SetImportVoidPointer((double*)(dataBlock.data.data()));
  importer->Update();

  // Write the file by vtkXMLDataSetWriter
  vtkSmartPointer<vtkXMLImageDataWriter> writer = vtkSmartPointer<vtkXMLImageDataWriter>::New();
  writer->SetFileName(fileName.data());

  // get the specific polydata and check the results
  writer->SetInputConnection(importer->GetOutputPort());
  // writer->SetInputData(importer->GetOutputPort());
  // Optional - set the mode. The default is binary.
  writer->SetDataModeToBinary();
  // writer->SetDataModeToAscii();
  writer->Write();
}

// refer to https://lorensen.github.io/VTKExamples/site/Cxx/IO/ImageWriter/
// refer to https://vtk.org/Wiki/VTK/Examples/Cxx/Visualization/RenderLargeImage
// refer to https://vtk.org/Wiki/VTK/Examples/Cxx/Utilities/Screenshot
// https://vtk.org/Wiki/VTK/Examples/Cxx/Visualization/VisualizeImageData

void outPutFigure(std::shared_ptr<DataBlock> dataBlock, std::string fileName)
{

  // generate the image data and output the rendered figure
  std::array<int, 3> indexlb = { { (int)dataBlock->offsets[0], (int)dataBlock->offsets[1],
    (int)dataBlock->offsets[2] } };
  std::array<int, 3> indexub = { { (int)(dataBlock->offsets[0] + dataBlock->dimensions[0] - 1),
    (int)(dataBlock->offsets[1] + dataBlock->dimensions[1] - 1),
    (int)(dataBlock->offsets[2] + dataBlock->dimensions[2] - 1) } };

  auto importer = vtkSmartPointer<vtkImageImport>::New();
  importer->SetDataSpacing(1, 1, 1);
  importer->SetDataOrigin(0, 0, 0);
  // from 0 to the shape -1 or from lb to the ub??
  importer->SetWholeExtent(indexlb[0], indexub[0], indexlb[1], indexub[1], indexlb[2], indexub[2]);
  importer->SetDataExtentToWholeExtent();
  importer->SetDataScalarTypeToDouble();
  importer->SetNumberOfScalarComponents(1);
  importer->SetImportVoidPointer((double*)(dataBlock->data.data()));
  importer->Update();

  // vtkSmartPointer<vtkImageData> imgdata = importer->GetOutput();

  ///////////////

  // create the mapper
  vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
  // or use SetInputData if we have existing data
  mapper->SetInputConnection(importer->GetOutputPort());

  // create the actor with the mapper
  vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);

  // create the render and the render window
  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();

  // create the interactor based on the renderWindow
  // vtkSmartPointer<vtkRenderWindowInteractor> interactor =
  //  vtkSmartPointer<vtkRenderWindowInteractor>::New();
  // interactor->SetRenderWindow(renderWindow);

  renderWindow->AddRenderer(renderer);
  renderer->AddActor(actor);

  // Let the renderer compute good position and focal point
  renderer->GetActiveCamera()->Azimuth(30);
  renderer->GetActiveCamera()->Elevation(30);
  renderer->ResetCamera();
  renderer->GetActiveCamera()->Dolly(1.4);
  renderer->ResetCameraClippingRange();
  renderer->SetBackground(.3, .4, .5);

  renderWindow->SetSize(640, 480);
  renderWindow->Render();

  // do not use the interact pattern
  // interactor->Start();
  int magnification = 1;
  std::cout << "Generating large image size: " << renderWindow->GetSize()[0] * magnification
            << " by " << renderWindow->GetSize()[1] * magnification << std::endl;

  // vtkSmartPointer<vtkRenderLargeImage> renderLarge = vtkSmartPointer<vtkRenderLargeImage>::New();
  // renderLarge->SetInput(renderer);
  // renderLarge->SetMagnification(magnification);

  // Screenshot
  vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter =
    vtkSmartPointer<vtkWindowToImageFilter>::New();
  windowToImageFilter->SetInput(renderWindow);
  // windowToImageFilter->SetMagnification(magnification);
  // set the resolution of the output image (3 times the
  // current resolution of vtk render window)
  windowToImageFilter->SetInputBufferTypeToRGBA(); // also record the alpha (transparency) channel
  windowToImageFilter->ReadFrontBufferOff();       // read from the back buffer
  windowToImageFilter->Update();
  fileName = fileName + ".png";
  std::cout << "Saving image in " << fileName << std::endl;
  vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
  writer->SetFileName(fileName.data());
  writer->SetInputConnection(windowToImageFilter->GetOutputPort());
  writer->Write();
}

} // namespace InSitu

#include "rapidxml/rapidxml.hpp"
#include <dirent.h>
#include <stdio.h>
#include <vtkCharArray.h>
#include <vtkCommunicator.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkTableReader.h>
#include <vtkTypeFloat32Array.h>
#include <vtkTypeInt64Array.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLMultiBlockDataReader.h>
#include <vtkXMLUnstructuredGridReader.h>

#include <mpi.h>
#include <spdlog/spdlog.h>
#include <vector>

#include "../TypeSizes.hpp"
#include "../pipeline/StagingClient.hpp"

#include <thallium.hpp>
#include <vector>

#ifdef USE_GNI
extern "C"
{
#include <rdmacred.h>
}
#include <margo.h>
#include <mercury.h>
#define DIE_IF(cond_expr, err_fmt, ...)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (cond_expr)                                                                                 \
    {                                                                                              \
      fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " err_fmt "\n", __FILE__, __LINE__,        \
        ##__VA_ARGS__);                                                                            \
      exit(1);                                                                                     \
    }                                                                                              \
  } while (0)
#endif

static std::string g_server_leader_config = "dynamic_server_leader.config";
static std::string g_drc_file = "dynamic_drc.config";
static std::string dataSetName = "dwater";

std::unique_ptr<tl::engine> globalServerEnginePtr;

void mockComputation(double time)
{
  sleep(time);
  return;
}

// source tar file
// /global/cscratch1/sd/zw241/build_mona-vtk-matthieu/DeepWaterImpactData
int main(int argc, char** argv)
{
  // MPI init
  MPI_Init(&argc, &argv);
  int rank;
  int procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &procs);
  spdlog::set_level(spdlog::level::from_str("info"));

  bool leader = false;
  // only the first created one is the leader
  // the new joined one is not the leader
  if (rank == 0)
  {
    leader = true;
  }

  // for the initial processes
  // the rank 0 process create drc things
  // other process get this by MPI
  struct hg_init_info hii;
  memset(&hii, 0, sizeof(hii));

// all processes can be viewd at g_join type in this case
// since the drc is started by the colza server
#ifdef USE_GNI
  // get the drc id from the shared file
  std::ifstream infile(g_drc_file);
  std::string cred_id;
  std::getline(infile, cred_id);
  if (rank == 0)
  {
    std::cout << "load cred_id: " << cred_id << std::endl;
  }

  char drc_key_str[256] = { 0 };
  uint32_t drc_cookie;
  uint32_t drc_credential_id;
  drc_info_handle_t drc_credential_info;
  int ret;
  drc_credential_id = (uint32_t)atoi(cred_id.c_str());

  ret = drc_access(drc_credential_id, 0, &drc_credential_info);
  DIE_IF(ret != DRC_SUCCESS, "drc_access %u", drc_credential_id);
  drc_cookie = drc_get_first_cookie(drc_credential_info);

  sprintf(drc_key_str, "%u", drc_cookie);
  hii.na_init_info.auth_key = drc_key_str;

  globalServerEnginePtr =
    std::make_unique<tl::engine>(tl::engine("gni", THALLIUM_SERVER_MODE, true, 4, &hii));

#else
  throw std::runtime_error("gni is supposed to be adopted by cori");
#endif

  // create the staging client
  StagingClient stagingClient(globalServerEnginePtr.get(), g_server_leader_config);
  // if the g_join is true the procs is 1
  std::cout << "rank " << rank << " create the controller and staging client ok " << std::endl;

  // start one step(get one new tar file and decompress it in the temp dir)
  // remember to remove temp dir after finish operation
  std::string dwdataDir = "/global/cscratch1/sd/zw241/build_mona-vtk-matthieu/DeepWaterImpactData";
    std::string configFile =
    "/global/homes/z/zw241/cworkspace/src/mona-vtk/example/leaderworker/dwaterclient/config.txt";
    
  int totalStep = 25;
  int totalFilePerStep = 512;
  int workload = totalFilePerStep / procs;
  int residual = totalFilePerStep % procs;
  if (residual != 0)
  {
    throw std::runtime_error("the 512 should divided by process number");
  }

  std::vector<std::string> tarNameList;
  int startIndex = rank * workload;
  int endIndex = (rank + 1) * workload - 1;

  std::string configFile =
    "/global/homes/z/zw241/cworkspace/src/mona-vtk/example/leaderworker/dwaterclient/config.txt";
  // load the name list from the csv
  // and put it into the tarNameList
  FILE* fp = freopen(configFile.data(), "r", stdin);
  char tempfileName[128];
  while (scanf("%s", tempfileName) != EOF)
  {
    tarNameList.push_back(std::string(tempfileName));
  }
  fclose(fp);
  std::vector<char> buffer;
  int buffersize = 0;

  std::vector<tl::async_response> asyncResponses;
  std::vector<tl::async_response> asyncStageResps;

  // TODO maybe make the computation time to be a changeable number?
  double computeTime = 3;

  // the actual one is totalStep
  auto executionStart = tl::timer::wtime();
  // from the step 18 to test if there is memory issue
  int startStep = 23;
  int step = 0;
  for (step = startStep; step < totalStep; step++)
  {
    auto computeStart = tl::timer::wtime();
    // mock sim computation
    mockComputation(computeTime);
    MPI_Barrier(MPI_COMM_WORLD);
    auto computeEnd = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} computation time is {}", step, computeEnd - computeStart);
    }

    // wait the execution finish
    if (step > startStep)
    {
      for (int i = 0; i < asyncResponses.size(); i++)
      {
        // wait the execution finish
        int ret = asyncResponses[i].wait();
        if (ret != 0)
        {
          throw std::runtime_error("failed for execution");
        }
      }

      MPI_Barrier(MPI_COMM_WORLD);
      auto executionEnd = tl::timer::wtime();

      if (leader)
      {
        spdlog::info("iteration {} execution time is {} wait time is {}", step - 1,
          executionEnd - executionStart, executionEnd - computeEnd);
      }
    }

    // clean up
    if (step > startStep)
    {
      auto dataCleanStart = tl::timer::wtime();
      // do not overlap the computation and ana currently
      // we clean up the last step data, not the current step
      stagingClient.cleanup(rank, dataSetName, step - 1);
      MPI_Barrier(MPI_COMM_WORLD);
      auto dataCleanEnd = tl::timer::wtime();
      if (leader)
      {
        spdlog::info(
          "iteration {} data cleanup time is {} ", step - 1, dataCleanEnd - dataCleanStart);
      }
    }

    // start from here for current step
    // the operations above is for the previous steps
    // sync stageService
    auto syncStageStart = tl::timer::wtime();
    if (leader)
    {
      //spdlog::info("start syncstage for step {}", step);
      stagingClient.leadersync(step);
    }
    // use the workflow sync based on MPI comm
    stagingClient.workerSync(leader, step, rank);

    MPI_Barrier(MPI_COMM_WORLD);
    auto syncStageEnd = tl::timer::wtime();

    if (leader)
    {
      spdlog::info("iteration {} syncstage time is {} ", step, syncStageEnd - syncStageStart);
    }

    // bcast the fileName to all processes
    if (rank == 0)
    {

      // decompress ith step into the current dir
      // try system call, it might not a good practice
      // we assume the file have been decompressed
      /* it can improve the speed a lot without copy and untar the file back and forth
      std::string command = "tar -xvf " + dwdataDir + "/" + tarNameList[step] + " >/dev/null";
      system(command.c_str());
      // process the file
      std::cout << "ok to decompress " << tarNameList[step] << " step " << step << std::endl;
      */
      // create the list for processing the vtu file
      // the typical name pv_insitu_10487_0_206.vtu pv_insitu_0_0_186.vtu
      // get info from the vtm file
      std::string tarName = tarNameList[step];
      int found = tarName.find_last_not_of(".tar");
      tarName.erase(found + 1);
      std::string vtmFileName = tarName + ".vtm";

      // std::string vtmFileName = "pv_insitu_07920.vtm";
      // std::cout << "vtmFileName: " << vtmFileName << std::endl;

      ifstream myfile(vtmFileName);

      /* "Read file into vector<char>"  See linked thread above*/
      std::vector<char> tempbuffer(
        (std::istreambuf_iterator<char>(myfile)), std::istreambuf_iterator<char>());
      tempbuffer.push_back('\0');
      buffer.clear();
      buffer = std::move(tempbuffer);
      buffersize = buffer.size();
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // bcast buffer to all processes
    // bcast size and then resize
    MPI_Bcast(&buffersize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    // std::cout << "buffersize is " << buffersize << std::endl;
    // bcast contents
    if (rank != 0)
    {
      buffer.clear();
    }
    buffer.resize(buffersize);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(buffer.data(), buffersize, MPI_CHAR, 0, MPI_COMM_WORLD);

    // std::cout << &buffer[0] << endl; /*test the buffer */
    rapidxml::xml_document<> doc;
    doc.parse<0>(&buffer[0]);

    // access to the file path
    rapidxml::xml_node<>* root = doc.first_node("VTKFile");
    if (root == NULL)
    {
      throw std::runtime_error("root node is null");
    }
    // refer to https://blog.csdn.net/v_xchen_v/article/details/75634273
    // check the other layer
    rapidxml::xml_node<>* layer1 = root->first_node();
    // std::cout << "layer1 " << layer1->name() << std::endl;

    // TODO some data file may contains multiple fieldarray
    // do multiple sibling, until find one that is vtkMultiBlockDataSet

    rapidxml::xml_node<>* layer1sib = layer1->next_sibling();
    std::string siblingName = layer1sib->name();
    // std::cout << "layer1sib " << siblingName << std::endl;

    while (siblingName.compare("vtkMultiBlockDataSet") != 0)
    {
      layer1sib = layer1sib->next_sibling();
      siblingName = layer1sib->name();
    }

    rapidxml::xml_node<>* layer2 = layer1sib->first_node();
    // std::cout << "layer2 " << layer2->name() << std::endl;

    // calculate the offset based on the rank
    int offset = startIndex;

    // std::cout << "rank " << rank << " offset is " << offset << std::endl;
    // jump to the offset position
    rapidxml::xml_node<>* tempsib = layer2->first_node();

    for (int i = 0; i < offset; i++)
    {
      tempsib = tempsib->next_sibling();
      // std::cout << "rank " << rank << " offset attribute name "
      //          << tempsib->first_attribute()->value() << std::endl;
    }

    auto dataStageStart = tl::timer::wtime();
    // go through the number of the offset
    // use the async rpc here
    asyncStageResps.clear();
    for (int blockID = startIndex; blockID <= endIndex; blockID++)
    {
      if (blockID != startIndex)
      {
        tempsib = tempsib->next_sibling();
      }
      rapidxml::xml_attribute<>* attr = tempsib->first_attribute("file");
      // std::cout << "rank " << rank << " attribute name " << attr->value() << std::endl;
      std::string vtuFileName = attr->value();

      // load the vtuFile and write the data to data staging service.
      // load the unstructured data from the vtu file

      // Read all the data from the file
      vtkSmartPointer<vtkXMLUnstructuredGridReader> reader =
        vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
      reader->SetFileName(vtuFileName.c_str());
      reader->Update();

      // get the specific unstructureGridData and check the results
      vtkUnstructuredGrid* unstructureGridData = reader->GetOutput();

      // start to transfer the different aspects of the grid data
      // just use the marshal operation here
      // marshal the vtk data into the string
      vtkSmartPointer<vtkCharArray> marshaledBuffer = vtkSmartPointer<vtkCharArray>::New();
      bool oktoMarshal = vtkCommunicator::MarshalDataObject(unstructureGridData, marshaledBuffer);

      if (oktoMarshal == false)
      {
        throw std::runtime_error("failed to marshal vtk vtkCharArray");
      }

      // send the marshaledBuffer to the data staging service
      auto type = Type::UINT8;

      size_t dataSize =
        marshaledBuffer->GetNumberOfTuples() * marshaledBuffer->GetNumberOfComponents();
      std::vector<size_t> dimensions;
      dimensions.push_back(dataSize);
      auto offsets = std::vector<int64_t>(0);

      // dims
      // std::cout << "marshal tuple: " << marshaledBuffer->GetNumberOfTuples()
      //          << " marshal components " << marshaledBuffer->GetNumberOfComponents()
      //          << " marshal size " << dataSize << std::endl;

      stagingClient.stage(dataSetName, step, blockID, dimensions, offsets, type,
        marshaledBuffer->GetPointer(0), rank);

      // auto asyncStageResp = stagingClient.asyncstage(dataSetName, step, blockID, dimensions,
      //  offsets, type, marshaledBuffer->GetPointer(0), rank);
      // we only have one allocated mem buffer here we need to guarantee it finish
      // unless we use multiple buffers, such as the pool that contains multiple buffers
      // could we use the async operation
      // asyncStageResps.push_back(std::move(asyncStageResp));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto dataStageEnd = tl::timer::wtime();
    if (leader)
    {
      spdlog::info("iteration {} data stage time is {} ", step, dataStageEnd - dataStageStart);
    }

    // ok to stage the data, start the execution operation
    executionStart = tl::timer::wtime();
    asyncResponses = stagingClient.execute(step, dataSetName, leader);

    // std::cout << "Node layer3 has attribute " << attr->name() << " ";
    // std::cout << "with value " << attr->value() << "\n";
    // for (int j = startIndex; j <= startIndex; j++)
    //{

    // remove the temp folder
    // if (rank == 0)
    //{
    //  system("rm -r pv_insitu_*");
    //}
    MPI_Barrier(MPI_COMM_WORLD);
  }

  // process the data for the last step
  // wait the execution finish for the last step

  for (int i = 0; i < asyncResponses.size(); i++)
  {
    // wait the execution finish
    int ret = asyncResponses[i].wait();
    if (ret != 0)
    {
      throw std::runtime_error("failed for execution");
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  auto executionEnd = tl::timer::wtime();

  if (leader)
  {
    spdlog::info("iteration {} execution time is {} wait time is {}", step ,
      executionEnd - executionStart, executionEnd - executionStart);
  }


  
  // partition, get the current fileName list according to
  // to the MPI rank and total avalible vtu files

  // range the fileName list and read the vtu file
  // load the unstructured data from the vtu file
  /*
  std::string filename = "pv_insitu_15674_0_0.vtu";

  // Read all the data from the file
  vtkSmartPointer<vtkXMLUnstructuredGridReader> reader =
    vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
  reader->SetFileName(filename.c_str());
  reader->Update();

  // get the specific unstructureGridData and check the results
  vtkUnstructuredGrid* unstructureGridData = reader->GetOutput();
  //unstructureGridData->Print(std::cout);

  // send the data to the staging service
  std::cout << "rank " << rank << "load file " << fileName << std::endl;
  */
  /*
  // then try to reconstruct it
  std::cout << "----reconstructed------" << std::endl;
  // start to reconstruct it based on different component
  vtkNew<vtkUnstructuredGrid> newunstructuredGrid;

  newunstructuredGrid->SetPoints(oldunstructureGridData->GetPoints());

  auto cellArray = oldunstructureGridData->GetCells();
  auto typeArray = oldunstructureGridData->GetCellTypesArray();
  // get cells offset
  // check the offset
  vtkTypeInt64Array* arrayoffset64 = cellArray->GetOffsetsArray64();
  std::cout << "arrayoffset64 size: " << arrayoffset64->GetNumberOfTuples() << std::endl;
  vtkTypeInt64* temp = (vtkTypeInt64*)arrayoffset64->GetVoidPointer(0);
  long int i=0;
  for (i = 0; i < 10; i++)
  {
    std::cout << *(temp + i) << std::endl;
  }
  //check the last value
  i = arrayoffset64->GetNumberOfTuples() - 1;
  std::cout << *(temp + i) << std::endl;
  // get cells conn
  vtkTypeInt64Array* arrayconnectivity64 = cellArray->GetConnectivityArray64();
  std::cout << "arrayconnectivity64 size: " << arrayconnectivity64->GetNumberOfTuples() <<
  std::endl; vtkTypeInt64* temp2 = (vtkTypeInt64*)arrayconnectivity64->GetVoidPointer(0); i=0;
  for (i = 0; i < 10; i++)
  {
    std::cout << *(temp2 + i) << std::endl;
  }
  //check the last value
  i = arrayconnectivity64->GetNumberOfTuples() - 1;
  std::cout << *(temp2 + i) << std::endl;

  // get cells type
  vtkNew<vtkUnsignedCharArray> celltypeArray;
  celltypeArray->SetNumberOfComponents(1);
  int celltypeArraySize = typeArray->GetNumberOfTuples() / sizeof(VTK_TYPE_NAME_UNSIGNED_CHAR);
  celltypeArray->SetArray((VTK_TYPE_NAME_UNSIGNED_CHAR*)typeArray->GetVoidPointer(0),
    static_cast<vtkIdType>(celltypeArraySize), 1);

  // set data
  std::cout << "---cellArray---" << std::endl;
  cellArray->Print(std::cout);

  std::cout << "---celltypeArray---" << std::endl;
  celltypeArray->Print(std::cout);

  newunstructuredGrid->SetCells(celltypeArray, cellArray);
  newunstructuredGrid->Print(std::cout);
  */
  return 0;
}
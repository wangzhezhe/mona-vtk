#include <vtkTypeFloat32Array.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkTypeInt64Array.h>
int main()
{
  // load the unstructured data from the vtu file
  std::string filename = "pv_insitu_15674_0_0.vtu";

  // Read all the data from the file
  vtkSmartPointer<vtkXMLUnstructuredGridReader> reader =
    vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
  reader->SetFileName(filename.c_str());
  reader->Update();

  // get the specific unstructureGridData and check the results
  vtkUnstructuredGrid* oldunstructureGridData = reader->GetOutput();
  oldunstructureGridData->Print(std::cout);
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
  std::cout << "arrayconnectivity64 size: " << arrayconnectivity64->GetNumberOfTuples() << std::endl;
  vtkTypeInt64* temp2 = (vtkTypeInt64*)arrayconnectivity64->GetVoidPointer(0);
  i=0;
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
  return 0;
}
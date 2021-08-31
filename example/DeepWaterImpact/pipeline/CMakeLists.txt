# (C) 2021 The University of Chicago
# See COPYRIGHT in top-level directory.
cmake_minimum_required (VERSION 3.13)
project (pycolza-dwi-pipeline CXX)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (colza REQUIRED)
find_package (spdlog REQUIRED)

add_library (pycolza-dwi-mpi-pipeline SHARED DWIMPIPipeline.cpp DWIInSituMPIAdaptor.cpp)
target_link_libraries (pycolza-dwi-mpi-pipeline 
colza-server spdlog::spdlog MPI::MPI_C
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel 
VTK::IOImage
VTK::IOXML
VTK::IOLegacy
)

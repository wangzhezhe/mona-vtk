# -ltinfo is for solving mesa issues, refer to https://github.com/halide/Halide/issues/1112
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -luuid -ltinfo")

# find colza and related package

find_package (colza REQUIRED)
find_package (thallium REQUIRED)
find_package (PkgConfig REQUIRED)

# search for mona by pkg
pkg_check_modules (MONA REQUIRED IMPORTED_TARGET mona)
# search for ssg by pkg
pkg_check_modules (SSG REQUIRED IMPORTED_TARGET ssg)
# search for TCLAP by pkg
pkg_check_modules (TCLAP REQUIRED IMPORTED_TARGET tclap)

# search for nlohmann/json
find_package (nlohmann_json REQUIRED)

# search for spdlog
find_package(spdlog REQUIRED)

if(NOT PARAVIEW_USE_MPI)
  message(SEND_ERROR "ParaView must be built with MPI enabled")
endif()
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})
find_package (Python3)

#set include dir for mochiController
include_directories(${CMAKE_SOURCE_DIR}/src)

# build the monabackend mbpipeline library
set (gsmonabackend-src-files
     ./pipeline/gsMonaBackend.cpp
     ./pipeline/gsMonaInSituAdaptor.cpp)

# monabackend pipeline library
add_library (gsmonabackend-pipeline ${gsmonabackend-src-files})
target_link_libraries (gsmonabackend-pipeline 
colza-server 
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel 
VTK::IOImage
VTK::IOXML
VTK::IOLegacy
MPI::MPI_C)

#mpibackend
set (gsmpibackend-src-files
     ./pipeline/gsMPIBackend.cpp
     ./pipeline/gsMPIInSituAdaptor.cpp)

add_library (gsmpibackend-pipeline ${gsmpibackend-src-files})
target_link_libraries (gsmpibackend-pipeline 
colza-server 
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel 
VTK::IOImage
VTK::IOXML
VTK::IOLegacy
MPI::MPI_C)

# controller
set (rescalecontroller-src-files
     ./RescaleController/Controller.cpp
     ./RescaleController/Rescaler.cpp)
add_library (rescalecontroller ${rescalecontroller-src-files})
target_link_libraries(rescalecontroller colza-client MPI::MPI_C ${DRC_LIBRARIES})

# client
add_executable (gsclient gsclient.cpp gray-scott.cpp settings.cpp)
target_link_libraries (gsclient rescalecontroller colza-client MPI::MPI_C ${DRC_LIBRARIES})

# server
add_executable (gsserver gsserver.cpp)
target_link_libraries (gsserver colza-server MPI::MPI_C ${DRC_LIBRARIES})

# test
add_executable (testcontroller ./RescaleController/test/testcontroller.cpp)
target_link_libraries (testcontroller rescalecontroller)

# install
install (TARGETS gsserver gsclient DESTINATION bin/example/GrayScottColza/)
install (TARGETS gsmpibackend-pipeline gsmonabackend-pipeline DESTINATION lib)

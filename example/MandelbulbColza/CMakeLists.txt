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

# buld the pipeline library
set (dummy-src-files
     ./pipeline/DummyBackend.cpp
     ./InSituAdaptor.cpp
     )

# dummy pipeline library
add_library (dummy-pipeline ${dummy-src-files})
target_link_libraries (dummy-pipeline 
colza-server 
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel MPI::MPI_C)

# buld the pipeline library without vtk things
#set (dummy-src-files
#     ./pipeline/DummyBackend.cpp)

# dummy pipeline library
#add_library (dummy-pipeline ${dummy-src-files})
#target_link_libraries (dummy-pipeline colza-server)

# client
add_executable (mbclient mbclient.cpp)
target_link_libraries (mbclient colza-client MPI::MPI_C nlohmann_json::nlohmann_json)

# server
add_executable (mbserver mbserver.cpp)
target_link_libraries (mbserver colza-server MPI::MPI_C)

# admin
add_executable (mbadmin ${CMAKE_CURRENT_SOURCE_DIR}/mbadmin.cpp)
target_link_libraries (mbadmin colza-admin)
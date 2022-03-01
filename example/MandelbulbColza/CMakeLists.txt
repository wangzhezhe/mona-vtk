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

# buld the monabackend mbpipeline library
set (monabackend-src-files
     ./pipeline/MonaBackend.cpp
     ./MonaInSituAdaptor.cpp)

# monabackend pipeline library
add_library (monabackend-pipeline ${monabackend-src-files})
target_link_libraries (monabackend-pipeline 
colza-server 
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel MPI::MPI_C)

# buld the mpibackend pipeline library
set (mpibackend-src-files
     ./pipeline/MPIBackend.cpp
     ./MPIInSituAdaptor.cpp)

# mpibackend pipeline library (we link mona things since we use one insitu adaptor file)
# but the mona things is not used in MPI case
add_library (mpibackend-pipeline ${mpibackend-src-files})
target_link_libraries (mpibackend-pipeline 
colza-server 
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel MPI::MPI_C)


# client
add_executable (mbclient mbclient.cpp)
target_link_libraries (mbclient colza-client MPI::MPI_C nlohmann_json::nlohmann_json ${DRC_LIBRARIES})

# server
add_executable (mbserver mbserver.cpp)
target_link_libraries (mbserver colza-server MPI::MPI_C ${DRC_LIBRARIES})

# admin
add_executable (mbadmin ${CMAKE_CURRENT_SOURCE_DIR}/mbadmin.cpp)
target_link_libraries (mbadmin colza-admin ${DRC_LIBRARIES})

#ssg API is updated
#add_executable (testssg ${CMAKE_CURRENT_SOURCE_DIR}/testssg.cpp)
#target_link_libraries (testssg colza-client MPI::MPI_C ${DRC_LIBRARIES})


#install
install (TARGETS mbadmin mbserver mbclient DESTINATION bin/example/MandelbulbColza/)
install (TARGETS mpibackend-pipeline monabackend-pipeline DESTINATION lib)

# this part is for colza related things
find_package (colza REQUIRED)
find_package (nlohmann_json REQUIRED)

######
find_package (thallium REQUIRED)
find_package (PkgConfig REQUIRED)

# search for mona by pkg
pkg_check_modules (MONA REQUIRED IMPORTED_TARGET mona)
# search for TCLAP by pkg
pkg_check_modules (TCLAP REQUIRED IMPORTED_TARGET tclap)
# ssg is used by colza server
pkg_check_modules (SSG REQUIRED IMPORTED_TARGET ssg)


include (xpkg-import)
xpkg_import_module (uuid REQUIRED uuid)


# search for spdlog
find_package(spdlog REQUIRED)

find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})


# set source files
add_executable (testsim 
${CMAKE_CURRENT_SOURCE_DIR}/testsim.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp
)
target_link_libraries (testsim MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium)

# this is the test for elasticity for only mb sim program
add_executable (elasticmb 
${CMAKE_CURRENT_SOURCE_DIR}/elasticmb.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp
)
target_link_libraries (elasticmb MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium)

# the mb sim is elstic and it also includes coza for this case
add_executable (elasticmbcolza
${CMAKE_CURRENT_SOURCE_DIR}/elasticmbcolza.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp
)
target_link_libraries (elasticmbcolza colza-client MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium)


# monabackend pipeline library
# build the pipeline used for the colza
include_directories(${CMAKE_SOURCE_DIR}/src)
set (monabackend-src-files
     ./pipeline/MonaBackend.cpp
     ./pipeline/MonaInSituAdaptor.cpp)
add_library (monabackend-pipeline-leaderworker ${monabackend-src-files})
target_link_libraries (monabackend-pipeline-leaderworker
colza-server 
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel MPI::MPI_C)

# build the colza server
add_executable (mbcolzaserver mbcolzaserver.cpp)
target_link_libraries (mbcolzaserver colza-server MPI::MPI_C ${DRC_LIBRARIES})
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
target_link_libraries (testsim MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)

# this is the test for elasticity for only mb sim program
add_executable (elasticmb 
${CMAKE_CURRENT_SOURCE_DIR}/elasticmb.cpp 
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp
)
target_link_libraries (elasticmb MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)

# the mb sim is elstic and it also includes coza for this case
#add_executable (elasticmbcolza
#${CMAKE_CURRENT_SOURCE_DIR}/elasticmbcolza.cpp 
#${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp
#)
#target_link_libraries (elasticmbcolza colza-client MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)


# monabackend pipeline library
# build the pipeline used for the colza
include_directories(${CMAKE_SOURCE_DIR}/src)
set (monabackend-src-files
     ./pipeline/MonaBackend.cpp
     ./pipeline/MonaInSituAdaptor.cpp
     ./dwaterserver/DWIInSituMonaAdaptor.cpp
     )
add_library (monabackend-pipeline-leaderworker SHARED ${monabackend-src-files})
target_link_libraries (monabackend-pipeline-leaderworker
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst 
VTK::ParallelMPI 
VTK::CommonDataModel
VTK::IOImage
VTK::IOXML
VTK::IOLegacy)



# build the colza server
add_executable (mbcolzaserver mbcolzaserver.cpp)
target_link_libraries (mbcolzaserver colza-server MPI::MPI_C ${DRC_LIBRARIES})

# build the leader worker server for testing
add_executable (teststagingserver ./pipeline/test/teststagingserver.cpp)
target_link_libraries (teststagingserver MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium 
monabackend-pipeline-leaderworker
)

add_executable (testsimsync ./pipeline/test/testsimsync.cpp)
target_link_libraries (testsimsync MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)

add_executable (elasticmbleaderworker 
${CMAKE_CURRENT_SOURCE_DIR}/elasticmbleaderworker.cpp
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp)
target_link_libraries (elasticmbleaderworker MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)

add_executable (elasticmbstage 
${CMAKE_CURRENT_SOURCE_DIR}/elasticmbstage.cpp
${CMAKE_CURRENT_SOURCE_DIR}/ControllerClient.cpp)
target_link_libraries (elasticmbstage MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)

add_executable (testDynamicProcessController ./pipeline/test/testDynamicProcessController.cpp)
add_subdirectory(dwaterclient)
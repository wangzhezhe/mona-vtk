
find_package (thallium REQUIRED)
find_package (PkgConfig REQUIRED)

# search for mona by pkg
pkg_check_modules (MONA REQUIRED IMPORTED_TARGET mona)
# search for TCLAP by pkg
pkg_check_modules (TCLAP REQUIRED IMPORTED_TARGET tclap)

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
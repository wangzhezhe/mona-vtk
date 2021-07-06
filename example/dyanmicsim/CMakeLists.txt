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

find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})


# set source files
add_executable (dynamic-sim ${CMAKE_CURRENT_SOURCE_DIR}/sim.cpp)
target_link_libraries (dynamic-sim colza-server MPI::MPI_C ${DRC_LIBRARIES})
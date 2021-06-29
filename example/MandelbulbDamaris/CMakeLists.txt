cmake_minimum_required(VERSION 3.14)
project(DamarisVTK CXX C)
set(CMAKE_CXX_STANDARD 14)
enable_language(CXX)

# setup cache variables for ccmake
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
         CACHE STRING "Choose the type of build." FORCE)
    set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                  "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif ()
set (CMAKE_PREFIX_PATH "" CACHE STRING "External dependencies path")
set (BUILD_SHARED_LIBS "OFF" CACHE BOOL "Build a shared library")

# link shared lib with full rpath
set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_definitions(-DBOOST_LOG_DYN_LINK)

find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Boost 1.65.0 REQUIRED COMPONENTS thread log log_setup filesystem system date_time)
pkg_check_modules (TCLAP REQUIRED IMPORTED_TARGET tclap)

#find_package(ParaView 5.8 REQUIRED COMPONENTS Catalyst PythonCatalyst)
#find_package(VTK COMPONENTS CommonDataModel ParallelMPI FiltersSources)

# find MPI
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})

# find Damaris and its dependencies
find_package(Damaris REQUIRED)

add_executable(mbclient mbclient.cpp)
target_link_libraries(mbclient spdlog::spdlog PkgConfig::TCLAP MPI::MPI_C
                      damaris Boost::filesystem Boost::log Boost::log_setup
                      Boost::system Boost::date_time Boost::thread)

add_library(mbrender SHARED mbrender.cpp)
target_link_libraries(mbrender spdlog::spdlog PkgConfig::TCLAP MPI::MPI_C
                      damaris Boost::filesystem Boost::log Boost::log_setup
                      Boost::system Boost::date_time Boost::thread)

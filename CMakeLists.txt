cmake_minimum_required(VERSION 3.14)
project(MonaVTK CXX C)
set(CMAKE_CXX_STANDARD 14)
enable_language(CXX)
option(ENABLE_EXAMPLE "Enable ENABLE_EXAMPLE" ON)
option(ENABLE_TEST "Enable ENABLE_TEST" ON)
# add our cmake module directory to the path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

#use the debug build when it is necessary
#SET(CMAKE_CXX_FLAGS  "-DDEBUG_BUILD")

option(USE_GNI "use cray gni" ON)

if(USE_GNI)
  add_definitions(-DUSE_GNI)
  find_package(DRC REQUIRED)
  include_directories(${DRC_INCLUDE_DIRS})
  message ("use cray DRC")
  message ("include drc" ${DRC_INCLUDE_DIRS})
  message ("link drc lib" ${DRC_LIBRARIES})
endif()

find_package(spdlog REQUIRED)

if(ENABLE_EXAMPLE)
# there is issue to find target Python3::Pyhon if we put this find_package command into the example folder
find_package(ParaView 5.8 REQUIRED COMPONENTS Catalyst PythonCatalyst)
endif()

# find VTK
find_package(VTK COMPONENTS
CommonDataModel
ParallelMPI
FiltersSources
)

# find MPI
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})

#find MPE when it is necessary
#find_package(MPE REQUIRED)
#include_directories(${MPE_LIBRARIES})


# find Mona packages
find_package (mona REQUIRED)
find_package (PkgConfig REQUIRED)
find_package (mercury REQUIRED)
pkg_check_modules (ARGOBOTS REQUIRED IMPORTED_TARGET argobots)


add_subdirectory(src)

if(ENABLE_EXAMPLE)
  add_subdirectory(example)
endif()

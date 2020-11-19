# -ltinfo is for solving mesa issues, refer to https://github.com/halide/Halide/issues/1112
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -luuid -ltinfo")


if(NOT PARAVIEW_USE_MPI)
  message(SEND_ERROR "ParaView must be built with MPI enabled")
endif()
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})
find_package (Python3)

#set include dir for mochiController
include_directories(${CMAKE_SOURCE_DIR}/src)

set(Adaptor_SRCS
    ${CMAKE_SOURCE_DIR}/example/MandelbulbCatalystExample/InSituAdaptor.cpp
    )
add_library(CatalystMandelbulbAdaptor ${Adaptor_SRCS})
target_link_libraries(CatalystMandelbulbAdaptor 
mona-vtk-icet
mona-vtk
ParaView::icet ParaView::RemotingViews ParaView::PythonCatalyst VTK::ParallelMPI VTK::CommonDataModel)

add_executable(MandelbulbDynamic 
     ${CMAKE_SOURCE_DIR}/example/MandelbulbCatalystExample/Mandelbulb_dynamic.cpp)
target_link_libraries(MandelbulbDynamic LINK_PRIVATE CatalystMandelbulbAdaptor)


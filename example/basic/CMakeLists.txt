include_directories(${CMAKE_SOURCE_DIR}/src)

find_package(VTK COMPONENTS
IOLegacy
CommonDataModel
FiltersCore
IOXML
)

add_executable(test_vtk_send_recv test_vtk_send_recv.cpp)
target_link_libraries(test_vtk_send_recv MPI::MPI_C ${VTK_LIBRARIES} mona-vtk)


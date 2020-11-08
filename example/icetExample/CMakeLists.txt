include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable(icet_simple icet_simple.cpp)
target_link_libraries(icet_simple 
MPI::MPI_C
${ICET_CORE_LIBS}
${ICET_GL_LIBS}
${ICET_MPI_LIBS}
mona-vtk-icet
mona-vtk
)
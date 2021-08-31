add_executable(testunstructure testunstructure.cpp)
target_link_libraries(testunstructure VTK::IOXML VTK::IOLegacy)
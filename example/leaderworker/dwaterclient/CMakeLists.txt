add_executable(vtureader vtureader.cpp)
target_link_libraries(vtureader VTK::IOXML VTK::IOLegacy VTK::ParallelCore
MPI::MPI_C ${DRC_LIBRARIES} PkgConfig::MONA thallium nlohmann_json::nlohmann_json)
find_package(Boost 1.65.0 REQUIRED COMPONENTS thread log log_setup filesystem system date_time)

# find Damaris and its dependencies
find_package(Damaris REQUIRED)

# search for TCLAP by pkg
pkg_check_modules (TCLAP REQUIRED IMPORTED_TARGET tclap)

# search for spdlog
find_package(spdlog REQUIRED)

# search for MPI
find_package(MPI REQUIRED)

add_executable(mbclient_damaris mbclient.cpp)
target_link_libraries(mbclient_damaris spdlog::spdlog PkgConfig::TCLAP MPI::MPI_C
                      damaris Boost::filesystem Boost::log Boost::log_setup
                      Boost::system Boost::date_time Boost::thread)

add_library(mbrender_damaris SHARED mbrender.cpp)
target_link_libraries(mbrender_damaris spdlog::spdlog PkgConfig::TCLAP MPI::MPI_C
                      damaris Boost::filesystem Boost::log Boost::log_setup
                      Boost::system Boost::date_time Boost::thread)

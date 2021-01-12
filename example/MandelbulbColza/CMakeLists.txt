# -ltinfo is for solving mesa issues, refer to https://github.com/halide/Halide/issues/1112
# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -luuid -ltinfo")

# find colza and related package

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

# buld the pipeline library
set (dummy-src-files
     ./pipeline/DummyBackend.cpp)

# dummy pipeline library
add_library (dummy-pipeline ${dummy-src-files})
target_link_libraries (dummy-pipeline colza-server)

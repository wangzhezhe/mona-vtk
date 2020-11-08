# list of source files
set(mona-vtk-src MonaCommunicator.cpp
                  MonaController.cpp
                  MonaUtilities.cpp)

set(mona-icet-src icet/mona.cpp)

# load package helper for generating cmake CONFIG packages
include (CMakePackageConfigHelpers)

# where to install files for "find_package"
set (mona-vtk-pkg "share/cmake/mona-vtk")

#
# library version set here (e.g. for shared libs).
#
set (MOCHIVTK_VERSION_MAJOR 0)
set (MOCHIVTK_VERSION_MINOR 1)
set (MOCHIVTK_VERSION_PATCH 0)
set (mona-vtk-vers "${MOCHIVTK_VERSION_MAJOR}.${MOCHIVTK_VERSION_MINOR}")
set (MOCHIVTK_VERSION "${remi-vers}.${MOCHIVTK_VERSION_PATCH}")

add_library(mona-vtk ${mona-vtk-src})
target_link_libraries (mona-vtk mona
	${VTK_LIBRARIES})
target_include_directories (mona-vtk PUBLIC $<INSTALL_INTERFACE:include>)

# local include's BEFORE, in case old incompatable .h files in prefix/include
target_include_directories (mona-vtk BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

# for shared libs, establish the lib version
set_target_properties (mona-vtk
    PROPERTIES VERSION ${MOCHIVTK_VERSION}
    SOVERSION ${MOCHIVTK_VERSION_MAJOR})

add_library(mona-vtk-icet ${mona-icet-src})
# use the icet that is build by the paraview
target_link_libraries (mona-vtk-icet mona ParaView::icet)

# for shared libs, establish the lib version
#set_target_properties (mona-vtk-icet
#    PROPERTIES VERSION ${MOCHIVTK_VERSION}
#    SOVERSION ${MOCHIVTK_VERSION_MAJOR})
#
# installation stuff (packaging and install commands)
#
write_basic_package_version_file(
    "mona-vtk-config-version.cmake"
    VERSION ${MOCHIVTK_VERSION}
    COMPATIBILITY AnyNewerVersion)

# generate our config file for find_package()
configure_file (mona-vtk-config.cmake.in mona-vtk-config.cmake @ONLY)

# some bits for the pkg-config file
set (DEST_DIR "${CMAKE_INSTALL_PREFIX}")
set (PRIVATE_LIBS "-lmona-vtk")
configure_file ("mona-vtk.pc.in" "mona-vtk.pc" @ONLY)

#
# "make install" rules
#
install (TARGETS mona-vtk EXPORT mona-vtk-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
install (EXPORT mona-vtk-targets
         DESTINATION ${mona-vtk-pkg}
         FILE "mona-vtk-targets.cmake")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/mona-vtk-config.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/mona-vtk-config-version.cmake"
               "../cmake/xpkg-import.cmake"
         DESTINATION ${mona-vtk-pkg} )
install (DIRECTORY ../include/mona-vtk
         DESTINATION include
         FILES_MATCHING PATTERN "*.hpp")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/mona-vtk.pc"
         DESTINATION "lib/pkgconfig/")



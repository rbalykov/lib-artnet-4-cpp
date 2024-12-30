# Source files for our artnet lib
set(ARTNET_SRC
    ArtNetController.cpp
    network_interface_bsd.cpp
    network_interface_linux.cpp
    utils.cpp
)

# Header files for our artnet lib
set(ARTNET_HDR
   ArtNetController.h
   artnet_types.h
   network_interface_bsd.h
   network_interface_linux.h
)

# Define an artnet library
add_library(artnet
    ${ARTNET_SRC}
    ${ARTNET_HDR}
)

# Set library properties
# set_target_properties(artnet PROPERTIES
#     VERSION ${PROJECT_VERSION}
#     SOVERSION ${PROJECT_VERSION_MAJOR}
#     PUBLIC_HEADER "include/artnet/artnet.hpp;include/artnet/controller.hpp;include/artnet/types.hpp;include/artnet/utils.hpp"
# )

# Set include directories
# target_include_directories(artnet
#     PUBLIC
#         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#         $<INSTALL_INTERFACE:include>
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/src
# )

# Set include for our artnet lib
target_include_directories(artnet PUBLIC .)

# Install targets
# include(GNUInstallDirs)
# install(TARGETS artnet
#     EXPORT artnet-targets
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/artnet
#     INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
# )


# Generate and install config files
# include(CMakePackageConfigHelpers)
# write_basic_package_version_file(
#     "artnet-config-version.cmake"
#     VERSION ${PROJECT_VERSION}
#     COMPATIBILITY SameMajorVersion
# )
#
# configure_package_config_file(
#     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/artnet-config.cmake.in"
#     "${CMAKE_CURRENT_BINARY_DIR}/artnet-config.cmake"
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/artnet
# )
#
# install(EXPORT artnet-targets
#     FILE artnet-targets.cmake
#     NAMESPACE Artnet::
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/artnet
# )
#
# install(FILES
#     "${CMAKE_CURRENT_BINARY_DIR}/artnet-config.cmake"
#     "${CMAKE_CURRENT_BINARY_DIR}/artnet-config-version.cmake"
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/artnet
# )

# Build examples if enabled
# if(ARTNET_BUILD_EXAMPLES)
#     add_subdirectory(examples)
# endif()

# Build tests if enabled
# if(ARTNET_BUILD_TESTS)
#     add_subdirectory(test)
# endif()

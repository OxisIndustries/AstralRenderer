# Installation configuration

include(GNUInstallDirs)

# Install the main library
install(TARGETS astral_renderer
    EXPORT AstralRendererTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/astral
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the example application if built
if(ASTRAL_BUILD_EXAMPLES)
    install(TARGETS AstralSandbox
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Install assets for the example
    if(ASTRAL_INSTALL_ASSETS)
        install(DIRECTORY assets
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

# Export configuration
install(EXPORT AstralRendererTargets
    FILE AstralRendererTargets.cmake
    NAMESPACE Astral::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralRenderer
)

# Create and install version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstralRendererConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AstralRendererConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralRenderer
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstralRendererConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstralRendererConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstralRenderer
)

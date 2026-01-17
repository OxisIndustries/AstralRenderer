#===============================================================================
# Astral Renderer - Main CMake Configuration
#===============================================================================
# Version: 0.1.0
#===============================================================================

cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

# Project definition
project(
    Astral_Renderer
    VERSION 0.1.0
    DESCRIPTION "Modern Vulkan-based physically based rendering engine"
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/inkbytefo/AstralRenderer"
)

# C++ Standard and Compiler Configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler.cmake)

# Build options
option(ASTRAL_BUILD_TESTS "Build test suite" OFF)
option(ASTRAL_BUILD_EXAMPLES "Build example applications" ON)
option(ASTRAL_STRICT_WARNINGS "Treat compiler warnings as errors" OFF)
option(ASTRAL_INSTALL_ASSETS "Install assets with library" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Install prefix defaults
include(GNUInstallDirs)

#===============================================================================
# Dependencies
#===============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake)

#===============================================================================
# Astral Renderer Library
#===============================================================================

# ImGui source files
set(ASTRAL_IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

# Core sources
set(ASTRAL_CORE_SOURCES
    src/core/context.cpp
    src/core/config.cpp
    src/core/vma_implementation.cpp
    src/core/commands.cpp
    src/application.cpp
)

# Platform sources
set(ASTRAL_PLATFORM_SOURCES
    src/platform/window.cpp
)

# Renderer sources
set(ASTRAL_RENDERER_SOURCES
    src/renderer/swapchain.cpp
    src/renderer/descriptor_manager.cpp
    src/renderer/pipeline.cpp
    src/renderer/render_graph.cpp
    src/renderer/sync.cpp
    src/renderer/scene_manager.cpp
    src/renderer/model.cpp
    src/renderer/gltf_loader.cpp
    src/renderer/camera.cpp
    src/renderer/compute_pipeline.cpp
    src/renderer/environment_manager.cpp
    src/renderer/ui_manager.cpp
    src/renderer/asset_manager.cpp
    src/renderer/assimp_loader.cpp
    src/renderer/renderer_system.cpp
)

# Resources sources
set(ASTRAL_RESOURCES_SOURCES
    src/resources/buffer.cpp
    src/resources/image.cpp
    src/resources/shader.cpp
    src/resources/sampler.cpp
    src/resources/stb_image_impl.cpp
)

# All sources
set(ASTRAL_SOURCES
    ${ASTRAL_IMGUI_SOURCES}
    ${ASTRAL_CORE_SOURCES}
    ${ASTRAL_PLATFORM_SOURCES}
    ${ASTRAL_RENDERER_SOURCES}
    ${ASTRAL_RESOURCES_SOURCES}
)

# Public headers
set(ASTRAL_PUBLIC_HEADERS
    include/astral/astral.hpp
    include/astral/core/context.hpp
    include/astral/core/commands.hpp
    include/astral/application.hpp
    include/astral/platform/window.hpp
    include/astral/renderer/swapchain.hpp
    include/astral/renderer/descriptor_manager.hpp
    include/astral/renderer/pipeline.hpp
    include/astral/renderer/render_graph.hpp
    include/astral/renderer/sync.hpp
    include/astral/renderer/scene_data.hpp
    include/astral/renderer/scene_manager.hpp
    include/astral/renderer/model.hpp
    include/astral/renderer/gltf_loader.hpp
    include/astral/renderer/camera.hpp
    include/astral/renderer/compute_pipeline.hpp
    include/astral/renderer/environment_manager.hpp
    include/astral/renderer/ui_manager.hpp
    include/astral/renderer/renderer_system.hpp
    include/astral/resources/buffer.hpp
    include/astral/resources/image.hpp
    include/astral/resources/sampler.hpp
    include/astral/resources/shader.hpp
)

# Create static library
add_library(astral_renderer STATIC ${ASTRAL_SOURCES} ${ASTRAL_PUBLIC_HEADERS})

# Add precompiled headers
astral_add_pch(astral_renderer)

# Public include directories
target_include_directories(astral_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/imgui>
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
        $<INSTALL_INTERFACE:include/imgui/backends>
        $<BUILD_INTERFACE:${stb_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/stb>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(astral_renderer
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
        spdlog::spdlog
        fastgltf::fastgltf
        nlohmann_json::nlohmann_json
        VulkanMemoryAllocator
        assimp::assimp
    PRIVATE
        ${ASTRAL_SHADERC_TARGET}
)

# Set output name
set_target_properties(astral_renderer PROPERTIES
    OUTPUT_NAME "astral_renderer"
    DEBUG_POSTFIX "d"
    RELEASE_POSTFIX ""
)

#===============================================================================
# Example Application
#===============================================================================
if(ASTRAL_BUILD_EXAMPLES)
    # glTF Viewer Example
    add_executable(AstralGltf examples/gltf_viewer.cpp)
    target_link_libraries(AstralGltf PRIVATE astral_renderer)
    
    add_custom_command(
        TARGET AstralGltf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:AstralGltf>/assets
        COMMENT "Copying assets for AstralGltf"
    )

    # FBX Viewer Example
    add_executable(AstralFbx examples/fbx_viewer.cpp)
    target_link_libraries(AstralFbx PRIVATE astral_renderer)

    add_custom_command(
        TARGET AstralFbx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:AstralFbx>/assets
        COMMENT "Copying assets for AstralFbx"
    )
endif()

#===============================================================================
# Tests
#===============================================================================
if(ASTRAL_BUILD_TESTS)
    add_subdirectory(tests)
endif()

#===============================================================================
# Installation
#===============================================================================
# include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install-config.cmake)

# Print configuration summary
message(STATUS "")
message(STATUS "Astral Renderer Configuration")
message(STATUS "================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Examples:       ${ASTRAL_BUILD_EXAMPLES}")
message(STATUS "  Tests:          ${ASTRAL_BUILD_TESTS}")
message(STATUS "  Strict Warnings: ${ASTRAL_STRICT_WARNINGS}")
message(STATUS "  Install Assets: ${ASTRAL_INSTALL_ASSETS}")
message(STATUS "================================")
message(STATUS "")

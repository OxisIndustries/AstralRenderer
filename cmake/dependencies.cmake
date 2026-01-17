include(FetchContent)

#===============================================================================
# Vulkan (System Install)
#===============================================================================
find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan: ${Vulkan_VERSION}")

# Attempt to find Shaderc from Vulkan SDK
find_library(SHADERC_LIB shaderc_combined 
    HINTS 
        $ENV{VULKAN_SDK}/lib 
        $ENV{VULKAN_SDK}/Lib
)
find_path(SHADERC_INCLUDE_DIR shaderc/shaderc.hpp 
    HINTS 
        $ENV{VULKAN_SDK}/include
)

if(SHADERC_LIB AND SHADERC_INCLUDE_DIR)
    add_library(shaderc_system STATIC IMPORTED)
    set_target_properties(shaderc_system PROPERTIES
        IMPORTED_LOCATION "${SHADERC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${SHADERC_INCLUDE_DIR}"
    )
    set(ASTRAL_SHADERC_TARGET shaderc_system)
    message(STATUS "Found Shaderc in Vulkan SDK: ${SHADERC_LIB}")
else()
    message(STATUS "Shaderc not found in Vulkan SDK, fetching from source...")
    FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG        v2023.7
    )
    FetchContent_MakeAvailable(shaderc)
    set(ASTRAL_SHADERC_TARGET shaderc)
endif()

#===============================================================================
# GLFW
#===============================================================================
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
FetchContent_MakeAvailable(glfw)

#===============================================================================
# GLM
#===============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

#===============================================================================
# Spdlog
#===============================================================================
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

#===============================================================================
# VulkanMemoryAllocator
#===============================================================================
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.0.1
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

#===============================================================================
# FastGLTF
#===============================================================================
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.8.0
)
FetchContent_MakeAvailable(fastgltf)

#===============================================================================
# ImGui
#===============================================================================
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)
# Note: imgui is not a CMake project, so we only need the source dir, which is handled 
# by FetchContent_MakeAvailable populating imgui_SOURCE_DIR. 
# We don't link to 'imgui' target because it doesn't define one by default.
# The main CMakeLists.txt handles compilation of imgui sources.

#===============================================================================
# stb
#===============================================================================
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# Compiler Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Function to enable precompiled headers
function(astral_add_pch target_name)
    target_precompile_headers(${target_name} PRIVATE
        <vector>
        <string>
        <memory>
        <functional>
        <algorithm>
        <iostream>
        <filesystem>
        <vulkan/vulkan.h>
        <glm/glm.hpp>
    )
endfunction()

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    if(ASTRAL_STRICT_WARNINGS)
        add_compile_options(/WX)
    endif()
    # Enable multi-processor compilation
    add_compile_options(/MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ASTRAL_STRICT_WARNINGS)
        add_compile_options(-Werror)
    endif()
endif()

# Astral Renderer

**Astral Renderer** is a modern, high-performance Vulkan-based physically based rendering (PBR) engine designed for visual fidelity and modularity.

## Features

- **Physically Based Rendering (PBR)**: Realistic lighting and material system.
- **Clustered Forward Rendering**: Efficient light handling for complex scenes.
- **Advanced Post-Processing**:
  - Bloom
  - Screen Space Ambient Occlusion (SSAO)
  - Temporal Anti-Aliasing (TAA)
  - Fast Approximate Anti-Aliasing (FXAA)
- **Dynamic Shadows**: Cascaded Shadow Maps (CSM).
- **GPU-Driven Culling**: Compute shader-based frustum and occlusion culling.
- **Render Graph Architecture**: Flexible and extensible frame graph system.

## Build Instructions

### Prerequisites
- **CMake** (3.20+)
- **Vulkan SDK**
- **C++20 Compliant Compiler** (MSVC 19.30+, GCC 11+, Clang 13+)

### Building
```bash
# Clone the repository
git clone https://github.com/inkbytefo/AstralRenderer.git
cd AstralRenderer

# Configure
cmake -S . -B build

# Build
cmake --build build --config Release
```

### Running
```bash
# Run the sandbox application
./build/bin/Release/AstralSandbox.exe
```

## Structure
- `src/core`: Low-level Vulkan wrappers and context management.
- `src/renderer`: High-level rendering logic (`RendererSystem`, `RenderGraph`).
- `src/platform`: OS-specific window and input handling.
- `src/resources`: Resource management (Buffers, Images, Shaders).

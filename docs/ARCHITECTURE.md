# System Architecture

Astral Renderer adopts a modular architecture separating application lifecycle from rendering logic.

## Core Components

### 1. Application (`astral::Application`)
The root class responsible for the application lifecycle:
- **Window Management**: Wraps GLFW for window creation and event polling.
- **Input Handling**: Processes keyboard and mouse input.
- **Main Loop**: Orchestrates the frame cycle (Poll Events -> Update -> Render).
- **Ownership**: Owns the `Window`, `Context`, `RendererSystem`, and `SceneManager`.

### 2. Renderer System (`astral::RendererSystem`)
The heart of the rendering engine:
- **Pipeline Management**: Initializes and stores all Graphics and Compute pipelines.
- **Resource Management**: Manages frame-specific resources (G-Buffer images, Shadow maps, Post-process buffers).
- **Frame Composition**: Implements the `render()` method which constructs the **Render Graph** for the current frame.

### 3. Render Graph (`astral::RenderGraph`)
A DAG (Directed Acyclic Graph) based frame scheduler:
- **Pass Declaration**: Passes are defined with inputs (dependencies) and outputs.
- **Automatic Synchronization**: Handles Vulkan barriers and layout transitions automatically based on the graph structure.
- **Execution**: LINEARLY executes the compiled graph (currently immediate mode execution).

### 4. Scene Management (`astral::SceneManager`)
Handles scene data:
- **Geometry**: Loads and stores GLTF models.
- **GPU Buffers**: Manages massive vertex/index buffers and distinct instance buffers.
- **Culling**: interfacing with compute shaders to perform GPU-side culling before rendering.

## Data Flow
1. **Init**: `Application` initializes `Context` (Vulkan Instance/Device) and `RendererSystem`.
2. **Update**: `Application` updates camera matrices and `SceneManager` updates animations/transforms.
3. **Render**: `Application` calls `RendererSystem::render()`.
4. **Graph Build**: `RendererSystem` defines passes (Shadows -> PBR -> PostProc) in the `RenderGraph`.
5. **Execute**: `RenderGraph` records commands to the primary `CommandBuffer` and submits to the GPU.

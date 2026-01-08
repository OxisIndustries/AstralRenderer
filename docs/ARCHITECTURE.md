# System Architecture

Astral Renderer adopts a modular architecture separating application lifecycle from rendering logic.

## Core Components

### 1. Application (`astral::Application`)
The root class responsible for the application lifecycle:
- **Window Management**: Wraps GLFW for window creation and event polling.
- **Input Handling**: Processes keyboard and mouse input, including callback chaining for ImGui.
- **Main Loop**: Orchestrates the frame cycle (Poll Events -> UI Update -> Logic -> Render).
- **Ownership**: Owns the `Window`, `Context`, `RendererSystem`, `SceneManager`, and `UIManager`.

### 2. UI Manager (`astral::UIManager`)
Integrates Dear ImGui into the Vulkan pipeline:
- **Backend Management**: Handles GLFW and Vulkan backend life-cycles.
- **Rendering**: Records ImGui draw commands into a separate render pass targeting the swapchain.
- **Custom Bindings**: Provides a high-level API for frame beginning/ending within the engine.

### 3. Renderer System (`astral::RendererSystem`)
The engine's heart, managing the frame construction:
- **Pipeline Management**: Initializes Graphics/Compute pipelines and layouts.
- **Render Graph Construction**: Builds the frame structure based on `UIParams`.
- **Resource Management**: Owns transient HDR buffers, shadow maps, and SSAO targets.

### 4. Scene Management (`astral::SceneManager`)
Handles scene state and GPU synchronization:
- **Resource Tracking**: Manages buffers for `SceneData`, `Light`, and `MaterialMetadata`.
- **Dynamic Updates**: Provides methods to update lights and materials during runtime with automatic GPU re-uploading.
- **Culling Support**: Feeds instance and transformation data to GPU culling passes.

## Data Flow
1. **Init**: `Application` initializes Vulkan `Context`, `Managers`, and `RendererSystem`.
2. **Input**: `Application` processes user input and updates `UIParams`.
3. **Update**: `Application` prepares camera, syncs `SceneManager` buffers, and handles frame-to-frame logic.
4. **Render Graph Build**: `RendererSystem` defines the pass structure (Shadows -> Clustered Shading -> PostProc).
5. **Execution**: `RenderGraph` handles barriers and command recording. `UIManager` injects the final UI overlay.
6. **Submission**: `Application` submits command buffers and presents the swapchain image.

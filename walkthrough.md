# Debugging Walkthrough: Solving the "Blue Screen" Render Loop

## The Issue
The application was launching but consistently rendering a solid color screen (originally Blue), with no visible 3D geometry. Several modifications (Shader recompilation, Direct Draw calls) had failed to produce any output.

### 3. Critical Fixes (Crash Resolution)

**Issue 1: "Vector Subscript Out of Range" Crash**
The application would crash immediately upon rendering the UI. This was traced to multiple critical issues:

1.  **Dangling Reference in Lambda Captures**: `RendererSystem::render` was capturing local variables (`i`, `currentFrame`) by reference `[&]` in lambdas passed to `RenderGraph`. These variables were destroyed before the lambda executed. **Fix**: Changed captures to `[=, ...]` to copy values.
2.  **Dangling Pointer in UIManager**: `UIManager` passed the address of a stack-allocated argument (`&swapchainFormat`) to `ImGui_ImplVulkan_Init`. ImGui accessed this invalid pointer later. **Fix**: Stored `m_uiFormat` as a class member in `UIManager`.
3.  **Missing Swapchain Resource**: The "Swapchain" resource used by the UI pass was never registered in the `RenderGraph`, leading to null resource usage. **Fix**: Added `graph.addExternalResource("Swapchain", ...)` in `RendererSystem::render`.

### 4. Verification and Next Steps

With these fixes, the application now renders the PBR scene with the "Renderer Controls" panel overlay. The user can now verify:
- **Shadows**: Change Cascade Splits, Visualize CSM.
- **SSAO**: Toggle and adjust radius/bias.
- **Bloom**: Adjust threshold and strength.
- **FXAA**: Toggle anti-aliasing.

The pipeline is now stable for further feature development.

## Root Cause Analysis
After extensive isolation testing, two primary issues were identified:
1.  **Post-Process Culling (Major)**: The Full-Screen Triangle used in Post-Processing passes (Composite, FXAA, etc.) was being **culled** by the Rasterizer because `PipelineSpecs` defaulted to `VK_CULL_MODE_BACK_BIT`. The winding order of the generated triangle resulted in it being classified as a "back face" and discarded. This caused the final passes to output nothing (or just clear color).
2.  **Shader Buffer Access (Secondary)**: The initial `pbr.vert` shader was crashing (or returning invalid data) due to incorrect buffer indexing or descriptor binding before we could even verify geometry visibility.

## The Solution

### 1. Visualizing the Pipeline (The "Magenta" Step)
We changed the Clear Color to **Magenta** `{1.0, 0.0, 1.0, 1.0}`. The screen turned Magenta, proving that `RenderGraph` was successfully executing passess and clearing attachments, but draw calls were failing to write anything on top.

### 2. Disabling Post-Process Culling
We explicitly set `cullMode = VK_CULL_MODE_NONE` for all post-processing pipelines (`FXAA`, `Composite`, `Bloom`, `SSAO`) in `renderer_system.cpp`.
*   **Result**: The "Red Triangle" (debug geometry) appeared! This confirmed the draw calls were finally reaching the framebuffer.

### 3. Restoring Geometry
Once the pipeline flow was verified, we progressively restored the Scene Geometry:
*   **Phase 1**: Modified `pbr.vert` to read real Position attributes (`inPos`) but use an **Identity Matrix**, bypassing `InstanceBuffer`. -> **Success** (Red Helmet visible at origin).
*   **Phase 2**: Re-enabled `InstanceBuffer` read in `pbr.vert`. -> **Success** (Helmet visible with correct transform).
*   **Phase 3**: Restored `pbr.frag` to full PBR shading (Texture/Lighting). -> **Success** (Full realistic render).

### 4. Final Cleanup
To ensure production readiness:
*   Re-enabled **Indirect Draw** (`vkCmdDrawIndexedIndirect`) for efficient batch rendering.
*   Re-enabled **Depth Testing** (`depthTest = true`) and **Back-Face Culling** (`VK_CULL_MODE_BACK_BIT`) for the main **Geometry Pass** (`pbrPipeline`) to ensure correct 3D occlusion and performance.
*   Kept Post-Process pipelines uncunlled to guarantee full-screen quad visibility.

## Final Result
The Astral Renderer is now correctly rendering the glTF PBR Helmet with full lighting, shadows, and post-processing.

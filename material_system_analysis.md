# Material & Texture System Analysis

## 1. Current State Overview

The `AstralRenderer` codebase has made significant progress on the "Material Refactor Roadmap". The foundation for a modern, bindless rendering system is largely in place.

### ✅ Architecture Highlights
-   **Bindless Textures**: The system uses a bindless approach where textures are registered with a `DescriptorManager` and referenced by integer indices in the `MaterialGPU` struct. This is a best-practice approach for Vulkan 1.2+.
-   **Unified Material Buffer**: All material data is stored in a single large SSBO (`m_materialBuffer`), allowing shaders to access any material by index.
-   **Automatic Mipmaps**: `Image` class correctly handles generating mipmaps using `vkCmdBlitImage`.
-   **Texture Caching**: `AssetManager` implements robust caching to prevent duplicate texture loads, falling back to default textures (magenta/flat normal) when files are missing.
-   **Transparency Handling**: `SceneManager` implements CPU-side sorting (Opaque: Front-to-Back, Transparent: Back-to-Front) and uploads sorted instances to the GPU. This is the correct standard approach for basic transparency.

## 2. Roadmap Status Check

Based on `material_refactor_roadmap.md` and code analysis:

| Milestone | Goal | Status | Notes |
| :--- | :--- | :--- | :--- |
| **M1: Mipmaps** | Automatic Generation | ✅ **Complete** | Implemented in `Image::generateMipmaps`. |
| **M2: Caching** | Texture Deduplication | ✅ **Complete** | Implemented in `AssetManager`. |
| **M3: Mat Struct** | GPU-friendly Struct | 🟡 **Partial** | `MaterialGPU` exists and uses indices, but lacks advanced slots (Transmission, Sheen, etc.). |
| **M4: Transparency** | Sorting + Blending | ✅ **Complete** | Sorting in `SceneManager`, Pipeline setup in `RendererSystem`. |
| **M5: Adv. PBR** | Emissive/Transmission | ❌ **Pending** | `emissiveStrength` exists but Transmission/Volume paths are missing. |

## 3. Code Analysis & Observations

### 🔍 Material System (`material.hpp`)
The `MaterialGPU` struct is well-aligned for std430 layouts (80 bytes).
```cpp
struct MaterialGPU {
    // Basic PBR (48 bytes)
    vk::vec4 baseColorFactor;
    vk::vec4 emissiveFactor;
    float metallic; float roughness; float alphaCutoff; uint alphaMode;
    
    // Texture Indices (20 bytes)
    int32_t baseColorIdx; ...
    
    // Flags (12 bytes including padding)
    uint32_t doubleSided;
    uint32_t padding[2];
};
```
**Observation**: It lacks fields for "Phase 3" features described in the roadmap (Transmission, Clearcoat). Modifying this struct later will require updating all shaders, so it is better to reserve space or add these fields now even if unused.

### 🔍 Loader & Assets (`gltf_loader.cpp`, `asset_manager.cpp`)
-   **Good**: Fallback textures are handled gracefully.
-   **Good**: The loader correctly maps glTF alpha modes to the engine's `AlphaMode`.
-   **Concern**: `stbi_load` is blocking. For a production renderer, texture loading should ideally be asynchronous to avoid stalling the main thread, especially given the "AA-quality" goal.
-   **Concern**: All textures are decompressed to raw RGBA8. There is no support for block-compressed formats (BC7/ASTC) via `.dds` or `.ktx2`. This will limit memory performance significantly as scene complexity grows.

### 🔍 Rendering (`renderer_system.cpp`)
-   **Transparency**: The engine uses a separate `m_pbrTransparentPipeline` with `depthWrite = false`. This is correct for sorted transparency.
-   **Missing**: There is no logical distinction between "Material Templates" and "Instances" yet. The roadmap mentioned this (2.2), but currently, everything renders with the same "Uber Shader" pipeline (`m_pbrPipeline`). If you plan to support different lighting models (e.g., Unlit, Cloth, Anisotropic), the current single-pipeline approach will become a bottleneck.

## 4. Recommendations

### Short Term (Refactor & Cleanup)
1.  **Expand `MaterialGPU`**: Add the missing PBR fields now to avoid breaking binary compatibility later.
    -   Add `transmissionFactor` (float), `ior` (float), `thicknessFactor` (float).
    -   Add `transmissionTextureIndex`, `thicknessTextureIndex`.
2.  **Verify Bindless Limits**: Ensure `DescriptorManager` is configured to handle the `max_textures` limit (usually 16k-32k on modern GPUs) effectively.

### Mid Term (Feature Implementation)
3.  **Implement Transmission**:
    -   **Why**: Required for glass/water.
    -   **How**: This requires a copy of the opaque scene color (Opaque Pass -> Copy to "BackgroundTexture" -> Transparent Pass reads "BackgroundTexture").
4.  **Material Pipelines (Templates)**:
    -   **Why**: Not all objects need the heavy PBR shader.
    -   **How**: Refactor `SceneManager` to group instances not just by Opaque/Transp but by `PipelineID`.

### Long Term (Optimization)
5.  **Texture Compression (Direct)**: Integrate `ktx` or `dds` loading to upload compressed blocks handling directly to GPU, saving 4x-8x texture memory.
6.  **Async Loading**: Move `stbi_load` to a thread pool.

## 5. Proposed Next Step
I recommend we focus on **completing M3 and starting M5 (Advanced PBR)**.

**Action Plan:**
1.  Update `MaterialGPU` struct in C++ and Shaders to include Transmission/IOR properties.
2.  Update `GltfLoader` to parse `KHR_materials_transmission` and `KHR_materials_volume`.
3.  Refactor `RendererSystem` to support reading the opaque scene color in the transparent pass (essential for glass).

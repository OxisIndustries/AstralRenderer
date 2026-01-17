# Material & Texture System Refactor Roadmap

This document outlines the strategic plan to modernize the `AstralRenderer` material and texture systems. The goal is to achieve AA-quality rendering capabilities, high performance through effective caching/batching, and a robust architecture extensible for future needs (e.g., ray tracing, post-processing).

## Phase 1: Texture System Overhaul (Foundation)

**Goal:** Eliminate aliasing artifacts, reduce memory redundancy, and support high-quality filtering.

### 1.1 Mipmap Generation (Critical Priority)
**Problem:** Textures currently load only Level 0. This causes severe shimmering (aliasing) on distant surfaces and poor cache locality.
**Solution:**
- Implement `Image::generateMipmaps()` using `vkCmdBlitImage`.
- Update `Image` constructor to automatically compute `mipLevels = floor(log2(max(w, h))) + 1`.
- Execute mip generation immediately after upload (stage -> image copy -> blit loop -> shader read transition).

### 1.2 Texture Caching & Deduplication
**Problem:** Loading the same texture path multiple times creates duplicate GPU resources.
**Solution:**
- Enhance `AssetManager` with a `std::unordered_map<std::string, std::shared_ptr<Image>> m_textureCache`.
- Implement `TextureLoader` or `ImageLoader` within `AssetManager`.
- When loading a model, requesting "path/to/texture.png" returns the existing handle if already loaded.

### 1.3 Sampler Management
**Problem:** Global samplers in `RendererSystem` are rigid.
**Solution:**
- Create a `SamplerCache` in `AssetManager` or `RendererSystem`.
- Support unique sampler properties (Repeat/Clamp/Mirror, Anisotropy Level) per texture or material requirement.
- Ensure Anisotropic Filtering (e.g., 16x) is enabled for oblique angles.

---

## Phase 2: Material System Architecture (Core)

**Goal:** Create a data-oriented, flexible material system supporting modern PBR limitations and transparency.

### 2.1 PBR Material Structure
**Data Layout:**
Redefine `Material` struct for GPU alignment and extensibility (STD140/STD430 compatible).
```cpp
struct MaterialGPU {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float emissiveStrength;
    
    // Texture Indices (Bindless)
    int baseColorIndex;
    int normalIndex;
    int metallicRoughnessIndex;
    int emissiveIndex;
    int occlusionIndex;

    // Flags & Modes
    uint alphaMode;      // 0=Opaque, 1=Mask, 2=Blend
    uint doubleSided;   // 0/1
    uint padding[2];
};
```

### 2.2 Material Instances
**Architecture:**
- **`MaterialTemplate` (Shader/Pipeline):** Represents the "Shader" (VS+FS + Pipeline State).
- **`MaterialInstance` (Data):** Holds specific parameter values (colors, texture IDs) referencing a Template.
- This decoupling allows thousands of objects to share the same Pipeline but vary in look.

### 2.3 Bindless Integration
**Verification:**
- Ensure the descriptor set limits support enough textures (currently `sampler2D textures[]` with variable size).
- Confirm dynamic indexing in shader (`nonuniformEXT`) is performant and correctly synchronized.

---

## Phase 3: Advanced Rendering Features (Quality)

**Goal:** Enhance visual fidelity with complex material interactions.

### 3.1 Alpha Modes & Sorting
**Problem:** No transparency support.
**Solution:**
- **Opaque Pass:** Render all `alphaMode == OPAQUE`. Write Depth.
- **Masked Pass:** Render `alphaMode == MASK` (Alpha Test). Write Depth. Use `discard` in fragment shader.
- **Transparent Pass:** Render `alphaMode == BLEND`. Read Depth (Write optional/disabled). **Sorting Required:** Transparent objects must be sorted Back-to-Front relative to camera.

### 3.2 Advanced PBR Channels
**Features to Add:**
- **Emissive Strength:** Multiplier for HDR bloom effects.
- **Transmission:** For glass/water (requires refractive logic/framebuffer copy).
- **Clearcoat:** Second specular layer for car paint/varnish.

### 3.3 Default Textures
**Experience:**
- If a texture is missing, fallback to a 1x1 Magenta/Checkerboard texture instead of crashing or black.
- If a Normal Map is missing, fallback to (0.5, 0.5, 1.0) "Flat Normal".
- If a Metallic/Roughness is missing, fallback to (0, 1) "Non-metal/Rough".

---

## Phase 4: Roadmap & Milestones

| Milestone | Task | Estimated Effort |
| :--- | :--- | :--- |
| **M1: Mipmaps** | Implement automatic mipmap generation in `Image`. | Low | ✅ Done |
| **M2: Caching** | Implement `TextureCache` in `AssetManager`. update Loaders. | Medium | ✅ Done |
| **M3: Mat Struct** | Update `Material` struct, SSBOs, and Shaders. | Medium |
| **M4: Transparency** | Implement Basic Transparency (Sorting + Blend Pipeline). | High |
| **M5: Adv. PBR** | Emissive, Clearcoat implementation. | High |


## Immediate Next Steps
1.  **Refactor `Material` struct** to match `MaterialGPU` layout.
2.  Create `Material.hpp` to centralize material definitions.

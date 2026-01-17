# Future Roadmap & Technical Implementation Plan

This document outlines the strategic roadmap for the next phases of `AstralRenderer`. It is based on a code analysis of the current system and targets AA-grade rendering quality and high performance.

## 1. Transparency Blending (Immediate Priority)
**Status:** [x] COMPLETED (Phase 3.1)
**Goal:** Correctly render transparent objects (glass, water, particle effects) by blending them with the background, rather than just overwriting.

### Current State Analysis
- **SceneManager:** Sorts instances (Opaque: Front-to-Back, Transparent: Back-to-Front).
- **RendererSystem:** Uses a single `m_pbrPipeline` for all geometry.
- **Pipeline:** Hardcoded `blendEnable = VK_FALSE` in `src/renderer/pipeline.cpp`.

### Technical Implementation Plan
1.  **Modify `PipelineSpecs`**:
    *   Add `bool enableBlending` and `VkBlendFactor srcBlend`, `dstBlend` to `PipelineSpecs`.
    *   Update `GraphicsPipeline` constructor to use these values instead of hardcoded `VK_FALSE`.
2.  **Dual Pipelines in `RendererSystem`**:
    *   Rename `m_pbrPipeline` to `m_pbrOpaquePipeline`.
    *   Create `m_pbrTransparentPipeline` with:
        *   `enableBlending = true`
        *   `srcBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA`
        *   `dstBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA`
        *   `depthWrite = false` (typically disabled for transparents to avoid occlusion issues, though optional).
3.  **Split Draw Calls**:
    *   Expose `getOpaqueInstances()` and `getTransparentInstances()` (or ranges/offsets) in `SceneManager`.
    *   In `RendererSystem::render`:
        *   Bind Opaque Pipeline -> Draw Opaque List.
        *   Bind Transparent Pipeline -> Draw Transparent List.

## 2. Enhanced Default Assets (Robustness)
**Status:** [x] COMPLETED (Phase 3.2)
**Goal:** Prevent visual artifacts when specific texture maps (Normal, Metallic) are missing.

### Current State Analysis
- `AssetManager` returns Magenta 1x1 for any missing texture.
- Missing Normal Map -> Magenta Normal -> Likely incorrect lighting vectors.

### Technical Implementation Plan
1.  **Expand `AssetManager` Defaults**:
    *   `m_defaultColor` (Magenta 1x1) - Already exists.
    *   `m_defaultNormal` (Flat Blue `128, 128, 255`) - For missing normal maps.
    *   `m_defaultWhite` (White 1x1) - For missing multiply maps (AO, Metallic).
    *   `m_checkerboard` (Generated Pattern) - For UV debugging.
2.  **Context-Aware Fallback**:
    *   Add usage/type enum to `getOrLoadTexture(path, type)`.
    *   If type is `Normal` and load fails -> return `m_defaultNormal`.

## 3. GPU-Driven Rendering (Performance)
**Status:** CPU-based Instance Accumulation & Sorting.
**Goal:** Offload culling and sorting to Compute Shaders to handle 10k+ instances efficiently.

### Current State Analysis
- `SceneManager` loops over all instances every frame on CPU to sort and upload.
- Sorting logic is simple `std::sort`.

### Technical Implementation Plan
1.  **Indirect Draw Buffer Generation**:
    *   Stop uploading `VkDrawIndexedIndirectCommand` from CPU.
    *   Initialize a Compute Shader (`cull.comp` or new `instance_processor.comp`).
2.  **GPU Culling**:
    *   Upload all scene instances to a generic `AllInstancesBuffer` (SSBO) once (or on change).
    *   Compute Shader checks Frustum Intersect -> Appends surviving instances to `VisibleInstancesBuffer` and writes `IndirectCommands`.
3.  **GPU Sorting (Radix Sort)**:
    *   Implement a parallel Radix Sort compute shader for Bitonic Merge Sort.
    *   Sort `VisibleInstancesBuffer` by distance.

## 4. Shadow Map Improvements (Visual Quality)
**Status:** [x] COMPLETED (Phase 3.3)
**Goal:** Softer, more realistic shadows.

### Technical Implementation Plan
1.  **Poisson Disk Sampling**:
    *   Inject a constant array of Poisson distribution points into `pbr.frag`.
    *   Sample shadow map at these offset points rotated by random noise.
2.  **Normal Bias**:
    *   Refine `shadowNormalBias` usage to prevent "peter panning" (shadow detaching from object).

## 5. Configuration System (Usability)
**Status:** Hardcoded values in `main.cpp`.
**Goal:** Data-driven application settings.

### Technical Implementation Plan
1.  **Config File**: Create `config.json` or `settings.ini`.
2.  **Config Manager**:
    *   Parse file on startup.
    *   Load Window Width/Height, Fullscreen capability, Shadow Quality (Map Size), Asset Paths.
    *   Hot-reload support (check file modification time).

---

## Recommended Execution Order

1.  [x] **Transparency Blending** (Fixes immediate visual correctness).
2.  [x] **Enhanced Default Assets** (improves developer experience).
3.  [x] **Shadow Improvements** (Quick visual win).
4.  [x] **Config System** (Quality of Life).
5.  **GPU-Driven Rendering** (Long-term optimization).

**Next Task:** Start Task 5 - GPU-Driven Rendering (Long-term optimization).

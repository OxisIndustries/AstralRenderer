---
name: AstralRenderer Architecture
description: Guidelines for navigating and extending the AstralRenderer engine architecture
---

# AstralRenderer Architecture Skill

This skill explains the specific architectural patterns used in the AstralRenderer engine.

## Directory Structure

- **`src/core`**: Low-level abstractions (Context, Device, Queue, Window).
- **`src/resources`**: GPU resource wrappers (Buffer, Image, Shader).
- **`src/renderer`**: High-level rendering logic (RenderGraph, Pipeline, SceneManager).
- **`include/astral`**: Public API headers.

## RenderGraph System

**Purpose:** Manages frame-based resource dependencies and barriers automatically.

**How to Add a Pass:**
1. Define a `RenderPass` struct or lambda.
2. Register input/output resources using `RenderGraph::addPass`.
3. Provide an execute callback.

**Example:**
```cpp
renderGraph.addPass("MyPass",
    [&](RenderGraphBuilder& builder) {
        // Define dependencies
        builder.read(sceneDepth);
        builder.write(outputColor);
    },
    [&](CommandBuffer& cmd) {
        // Execute rendering commands
    }
);
```

## Material System

**PBR Standard:**
- Uses a Metallic-Roughness workflow.
- Data is passed via **Material Buffers** (Set #2).
- Shaders expected: `pbr.vert`, `pbr.frag`.

## Coding Conventions

- **Namespaces:** All code must be within the `astral` namespace.
- **Classes:** PascalCase (`RendererSystem`).
- **Methods/Variables:** camelCase (`createInstance`, `m_device`).
- **Members:** Prefix private members with `m_` (e.g., `m_context`).
- **Error Handling:** Use exceptions (`std::runtime_error`) for fatal initialization errors; use return codes or `std::expected` for runtime recoverable errors.

## Adding New Features

1. **Header:** Create public interface in `include/astral/<layer>/`.
2. **Source:** Implement specific logic in `src/<layer>/`.
3. **Registration:** If it's a renderer system, verify if it needs to be added to `RendererSystem` or `Application`.

# Features

Astral Renderer is a modern Vulkan-based rendering engine designed for high-performance and visual fidelity.

## Rendering Features
- **PBR Rendering**: Metallic-roughness workflow using Cook-Torrance BRDF.
- **Image-Based Lighting (IBL)**: High-quality environment lighting with pre-filtered importance sampling.
- **Clustered Forward Shading**: Efficiently handles thousands of dynamic lights by partitioning the view frustum.
- **Cascaded Shadow Maps (CSM)**: Multi-layered shadow maps with PCF filtering for smooth distance transitions.
- **Modern Post-Processing Stack**:
  - **HDR/Bloom**: High-quality light glows using dual-filtering.
  - **SSAO**: Screen-space ambient occlusion with Gaussian blur.
  - **Anti-Aliasing**: Support for FXAA and a structural foundation for TAA.
  - **Tone Mapping**: ACES and Reinhard tone mapping operators.

## Performance & Architecture
- **Bindless-Style Descriptors**: Uses high-capacity descriptor pools and indexing to minimize state changes.
- **Render Graph**: Automatic memory barriers and layout transitions for flexible frame composition.
- **Compute Pass Optimization**: GPU-side frustum culling and light culling.
- **Multi-Buffering**: Double-buffered uniforms and resource uploads for overlap between CPU and GPU.

## User Interface & Tooling
- **Real-time Scene Inspector**: Live editing of lights (color, intensity, position) and materials (factors, alpha).
- **Dynamic Controls**: Extensive panel for exposure, gamma, shadow bias, and post-process parameters.
- **Performance Profiling**: On-screen FPS and frame timing counters.
- **GLTF Support**: Fast glTF 2.0 loading using `fastgltf`.

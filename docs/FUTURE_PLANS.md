# Future Plans & Roadmap

The following features and optimizations are planned for future versions of the Astral Renderer.

## Rendering Enhancements
- **Temporal Anti-Aliasing (TAA)**: Full integration of sub-pixel jittering and historical color clamping to eliminate jagged edges.
- **Advanced Materials**: Support for clear coat, anisotropy, cloth models, and subsurface scattering.
- **Volumetric Rendering**: Support for volumetric fog and light shafts (God rays).
- **Environment Parallax**: Support for parallax-corrected environment maps.

## Performance & Scalability
- **Multi-Threaded Recording**: Utilizing multiple CPU cores to build command buffers across groups of render passes.
- **Dynamic Resolution Scaling**: Adjusting internal render resolution based on GPU load to maintain target framerates.
- **Ray Traced Hybrid Effects**:
  - **RTRS**: Ray Traced Soft Shadows.
  - **RTR**: Ray Traced Reflections for perfect specular surfaces.
  - **RTGI**: Hardware-accelerated Global Illumination.

## Tooling & Usability
- **Scripting Integration**: Support for Lua or C# for defining scene behavior and custom render effects.
- **Hot-Reloadable Shaders**: Automatic recompilation of GLSL shaders upon file changes for rapid iteration.
- **Scene Serialization**: Ability to save and load modified scene states (lights/materials) back to disk.
- **Asset Pipeline**: Support for texture compression (BCn/ASTC) and optimized binary mesh formats for faster loading.

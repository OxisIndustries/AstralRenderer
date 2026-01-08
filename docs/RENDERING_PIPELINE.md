# Rendering Pipeline

Astral Renderer utilizes a hybrid rendering pipeline combining Clustered Forward Rendering with an extensive post-processing stack.

## Pipeline Stages

### 1. Compute Pre-Passes
- **Culling (`CullShader`)**: Performs frustum culling on mesh instances and generates indirect draw commands.
- **Cluster Building**: Generates a 3D grid partition of the view frustum.
- **Light Culling**: Prunes lights per-cluster to optimize the forward shading pass.

### 2. Shadow Mapping (CSM)
- **Technique**: 4-cascade Cascaded Shadow Maps with depth-clamping and front-face culling.
- **Filtering**: PCF (Percentage-Closer Filtering) with configurable range (0 to 4 samples).
- **Update**: Managed by push constants for individual cascade matrices.

### 3. Forward Shading Pass
- **BRDF**: Physically Based Rendering (Cook-Torrance) using `Metallic-Roughness` workflow.
- **IBL**: Image-Based Lighting with configurable intensity and skybox visibility.
- **Resources**: Outputs HDR Color, World-Space Normals, Linear Depth, and Velocity.

### 4. Screen-Space Ambient Occlusion (SSAO)
- **Calculation**: Uses depth and normal buffers with a configurable radius and bias.
- **Filtering**: Separable Gaussian blur pass to eliminate noise.

### 5. Post-Processing Stack (Composite)
The `Composite` pass serves as the final integration stage:
- **Bloom**: Advanced glow effect with dual-filtering (configurable threshold, strength, and softness).
- **Tone Mapping**: High-dynamic-range to LDR conversion (ACES/Reinhard).
- **Gamma Correction**: Final 1/gamma correction (configurable, default 2.2).
- **AA**: Final anti-aliasing (FXAA) before output.

### 6. UI Overlay (`UIPass`)
- **Integration**: A specific render pass targeting the swapchain image AFTER all composition.
- **Interactivity**: Captures window events via callback chaining to allow real-time parameter tweaking.

## Configuration & Control
Most stages are controlled via `UIParams`, passed as push constants to the `Composite` shader or uniforms to the `PBR` shader:
- **Post-Process**: Strength and Threshold toggles.
- **Shadows**: Normal/Shadow bias adjustment.
- **Tonemapping**: Exposure and Gamma sliders.
- **Scene**: Real-time light intensity/color and material property editing.

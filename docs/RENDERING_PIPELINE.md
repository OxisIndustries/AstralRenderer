# Rendering Pipeline

Astral Renderer utilizes a hybrid rendering pipeline combining Clustered Forward Rendering with extensive post-processing.

## Pipeline Stages

### 1. Compute Pre-Passes
- **Culling (`CullShader`)**:
  - Frustum culling of mesh instances.
  - Generates indirect draw commands (`VkDrawIndexedIndirectCommand`).
- **Cluster Building (`ClusterBuildShader`)**:
  - Divides the view frustum into a 3D grid (Clusters).
- **Light Culling (`ClusterCullShader`)**:
  - Assigns dynamic lights to the generated clusters for efficient lookup during the PBR pass.

### 2. Shadow Mapping
- **Technique**: Cascaded Shadow Maps (CSM).
- **Resolution**: 4096x4096 per cascade.
- **Passes**: 4 distinct render passes (one per cascade layer) rendering scene depth.
- **Culling**: Front-face culling to reduce shadow acne.

### 3. Geometry Pass (Forward+)
- **Target**: HDR Color Buffer (`R16G16B16A16_SFLOAT`), Normal Buffer, Depth Buffer, Velocity Buffer.
- **Shading**: PBR (Cook-Torrance BRDF) with Image-Based Lighting (IBL).
- **Lighting**: Clustered light lookup using the data from the pre-passes.
- **Skybox**: Renders the environment cubemap.

### 4. Screen Space Ambient Occlusion (SSAO)
- **Base Pass**: Calculates occlusion based on depth and normal buffers.
- **Blur Pass**: Separable blur to reduce noise.

### 5. Bloom
- **Extraction**: Thresholds bright areas from the HDR buffer.
- **Downsample/Upsample**: Dual-filtering method for high-quality bloom.
- **Composition**: Additively blended back into the HDR buffer (conceptually).

### 6. Composite
- **Inputs**: HDR Color, Bloom Buffer, SSAO Buffer.
- **Operations**:
  - Applies SSAO.
  - Adds Bloom.
  - Tone Mapping (ACES or Reinhard).
  - Exposure Adjustment.
- **Output**: LDR Color Buffer (`R8G8B8A8_UNORM`).

### 7. Anti-Aliasing
- **TAA (Temporal Anti-Aliasing)**:
  - Uses specific velocity buffer and previous frame history.
  - *Note: Integrated into pipeline flow, currently configured as a potential pass.*
- **FXAA (Fast Approximate Anti-Aliasing)**:
  - Final post-process pass applied to the logic buffer before presentation.

## Shader Resource Binding
- **Bindless Architecture**: Heavy use of descriptor indexing (where supported) or simplified sets.
- **Push Constants**: Used for frequently changing data (transforms, indices, material IDs).

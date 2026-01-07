# Current Status: Render Pipeline Debugging

## Latest Milestone: **PIPELINE PROVEN FUNCTIONAL**
**Success**: The application now renders a **Red Triangle** over a **3D Skybox**.
*   **Significance**:
    1.  **Vulkan Backend Works**: Device, Queue, Swapchain are perfect.
    2.  **Render Graph Works**: All passes (`Skybox` -> `Geometry` -> `PostProcess`) are executing.
    3.  **Rasterizer Works**: Geometry is being drawn and not discarded.
    4.  **Post-Processing Fixed**: The "Blue/Magenta Screen" was caused by **Back-Face Culling** discarding the full-screen quad in post-process passes. We fixed this by forcing `VK_CULL_MODE_NONE`.

## Remaining Issues (The "Real" Bug)
Now that the debug overrides (`force draw`, `hardcoded triangle`) are working, we must find why the **Real Geometry (Damaged Helmet)** wasn't drawing.
Suspects:
1.  **Vertex Input**: `inPos` might be reading garbage (stride/offset mismatch).
2.  **Instance Buffer**: `pbr.vert` reads model matrices from `InstanceBuffer`. If this descriptor is invalid/empty, the specific transform is (0,0,0), collapsing geometry to a point.
3.  **Material Buffer**: `pbr.frag` reads materials. Invalid index = crash/black.

## Next Steps: Progressive Restoration
1.  **Verify Scene Data**: Checking `skybox.vert` to see how it successfully reads View/Proj matrices.
2.  **Restore `pbr.vert` Input**: Modify `pbr.vert` to use real `inPos` and `SceneBuffer` (ViewProj), but keep `InstanceBuffer` bypassed (use Identity Matrix) to rule out Instance data issues.
    *   *Goal*: See the Helmet shape (Red).
3.  **Enable Instance Buffer**: Restore Model Matrix read.
    *   *Goal*: See Helmet at correct transform.
4.  **Restore Fragments**: Revert `pbr.frag` to PBR shading.

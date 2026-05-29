#pragma once
// =============================================================================
// EngineOrigin -- floating-origin / camera-relative receptacle (Foundations
// Audit \xc2\xa71, top priority)
// =============================================================================
// EngineOrigin::current() returns the world point that the rendering data
// path treats as the local origin.
//
// Today: always (0,0,0). Engine-relative == world; every subtraction by
// EngineOrigin::current() is numerically a no-op. The receptacle exists so
// that the floating-origin upgrade (when the camera drifts far enough from
// the current origin that float precision near the camera starts to
// degrade) can be turned on without revisiting every site that puts world
// coordinates onto the GPU.
//
// Why this matters (Foundations Audit \xc2\xa71 / \xe2\x98\x85\xe2\x98\x85\xe2\x98\x85):
//   float precision halves every doubling distance from the origin. At
//   ~2 km the smallest representable step is ~0.5 mm, at ~16 km it is ~4 mm,
//   and any pipeline that bakes world coordinates into matrices (DrawData,
//   CullObject) inherits that error. Hi-Z screen-space AABB projection
//   amplifies it. Open-world engines (UE5 LWC, Godot LWC, classic floating-
//   origin) fix this by either (a) doubling the CPU representation and
//   downcasting to camera-relative float at draw time, or (b) periodically
//   re-baseing the world so |position| stays small. (b) maps cleanly onto
//   this engine and is what current() supports.
//
// Upgrade plan when current() starts returning a non-zero shift:
//   1. Every site that uploads world coordinates subtracts current()
//      first. The list (with file + line as of A6):
//        - DrawData.model translation column   (static_cull_build.h emit)
//        - CullObject.centerRadius.xyz         (static_cull_build.h)
//        - FrameUBO.viewPos.xyz                (camera_system.cpp,
//                                                title_layer.cpp,
//                                                pass_chain.cpp reflect)
//        - View matrix translation             (camera.getViewMatrix,
//                                                title_layer.cpp lookAt)
//        - Shadow lightView translation        (camera_system.cpp,
//                                                title_layer.cpp)
//        - cull.comp push constant viewPos     (CullingPass::execute via
//                                                pass_chain info.viewPos)
//   2. Trigger: when |cameraWorld - currentOrigin| crosses a threshold
//      (~512 m is a common choice), rebase by setting currentOrigin to
//      e.g. floor(cameraWorldXZ / shiftStep) * shiftStep.
//   3. Cross-rebase continuity: prevViewProj (motion vectors) must be
//      remapped by the same delta on the rebase frame so the (cur - prev)
//      reprojection stays continuous; otherwise a 1-frame motion glitch
//      shows up exactly when the rebase fires.
//   4. Shaders never see currentOrigin -- they always compute against
//      engine-relative coordinates -- so no shader change is required for
//      the upgrade.
//
// Floating origin is intentionally NOT wired up today (Foundations \xc2\xa71
// recommends \xe3\x80\x8c\xe8\xa6\x8f\xe6\xa8\xa1\xe3\x81\x8c\xe5\xb0\x8f\xe3\x81\x95\xe3\x81\x84\xe4\xbb\x8a\xe3\x81\x93\xe3\x81\x9d\xe8\xa6\x8f\xe7\xb4\x84\xe3\x81\xa0\xe3\x81\x91\xe5\x85\x88\xe3\x81\xab\xe6\xb1\xba\xe3\x82\x81\xe3\x82\x8b\xe3\x80\x8d). When the world reaches a size that
// makes precision a problem (or proactively, before then), wire the
// subtraction at the sites listed above and provide a real implementation
// of current() that tracks the rebase state.
// =============================================================================
#include <glm/glm.hpp>

namespace myengine::world {

class EngineOrigin {
   public:
    // The world point that engine-relative coordinates are measured from.
    // Today: (0,0,0). When floating origin engages, this returns the current
    // rebase shift.
    static glm::vec3 current() noexcept { return glm::vec3(0.0f); }
};

}  // namespace myengine::world

#pragma once
// =============================================================================
// SkyAtmosphere -- X: Hillaire 2020 atmospheric scattering LUT receptacle
// =============================================================================
// "A Scalable and Production Ready Sky and Atmosphere Rendering Technique"
// (Sebastien Hillaire, 2020) is the modern open-world standard for physical
// sky -- UE5, Frostbite, modern Godot, idTech 7 all run a variant. It
// composes four LUTs ("look-up tables", small 2D images) in compute and
// samples them in the sky shader plus per-fragment for aerial perspective:
//
//   1. Transmittance LUT    (256 x 64 RGB16F)
//        sun -> point through atmosphere.
//   2. Multi-scattering LUT (32 x 32 RGB16F)
//        accumulated higher-order scattering integral.
//   3. SkyView LUT          (192 x 108 RGB16F, view-dependent)
//        ray-marched per camera view direction, regenerated every frame.
//   4. Aerial perspective LUT (32 x 32 x 32 RGBA16F, per-view 3D)
//        per-distance accumulated scattering for fog-in-volume.
//
// Receptacle shape:
//   - lutTransmittance_ / lutMultiscatter_ : built once at init (sun-
//     independent in the sense that they capture how the atmosphere
//     attenuates light regardless of sun direction).
//   - lutSkyView_ / lutAerial_ : per-frame compute pass driven by the
//     current sun direction (FrameUBO.lightDir).
//   - SkyDome render samples the SkyView LUT; opaque fragments composite
//     against the aerial-perspective LUT for fog.
//
// What this receptacle unlocks:
//   - Physically-based day/night cycle (lightDir varies, sky responds).
//   - Distance fog with the same physical basis (no separate fog formula).
//   - Correct sun colour at sunrise/sunset (Rayleigh + Mie scattering).
//   - The same LUT machinery feeds volumetric clouds and atmospheric
//     light shafts in later Phases.
//
// Per-Phase activation:
//   1. Add sky.comp (Mie + Rayleigh integrals) + skyview.comp (per-view
//      ray-march) + aerial.comp (3D volume) compute shaders.
//   2. Allocate the four LUTs above via VmaImage.
//   3. Insert a compute dispatch into pass_chain BEFORE main pass (so
//      opaque fragments can sample aerial perspective).
//   4. SkyDome draw samples lutSkyView_.
//   5. FrameUBO carries the current sun direction so the per-frame LUTs
//      stay in sync with the lighting system (lightDir already does).
// =============================================================================
#include "renderer/vma_image.h"

class VulkanContext;

namespace myengine::renderer {

class SkyAtmosphere {
   public:
    void init(VulkanContext* /*ctx*/) {}
    void shutdown() {
        lutTransmittance_.reset();
        lutMultiscatter_.reset();
        lutSkyView_.reset();
        lutAerial_.reset();
    }

   private:
    VmaImage lutTransmittance_;
    VmaImage lutMultiscatter_;
    VmaImage lutSkyView_;
    VmaImage lutAerial_;  // 3D
};

}  // namespace myengine::renderer

#pragma once
// =============================================================================
// AutoExposure -- V: luminance-histogram compute + exposure EMA receptacle
// =============================================================================
// PostPass today uses a static `exposure_` field configurable from the HUD.
// V replaces it with a per-frame histogram of the HDR target's luminance
// (compute pass), an average-luminance reduction (compute pass), and an
// exponential-moving-average eye-adaptation pass that smooths the per-frame
// jump so a sudden indoor/outdoor transition takes ~0.5 s rather than 1
// frame. This is the modern UE5 / Frostbite default and is what makes HDR
// rendering feel natural.
//
// Receptacle shape (Phase activation lands in PostPass refactor):
//   - histogram_  : VmaBuffer (256 uint32 bins) -- the per-frame histogram.
//   - average_    : VmaBuffer (1 float)         -- EMA-smoothed log-luminance.
//   - settings_   : evaluation parameters (min/max log-luminance bin edges,
//                   adaptation speed in seconds).
//   - PostPass reads average_ instead of a static exposure value.
//
// The two compute passes (build + reduce) hand off through the histogram
// buffer. Build dispatches one thread per pixel and atomicAdd's the bin.
// Reduce dispatches one workgroup over the histogram, walks the bins,
// computes the weighted-average log-luminance, EMAs against average_'s
// previous value (delta = 1 - exp(-dt / tau)), writes back.
// =============================================================================
#include "renderer/vma_buffer.h"

#include <cstdint>

class VulkanContext;
class DeletionQueue;

namespace myengine::renderer {

struct AutoExposureSettings {
    float minLogLuminance = -10.0f;
    float maxLogLuminance = +2.0f;
    float adaptationSpeed = 1.1f;  // seconds to reach equilibrium under step change
    float bias            = 0.0f;  // additive log-luminance shift (artistic)
    bool  enabled         = false; // receptacle today; PostPass takes over per-Phase
};

class AutoExposure {
   public:
    void init(VulkanContext* /*ctx*/, DeletionQueue* /*dq*/) {
        // Phase: allocate histogram_ (256 * 4 B) and average_ (4 B) via VmaBuffer.
        // Today the buffers stay empty; PostPass reads its static exposure.
    }
    void shutdown() {
        histogram_.reset();
        average_.reset();
    }

    AutoExposureSettings& settings() noexcept { return settings_; }

   private:
    AutoExposureSettings settings_{};
    VmaBuffer histogram_;
    VmaBuffer average_;
};

}  // namespace myengine::renderer

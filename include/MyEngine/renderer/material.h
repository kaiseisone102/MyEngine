#pragma once
// =============================================================================
// material.h - Material (bindless texture slot + SSBO material id)
// =============================================================================
// S6-c: Material no longer owns any Vulkan handle. Every draw path resolves its
// texture through materials[materialId] in the global MaterialRegistry SSBO and
// samples bindlessTextures[albedoIdx] from the bindless set. A Material is now a
// plain data holder: a bindless texture index (copied onto the GpuMaterial as
// albedoIdx) and a materialId (the SSBO slot the shader indexes by).
//
//   bindlessIndex_ : slot in the bindless texture array (UINT32_MAX = none)
//   materialId_    : slot in the MaterialRegistry SSBO  (0 = default material)
// =============================================================================
#include <cstdint>

class Material {
   public:
    // S6-c: kept as a no-op for call-site compatibility (world_terrain /
    // world_water / asset_registry still call destroy()). Material owns no GPU
    // resource now, so there is nothing to free.
    void destroy() {}

    // Bindless texture slot (Phase 1D).
    uint32_t bindlessIndex() const { return bindlessIndex_; }
    void setBindlessIndex(uint32_t idx) { bindlessIndex_ = idx; }

    // Slot in the global MaterialRegistry SSBO (Phase 1K-2 S4-d).
    uint32_t materialId() const { return materialId_; }
    void setMaterialId(uint32_t id) { materialId_ = id; }

   private:
    uint32_t bindlessIndex_ = UINT32_MAX;
    uint32_t materialId_ = 0;  // 0 = default material until registered
};
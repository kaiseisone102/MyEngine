// =============================================================================
// shared/types.h - C++ / GLSL shared type definitions (Phase 1B-4)
// =============================================================================
// Both C++ and GLSL include this file. struct layouts match exactly between
// CPU and GPU side. This prevents the kind of UBO/push constant mismatch that
// caused Phase 1C migration issues.
//
// USAGE:
//   C++:  #include "shaders/shared/types.h"  -> use myengine::shared::FrameUBO
//   GLSL: #extension GL_GOOGLE_include_directive : require
//         #include "shared/types.h"          -> use FrameUBO directly
//
// LAYOUT RULES:
//   - All vec3 fields MUST be padded to vec4 alignment (Vulkan std140/std430)
//   - mat4 starts at 16-byte boundary
//   - Use vec4 instead of vec3 + float pairs unless explicit padding is clearer
//   - VkDeviceAddress (uint64_t) is 8-byte aligned, often paired for 16-byte
//
// PHASE 1B-4 BDA NOTE:
//   SkinnedPushConstants / ShadowSkinnedPushConstants now carry a 64-bit GPU
//   address (skinBuffer) that points directly to the skin matrix array.
//   On the GLSL side this becomes a buffer_reference typed pointer.
//   See triangle_skinned.vert / shadow_skinned.vert for usage.
// =============================================================================
#ifndef MYENGINE_SHARED_TYPES_H
#define MYENGINE_SHARED_TYPES_H

#ifdef __cplusplus
  #include <cstdint>
  #include <vulkan/vulkan.h>  // for VkDeviceAddress
  #include <glm/glm.hpp>
  // Aliases so GLSL-style type names work in C++ context
  using vec2 = glm::vec2;
  using vec3 = glm::vec3;
  using vec4 = glm::vec4;
  using ivec2 = glm::ivec2;
  using ivec3 = glm::ivec3;
  using ivec4 = glm::ivec4;
  using mat3 = glm::mat3;
  using mat4 = glm::mat4;
  using uint = uint32_t;
  namespace myengine::shared {
#endif

// -----------------------------------------------------------------------------
// FrameUBO: per-frame global data shared across all 3D passes
// (matches existing FrameUniforms::LightingUBO)
// -----------------------------------------------------------------------------
struct FrameUBO {
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDir;       // direction the light travels (normalized)
    vec4 lightColor;     // .rgb used
    vec4 ambient;        // .rgb used
    vec4 viewPos;        // .xyz = camera position
    vec4 shadowParams;   // .x = shadow strength, yzw = reserved
};

// -----------------------------------------------------------------------------
// StaticPushConstants: per-draw data for non-skinned 3D meshes (80 bytes)
// -----------------------------------------------------------------------------
struct StaticPushConstants {
    mat4 model;
    float alpha;
    float _pad0;
    float _pad1;
    float _pad2;
};

// -----------------------------------------------------------------------------
// SkinnedPushConstants: per-draw data for skinned 3D meshes (96 bytes)
//
// Layout (CPU/GPU must agree):
//   offset  0 : mat4 model                  (64 bytes)
//   offset 64 : uint64 skinBuffer (GPU ptr) ( 8 bytes)
//   offset 72 : int skinOffset              ( 4 bytes)
//   offset 76 : float alpha                 ( 4 bytes)
//   offset 80 : pad                         (16 bytes)
//   total = 96 bytes
//
// GLSL note: on the shader side, skinBuffer is declared as a buffer_reference
// typed pointer (see triangle_skinned.vert). On the CPU side it's just a
// VkDeviceAddress / uint64 obtained from vkGetBufferDeviceAddress.
// -----------------------------------------------------------------------------
#ifdef __cplusplus
struct SkinnedPushConstants {
    mat4 model;            // 64
    VkDeviceAddress skinBuffer;  // 8  (uint64_t)
    int32_t skinOffset;    // 4
    float alpha;           // 4
    int32_t _pad0;         // 4
    int32_t _pad1;         // 4
    int32_t _pad2;         // 4
    int32_t _pad3;         // 4
};
#else
// GLSL forward declaration. SkinMatrices is defined in the consuming shader.
// We use uint64_t here for the address; the shader then casts it to a typed pointer.
struct SkinnedPushConstants {
    mat4 model;
    uvec2 skinBuffer;  // 64-bit GPU address as uvec2 (use buffer_reference cast)
    int skinOffset;
    float alpha;
    int _pad0;
    int _pad1;
    int _pad2;
    int _pad3;
};
#endif

// -----------------------------------------------------------------------------
// ShadowStaticPushConstants: shadow pass for non-skinned meshes (64 bytes)
// -----------------------------------------------------------------------------
struct ShadowStaticPushConstants {
    mat4 model;
};

// -----------------------------------------------------------------------------
// ShadowSkinnedPushConstants: shadow pass for skinned meshes (96 bytes)
// Same layout as SkinnedPushConstants but no alpha.
// -----------------------------------------------------------------------------
#ifdef __cplusplus
struct ShadowSkinnedPushConstants {
    mat4 model;            // 64
    VkDeviceAddress skinBuffer;  // 8
    int32_t skinOffset;    // 4
    int32_t _pad0;         // 4
    int32_t _pad1;         // 4
    int32_t _pad2;         // 4
    int32_t _pad3;         // 4
    int32_t _pad4;         // 4
};
#else
struct ShadowSkinnedPushConstants {
    mat4 model;
    uvec2 skinBuffer;  // 64-bit GPU address as uvec2 (use buffer_reference cast)
    int skinOffset;
    int _pad0;
    int _pad1;
    int _pad2;
    int _pad3;
    int _pad4;
};
#endif

// -----------------------------------------------------------------------------
// WaterPushConstants: water surface draw data (112 bytes)
// -----------------------------------------------------------------------------
struct WaterPushConstants {
    mat4 model;            // offset 0,  size 64
    float time;            // offset 64
    float waveAmp;         // offset 68
    float waveSpeed;       // offset 72
    float waveWavelength;  // offset 76
    vec4 shallowColor;     // offset 80, size 16
    vec4 deepColor;        // offset 96, size 16
    // total = 112 bytes
};

#ifdef __cplusplus
  }  // namespace myengine::shared
#endif

#endif  // MYENGINE_SHARED_TYPES_H

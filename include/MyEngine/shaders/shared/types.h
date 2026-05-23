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
  using uvec2 = glm::uvec2;
  using uvec4 = glm::uvec4;
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

    // === Phase 1C extensions ===
    vec4 time;          // x=time(sec), y=deltaTime, z=frameNumber, w=sin(time*2pi)
    vec4 screenSize;    // xy=(width,height), zw=(1/width, 1/height)
    vec4 jitter;        // xy=current frame jitter, zw=previous (TAA reserved)
    vec4 cameraParams;  // x=nearZ, y=farZ, z=fov(rad), w=aspect

    // === Phase 1K-2: unified material SSBO address (BDA) ===
    uvec4 materialBuffer;  // xy = 64-bit GPU address (lo,hi); zw reserved
};

// -----------------------------------------------------------------------------
// StaticPushConstants: per-draw data for non-skinned 3D meshes (80 bytes)
// -----------------------------------------------------------------------------
struct StaticPushConstants {
    mat4 model;
    float alpha;
    uint materialId;  // Phase 1K-2 S4: index into the material SSBO
    float _pad1;
    float _pad2;
};

// -----------------------------------------------------------------------------
// StaticBindlessPushConstants: Phase 1D static draw with bindless texture (96 bytes)
//
// Layout:
//   offset  0 : mat4 model     (64 bytes)
//   offset 64 : float alpha    ( 4 bytes)
//   offset 68 : int albedoIdx  ( 4 bytes) - bindless texture slot
//   offset 72 : pad (24 bytes total to reach 96, aligned to 16)
//   total = 96 bytes
// -----------------------------------------------------------------------------
#ifdef __cplusplus
struct StaticBindlessPushConstants {
    mat4 model;
    float alpha;
    int32_t albedoIdx;
    float metallic;   // Phase 1K-2
    float roughness;  // Phase 1K-2
    int32_t _pad2;
    int32_t _pad3;
    int32_t _pad4;
    int32_t _pad5;
};
#else
struct StaticBindlessPushConstants {
    mat4 model;
    float alpha;
    int albedoIdx;
    float metallic;   // Phase 1K-2
    float roughness;  // Phase 1K-2
    int _pad2;
    int _pad3;
    int _pad4;
    int _pad5;
};
#endif

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
    uint32_t materialId;   // 4  Phase 1K-2 S5: material SSBO index
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
    uint materialId;   // Phase 1K-2 S5
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
// InstanceData: Phase 1F - unified per-instance data for ALL instanced draws
//   (grass, and future clouds / trees / flowers). Stored in an SSBO indexed by
//   gl_InstanceIndex. model is always used; color/params are optional per use:
//     color : rgb tint (multiplied with albedo) + a spare
//     params: x=kind, y=windInfluence, z=ao, w=reserved
//   mat4(64) + vec4(16) + vec4(16) = 96 bytes, std430 16-byte aligned.
//   No BDA inside, so one definition serves both C++ and GLSL.
// -----------------------------------------------------------------------------
struct InstanceData {
    mat4 model;
    vec4 color;
    vec4 params;
};

// -----------------------------------------------------------------------------
// GpuMaterial: Phase 1K-2 - unified PBR material, stored in an SSBO indexed by
//   materialId. Read via BDA (buffer address passed in the frame UBO).
//   Texture indices are bindless slots; -1 means "no texture, use the factor".
//   vec4(16) + float*4(16) + int*8(32) = 64 bytes, std430 16-byte aligned.
//   No BDA inside, so one definition serves both C++ and GLSL.
// -----------------------------------------------------------------------------
struct GpuMaterial {
    vec4  baseColorFactor;   // fallback / tint when albedoIdx < 0
    float metallic;          // fallback when mrIdx < 0
    float roughness;         // fallback when mrIdx < 0
    float emissiveStrength;  // reserved (emissive use later)
    float _pad0;
    int   albedoIdx;         // bindless texture slot, -1 = none
    int   normalIdx;         // 1K-5 normal map, -1 = use vertex normal
    int   mrIdx;             // 1K-4 metallic-roughness map, -1 = use factors
    int   aoIdx;             // 1K-6 AO map, -1 = none
    int   emissiveIdx;       // reserved, -1 = none
    int   _pad1;
    int   _pad2;
    int   _pad3;
};

// -----------------------------------------------------------------------------
// InstancedPushConstants: Phase 1E - instanced static mesh draw
//   instanceBuffer (BDA) points to an array of mat4 model matrices.
//   gl_InstanceIndex selects the row. albedoIdx = bindless texture slot.
//
//   offset  0 : uint64 instanceBuffer (GPU ptr)  ( 8 bytes)
//   offset  8 : int    albedoIdx                 ( 4 bytes)
//   offset 12 : float  alpha                     ( 4 bytes)
//   total = 16 bytes
// -----------------------------------------------------------------------------
#ifdef __cplusplus
struct InstancedPushConstants {
    VkDeviceAddress instanceBuffer;  // 8
    int32_t albedoIdx;               // 4
    float alpha;                     // 4
};
#else
struct InstancedPushConstants {
    uvec2 instanceBuffer;  // 64-bit GPU address (buffer_reference cast)
    int albedoIdx;
    float alpha;
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

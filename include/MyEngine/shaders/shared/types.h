// =============================================================================
// shared/types.h - C++ / GLSL shared type definitions (Phase 1A2)
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
// =============================================================================
#ifndef MYENGINE_SHARED_TYPES_H
#define MYENGINE_SHARED_TYPES_H

#ifdef __cplusplus
  #include <cstdint>
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
// StaticPushConstants: per-draw data for non-skinned 3D meshes
// (matches MainPass::StaticPushConstants, 80 bytes)
// -----------------------------------------------------------------------------
struct StaticPushConstants {
    mat4 model;
    float alpha;
    float _pad0;
    float _pad1;
    float _pad2;
};

// -----------------------------------------------------------------------------
// SkinnedPushConstants: per-draw data for skinned 3D meshes
// (matches MainPass::SkinnedPushConstants, 80 bytes)
// -----------------------------------------------------------------------------
struct SkinnedPushConstants {
    mat4 model;
    int skinOffset;
    float alpha;
    int _pad0;
    int _pad1;
};

// -----------------------------------------------------------------------------
// ShadowStaticPushConstants: shadow pass for non-skinned meshes (64 bytes)
// -----------------------------------------------------------------------------
struct ShadowStaticPushConstants {
    mat4 model;
};

// -----------------------------------------------------------------------------
// ShadowSkinnedPushConstants: shadow pass for skinned meshes (80 bytes)
// -----------------------------------------------------------------------------
struct ShadowSkinnedPushConstants {
    mat4 model;
    int skinOffset;
    int _pad0;
    int _pad1;
    int _pad2;
};

// -----------------------------------------------------------------------------
// WaterPushConstants: water surface draw data (matches water.vert/frag)
// IMPORTANT: 112 bytes total. C++ side WaterPipeline::PushConstants must
// be updated to match this layout exactly.
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

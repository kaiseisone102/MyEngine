// include/MyEngine/renderer/mesh.h
#pragma once
// =============================================================================
// Mesh - geometry uploaded into the shared GeometryBuffer megabuffer.
// =============================================================================
// Vertex layout (matches triangle.vert / triangle_skinned.vert):
//   - pos          (vec3,  location=0)
//   - color        (vec3,  location=1)
//   - texCoord     (vec2,  location=2)
//   - normal       (vec3,  location=3)
//   - jointIndices (ivec4, location=4)  cube fills 0, Model fills from aiBone
//   - jointWeights (vec4,  location=5)  cube fills 0, Model fills from aiBone
// =============================================================================

#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

class VulkanContext;
class GeometryBuffer;

struct Vertex {
    glm::vec3  pos;                                  // location=0
    glm::vec3  color;                                // location=1
    glm::vec2  texCoord;                             // location=2
    glm::vec3  normal;                               // location=3
    glm::ivec4 jointIndices = {0, 0, 0, 0};          // location=4
    glm::vec4  jointWeights = {0.f, 0.f, 0.f, 0.f};  // location=5

    bool operator==(const Vertex& o) const {
        return pos == o.pos && color == o.color && texCoord == o.texCoord &&
               normal == o.normal && jointIndices == o.jointIndices &&
               jointWeights == o.jointWeights;
    }
};

namespace std {
template <>
struct hash<Vertex> {
    size_t operator()(Vertex const& v) const noexcept {
        size_t h = std::hash<glm::vec3>()(v.pos);
        h = (h ^ (std::hash<glm::vec3>()(v.color) << 1)) >> 1;
        h = (h ^ (std::hash<glm::vec2>()(v.texCoord) << 1)) >> 1;
        h = (h ^ (std::hash<glm::vec3>()(v.normal) << 1)) >> 1;
        h = (h ^ (std::hash<int>()(v.jointIndices.x) ^ std::hash<int>()(v.jointIndices.y) ^
                  std::hash<int>()(v.jointIndices.z) ^ std::hash<int>()(v.jointIndices.w))) >>
            1;
        h = h ^ std::hash<glm::vec4>()(v.jointWeights);
        return h;
    }
};
}  // namespace std

class Mesh {
   public:
    // Geometry is uploaded into the shared GeometryBuffer megabuffer
    // (returns a MeshHandle range stored in firstIndex_/vertexOffset_/blockIndex_).
    void createCube(const VulkanContext* ctx, GeometryBuffer* geom);
    void createCrossQuad(const VulkanContext* ctx, GeometryBuffer* geom);

    // GeometryBuffer owns the underlying memory; this just clears local handles
    // so explicit shutdown ordering matches the rest of the asset clean-up.
    void destroy() {
        geom_ = nullptr;
        ctx_ = nullptr;
        firstIndex_ = 0;
        vertexOffset_ = 0;
        blockIndex_ = 0;
        indexCount_ = 0;
    }

    void bind(VkCommandBuffer cmd) const;
    // Bind this mesh's block and draw its range in one call so the block bind
    // and firstIndex/vertexOffset can never desync. firstInstance carries drawId
    // for the per-draw SSBO lookup (PART3c).
    void bindAndDraw(VkCommandBuffer cmd, uint32_t instanceCount = 1,
                     uint32_t firstInstance = 0) const;

    uint32_t indexCount() const { return indexCount_; }
    uint32_t firstIndex() const { return firstIndex_; }
    int32_t  vertexOffset() const { return vertexOffset_; }
    uint32_t blockIndex() const { return blockIndex_; }

   private:
    const VulkanContext* ctx_ = nullptr;
    GeometryBuffer* geom_ = nullptr;
    uint32_t firstIndex_ = 0;
    int32_t  vertexOffset_ = 0;
    uint32_t blockIndex_ = 0;
    uint32_t indexCount_ = 0;
};

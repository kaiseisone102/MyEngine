// include/MyEngine/renderer/mesh.h
#pragma once
// =============================================================================
// Mesh — Phase 2 段階A
// =============================================================================
// Vertex 拡張:
//   - jointIndices (ivec4, location=4) : 影響を受ける最大4ボーンのインデックス
//   - jointWeights (vec4,  location=5) : 各ボーンの重み (合計1.0)
//
// cube (Mesh) はスケルタルアニメ非対応なので jointIndices=0, weights=0 のまま。
// knight (Model) は ModelLoader が aiMesh::mBones から値を埋める (段階B)。
// =============================================================================

#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>
#include "renderer/vk_unique.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <string>
#include <vector>

class VulkanContext;
class ResourceFactory;
class GeometryBuffer;

struct Vertex {
    glm::vec3  pos;                              // location=0
    glm::vec3  color;                            // location=1
    glm::vec2  texCoord;                         // location=2
    glm::vec3  normal;                           // location=3
    glm::ivec4 jointIndices = {0, 0, 0, 0};      // location=4
    glm::vec4  jointWeights = {0.f, 0.f, 0.f, 0.f}; // location=5

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
        // joint 情報は Mesh (cube) では全部 0 で同一値のため、hash には入れなくても
        // 衝突は増えない。しかし正しさ優先で入れておく。
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
    // Phase 2B PART3a: if `geom` is given, the geometry is uploaded into the shared
    // GeometryBuffer megabuffer (returns a MeshHandle range) instead of a private
    // vertex/index buffer. If `geom` is null, the legacy private-buffer path is
    // used (kept during migration; removed once all meshes are on the megabuffer).
    void loadFromObj(const VulkanContext* ctx, const ResourceFactory* resources,
                     const std::string& path, GeometryBuffer* geom = nullptr);
    void createCube(const VulkanContext* ctx, const ResourceFactory* resources,
                    GeometryBuffer* geom = nullptr);
    void createCrossQuad(const VulkanContext* ctx, const ResourceFactory* resources,
                         GeometryBuffer* geom = nullptr);

    // Free GPU buffers now. Kept for explicit shutdown ordering; the auto
    // destructor (VkUnique members) frees too, so calling twice is a no-op.
    void destroy();
    void bind(VkCommandBuffer cmd) const;  // hybrid: megabuffer bind if on geom, else private
    // PART3b: bind this mesh's block + draw its range in one call so the block
    // bind and firstIndex/vertexOffset can never desync (structural fix for the
    // PART3a missed-bind device-lost). PART3c will pass firstInstance = drawId.
    void bindAndDraw(VkCommandBuffer cmd, uint32_t instanceCount = 1,
                     uint32_t firstInstance = 0) const;
    uint32_t indexCount() const { return indexCount_; }
    // Phase 2B PART3a: megabuffer range. When on the GeometryBuffer these are the
    // handle's offsets; on the legacy private path both are 0 (draw from buffer start).
    uint32_t firstIndex() const { return firstIndex_; }
    int32_t vertexOffset() const { return vertexOffset_; }
    uint32_t blockIndex() const { return blockIndex_; }
    bool onGeometryBuffer() const { return geom_ != nullptr; }

   private:
    const VulkanContext* ctx_ = nullptr;

    VkUnique<VkBuffer> vertexBuffer_;
    VkUnique<VkDeviceMemory> vertexBufferMemory_;
    VkUnique<VkBuffer> indexBuffer_;
    VkUnique<VkDeviceMemory> indexBufferMemory_;
    uint32_t indexCount_ = 0;

    // Phase 2B PART3a: when uploaded into the shared GeometryBuffer, geom_ is set
    // and firstIndex_/vertexOffset_ locate this mesh inside the megabuffers. The
    // private VkUnique buffers above stay empty in that case.
    GeometryBuffer* geom_ = nullptr;
    uint32_t firstIndex_ = 0;
    int32_t vertexOffset_ = 0;
    uint32_t blockIndex_ = 0;

    void uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkUnique<VkBuffer>& buffer,
                      VkUnique<VkDeviceMemory>& memory) const;
};

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

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <string>
#include <vector>

class VulkanContext;
class ResourceFactory;

struct Vertex {
    glm::vec3 pos;                                  // location=0
    glm::vec3 color;                                // location=1
    glm::vec2 texCoord;                             // location=2
    glm::vec3 normal;                               // location=3
    glm::ivec4 jointIndices = {0, 0, 0, 0};         // location=4
    glm::vec4 jointWeights = {0.f, 0.f, 0.f, 0.f};  // location=5

    bool operator==(const Vertex& o) const {
        return pos == o.pos && color == o.color && texCoord == o.texCoord && normal == o.normal &&
               jointIndices == o.jointIndices && jointWeights == o.jointWeights;
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
    void loadFromObj(const VulkanContext* ctx, const ResourceFactory* resources,
                     const std::string& path);
    void destroy();
    void bind(VkCommandBuffer cmd) const;
    uint32_t indexCount() const { return indexCount_; }

   private:
    const VulkanContext* ctx_ = nullptr;

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    uint32_t indexCount_ = 0;

    void uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) const;
};

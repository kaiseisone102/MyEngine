// include/MyEngine/renderer/model.h
#pragma once
// =============================================================================
// model.h — + localAABB (頂点全走査で計算、 衝突判定用)
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "core/aabb.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/vk_unique.h"
#include "renderer/skeleton.h"
#include "renderer/texture.h"

class VulkanContext;
class ResourceFactory;
class AssetRegistry;

struct SubMesh {
    VkUnique<VkBuffer> vertexBuffer;
    VkUnique<VkDeviceMemory> vertexBufferMemory;
    VkUnique<VkBuffer> indexBuffer;
    VkUnique<VkDeviceMemory> indexBufferMemory;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;

    void bind(VkCommandBuffer cmd) const;
};

class Model {
   public:
    void destroy();

    const std::vector<SubMesh>& subMeshes() const { return subMeshes_; }
    std::vector<SubMesh>& subMeshes() { return subMeshes_; }

    const std::vector<Material>& materials() const { return materials_; }
    std::vector<Material>& materials() { return materials_; }

    const std::vector<Texture>& textures() const { return textures_; }
    std::vector<Texture>& textures() { return textures_; }

    const Skeleton& skeleton() const { return skeleton_; }
    Skeleton& skeleton() { return skeleton_; }

    bool empty() const { return subMeshes_.empty(); }
    bool hasSkeleton() const { return !skeleton_.empty(); }

    // ─── ローカル座標系の AABB (頂点ロード時に ModelLoader が計算) ────
    // CTransform で scale + yaw + translate を適用すればワールド AABB が得られる。
    // 衝突判定用に CObstacle に保存され、 物理側で transform 適用される。
    const AABB& localAABB() const { return localAABB_; }

   private:
    friend class ModelLoader;

    const VulkanContext* ctx_ = nullptr;
    std::vector<SubMesh> subMeshes_;
    std::vector<Material> materials_;
    std::vector<Texture> textures_;
    Skeleton skeleton_;
    AABB localAABB_{};  // ModelLoader が埋める
};

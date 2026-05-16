// include/MyEngine/renderer/model.h
#pragma once
// =============================================================================
// model.h — Assimp で読み込んだ複数サブメッシュのモデル
// =============================================================================
// 役割:
//   - 複数の SubMesh を所有する。各サブメッシュは独立した頂点/インデックスバッファ。
//   - サブメッシュ単位で描画コマンドを発行できるようにする (将来のマテリアル対応)。
//
// Mesh との違い:
//   - Mesh: OBJ 1ファイル = 1メッシュ前提。1つのバッファで全部描画。
//   - Model: glTF/FBX を想定。1ファイル = 複数サブメッシュ。各バッファ別。
//
// 共有:
//   - 頂点フォーマットは Vertex (mesh.h で定義) を流用。
//     これにより既存シェーダー / Pipeline の VertexInput 定義をそのまま使える。
//
// 設計判断:
//   - 段階Aでは型と destroy() のみ。ロード/転送は段階B-C。
//   - SubMesh は Model のメンバ public で公開。Model は単なるコンテナ。
//   - materialIndex は今は未使用 (将来 1-D で使う)。
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "renderer/mesh.h"  // Vertex 構造体を共有

class VulkanContext;
class ResourceFactory;

// 1 つのサブメッシュ = 1 つの頂点バッファ + 1 つのインデックスバッファ
struct SubMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;

    // 将来 (Phase 1-D): マテリアル配列のインデックス。今は 0 固定。
    uint32_t materialIndex = 0;

    // コマンドバッファに頂点・インデックスバッファを bind するヘルパ。
    // Mesh::bind と同じ役割を SubMesh 単位で持つ。
    void bind(VkCommandBuffer cmd) const;
};

class Model {
   public:
    // 段階Aでは未実装。段階B-C で Assimp から呼ばれる。
    // void loadFromFile(const VulkanContext* ctx, const ResourceFactory* resources,
    //                   const std::string& path);

    // 全サブメッシュの GPU リソースを破棄する。
    // Mesh::destroy と同じ規約 (明示的に呼ぶ)。
    void destroy();

    const std::vector<SubMesh>& subMeshes() const { return subMeshes_; }
    std::vector<SubMesh>& subMeshes() { return subMeshes_; }

    bool empty() const { return subMeshes_.empty(); }

   private:
    friend class ModelLoader;  // ModelLoader が直接 subMeshes_ や ctx_ に書ける

    const VulkanContext* ctx_ = nullptr;
    std::vector<SubMesh> subMeshes_;
};

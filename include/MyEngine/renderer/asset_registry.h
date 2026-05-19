// include/MyEngine/renderer/asset_registry.h
#pragma once
// =============================================================================
// asset_registry.h — Phase 3 段階 (textures_/materials_ レジストリ追加)
// =============================================================================
// Phase 2 段階G-1 までの内容:
//   models_           : 名前でアクセスできる複数 Model のレジストリ
//   animationLibrary_ : 名前でアクセスできる AnimationClip のレジストリ
//
// Phase 3 段階 (本 .h) で追加:
//   textures_  : 名前付きテクスチャのレジストリ (stone_wall / wood_floor 等)
//   materials_ : 名前付きマテリアルのレジストリ (DescriptorSet 持ち)
//
// 使い方:
//   assets.registerTexture("stone_wall", "textures/stone_wall.png");
//   assets.registerMaterial("stone_wall", "stone_wall");
//   const Material* mat = assets.getMaterial("stone_wall");
// =============================================================================

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "renderer/animation.h"
#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/texture.h"

class VulkanContext;
class ResourceFactory;

class AssetRegistry {
   public:
    void init(VulkanContext* ctx, ResourceFactory* resources, const std::string& assetDir);
    void shutdown();

    // ─── Phase 2 段階G-1 API ─────────────────────────────────
    bool registerModel(const std::string& name, const std::string& path);
    bool registerAnimation(const std::string& name, const std::string& path);
    const Model* getModel(const std::string& name) const;
    const AnimationClip* getAnimation(const std::string& name) const;

    // ─── テクスチャ/マテリアル レジストリ (Phase 3 段階) ─────
    // 任意のテクスチャ画像をロード/登録。 ファイル無ければチェッカーボード fallback。
    //   name: レジストリキー (例: "stone_wall")
    //   path: 画像ファイルパス (assetDir 相対、 例: "textures/stone_wall.png")
    // 戻り値: 成功時 true (チェッカーボード fallback でも true)。
    bool registerTexture(const std::string& name, const std::string& path);

    // 名前付きテクスチャに対応する Material (DescriptorSet を持つ) を登録。
    //   name:        Material のレジストリキー (textureName と同じでも良い)
    //   textureName: 既に registerTexture で登録済みのテクスチャ名
    // 戻り値: 成功時 true (texture が見つからない場合 false)。
    bool registerMaterial(const std::string& name, const std::string& textureName);

    // 名前で参照。 見つからなければ nullptr。
    const Texture* getTexture(const std::string& name) const;
    const Material* getMaterial(const std::string& name) const;

    // ─── 既存API (互換性のため残す) ──────────────────────────
    bool loadModelFromFile(const std::string& path);
    const Model* activeModel() const;

    // ─── デフォルトリソース ───────────────────────────────────
    const Mesh& defaultMesh() const { return defaultMesh_; }
    const Texture& defaultTexture() const { return defaultTexture_; }
    const Material& defaultMaterial() const { return defaultMaterial_; }
    VkDescriptorPool materialPool() const { return materialPool_; }
    VkDescriptorSetLayout materialSetLayout() const { return materialSetLayout_; }

   private:
    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    std::string assetDir_;

    Mesh defaultMesh_;
    Texture defaultTexture_;
    VkDescriptorPool materialPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
    Material defaultMaterial_;

    // ─── 段階G-1 ────────────────────────────────────────────
    std::unordered_map<std::string, std::unique_ptr<Model>> models_;
    std::unordered_map<std::string, std::vector<AnimationClip>> animationLibrary_;

    // Phase 3 段階: 名前付きテクスチャ/マテリアルのレジストリ
    // Texture と Material は VkImage/VkDescriptorSet を持つので unique_ptr で管理。
    // map のリハッシュで本体がコピー/ムーブされないようにする。
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;

    // 互換性: 最初にloadModelFromFileされたモデルの名前
    std::string activeModelName_;

    void createDefaultMesh();
    void createDefaultTexture();
    void createMaterialDescriptorPool();
    void createMaterialSetLayout();
    void createDefaultMaterial();
};

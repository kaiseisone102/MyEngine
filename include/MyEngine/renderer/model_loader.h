// include/MyEngine/renderer/model_loader.h
#pragma once
// =============================================================================
// model_loader.h — Phase 2 段階G-1
// =============================================================================
// load(): メッシュ + マテリアル + スケルトン + アニメ込みでフルロード
//          (アニメは戻り値の outAnimations に書き出す)
// loadAnimationsOnly(): メッシュ無視でアニメだけ抽出
//          (Mixamo の "Without Skin" でダウンロードした walk.glb 等に使う)
// =============================================================================

#include <string>
#include <vector>

#include "renderer/animation.h"
#include "renderer/model.h"

class VulkanContext;
class ResourceFactory;
class AssetRegistry;

class ModelLoader {
   public:
    static bool probe(const std::string& path);

    // フルロード: メッシュ + マテリアル + スケルトン + アニメ
    // - outModel        : メッシュ・マテリアル・スケルトンが入る
    // - outAnimations   : このファイルから抽出したアニメクリップ群
    // 戻り値: ロード成功かどうか
    static bool load(const VulkanContext* ctx, const ResourceFactory* resources,
                     AssetRegistry& assets, const std::string& path,
                     Model& outModel, std::vector<AnimationClip>& outAnimations);

    // アニメだけロード: メッシュ・マテリアル・テクスチャは無視
    // 戻り値: 成功時 true (clips に1個以上アニメが入る)
    static bool loadAnimationsOnly(const std::string& path,
                                   std::vector<AnimationClip>& outAnimations);
};

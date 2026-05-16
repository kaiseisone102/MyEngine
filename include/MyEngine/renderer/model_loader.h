// include/MyEngine/renderer/model_loader.h
#pragma once
// =============================================================================
// model_loader.h — Assimp による 3D モデル読み込み
// =============================================================================
// 提供する API:
//   - probe(path)           : 診断用。コンソールに頂点数等を表示。Phase 1-B 名残。
//   - load(ctx, res, path)  : 本番用。Assimp で読んで Model に詰めて返す。
// =============================================================================

#include <string>

#include "renderer/model.h"

class VulkanContext;
class ResourceFactory;

class ModelLoader {
   public:
    // 診断用 (Phase 1-B)。コンソールに概要を表示するだけ。
    static bool probe(const std::string& path);

    // 本番ロード API (Phase 1 段階B-C)。
    //   - Assimp でファイルを読み、各 aiMesh を SubMesh に変換
    //   - GPU バッファ (VertexBuffer/IndexBuffer) も作成
    //   - 失敗時は空の Model を返す (Model::empty() == true)
    //
    // 注: テクスチャ/マテリアルは段階Bでは扱わない。Phase 1-D で対応。
    static Model load(const VulkanContext* ctx, const ResourceFactory* resources,
                      const std::string& path);
};

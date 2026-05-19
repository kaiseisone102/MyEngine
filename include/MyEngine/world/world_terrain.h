#pragma once
// =============================================================================
// world_terrain.h — 複数 TerrainMesh の集約クラス
// =============================================================================
// 1 つのステージに 1 つの TerrainMesh だった既存設計を拡張し、 「複数の
// 独立した terrain (= 浮島・台地・大陸など)」 を扱えるようにする集約クラス。
//
// 設計:
//   - 各 TerrainMesh は独立した XZ 範囲 + 独立した heightMap/normalMap/material を持つ
//   - sampleHeight(x, z): 全 mesh の中で「player の XZ を含む」 ものを線形試行し、
//     一番高い y を返す。 どれにも含まれなければ lowest を返す (= 落下)
//   - sampleNormal(x, z): 同じく一番高い mesh の法線を返す
//   - 描画は meshes() でイテレーションして各 mesh を個別に描画 (per-mesh material)
//
// 「単一島 = 既存挙動」 も「複数島 = 新規」 も、 同じ API で扱える。
// =============================================================================

#include <memory>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "renderer/terrain_mesh.h"

class VulkanContext;

class WorldTerrain {
   public:
    // mesh を所有権ごと追加。 既に init 済みである必要あり。
    void add(std::unique_ptr<TerrainMesh> mesh) {
        if (mesh) meshes_.push_back(std::move(mesh));
    }

    // 全 mesh を destroy して配列をクリア。
    // 注意: GPU が VkBuffer を使用中に destroy するとクラッシュするので、
    // 呼び出し側で vkDeviceWaitIdle を済ませてから呼ぶこと。
    void clear() {
        for (auto& m : meshes_) {
            if (m) m->destroy();
        }
        meshes_.clear();
    }

    // 全 mesh の中から「(x, z) が範囲内」 で「最高 y」 を返す。
    // どこにも含まれなければ std::numeric_limits<float>::lowest() を返す。
    float sampleHeight(float worldX, float worldZ) const;

    // 最高 y をもつ mesh の法線を返す。 どこにも含まれなければ (0, 1, 0)。
    glm::vec3 sampleNormal(float worldX, float worldZ) const;

    // (x, z) を含む mesh が 1 つでも存在するか
    bool isInsideAny(float worldX, float worldZ) const;

    // 描画用イテレーション
    const std::vector<std::unique_ptr<TerrainMesh>>& meshes() const { return meshes_; }
    bool empty() const { return meshes_.empty(); }
    size_t size() const { return meshes_.size(); }

   private:
    std::vector<std::unique_ptr<TerrainMesh>> meshes_;
};

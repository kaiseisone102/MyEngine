#pragma once
// =============================================================================
// world_water.h — 複数 WaterMesh の集約クラス (WorldTerrain と同じパターン)
// =============================================================================
// Stage 1 つに複数の水たまりを持てる。 各 WaterMesh は独立した位置・サイズを
// 持つ単純な所有コンテナ (spatial 判定なし、 物理判定なし、 描画のみ)。
// =============================================================================

#include <memory>
#include <vector>

#include "renderer/water_mesh.h"

class WorldWater {
   public:
    void add(std::unique_ptr<WaterMesh> mesh) {
        if (mesh) meshes_.push_back(std::move(mesh));
    }

    // 全 mesh を destroy して配列をクリア。
    // 注意: GPU 使用中の destroy はクラッシュするので、 呼び出し側で
    // vkDeviceWaitIdle を済ませてから呼ぶこと (WorldTerrain と同じ規則)。
    void clear() {
        for (auto& m : meshes_) {
            if (m) m->destroy();
        }
        meshes_.clear();
    }

    const std::vector<std::unique_ptr<WaterMesh>>& meshes() const { return meshes_; }
    bool empty() const { return meshes_.empty(); }
    size_t size() const { return meshes_.size(); }

   private:
    std::vector<std::unique_ptr<WaterMesh>> meshes_;
};

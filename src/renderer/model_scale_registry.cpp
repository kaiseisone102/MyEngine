// =============================================================================
// model_scale_registry.cpp — モデル別スケールの定義
// =============================================================================
// equippedScale が (0,0,0) のときは「装備時にも defaultScale を使う」 と解釈。
// 装備しないモデル (grave/rock/tree 等) は (0,0,0) のままで OK。
// =============================================================================
#include "renderer/model_scale_registry.h"

#include <iostream>
#include <unordered_map>

namespace model_scale {

namespace {

struct Entry {
    glm::vec3 defaultScale;
    glm::vec3 equippedScale;  // (0,0,0) = 未指定 → defaultScale を使う
};

const std::unordered_map<std::string, Entry>& table() {
    static const std::unordered_map<std::string, Entry> kTable = {
        // ─── 盾 ──────────────────────────────────────────────
        // 床に置いてあるとき: 見つけやすく少し大きめ
        // 装備時: 手のサイズに合わせて小さめ
        {"shield_iron", {{1.f, 1.f, 1.f}, {100.f, 100.f, 100.f}}},
        {"shield_silver", {{1.f, 1.f, 0.30f}, {100.f, 100.f, 100.f}}},
        {"shield_gold", {{1.f, 1.f, 1.f}, {100.f, 100.f, 100.f}}},

        // ─── 武器 ────────────────────────────────────────────
        // 主に装備用、 デフォルト 1.0 で十分 (床配置の用途は今はない)
        {"sword_std", {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}},

        // ─── 装飾オブジェクト ─────────────────────────────────
        // 装備されない (equippedScale は (0,0,0) のまま、 ここに来ることもない想定)
        // 実物大が分からないので、 まず 1.0f で配置 → 必要に応じて調整
        {"grave_1", {{1.0f, 1.0f, 1.0f}, {0.f, 0.f, 0.f}}},
        {"grave_spirit", {{1.0f, 1.0f, 1.0f}, {0.f, 0.f, 0.f}}},
        {"rock_1", {{1.0f, 1.0f, 1.0f}, {0.f, 0.f, 0.f}}},
        {"tree_noLeaves_1", {{1.0f, 1.0f, 1.0f}, {0.f, 0.f, 0.f}}},
        {"tree_noLeaves_2", {{1.0f, 1.0f, 1.0f}, {0.f, 0.f, 0.f}}},
    };
    return kTable;
}

bool isZero(const glm::vec3& v) { return v.x == 0.f && v.y == 0.f && v.z == 0.f; }

}  // namespace

glm::vec3 get(const std::string& modelName, Context ctx) {
    const auto& t = table();
    auto it = t.find(modelName);
    if (it == t.end()) {
        std::cerr << "[ModelScale] WARNING: not registered: '" << modelName << "', using {1,1,1}\n";
        return glm::vec3{1.f, 1.f, 1.f};
    }
    const Entry& entry = it->second;
    if (ctx == Context::Equipped) {
        // 装備用が未指定 (0,0,0) のときは defaultScale にフォールバック
        return isZero(entry.equippedScale) ? entry.defaultScale : entry.equippedScale;
    }
    return entry.defaultScale;
}

}  // namespace model_scale

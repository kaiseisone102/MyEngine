#pragma once
// =============================================================================
// model_scale_registry.h — モデル名 + 用途別の基準スケール管理
// =============================================================================
// 同じモデルでも「床に置いてある状態」 と「プレイヤーが装備した状態」 では
// 適切なサイズが違う場合がある。 例: shield_iron は床ではちょっと大きめ
// (見つけやすい)、 装備時は手のサイズに合わせて小さめ。
//
// 使い方:
//   const glm::vec3 s = model_scale::get("shield_iron",
//                                         model_scale::Context::Default);
//   transform.scale = s;
//
// 新モデル追加:
//   src/renderer/model_scale_registry.cpp の kModelScales に行を追加するだけ。
// =============================================================================

#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace model_scale {

enum class Context {
    Default,     // 床・空間に配置 (装飾、 アイテム表示)
    Equipped,    // プレイヤー or 敵が手に装備
};

// 未登録モデルの場合: warning ログ + {1,1,1} を返す。
glm::vec3 get(const std::string& modelName, Context ctx = Context::Default);

}  // namespace model_scale

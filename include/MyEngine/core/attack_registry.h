#pragma once
// =============================================================================
// attack_registry.h — 攻撃モーションの定義テーブル
// =============================================================================
// AttackKind 毎の AttackDef を集中管理する。
//
// 新しい攻撃モーションを追加するには:
//   1. attack_def.h の AttackKind に追加
//   2. attack_registry.cpp の kAttackDefs に行を追加
// =============================================================================

#include "core/attack_def.h"

namespace attack_registry {

// 指定 AttackKind の定義を取得 (見つからない場合は Slash にフォールバック)。
const AttackDef& get(AttackKind kind);

}  // namespace attack_registry

// src/scene/scene_data.cpp
// =============================================================================
// Phase 1C: toLightingData は廃止。
// LightingUBO の構築は camera_system が CPU 側で直接行い、 VulkanRenderer::setLighting
// 経由で渡す設計に変更。
// SceneData はもう lighting/camera の情報を持たない、 純粋な DrawList コンテナ。
// =============================================================================
#include "scene/scene_data.h"

// 全実装はヘッダー側にインライン化されている (getter/setter のみのため)。
// 旧 toLightingData の実装は削除済み。

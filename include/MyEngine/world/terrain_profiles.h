#pragma once
// =============================================================================
// terrain_profiles.h — ステージごとの高さ関数集
// =============================================================================
// TerrainMesh::HeightFunc に渡せる「高さ関数」 を集めたヘッダ。
// 各関数は (worldX, worldZ) を受け取って高さ (m) を返す純関数。
//
// 設計:
//   - sin/cos の合成で perlin noise と同等の「滑らかな起伏」 を表現
//   - 外部ライブラリ依存なし (std::sin / std::cos のみ)
//   - 各プロファイルは大・中・細の 3 段スケールで自然な見た目を出す
//   - ステージごとに `addTerrain(data, ..., terrain_profile::xxx)` で使い分け
//
// 新しいプロファイル追加手順:
//   1. ここに inline float xxx(float x, float z) を追加
//   2. stage_registry.cpp の対応ステージで addTerrain の第 4 引数に渡す
// =============================================================================

#include <cmath>

namespace terrain_profile {

// ─── flat: 完全に平ら ─────────────────────────────────────
// Terminal のような屋内的・整地済みステージで使う。
inline float flat(float, float) { return 0.f; }

// ─── rollingHills: 緩やかな丘陵 (±1m 控えめ) ─────────────
// player の cube ground (平らな板) と最大 1m 程度のズレで済む控えめ起伏。
// 3 段スケール合成で「自然な草原のうねり」 を表現。
//   大スケール: 波長 ~30m、 振幅 ±0.6m  (大きなうねり)
//   中スケール: 波長 ~10m、 振幅 ±0.3m  (中間の起伏)
//   細スケール: 波長 ~3m 、 振幅 ±0.1m  (細かい凹凸)
//   合計: 概ね ±1m に収まる
inline float rollingHills(float x, float z) {
    const float h1 = std::sin(x * 0.21f) * std::cos(z * 0.18f) * 0.6f;
    const float h2 = std::sin(x * 0.60f + 1.3f) * std::sin(z * 0.55f + 0.7f) * 0.3f;
    const float h3 = std::sin(x * 1.90f) * std::cos(z * 2.10f) * 0.1f;
    return h1 + h2 + h3;
}

// ─── stage1_1_terrain: Stage 1-1 用 (起伏 + 急な丘 + くぼみ) ─
// rollingHills をベースに、 特定の位置に「mesa (テーブルマウンテン)」 と
// くぼみを追加する。
//
// 急な丘 (mesa): 中心 (15, 0, 15)
//   - 頂上半径 2m: 平らな高さ +2m
//   - 急斜面 半径 2m〜3.5m: smoothstep で 2m → 0m に補間
//     最大勾配 = 高さ 2m / 幅 1.5m × 1.5 (smoothstep 係数) = 2.0 → 約 63°
//     45° 閾値より確実に急で、 movement_system が登れない判定をする。
//   - 半径 3.5m 以上: 平地 (高さ 0)
//
// くぼみ: 中心 (-12, -12)、 σ=7m、 深さ 3m
//   勾配は 3/7 ≈ 0.43 (23°) 程度、 普通に降りられる。
inline float stage1_1_terrain(float x, float z) {
    // ベース: 緩やかな丘陵 (±1m 程度)
    float h = rollingHills(x, z);

    // 急な丘 (mesa 形状、 登れない想定)
    {
        const float dx = x - 15.f;
        const float dz = z - 15.f;
        const float r = std::sqrt(dx * dx + dz * dz);
        constexpr float innerR = 2.0f;  // 平らな頂上の半径
        constexpr float outerR = 3.5f;  // 急斜面の外端
        constexpr float height = 2.0f;
        if (r < innerR) {
            h += height;
        } else if (r < outerR) {
            // smoothstep で innerR (high) → outerR (low) に補間
            const float t = (r - innerR) / (outerR - innerR);  // 0..1
            const float s = t * t * (3.f - 2.f * t);           // smoothstep
            h += height * (1.f - s);
        }
        // r >= outerR: 加算なし (平地に戻る)
    }

    // くぼみ (降りられる想定、 ガウス関数)
    {
        const float dx = x - (-12.f);
        const float dz = z - (-12.f);
        const float r2 = dx * dx + dz * dz;
        constexpr float sigma = 7.f;
        constexpr float amp = 3.f;
        h -= amp * std::exp(-r2 / (sigma * sigma));
    }

    return h;
}

// (将来追加候補)
// - mountainous: 山岳地形 (±5m、 急斜面)
// - meadow:      平原 (±0.3m、 ほぼ平ら)
// - canyon:      谷地形 (X 方向に長い谷)
// - islands:     海面 + 小島群

}  // namespace terrain_profile

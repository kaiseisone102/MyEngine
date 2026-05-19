#pragma once
// =============================================================================
// core/particle.h — パーティクル基本データ型
// =============================================================================
// 設計:
//   Particle           : 個別パーティクルの全状態 (CPU 側で物理シミュレート)
//   ParticleInstance   : GPU に送る描画用 instance データ (instance rate 頂点入力)
//   EmitterShape       : 発生形状の enum
//   BlendMode          : 描画ブレンドモード
//
// パーティクルシステム全体の流れ:
//   1. CParticleEmitter (ECS コンポーネント) を持つ entity から ParticleSystem
//      が emit する → Particle Pool に追加
//   2. 毎フレーム ParticleSystem::update が全 Particle の age, vel, pos を進める
//   3. レンダリング: ParticlePass が alive な Particle を ParticleInstance に
//      変換 → instance buffer に upload → GPU Instancing で描画
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstdint>
#include <glm/glm.hpp>

namespace particle {

// 発生形状
enum class EmitterShape : uint8_t {
    Point,       // 1 点から発生 (shapeParams 未使用)
    Sphere,      // shapeParams.x = radius (球内ランダム)
    Line,        // shapeParams = 線分の終点 (始点は emitter 位置、 線上ランダム)
    Cone,        // shapeParams.x = 半径、 .y = 高さ (上向き円錐)
    Box,         // shapeParams = half extents (箱内ランダム)
};

// ブレンドモード
enum class BlendMode : uint8_t {
    Alpha,       // 通常半透明 (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
    Additive,    // 加算合成 (SRC_ALPHA, ONE) — 炎・光に最適
};

// 個別パーティクルの全状態。 Pool で管理される。
// CPU 側で物理シミュレートし、 GPU 送信時に ParticleInstance に変換する。
struct Particle {
    glm::vec3 pos{0.f};
    float age = 0.f;            // 0..lifetime
    glm::vec3 vel{0.f};         // m/s
    float lifetime = 1.f;       // 寿命 (秒)
    glm::vec4 colorStart{1.f};
    glm::vec4 colorEnd{1.f, 1.f, 1.f, 0.f};
    glm::vec3 gravity{0.f};     // m/s^2 (上向き正)
    float sizeStart = 0.1f;
    glm::vec3 _pad{0.f};
    float sizeEnd = 0.3f;
    float drag = 0.f;
    BlendMode blendMode = BlendMode::Additive;
    bool alive = false;

    // age01 = age / lifetime (0..1)、 補間用
    float age01() const {
        return (lifetime > 0.f) ? (age / lifetime) : 0.f;
    }
};

// GPU 送信用 instance データ。
// Vertex Shader で instance rate attribute として読まれる。
// レイアウトはシェーダーと一致する必要がある (location 1..4)。
struct ParticleInstance {
    glm::vec3 pos;       // location=1
    float size;          // location=2
    glm::vec4 color;     // location=3 (補間済み)
    float age01;         // location=4
    float _pad[3];       // 16 byte align
};
static_assert(sizeof(ParticleInstance) == 48,
              "ParticleInstance size mismatch (must match shader layout)");

}  // namespace particle

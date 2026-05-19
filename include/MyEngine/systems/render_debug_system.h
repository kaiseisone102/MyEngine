#pragma once

#include <glm/glm.hpp>

#include <string>

#include "core/components.h"
#include "core/grip.h"

struct DebugOverlayData {
    int entityCount = 0;
    float fps = 0.f;
    glm::vec3 playerPos{0.f};
    float velY = 0.f;
    bool onGround = false;
    bool isTps = true;
    bool attackActive = false;
    float attackTime = 0.f;
    // HP 関連
    int hpSegments = 1;
    int hpUnlocked = 1;
    int hpCurrentHp = 3;
    float invincTimer = 0.f;
    // 盾関連
    ShieldType shieldType = ShieldType::None;
    int shieldDurability = 0;
    // Grip 関連
    GripType gripType = GripType::None;
    int gripDurability = 0;
    int gripMaxDurability = 0;
    // 鍵カウント
    int goldKeys = 0;
    int silverKeys = 0;
    // お金
    int money = 0;
    // SkinBufferPool
    int skinnedAllocated = 0;
    int skinnedCapacity = 128;

    // ─── キーバインド ───────────────────────────────────────
    std::string moveKeysLabel        = "W/A/S/D";
    std::string jumpKeyLabel         = "Space";
    std::string attackKeyLabel       = "LMB";
    std::string strongAttackKeyLabel = "RMB";
    std::string toggleCameraKeyLabel = "Tab";

    // ─── Layer 情報 ──────────────────────────────────────────
    std::string topLayerName = "Gameplay";
    int layerStackDepth = 1;
};

class RenderDebugSystem {
   public:
    enum class Corner {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    void draw(const DebugOverlayData& d, Corner corner = Corner::TopLeft) const;
};

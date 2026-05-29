#include "systems/render_debug_system.h"

#include <imgui.h>

namespace {

const char* gripTypeName(GripType t) {
    switch (t) {
        case GripType::None:  return "None";
        case GripType::Fire:  return "Fire";
        case GripType::Light: return "Light";
    }
    return "?";
}

ImVec4 gripUiColor(GripType t) {
    switch (t) {
        case GripType::Fire:  return ImVec4{1.0f, 0.30f, 0.20f, 1.0f};
        case GripType::Light: return ImVec4{1.0f, 0.95f, 0.30f, 1.0f};
        case GripType::None:
        default:              return ImVec4{0.6f, 0.6f, 0.6f, 1.0f};
    }
}

}  // namespace

void RenderDebugSystem::draw(const DebugOverlayData& d, Corner corner) const {
    const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    constexpr float kMargin = 10.f;

    ImVec2 pos{};
    ImVec2 pivot{};
    switch (corner) {
        case Corner::TopLeft:
            pos = {kMargin, kMargin};
            pivot = {0.f, 0.f};
            break;
        case Corner::TopRight:
            pos = {viewportSize.x - kMargin, kMargin};
            pivot = {1.f, 0.f};
            break;
        case Corner::BottomLeft:
            pos = {kMargin, viewportSize.y - kMargin};
            pivot = {0.f, 1.f};
            break;
        case Corner::BottomRight:
            pos = {viewportSize.x - kMargin, viewportSize.y - kMargin};
            pivot = {1.f, 1.f};
            break;
    }
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowSize({270.f, 0.f}, ImGuiCond_Always);
    ImGui::Begin("Debug", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored({0.4f, 0.8f, 1.f, 1.f}, "[ECS]");
    ImGui::SameLine();
    ImGui::Text("Entities: %d", d.entityCount);
    ImGui::Separator();

    ImGui::TextColored({1.f, 1.f, 0.f, 1.f}, "FPS  : %.1f", d.fps);

    {
        const float ratio = d.skinnedCapacity > 0 ? static_cast<float>(d.skinnedAllocated) /
                                                        static_cast<float>(d.skinnedCapacity)
                                                  : 0.f;
        ImVec4 col = {0.7f, 0.7f, 0.7f, 1.f};
        if (ratio > 0.5f) col = {1.f, 1.f, 0.4f, 1.f};
        if (ratio > 0.85f) col = {1.f, 0.4f, 0.3f, 1.f};
        ImGui::TextColored(col, "Skin : %d / %d", d.skinnedAllocated, d.skinnedCapacity);
    }
    {
        // Phase 1F: instanced culling stats
        const float iratio = d.instancedTotal > 0 ? static_cast<float>(d.instancedVisible) /
                                                         static_cast<float>(d.instancedTotal)
                                                  : 0.f;
        ImVec4 icol = {0.4f, 1.f, 0.6f, 1.f};
        ImGui::TextColored(icol, "Inst : %d / %d  (%.0f%%)", d.instancedVisible, d.instancedTotal, iratio * 100.f);
    }
    // Pure GPU-driven: the prop cull's visible count lives only on the GPU
    // (compactCmd is device-local; CPU never reads it back). No HUD line.
    ImGui::Separator();

    ImGui::Text("Pos  : (%.2f, %.2f, %.2f)", d.playerPos.x, d.playerPos.y, d.playerPos.z);
    ImGui::Text("VelY : %.2f  %s", d.velY,
                d.onGround ? "[Ground]" : (d.velY > 0.f ? "[Jump]" : "[Fall]"));

    ImGui::TextColored(d.isTps ? ImVec4{0.4f, 1.f, 0.4f, 1.f} : ImVec4{0.4f, 0.8f, 1.f, 1.f},
                       "Cam  : %s", d.isTps ? "TPS" : "FPS");
    ImGui::Text("Atk  : %s (%.2fs)", d.attackActive ? "Active" : "Idle", d.attackTime);

    ImGui::Separator();

    // ─── HP ────────────────────────────────────────────────
    const bool invincBlink =
        (d.invincTimer > 0.f) && (static_cast<int>(d.invincTimer * 6.f) % 2 == 0);
    const ImVec4 hpColor = invincBlink ? ImVec4{1.f, 0.6f, 0.6f, 1.f}
                                        : ImVec4{1.f, 0.3f, 0.3f, 1.f};
    ImGui::TextColored(hpColor, "HP   : %d/%d segs, %d/%d hp", d.hpSegments, d.hpUnlocked,
                       d.hpCurrentHp, CHealth::kSegmentSize);
    if (d.invincTimer > 0.f) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 1.f, 0.3f, 1.f}, " Invinc %.1fs", d.invincTimer);
    }

    // ─── 盾 ────────────────────────────────────────────────
    if (d.shieldType == ShieldType::None) {
        ImGui::TextColored({0.4f, 0.4f, 0.4f, 1.f}, "Shield: None");
    } else {
        ImVec4 col = {0.9f, 0.9f, 0.9f, 1.f};
        if (d.shieldType == ShieldType::Silver) col = {0.6f, 0.85f, 1.f, 1.f};
        if (d.shieldType == ShieldType::Gold)   col = {1.f, 0.82f, 0.1f, 1.f};

        ImGui::TextColored(col, "Shield: %s %d/%d", CShield::typeName(d.shieldType),
                           d.shieldDurability, CShield::maxDurability(d.shieldType));
    }

    // ─── Grip ──────────────────────────────────────────────
    if (d.gripType == GripType::None || d.gripMaxDurability <= 0) {
        ImGui::TextColored({0.4f, 0.4f, 0.4f, 1.f}, "Grip  : None");
    } else {
        ImGui::TextColored(gripUiColor(d.gripType), "Grip  : %s %d/%d",
                           gripTypeName(d.gripType),
                           d.gripDurability, d.gripMaxDurability);
    }

    // ─── 鍵カウント ──────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextColored({1.f, 0.82f, 0.1f, 1.f},  "Gold  : %d", d.goldKeys);
    ImGui::SameLine();
    ImGui::TextColored({0.7f, 0.85f, 1.f, 1.f},  "  Silver: %d", d.silverKeys);

    // ─── お金 ──────────────────────────────────────────────
    ImGui::TextColored({1.f, 0.85f, 0.2f, 1.f}, "Money : %d", d.money);

    // ─── Layer 情報 ──────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextColored({0.7f, 0.9f, 0.5f, 1.f}, "Top  : %s  (Depth=%d)", d.topLayerName.c_str(),
                       d.layerStackDepth);

    // ─── 操作キー ────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("%s: Move   %s: Jump", d.moveKeysLabel.c_str(), d.jumpKeyLabel.c_str());
    ImGui::TextDisabled("%s: Slash   %s: SmashDown", d.attackKeyLabel.c_str(),
                        d.strongAttackKeyLabel.c_str());
    ImGui::TextDisabled("%s: FPS/TPS   ESC: Pause/Menu", d.toggleCameraKeyLabel.c_str());
    ImGui::TextDisabled("F6:DbgPos  F7:DbgToggle  F8:Hitbox");
    ImGui::TextDisabled("F9:AnimDump  F10:PoolLog  F11:Clear  F12:Burst");

    ImGui::End();
}

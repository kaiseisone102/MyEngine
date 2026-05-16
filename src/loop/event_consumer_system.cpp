// C:\MyEngine\src\loop\event_consumer_system.cpp

#include "loop/event_consumer_system.h"

#include <iostream>

void EventConsumerSystem::consume(EventBus& bus, bool& running, bool& mouseCapture, Camera& camera,
                                  CameraSystem& cameraSystem, CombatSystem& combatSystem,
                                  AudioEventSystem& audioEventSystem, flecs::entity player,
                                  SoundManager& sound, VulkanRenderer& renderer,
                                  const std::function<void(bool)>& setMouseCapture) const {
    auto events = bus.drain();  // 薄い文字は inlay hints 気にしなくていい, auto:
                                // コンパイル時に型を推論させる(長い型を書かなくてよい)
    for (const GameEvent& ev : events) {
        if (std::holds_alternative<QuitRequested>(ev)) {
            running = false;
            continue;
        }
        if (std::holds_alternative<WindowResizeRequested>(ev)) {
            renderer.onResize();
            continue;
        }
        if (std::holds_alternative<ToggleMouseCaptureRequested>(ev)) {
            setMouseCapture(!mouseCapture);
            continue;
        }
        if (std::holds_alternative<CaptureMouseRequested>(ev)) {
            if (!mouseCapture) setMouseCapture(true);
            continue;
        }
        if (std::holds_alternative<ToggleCameraRequested>(
                ev)) {  // holds_alternative: ev が std::variant
                        // 型の値のとき、<指定された型>のイベントかどうかを判定
            cameraSystem.toggleMode(camera);
            std::cout << "[Camera] "  // cout: 標準出力ストリーム, <<: ストリームに出力
                      << (camera.mode == CameraMode::TPS ? "TPS" : "FPS") << " mode\n";  // \n: 改行
            continue;  // continue: 次のイベント(このforの中のイベント処理)に進む
        }
        if (std::holds_alternative<JumpRequested>(ev)) {
            if (!combatSystem.isInputLocked(player)) {
                player.ensure<CPhysics>().jumpReq = true;
            }
            audioEventSystem.onJumpRequested(player, combatSystem.isInputLocked(player), sound);
            continue;
        }
        if (std::holds_alternative<AttackRequested>(ev)) {
            combatSystem.requestAttack(player);
            continue;
        }
        if (std::holds_alternative<StrongAttackRequested>(ev)) {
            combatSystem.requestStrongAttack(player);
            continue;
        }
        if (const auto* look = std::get_if<MouseLookDelta>(
                &ev)) {  // get_if: ev が std::variant
                         // 型の値のとき、<指定された型>ならその値のポインタを返す、違えば
                         // nullptr を返す
            cameraSystem.applyMouseLook(camera, mouseCapture, look->xrel, look->yrel);
            continue;
        }
    }
}

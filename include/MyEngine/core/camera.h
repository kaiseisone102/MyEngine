#pragma once
// =============================================================================
// camera.h — ステップ9: FPS / TPS カメラ（マリオ64スタイル）
// =============================================================================
// CameraMode::FPS … 一人称視点。カメラ = プレイヤーの目。自由移動。
// CameraMode::TPS … 三人称視点。カメラ = プレイヤーの後ろ上空を追尾。
//
// Phase 5-F 変更:
//   playerPos が「Player の足元」 を表すようになったので、 注視点を
//   playerPos.y + 1.2 (Player の腰〜胸付近) に設定。
//   旧: playerPos.y + 0.7 (中心 0.5 + 0.7 = Y=1.2)
//   新: playerPos.y + 1.2 (足元 0.0 + 1.2 = Y=1.2、 同じ高さ)
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

enum class CameraMode {
    FPS,
    TPS
};

class Camera {
public:
    CameraMode mode        = CameraMode::TPS;
    float      fov         = 45.f;
    float      sensitivity = 0.15f;

    // ─── TPS 設定 ──────────────────────────────────────────────────────────
    float tpsDistance = 6.f;
    // Phase 5-F: 足元基準対応
    //   tpsHeight = カメラ位置のオフセット (Player の足元から上に何メートル)
    //   旧 1.4 のまま使用
    float tpsHeight   = 1.4f;
    float tpsYaw      = 0.f;
    float tpsPitch    = 20.f;

    // Phase 5-F: 注視点の Y オフセット (足元から上に何メートルを注視するか)
    //   Player の腰〜胸を注視。 旧の "tpsHeight * 0.5 = 0.7 + Player中央 0.5 = Y=1.2"
    //   と同じ高さを保つには、 足元基準 + 1.2 にする。
    float tpsLookAtHeight = 1.2f;

    // ─── FPS 設定 ──────────────────────────────────────────────────────────
    float     fpsYaw       = -90.f;
    float     fpsPitch     =   0.f;
    glm::vec3 fpsPosition  = {0.f, 1.6f, 5.f};
    float     fpsMoveSpeed =  5.f;

    void toggleMode() {
        mode = (mode == CameraMode::FPS) ? CameraMode::TPS : CameraMode::FPS;
    }

    void processMouseMovement(float dx, float dy) {
        if (mode == CameraMode::TPS) {
            tpsYaw   += dx * sensitivity;
            tpsPitch -= dy * sensitivity;
            if (tpsPitch > 75.f)  tpsPitch = 75.f;
            if (tpsPitch < -15.f) tpsPitch = -15.f;
        } else {
            fpsYaw   += dx * sensitivity;
            fpsPitch -= dy * sensitivity;
            if (fpsPitch >  89.f) fpsPitch =  89.f;
            if (fpsPitch < -89.f) fpsPitch = -89.f;
        }
    }

    void processFpsKeyboard(bool w, bool s, bool a, bool d,
                             bool up, bool down, float dt)
    {
        if (mode != CameraMode::FPS) return;
        const glm::vec3 front = fpsFront();
        const glm::vec3 right = glm::normalize(glm::cross(front, {0.f, 1.f, 0.f}));
        if (w)    fpsPosition += front * fpsMoveSpeed * dt;
        if (s)    fpsPosition -= front * fpsMoveSpeed * dt;
        if (d)    fpsPosition += right * fpsMoveSpeed * dt;
        if (a)    fpsPosition -= right * fpsMoveSpeed * dt;
        if (up)   fpsPosition.y += fpsMoveSpeed * dt;
        if (down) fpsPosition.y -= fpsMoveSpeed * dt;
    }

    glm::vec3 getTpsForward() const {
        return glm::normalize(glm::vec3{
            -std::sin(glm::radians(tpsYaw)),
             0.f,
            -std::cos(glm::radians(tpsYaw))
        });
    }

    glm::vec3 getTpsRight() const {
        return glm::normalize(glm::vec3{
             std::cos(glm::radians(tpsYaw)),
             0.f,
            -std::sin(glm::radians(tpsYaw))
        });
    }

    // playerPos: TPS では追跡するプレイヤーの座標 (足元)
    glm::mat4 getViewMatrix(const glm::vec3& playerPos = {0.f, 0.f, 0.f}) const {
        if (mode == CameraMode::TPS) {
            const float yawRad   = glm::radians(tpsYaw);
            const float pitchRad = glm::radians(tpsPitch);

            const glm::vec3 offset = {
                std::sin(yawRad) * std::cos(pitchRad) * tpsDistance,
                std::sin(pitchRad)                    * tpsDistance,
                std::cos(yawRad) * std::cos(pitchRad) * tpsDistance
            };

            // Phase 5-F: 足元基準
            //   camPos  = playerPos (足元) + offset + tpsHeight (= 1.4 上方) -> Y=1.4 + offset.y
            //   lookAt  = playerPos (足元) + tpsLookAtHeight (= 1.2 上方、 腰付近)
            const glm::vec3 camPos  = playerPos + offset + glm::vec3{0.f, tpsHeight, 0.f};
            const glm::vec3 lookAt  = playerPos + glm::vec3{0.f, tpsLookAtHeight, 0.f};
            return glm::lookAt(camPos, lookAt, {0.f, 1.f, 0.f});
        } else {
            return glm::lookAt(fpsPosition, fpsPosition + fpsFront(), {0.f, 1.f, 0.f});
        }
    }

    glm::vec3 getCameraWorldPosition(const glm::vec3& playerPos = {0.f,0.f,0.f}) const {
        if (mode == CameraMode::TPS) {
            const float yawRad   = glm::radians(tpsYaw);
            const float pitchRad = glm::radians(tpsPitch);
            const glm::vec3 offset = {
                std::sin(yawRad) * std::cos(pitchRad) * tpsDistance,
                std::sin(pitchRad)                    * tpsDistance,
                std::cos(yawRad) * std::cos(pitchRad) * tpsDistance
            };
            return playerPos + offset + glm::vec3{0.f, tpsHeight, 0.f};
        } else {
            return fpsPosition;
        }
    }

    glm::mat4 getProjectionMatrix(float aspect) const {
        glm::mat4 proj = glm::perspective(
            glm::radians(fov), aspect,
            0.1f,
            200.f
        );
        proj[1][1] *= -1.f;  // Vulkan Y軸反転
        return proj;
    }

private:
    glm::vec3 fpsFront() const {
        return glm::normalize(glm::vec3{
            std::cos(glm::radians(fpsYaw)) * std::cos(glm::radians(fpsPitch)),
            std::sin(glm::radians(fpsPitch)),
            std::sin(glm::radians(fpsYaw)) * std::cos(glm::radians(fpsPitch))
        });
    }
};

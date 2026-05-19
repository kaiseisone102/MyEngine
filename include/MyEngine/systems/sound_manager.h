#pragma once
// =============================================================================
// sound_manager.h — BGM・効果音の管理 (ターン3: chest / gate / 仮代用 SE)
// =============================================================================
// 追加 (ターン3):
//   playPickupArmor / playPickupPotion / playPickupSpirit — 仮代用 (coin.wav)
//   playGateOpen — 仮代用 (coin.wav)、 開き始め + 開き切り の 2 タイミング
//   playChestOpen — 本物 (se/chests/chest.wav)
// 全 SE は sfxGroup 経由なので setSFXVolume で一括音量制御。
// =============================================================================

#include <memory>
#include <string>

#include "core/money.h"

class SoundManager {
   public:
    SoundManager();
    ~SoundManager();

    bool init(const std::string& assetsDir);
    void shutdown();

    void playBGM();
    void stopBGM();
    void playJump();
    void playLand();

    // ─── ターン2: アイテム拾い / 攻撃 SE ────
    void playPickupShield();
    void playPickupGrip();
    void playPickupKey();
    void playPickupMoney(MoneyType type);
    void playSlash();

    // ─── ターン3: 仮代用 + chest / gate ────
    void playPickupArmor();    // 仮 (coin.wav)
    void playPickupPotion();   // 仮 (coin.wav)
    void playPickupSpirit();   // 仮 (coin.wav)
    void playGateOpen();       // 仮 (coin.wav)、 開き始め + 開き切りで呼ぶ
    void playChestOpen();      // 本物 (se/chests/chest.wav)

    bool isInitialized() const { return initialized_; }

    void setBGMVolume(float v);
    void setSFXVolume(float v);
    float bgmVolume() const { return bgmVolume_; }
    float sfxVolume() const { return sfxVolume_; }

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool initialized_ = false;

    float bgmVolume_ = 0.5f;
    float sfxVolume_ = 1.0f;
};

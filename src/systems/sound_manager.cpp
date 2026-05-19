// =============================================================================
// sound_manager.cpp — ターン3 (chest / gate / 仮代用 SE)
// =============================================================================
// 仮代用 (armor / potion / spirit / gate) は coin.wav を流す。
// 後で SE 揃ったら Impl::xxxPath の代入だけ差し替える。
// chest は本物 (se/chests/chest.wav)。
// =============================================================================

#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#include "systems/sound_manager.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "systems/demonVillageBgmGenerator.h"

namespace fs = std::filesystem;

// =============================================================================
// WAV 生成ユーティリティ (既存と同じ)
// =============================================================================

static void writeWav(const std::string& path, const std::vector<int16_t>& samples,
                     int sampleRate = 44100) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[Sound] WAV write failed: " << path << "\n";
        return;
    }
    uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
    uint32_t riffSize = 36 + dataSize;
    uint16_t fmtTag = 1;
    uint16_t channels = 1;
    uint32_t rate = static_cast<uint32_t>(sampleRate);
    uint32_t byteRate = rate * 2;
    uint16_t blkAlign = 2;
    uint16_t bits = 16;
    uint32_t fmtSz = 16;

    f.write("RIFF", 4);
    f.write(reinterpret_cast<char*>(&riffSize), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    f.write(reinterpret_cast<char*>(&fmtSz), 4);
    f.write(reinterpret_cast<char*>(&fmtTag), 2);
    f.write(reinterpret_cast<char*>(&channels), 2);
    f.write(reinterpret_cast<char*>(&rate), 4);
    f.write(reinterpret_cast<char*>(&byteRate), 4);
    f.write(reinterpret_cast<char*>(&blkAlign), 2);
    f.write(reinterpret_cast<char*>(&bits), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<char*>(&dataSize), 4);
    f.write(reinterpret_cast<const char*>(samples.data()), dataSize);
}

static std::vector<int16_t> makeChirp(float f0, float f1, float duration, float volume = 0.35f) {
    constexpr float PI = 3.14159265f;
    const int sr = 44100;
    int N = static_cast<int>(sr * duration);
    std::vector<int16_t> buf(N);
    for (int i = 0; i < N; i++) {
        float t = static_cast<float>(i) / sr;
        float T = duration;
        float ph = 2.f * PI * (f0 * t + 0.5f * (f1 - f0) * t * t / T);
        float prg = t / T;
        float env = std::min(prg / (0.01f / T), 1.f) *
                    std::max(0.f, 1.f - std::max(0.f, prg - 0.7f) / 0.3f);
        buf[i] = static_cast<int16_t>(std::sin(ph) * env * volume * 32767.f);
    }
    return buf;
}

static void ensureAudioAssets(const std::string& dir) {
    const auto jump = dir + "jump.wav";
    const auto land = dir + "land.wav";
    const auto bgm = dir + "bgm.wav";

    if (!fs::exists(jump)) {
        writeWav(jump, makeChirp(300.f, 800.f, 0.25f, 0.40f));
        std::cout << "[Sound] wrote jump.wav\n";
    }
    if (!fs::exists(land)) {
        writeWav(land, makeChirp(180.f, 55.f, 0.15f, 0.45f));
        std::cout << "[Sound] wrote land.wav\n";
    }
    writeWav(bgm, DemonVillageBgmGenerator::generate());
    std::cout << "[Sound] wrote bgm.wav\n";
}

// =============================================================================
// SoundManager::Impl — SE パスを保持
// =============================================================================
struct SoundManager::Impl {
    ma_engine engine{};
    ma_sound bgm{};
    ma_sound_group sfxGroup{};
    bool engineReady = false;
    bool bgmLoaded = false;
    bool sfxGroupReady = false;
    std::string jumpPath;
    std::string landPath;

    // ─── ターン2: アイテム拾い / 攻撃 SE のパス ────
    std::string shieldPath;
    std::string gripPath;
    std::string keyPath;
    std::string moneyCoinPath;
    std::string moneyCoinBagPath;
    std::string moneyDiamondPath;
    std::string slashPath;

    // ─── ターン3: 仮代用 + chest / gate ────
    // 仮 (armor / potion / spirit / gate) → coin.wav を指すように代入する
    std::string armorPath;
    std::string potionPath;
    std::string spiritPath;
    std::string gateOpenPath;
    std::string chestOpenPath;  // 本物 (se/chests/chest.wav)

    // sfxGroup 経由で再生 (空パスは無音)
    void playSfx(const std::string& path) {
        if (path.empty()) return;
        ma_node* node = sfxGroupReady ? reinterpret_cast<ma_node*>(&sfxGroup) : nullptr;
        ma_engine_play_sound_ex(&engine, path.c_str(), node, 0);
    }
};

SoundManager::SoundManager() : impl_(std::make_unique<Impl>()) {}
SoundManager::~SoundManager() { shutdown(); }

bool SoundManager::init(const std::string& assetsDir) {
    ensureAudioAssets(assetsDir);

    ma_engine_config cfg = ma_engine_config_init();
    if (ma_engine_init(&cfg, &impl_->engine) != MA_SUCCESS) {
        std::cerr << "[Sound] ma_engine_init failed; sound disabled.\n";
        return false;
    }
    impl_->engineReady = true;
    impl_->jumpPath = assetsDir + "jump.wav";
    impl_->landPath = assetsDir + "land.wav";

    // ─── ターン2: SE パス ────
    impl_->shieldPath = assetsDir + "sound_effect/shield/shield.wav";
    impl_->gripPath = assetsDir + "sound_effect/grips/grip.wav";
    impl_->keyPath = assetsDir + "sound_effect/keys/key.wav";
    impl_->moneyCoinPath = assetsDir + "sound_effect/money/coin.wav";
    impl_->moneyCoinBagPath = assetsDir + "sound_effect/money/coin_bag.wav";
    impl_->moneyDiamondPath = assetsDir + "sound_effect/money/diamond.wav";
    impl_->slashPath = assetsDir + "sound_effect/attack/slash.mp3";

    // ─── ターン3: 仮代用 (= coin.wav) ────
    // 後で SE 揃ったらここを差し替えるだけ
    impl_->armorPath = impl_->moneyCoinPath;     // 仮
    impl_->potionPath = impl_->moneyCoinPath;    // 仮
    impl_->spiritPath = impl_->moneyCoinPath;    // 仮
    impl_->gateOpenPath = impl_->moneyCoinPath;  // 仮
    // chest は本物
    impl_->chestOpenPath = assetsDir + "sound_effect/chests/chest.wav";

    // 存在チェック (なくても初期化は続行)
    for (const auto* p : {&impl_->shieldPath, &impl_->gripPath, &impl_->keyPath,
                          &impl_->moneyCoinPath, &impl_->moneyCoinBagPath, &impl_->moneyDiamondPath,
                          &impl_->slashPath, &impl_->chestOpenPath}) {
        if (!fs::exists(*p)) {
            std::cerr << "[Sound] WARNING: SE file not found: " << *p << "\n";
        }
    }

    if (ma_sound_group_init(&impl_->engine, 0, nullptr, &impl_->sfxGroup) == MA_SUCCESS) {
        impl_->sfxGroupReady = true;
        ma_sound_group_set_volume(&impl_->sfxGroup, sfxVolume_);
    } else {
        std::cerr << "[Sound] sfx sound group init failed (SFX volume control disabled)\n";
    }

    const std::string bgmPath = assetsDir + "bgm.wav";
    if (ma_sound_init_from_file(&impl_->engine, bgmPath.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION,
                                nullptr, nullptr, &impl_->bgm) == MA_SUCCESS) {
        ma_sound_set_looping(&impl_->bgm, MA_TRUE);
        ma_sound_set_volume(&impl_->bgm, bgmVolume_);
        impl_->bgmLoaded = true;
    } else {
        std::cerr << "[Sound] BGM load failed: " << bgmPath << "\n";
    }

    initialized_ = true;
    std::cout << "[Sound] miniaudio " << MA_VERSION_STRING << " initialized\n";
    return true;
}

void SoundManager::shutdown() {
    if (!initialized_) return;
    if (impl_->bgmLoaded) {
        ma_sound_stop(&impl_->bgm);
        ma_sound_uninit(&impl_->bgm);
        impl_->bgmLoaded = false;
    }
    if (impl_->sfxGroupReady) {
        ma_sound_group_uninit(&impl_->sfxGroup);
        impl_->sfxGroupReady = false;
    }
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineReady = false;
    }
    initialized_ = false;
}

void SoundManager::playBGM() {
    if (!initialized_ || !impl_->bgmLoaded) return;
    ma_sound_start(&impl_->bgm);
}

void SoundManager::stopBGM() {
    if (!initialized_ || !impl_->bgmLoaded) return;
    ma_sound_stop(&impl_->bgm);
}

void SoundManager::playJump() {
    if (!initialized_) return;
    impl_->playSfx(impl_->jumpPath);
}

void SoundManager::playLand() {
    if (!initialized_) return;
    impl_->playSfx(impl_->landPath);
}

// ─── ターン2 ────
void SoundManager::playPickupShield() {
    if (!initialized_) return;
    impl_->playSfx(impl_->shieldPath);
}

void SoundManager::playPickupGrip() {
    if (!initialized_) return;
    impl_->playSfx(impl_->gripPath);
}

void SoundManager::playPickupKey() {
    if (!initialized_) return;
    impl_->playSfx(impl_->keyPath);
}

void SoundManager::playPickupMoney(MoneyType type) {
    if (!initialized_) return;
    switch (type) {
        case MoneyType::Coin:
            impl_->playSfx(impl_->moneyCoinPath);
            break;
        case MoneyType::CoinBag:
            impl_->playSfx(impl_->moneyCoinBagPath);
            break;
        case MoneyType::Diamond:
            impl_->playSfx(impl_->moneyDiamondPath);
            break;
    }
}

void SoundManager::playSlash() {
    if (!initialized_) return;
    impl_->playSfx(impl_->slashPath);
}

// ─── ターン3 ────
void SoundManager::playPickupArmor() {
    if (!initialized_) return;
    impl_->playSfx(impl_->armorPath);
}

void SoundManager::playPickupPotion() {
    if (!initialized_) return;
    impl_->playSfx(impl_->potionPath);
}

void SoundManager::playPickupSpirit() {
    if (!initialized_) return;
    impl_->playSfx(impl_->spiritPath);
}

void SoundManager::playGateOpen() {
    if (!initialized_) return;
    impl_->playSfx(impl_->gateOpenPath);
}

void SoundManager::playChestOpen() {
    if (!initialized_) return;
    impl_->playSfx(impl_->chestOpenPath);
}

void SoundManager::setBGMVolume(float v) {
    bgmVolume_ = std::clamp(v, 0.f, 1.f);
    if (initialized_ && impl_->bgmLoaded) {
        ma_sound_set_volume(&impl_->bgm, bgmVolume_);
    }
}

void SoundManager::setSFXVolume(float v) {
    sfxVolume_ = std::clamp(v, 0.f, 1.f);
    if (initialized_ && impl_->sfxGroupReady) {
        ma_sound_group_set_volume(&impl_->sfxGroup, sfxVolume_);
    }
}

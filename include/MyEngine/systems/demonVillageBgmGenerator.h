#ifndef DEMON_VILLAGE_BGM_GENERATOR_H
#define DEMON_VILLAGE_BGM_GENERATOR_H

#include <cstdint>
#include <vector>

/**
 * @brief レトロゲーム (MOTHER 系) 風のノスタルジックなループ BGM を生成するクラス。
 *
 * 4/4拍子・BPM80・Aマイナーで構成された 90 秒のループ曲を、
 * 44.1kHz / 16bit / モノラルの PCM データとして生成します。
 * ループ点でクリックノイズが出ないよう、末尾と先頭はフェードでシームレスに接続。
 */
class DemonVillageBgmGenerator {
   public:
    /**
     * @brief 約90秒のBGMを生成します。
     * @return 16bit整数型のPCMデータのベクトル（44.1kHz, モノラル）
     */
    static std::vector<int16_t> generate();

   private:
    // --- 定数 ---
    static constexpr float PI = 3.14159265f;
    static constexpr int SR = 44100;         // サンプルレート
    static constexpr float BPM = 80.0f;      // テンポ (MOTHER 風ゆったり)
    static constexpr int BEATS_PER_BAR = 4;  // 4/4 拍子
    static constexpr float DURATION = 90.0f; // 曲全体の長さ（秒）

    // --- 楽器音色ID ---
    enum Instrument {
        INST_BASS = 0,       // 重いベース（矩形波＋低域）
        INST_BRASS = 1,      // 金管風（鋸波＋わずかなデチューン）
        INST_LEAD = 2,       // メロディ用リード（鋸波＋三角波ミックス）
        INST_PLUCK = 3,      // 短い伴奏音 (アルペジオ用)
        INST_DRUM_KICK = 4,  // キック
        INST_DRUM_HAT = 5,   // ハイハット（ノイズ）
    };

    // --- ユーティリティ波形 ---
    static float saw(float f, float t);
    static float square(float f, float t, float duty = 0.5f);
    static float sine(float f, float t);
    static float triangle(float f, float t);
    static float noise();
};

#endif  // DEMON_VILLAGE_BGM_GENERATOR_H

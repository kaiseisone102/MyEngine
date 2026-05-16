#ifndef DEMON_VILLAGE_BGM_GENERATOR_H
#define DEMON_VILLAGE_BGM_GENERATOR_H

#include <cstdint>
#include <vector>

/**
 * @brief 「魔界村」墓場ステージ風のループBGMを生成するクラス。
 *
 * 6/8拍子・BPM140・Aマイナーで構成された90秒のループ曲を、
 * 44.1kHz / 16bit / モノラルのPCMデータとして生成します。
 * ループ点でクリックノイズが出ないよう、末尾と先頭がシームレスに接続されます。
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
    static constexpr int SR = 44100;          // サンプルレート
    static constexpr float BPM = 110.0f;      // テンポ
    static constexpr int BEATS_PER_BAR = 6;   // 6/8拍子
    static constexpr float DURATION = 90.0f;  // 曲全体の長さ（秒）

    // --- 楽器音色ID ---
    enum Instrument {
        INST_BASS = 0,       // 重いベース（矩形波＋低域）
        INST_BRASS = 1,      // 金管風（鋸波＋わずかなデチューン）
        INST_LEAD = 2,       // メロディ用リード（鋸波＋三角波ミックス）
        INST_PLUCK = 3,      // 短い伴奏音
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

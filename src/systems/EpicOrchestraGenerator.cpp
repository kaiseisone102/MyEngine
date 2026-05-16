#include "systems/epicOrchestraGenerator.h"

#include <algorithm>
#include <cmath>

// 実装側では static キーワードは不要です（クラス名::で修飾します）
float EpicOrchestraGenerator::saw(float f, float t) {
    return 2.0f * (t * f - std::floor(0.5f + t * f));
}

float EpicOrchestraGenerator::sine(float f, float t) { return std::sin(2.0f * PI * f * t); }

std::vector<int16_t> EpicOrchestraGenerator::generate() {
    const float duration = 60.0f;
    const int totalSamples = static_cast<int>(SR * duration);
    std::vector<float> master(totalSamples, 0.0f);

    // ラムダ関数による楽器合成
    auto addNote = [&](int type, float freq, float startT, float durT, float vol) {
        int startS = static_cast<int>(startT * SR);
        int lenS = static_cast<int>(durT * SR);
        for (int i = 0; i < lenS && (startS + i) < totalSamples; i++) {
            float t = static_cast<float>(i) / SR;
            float prg = static_cast<float>(i) / lenS;
            float env = std::min(prg / 0.05f, 1.0f) * std::pow(1.0f - prg, 1.5f);
            float s = 0;

            if (type == 0)
                s = saw(freq, t) * 0.7f + saw(freq * 1.005f, t) * 0.3f;
            else if (type == 1)
                s = saw(freq, t) * 0.5f + sine(freq, t) * 0.5f;
            else if (type == 2)
                s = sine(freq * std::exp(-3.0f * prg), t);
            else if (type == 3)
                s = sine(freq, t);

            master[startS + i] += s * env * vol;
        }
    };

    // --- 以下、提供されたアルゴリズムの実装 ---
    float chords[4][4] = {
        {261.63f, 329.63f, 392.00f, 523.25f},  // C
        {196.00f, 246.94f, 293.66f, 392.00f},  // G
        {220.00f, 261.63f, 329.63f, 440.00f},  // Am
        {174.61f, 220.00f, 261.63f, 349.23f}   // F
    };

    for (int sec = 0; sec < 60; sec++) {
        float baseT = static_cast<float>(sec);
        int chordIdx = (sec / 4) % 4;

        // ベース・ティンパニ
        addNote(2, chords[chordIdx][0] / 4.0f, baseT, 1.0f, 0.4f);
        if (sec % 2 == 0) addNote(2, 40.0f, baseT, 0.5f, 0.8f);

        // 伴奏
        for (int note = 0; note < 3; note++) {
            addNote(1, chords[chordIdx][note], baseT, 1.1f, 0.15f);
        }

        // メロディ展開
        if (sec < 15) {
            float mel[] = {chords[chordIdx][2], chords[chordIdx][3]};
            addNote(3, mel[sec % 2], baseT, 0.5f, 0.2f);
        } else if (sec < 45) {
            float mel[] = {chords[chordIdx][0] * 2, chords[chordIdx][1] * 2,
                           chords[chordIdx][2] * 2, chords[chordIdx][3] * 2};
            addNote(0, mel[sec % 4], baseT, 0.25f, 0.3f);
            addNote(1, mel[(sec + 1) % 4], baseT + 0.5f, 0.25f, 0.2f);
        } else {
            for (int m = 0; m < 4; m++) {
                addNote(0, chords[chordIdx][m] * 2.0f, baseT, 0.5f, 0.4f);
                addNote(1, chords[chordIdx][m] * 4.0f, baseT + 0.25f, 0.25f, 0.2f);
            }
        }
    }

    // エコー
    int delayS = static_cast<int>(SR * 0.3f);
    for (int i = delayS; i < totalSamples; i++) {
        master[i] += master[i - delayS] * 0.25f;
    }

    // 出力
    std::vector<int16_t> result;
    result.reserve(totalSamples);
    for (float s : master) {
        result.push_back(static_cast<int16_t>(std::clamp(s * 0.6f, -1.0f, 1.0f) * 32767.0f));
    }
    return result;
}
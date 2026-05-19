// \MyEngine\src\systems\demonVillageBgmGenerator.cpp
// =============================================================================
// demonVillageBgmGenerator.cpp
// =============================================================================
// 90 秒の MOTHER 風ノスタルジー BGM を 44.1kHz / 16bit / モノラルで生成する。
//
// 曲の構成 (BPM 80 / 4/4 → 1 小節 = 3.0 秒、 全 30 小節):
//   イントロ 2 小節: PLUCK のみ、 アルペジオで雰囲気作り
//   A       8 小節: PLUCK + BASS + LEAD (主旋律)
//   B       8 小節: BRASS が加わり感情が膨らむ
//   A'      8 小節: A 再現 + 軽い DRUM
//   アウトロ 4 小節: フェードしてループ先頭へシームレスに
//
// 和声 (Aマイナー):
//   イントロ : Am - Am
//   A        : Am - F  - C  - G  - Am - Em - Dm - E7
//   B        : Dm - G  - C  - Am - F  - G  - Am - E7
//   A'       : Am - F  - C  - G  - Am - Em - Dm - E7
//   アウトロ : Am - F  - Em - Am
//
// ループ点処理:
//   末尾 0.1 秒でフェードアウト + 先頭 0.05 秒でフェードイン。
//   これでループ再生時にクリックノイズ無し。
// =============================================================================
#include "systems/demonVillageBgmGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// ─── 音名 → 周波数 (12平均律、 A4=440Hz) ────────────────────────────────
// MIDI ノート番号方式: A4 = 69
constexpr float A4_FREQ = 440.0f;
float midiToFreq(int midiNote) {
    return A4_FREQ * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

// MIDI ノート番号の定数 (使う範囲だけ)
//   Aマイナー = A, B, C, D, E, F, G
//   オクターブ 2 (低音) ～ 5 (高音)
constexpr int A2 = 45;
constexpr int B2 = 47;
constexpr int C3 = 48;
constexpr int D3 = 50;
constexpr int E3 = 52;
constexpr int F3 = 53;
constexpr int G3 = 55;
constexpr int A3 = 57;
constexpr int B3 = 59;
constexpr int C4 = 60;
constexpr int D4 = 62;
constexpr int E4 = 64;
constexpr int F4 = 65;
constexpr int G4 = 67;
constexpr int A4 = 69;
constexpr int B4 = 71;
constexpr int C5 = 72;
constexpr int D5 = 74;
constexpr int E5 = 76;
constexpr int F5 = 77;
constexpr int G5 = 79;
constexpr int A5 = 81;
constexpr int REST = -1;  // 休符マーカー

// ─── タイミング ────────────────────────────────────────────────────────
// BPM 80 → 1 拍 = 60/80 = 0.75 秒、 1 小節 (4 拍) = 3.0 秒
// 1 拍 = 44100 * 0.75 = 33075 サンプル
constexpr int SR_LOCAL = 44100;
constexpr float BPM_LOCAL = 80.0f;
constexpr float SEC_PER_BEAT = 60.0f / BPM_LOCAL;
constexpr int SAMPLES_PER_BEAT = static_cast<int>(SR_LOCAL * SEC_PER_BEAT);
constexpr int SAMPLES_PER_BAR = SAMPLES_PER_BEAT * 4;

// 全体サンプル数 (90 秒)
constexpr int TOTAL_SAMPLES = SR_LOCAL * 90;

// ─── ADSR エンベロープ ────────────────────────────────────────────────
// 各楽器ごとに音色を変える。 a/d/s/r は秒単位、 s は sustain レベル (0~1)。
struct Envelope {
    float a;  // attack 時間
    float d;  // decay 時間
    float s;  // sustain レベル
    float r;  // release 時間
};

float envelopeAt(const Envelope& env, float t, float noteDuration) {
    // ノート長 noteDuration 秒のうち、 経過時間 t 秒のときのエンベロープ値を返す。
    // attack → decay → sustain → release の順。
    if (t < 0.0f) return 0.0f;
    if (t < env.a) {
        // attack: 0 → 1 でリニア
        return t / env.a;
    }
    if (t < env.a + env.d) {
        // decay: 1 → s でリニア
        float k = (t - env.a) / env.d;
        return 1.0f + (env.s - 1.0f) * k;
    }
    if (t < noteDuration) {
        // sustain: s のまま
        return env.s;
    }
    // release: s → 0 でリニア
    float rt = t - noteDuration;
    if (rt < env.r) {
        return env.s * (1.0f - rt / env.r);
    }
    return 0.0f;
}

// ─── 楽器音色 ─────────────────────────────────────────────────────────
// 与えられた周波数 f、 時刻 t (ノート開始からの経過秒) で 1 サンプルの値を返す。
// 振幅は -1.0 ～ 1.0 程度に正規化。

float instrBass(float f, float t) {
    // 矩形波 + 低オクターブのサイン、 重いベース感
    float a = (std::fmod(f * t, 1.0f) < 0.5f) ? 1.0f : -1.0f;
    float b = std::sin(2.0f * 3.14159265f * (f * 0.5f) * t);
    return 0.5f * a + 0.5f * b;
}

float instrBrass(float f, float t) {
    // 鋸波 + わずかなデチューン (3% ほど) のレイヤーで広がり、 金管っぽさ
    auto saw = [](float ff, float tt) {
        float x = std::fmod(ff * tt, 1.0f);
        return 2.0f * x - 1.0f;
    };
    return 0.5f * saw(f, t) + 0.5f * saw(f * 1.005f, t);
}

float instrLead(float f, float t) {
    // 鋸波 + 三角波のミックス、 メロディに合う柔らかさ
    auto saw = [](float ff, float tt) {
        float x = std::fmod(ff * tt, 1.0f);
        return 2.0f * x - 1.0f;
    };
    auto tri = [](float ff, float tt) {
        float x = std::fmod(ff * tt, 1.0f);
        return 4.0f * std::abs(x - 0.5f) - 1.0f;
    };
    return 0.6f * saw(f, t) + 0.4f * tri(f, t);
}

float instrPluck(float f, float t) {
    // サイン波 + 高い倍音、 短い減衰で「ポロン」と鳴らす想定 (エンベロープで制御)
    float fundamental = std::sin(2.0f * 3.14159265f * f * t);
    float overtone = 0.3f * std::sin(2.0f * 3.14159265f * f * 2.0f * t);
    return fundamental + overtone;
}

float instrKick(float t) {
    // 周波数スイープ (150Hz → 50Hz) のサイン + 短いエンベロープ
    float freq = 150.0f - 100.0f * std::min(t / 0.05f, 1.0f);
    return std::sin(2.0f * 3.14159265f * freq * t);
}

// ホワイトノイズ (擬似ランダム、 LCG)
float pseudoNoise() {
    static uint32_t state = 0xCAFEBABEu;
    state = state * 1664525u + 1013904223u;
    return (static_cast<int32_t>(state) / static_cast<float>(0x7FFFFFFF));
}

float instrHat(float /*t*/) {
    // ノイズ + 高域強調、 ハイハット風
    return pseudoNoise();
}

// ─── ノートを書き込むヘルパー ─────────────────────────────────────────
// out にミックス追加 (=) ではなく += で書き込む。 振幅は呼び出し側で調整。

void writeNote(std::vector<float>& out, int startSample, int noteLengthSamples, int midiNote,
               int instrument, float volume) {
    if (midiNote == REST) return;
    if (startSample >= static_cast<int>(out.size())) return;

    const float f = midiToFreq(midiNote);
    const float noteDur = static_cast<float>(noteLengthSamples) / SR_LOCAL;

    // 楽器ごとのエンベロープ
    Envelope env{};
    switch (instrument) {
        case 0: // BASS
            env = {0.01f, 0.10f, 0.7f, 0.10f};
            break;
        case 1: // BRASS
            env = {0.08f, 0.10f, 0.8f, 0.20f};
            break;
        case 2: // LEAD
            env = {0.03f, 0.15f, 0.7f, 0.15f};
            break;
        case 3: // PLUCK
            env = {0.005f, 0.30f, 0.0f, 0.05f};  // 短く減衰
            break;
        case 4: // KICK (使用しないがフォールバック)
            env = {0.001f, 0.10f, 0.0f, 0.05f};
            break;
        case 5: // HAT
            env = {0.001f, 0.05f, 0.0f, 0.02f};
            break;
    }

    // リリースまで含めた合計時間
    const float totalDur = noteDur + env.r;
    const int totalSamples = static_cast<int>(totalDur * SR_LOCAL);

    for (int i = 0; i < totalSamples; ++i) {
        int dst = startSample + i;
        if (dst < 0 || dst >= static_cast<int>(out.size())) break;

        const float t = static_cast<float>(i) / SR_LOCAL;
        const float e = envelopeAt(env, t, noteDur);
        if (e <= 0.0f) continue;

        float s = 0.0f;
        switch (instrument) {
            case 0: s = instrBass(f, t); break;
            case 1: s = instrBrass(f, t); break;
            case 2: s = instrLead(f, t); break;
            case 3: s = instrPluck(f, t); break;
            case 4: s = instrKick(t); break;
            case 5: s = instrHat(t); break;
        }
        out[dst] += s * e * volume;
    }
}

// ─── ドラムを書き込むヘルパー ────────────────────────────────────────
void writeKick(std::vector<float>& out, int startSample, float volume) {
    constexpr int len = SR_LOCAL / 5;  // 0.2 秒
    Envelope env{0.001f, 0.10f, 0.0f, 0.05f};
    for (int i = 0; i < len; ++i) {
        int dst = startSample + i;
        if (dst < 0 || dst >= static_cast<int>(out.size())) break;
        float t = static_cast<float>(i) / SR_LOCAL;
        float e = envelopeAt(env, t, 0.1f);
        if (e <= 0.0f) continue;
        out[dst] += instrKick(t) * e * volume;
    }
}

void writeHat(std::vector<float>& out, int startSample, float volume) {
    constexpr int len = SR_LOCAL / 20;  // 0.05 秒
    Envelope env{0.001f, 0.03f, 0.0f, 0.02f};
    for (int i = 0; i < len; ++i) {
        int dst = startSample + i;
        if (dst < 0 || dst >= static_cast<int>(out.size())) break;
        float t = static_cast<float>(i) / SR_LOCAL;
        float e = envelopeAt(env, t, 0.03f);
        if (e <= 0.0f) continue;
        out[dst] += instrHat(t) * e * volume;
    }
}

// ─── コード進行 (各小節のコード = ベースの根音 + メロディの背景) ──────
// 各小節 1 コード、 全 30 小節 = イントロ 2 + A 8 + B 8 + A' 8 + アウトロ 4
struct Chord {
    int rootMidi;  // ベース音 (低オクターブ)
    // コードトーン 3 つ (アルペジオ用)
    int t1;
    int t2;
    int t3;
};

const std::array<Chord, 30> CHORDS = {{
    // イントロ (2 小節)
    {A2, A3, C4, E4},  // Am
    {A2, A3, C4, E4},  // Am

    // A セクション (8 小節): Am - F - C - G - Am - Em - Dm - E7
    {A2, A3, C4, E4},  // Am
    {F3 - 12, F3, A3, C4},  // F  (root = F2)
    {C3, C4, E4, G4},  // C
    {G3 - 12, G3, B3, D4},  // G (root = G2)
    {A2, A3, C4, E4},  // Am
    {E3 - 12, E3, G3, B3},  // Em (root = E2)
    {D3, D4, F4, A4},  // Dm
    {E3 - 12, E3, G3, B3},  // E7 (簡略 = Em で代用、 ベース E2)

    // B セクション (8 小節): Dm - G - C - Am - F - G - Am - E7
    {D3, D4, F4, A4},  // Dm
    {G3 - 12, G3, B3, D4},  // G
    {C3, C4, E4, G4},  // C
    {A2, A3, C4, E4},  // Am
    {F3 - 12, F3, A3, C4},  // F
    {G3 - 12, G3, B3, D4},  // G
    {A2, A3, C4, E4},  // Am
    {E3 - 12, E3, G3, B3},  // E7

    // A' セクション (8 小節): Am - F - C - G - Am - Em - Dm - E7
    {A2, A3, C4, E4},  // Am
    {F3 - 12, F3, A3, C4},  // F
    {C3, C4, E4, G4},  // C
    {G3 - 12, G3, B3, D4},  // G
    {A2, A3, C4, E4},  // Am
    {E3 - 12, E3, G3, B3},  // Em
    {D3, D4, F4, A4},  // Dm
    {E3 - 12, E3, G3, B3},  // E7

    // アウトロ (4 小節): Am - F - Em - Am
    {A2, A3, C4, E4},  // Am
    {F3 - 12, F3, A3, C4},  // F
    {E3 - 12, E3, G3, B3},  // Em
    {A2, A3, C4, E4},  // Am
}};

// ─── メロディ (主旋律、 LEAD パート) ──────────────────────────────────
// 各小節 4 拍、 各拍を 8 分音符 2 つに分けて 8 音符/小節を基本とする。
// REST は休符。 A セクションでノスタルジックな上行 → 下行の形を作る。
//
// 各エントリ = 1 小節分の 8 個の 8 分音符 (4/4 拍子)
struct MelodyBar {
    int notes[8];
};

const std::array<MelodyBar, 30> MELODY = {{
    // イントロ 2 小節: 休符 (PLUCK のアルペジオのみ)
    {{REST, REST, REST, REST, REST, REST, REST, REST}},
    {{REST, REST, REST, REST, REST, REST, REST, REST}},

    // A セクション 8 小節
    // Am: 旋律は E-A-C-E (上行) → A をベースに
    {{E4,   A4,   C5,   B4,   A4,   REST, E4,   REST}},
    // F : 同じ動きで F に着地
    {{F4,   A4,   C5,   A4,   F4,   REST, A4,   REST}},
    // C : 明るい瞬間
    {{G4,   C5,   E5,   D5,   C5,   REST, G4,   REST}},
    // G : 半度落として浮遊感
    {{D5,   B4,   G4,   B4,   D5,   REST, REST, REST}},
    // Am 再帰
    {{E4,   A4,   C5,   E5,   A4,   REST, E4,   REST}},
    // Em : 暗く沈む
    {{E4,   G4,   B4,   G4,   E4,   REST, REST, REST}},
    // Dm : 上昇
    {{D4,   F4,   A4,   D5,   F5,   REST, D5,   REST}},
    // E7 : 緊張、 解決待ち
    {{E4,   G4,   B4,   REST, E4,   REST, REST, REST}},

    // B セクション 8 小節: メロディが少し情熱的に
    {{D4,   F4,   A4,   F4,   D4,   F4,   A4,   F4}},
    {{D5,   B4,   G4,   B4,   D5,   REST, REST, REST}},
    {{C5,   E5,   G5,   E5,   C5,   E5,   G4,   REST}},
    {{A4,   C5,   E5,   C5,   A4,   E4,   A4,   REST}},
    {{F4,   A4,   C5,   F5,   A5,   REST, F5,   REST}},
    {{G4,   B4,   D5,   G5,   REST, D5,   B4,   REST}},
    {{A4,   C5,   E5,   A5,   E5,   C5,   A4,   REST}},
    {{E4,   G4,   B4,   D5,   REST, REST, REST, REST}},

    // A' セクション 8 小節: A 再現、 オクターブ上げ等で変化
    {{E5,   A4,   C5,   B4,   A4,   REST, E4,   REST}},
    {{F4,   A4,   C5,   A4,   F4,   REST, A4,   REST}},
    {{G4,   C5,   E5,   D5,   C5,   REST, G4,   REST}},
    {{D5,   B4,   G4,   B4,   D5,   REST, REST, REST}},
    {{E4,   A4,   C5,   E5,   A4,   REST, E4,   REST}},
    {{E4,   G4,   B4,   G4,   E4,   REST, REST, REST}},
    {{D4,   F4,   A4,   D5,   F5,   REST, D5,   REST}},
    {{E4,   G4,   B4,   REST, REST, REST, REST, REST}},

    // アウトロ 4 小節: メロディは抜けて余韻
    {{A4,   REST, REST, REST, REST, REST, REST, REST}},
    {{F4,   REST, REST, REST, REST, REST, REST, REST}},
    {{E4,   REST, REST, REST, REST, REST, REST, REST}},
    {{A4,   REST, REST, REST, REST, REST, REST, REST}},
}};

}  // namespace

// =============================================================================
// 公開: 波形ユーティリティ (.h 宣言の static メソッド)
// =============================================================================

float DemonVillageBgmGenerator::saw(float f, float t) {
    float x = std::fmod(f * t, 1.0f);
    return 2.0f * x - 1.0f;
}
float DemonVillageBgmGenerator::square(float f, float t, float duty) {
    float x = std::fmod(f * t, 1.0f);
    return (x < duty) ? 1.0f : -1.0f;
}
float DemonVillageBgmGenerator::sine(float f, float t) {
    return std::sin(2.0f * PI * f * t);
}
float DemonVillageBgmGenerator::triangle(float f, float t) {
    float x = std::fmod(f * t, 1.0f);
    return 4.0f * std::abs(x - 0.5f) - 1.0f;
}
float DemonVillageBgmGenerator::noise() {
    return pseudoNoise();
}

// =============================================================================
// 公開: generate()
// =============================================================================

std::vector<int16_t> DemonVillageBgmGenerator::generate() {
    // float バッファでミックス → 最後にクリップ + int16 化
    std::vector<float> buf(TOTAL_SAMPLES, 0.0f);

    // 8 分音符の長さ (= 1 拍の半分)
    const int eighthSamples = SAMPLES_PER_BEAT / 2;

    // ─── 各小節を順に書き込む ──────────────────────────────────────
    for (int bar = 0; bar < 30; ++bar) {
        const int barStart = bar * SAMPLES_PER_BAR;
        if (barStart >= TOTAL_SAMPLES) break;

        const Chord& chord = CHORDS[bar];
        const MelodyBar& mel = MELODY[bar];

        // ─── BASS: 各拍頭にコード根音を 1 拍分 ────
        // イントロは BASS なし、 アウトロ最終小節も控えめ
        const bool useBass = (bar >= 2);  // イントロ 2 小節は無し
        if (useBass) {
            for (int beat = 0; beat < 4; ++beat) {
                int pos = barStart + beat * SAMPLES_PER_BEAT;
                // ベースパターン: 1, 3 拍目に根音、 2, 4 拍目に 1 オクターブ上
                int note = (beat % 2 == 0) ? chord.rootMidi : (chord.rootMidi + 12);
                writeNote(buf, pos, SAMPLES_PER_BEAT, note, 0 /*BASS*/, 0.35f);
            }
        }

        // ─── PLUCK: コードのアルペジオ (8 分音符で 1 拍 2 音、 1 小節 8 音) ────
        // 全セクションで鳴らす (静かな雰囲気作り)
        for (int i = 0; i < 8; ++i) {
            int pos = barStart + i * eighthSamples;
            int note;
            // 0 1 2 3 -> t1 t2 t3 t2 (上行 → 下行) を 1 拍ごとに繰り返す
            int idx = i % 4;
            switch (idx) {
                case 0: note = chord.t1; break;
                case 1: note = chord.t2; break;
                case 2: note = chord.t3; break;
                default: note = chord.t2; break;
            }
            writeNote(buf, pos, eighthSamples, note, 3 /*PLUCK*/, 0.15f);
        }

        // ─── LEAD: メロディ (8 分音符 × 8) ────
        // イントロは LEAD なし。 A/B/A' は LEAD あり。 アウトロは僅か。
        const bool useLead = (bar >= 2 && bar < 26);  // 2-25 (A, B, A')
        const bool useLeadOutro = (bar >= 26);  // アウトロも 1 音だけ
        if (useLead || useLeadOutro) {
            const float leadVol = useLead ? 0.30f : 0.20f;
            for (int i = 0; i < 8; ++i) {
                int pos = barStart + i * eighthSamples;
                int note = mel.notes[i];
                if (note != REST) {
                    writeNote(buf, pos, eighthSamples, note, 2 /*LEAD*/, leadVol);
                }
            }
        }

        // ─── BRASS: B セクションでコードトーンを伸ばす ────
        const bool useBrass = (bar >= 10 && bar < 18);  // B セクション
        if (useBrass) {
            // 1 小節 = 1 つの長いブラス和音 (t1 と t3 をユニゾン)
            writeNote(buf, barStart, SAMPLES_PER_BAR, chord.t1, 1 /*BRASS*/, 0.18f);
            writeNote(buf, barStart, SAMPLES_PER_BAR, chord.t3, 1 /*BRASS*/, 0.14f);
        }

        // ─── DRUM: A' セクションで軽く ────
        const bool useDrum = (bar >= 18 && bar < 26);
        if (useDrum) {
            // キックは 1, 3 拍目
            writeKick(buf, barStart + 0 * SAMPLES_PER_BEAT, 0.40f);
            writeKick(buf, barStart + 2 * SAMPLES_PER_BEAT, 0.40f);
            // ハイハットは 8 分音符で全拍
            for (int i = 0; i < 8; ++i) {
                writeHat(buf, barStart + i * eighthSamples, 0.10f);
            }
        }
    }

    // ─── ループ点処理: 末尾フェードアウト + 先頭フェードイン ─────────
    const int fadeOutSamples = SR_LOCAL / 10;  // 0.1 秒
    const int fadeInSamples = SR_LOCAL / 20;   // 0.05 秒
    for (int i = 0; i < fadeOutSamples; ++i) {
        int idx = TOTAL_SAMPLES - fadeOutSamples + i;
        if (idx < 0 || idx >= TOTAL_SAMPLES) continue;
        float gain = 1.0f - static_cast<float>(i) / fadeOutSamples;
        buf[idx] *= gain;
    }
    for (int i = 0; i < fadeInSamples; ++i) {
        if (i >= TOTAL_SAMPLES) break;
        float gain = static_cast<float>(i) / fadeInSamples;
        buf[i] *= gain;
    }

    // ─── ソフトクリップ + int16 化 ────────────────────────────────
    std::vector<int16_t> out(TOTAL_SAMPLES);
    for (int i = 0; i < TOTAL_SAMPLES; ++i) {
        float s = buf[i];
        // tanh ベースのソフトクリップで気持ちよく潰す
        s = std::tanh(s * 0.8f);
        // -1.0 ～ 1.0 → -32767 ～ 32767
        int v = static_cast<int>(s * 32767.0f);
        v = std::clamp(v, -32767, 32767);
        out[i] = static_cast<int16_t>(v);
    }
    return out;
}

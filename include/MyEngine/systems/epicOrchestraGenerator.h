#ifndef EPIC_ORCHESTRA_GENERATOR_H
#define EPIC_ORCHESTRA_GENERATOR_H

#include <cstdint>
#include <vector>


class EpicOrchestraGenerator {
   public:
    /**
     * @brief 1分間の壮大なゲームBGM（44.1kHz, 16bit, Mono）を生成します。
     * @return 16bit整数型のPCMデータのベクトル
     */
    static std::vector<int16_t> generate();

   private:
    // ヘッダに定数を置く場合は static constexpr を使用
    static constexpr float PI = 3.14159265f;
    static constexpr int SR = 44100;

    // 内部計算用のユーティリティ関数
    static float saw(float f, float t);
    static float sine(float f, float t);
};

#endif  // EPIC_ORCHESTRA_GENERATOR_H
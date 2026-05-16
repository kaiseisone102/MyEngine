#pragma once
// =============================================================================
// sound_manager.h — ステップ15: BGM・効果音の管理
// =============================================================================
// miniaudio（シングルヘッダー C ライブラリ）を使って音を再生するクラス。
//
// 設計ポイント:
//   - Pimpl パターン（impl_ に miniaudio の型を隠蔽）
//       -> miniaudio.h（4MB超）を h ファイルに含めないため。
//   - WAV ファイルが存在しなければ起動時にプログラム内で自動生成する
//       -> 外部の音源ファイルが不要で、即動作する。
//
// 使い方（main.cpp から）:
//   sound_.init("C:/path/to/assets/");
//   sound_.playBGM();                    // 起動時に BGM ループ開始
//   sound_.playJump();                   // ジャンプ時
//   sound_.playLand();                   // 着地時
//   sound_.shutdown();                   // 終了時
// =============================================================================

#include <memory>
#include <string>

class SoundManager {
   public:
    SoundManager();
    ~SoundManager();

    // assetsDir: WAV を置く/生成するディレクトリ（末尾スラッシュ必須）
    // 例: "C:/MyEngine/build/Debug/assets/"
    // 戻り値: false = miniaudio エンジン初期化失敗（音なしで続行可能）
    bool init(const std::string& assetsDir);
    void shutdown();

    void playBGM();   // BGM をループ再生開始
    void stopBGM();   // BGM を停止
    void playJump();  // ジャンプ効果音（fire-and-forget）
    void playLand();  // 着地効果音（fire-and-forget）

    bool isInitialized() const { return initialized_; }

   private:
    // miniaudio の型（ma_engine, ma_sound 等）を .cpp 側に完全に隠す
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool initialized_ = false;
};

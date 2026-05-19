#pragma once
// =============================================================================
// settings_io.h — GameSettings の JSON 永続化
// =============================================================================
// 仕様:
//   - 保存先: SDL_GetPrefPath("MyEngine", "MyEngine") + "settings.json"
//     (Windows: %APPDATA%/MyEngine/MyEngine/settings.json)
//   - 保存形式: JSON (rapidjson)。 scancode/button は数値で保存。
//   - 読み込み失敗時: デフォルト値を返す (起動を止めない)
//   - 書き込み失敗時: false を返す (ログのみ、 起動を止めない)
//
// バージョン管理:
//   - "version": 1
//   - 将来フォーマット変更時に互換性確保 (現状は v1 のみ対応)
// =============================================================================

#include <string>

struct GameSettings;

namespace settings_io {

// 保存先パスを返す (内部で SDL_GetPrefPath を 1 回呼んでキャッシュ)。
// 末尾はファイル名まで含む完全なパス。
const std::string& defaultSettingsPath();

// ファイルから読み込み。 失敗時はデフォルト値を返す。
// out 引数を取らず値返却にしているのは「失敗時にも常に有効な値を返す」 ため。
GameSettings load(const std::string& path);

// ファイルへ書き込み。 成功なら true、 失敗時は false (ログ出力)。
bool save(const GameSettings& settings, const std::string& path);

}  // namespace settings_io

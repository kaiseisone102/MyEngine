// =============================================================================
// settings_io.cpp — GameSettings の JSON 永続化 実装
// =============================================================================
#include "core/settings_io.h"

#include <SDL3/SDL.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "core/game_settings.h"

namespace settings_io {

namespace {

constexpr int kSettingsVersion = 1;

// SDL_GetPrefPath は確保した文字列の所有権を呼び出し側に渡す (SDL_free が必要)。
// プロセスで 1 度だけ計算して static にキャッシュする。
std::string computeSettingsPath() {
    // SDL_GetPrefPath  : char* を返す。 SDL_free で解放必要。
    // SDL_GetBasePath  : const char* を返す。 解放不要 (内部静的)。
    char* prefPath = SDL_GetPrefPath("MyEngine", "MyEngine");
    if (!prefPath) {
        // 取得失敗時は実行ファイル隣にフォールバック
        const char* bp = SDL_GetBasePath();
        std::string fallback = bp ? bp : "";
        fallback += "settings.json";
        std::cerr << "[SettingsIO] WARNING: SDL_GetPrefPath failed, falling back to " << fallback
                  << "\n";
        return fallback;
    }
    std::string path = prefPath;
    SDL_free(prefPath);
    path += "settings.json";
    return path;
}

// rapidjson に値の存否 + 型チェックを行う小さなヘルパ
bool readFloat(const rapidjson::Value& obj, const char* key, float& out) {
    if (!obj.HasMember(key)) return false;
    const auto& v = obj[key];
    if (!v.IsNumber()) return false;
    out = static_cast<float>(v.GetDouble());
    return true;
}

bool readBinding(const rapidjson::Value& obj, const char* key, InputBinding& out) {
    if (!obj.HasMember(key)) return false;
    const auto& b = obj[key];
    if (!b.IsObject()) return false;
    if (!b.HasMember("source") || !b["source"].IsString()) return false;
    if (!b.HasMember("code") || !b["code"].IsInt()) return false;

    const std::string source = b["source"].GetString();
    const int code = b["code"].GetInt();

    if (source == "keyboard") {
        out = InputBinding{InputBinding::Source::Keyboard, code};
        return true;
    }
    if (source == "mouse") {
        out = InputBinding{InputBinding::Source::Mouse, code};
        return true;
    }
    return false;  // 不明な source
}

// Writer に Binding を書き出すヘルパ
template <typename Writer>
void writeBinding(Writer& w, const char* key, const InputBinding& b) {
    w.Key(key);
    w.StartObject();
    w.Key("source");
    w.String(b.source == InputBinding::Source::Keyboard ? "keyboard" : "mouse");
    w.Key("code");
    w.Int(b.code);
    w.EndObject();
}

}  // namespace

const std::string& defaultSettingsPath() {
    // C++11 以降 thread-safe な local static 初期化
    static const std::string path = computeSettingsPath();
    return path;
}

GameSettings load(const std::string& path) {
    GameSettings settings;  // デフォルト値

    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::cout << "[SettingsIO] No settings file found at '" << path
                  << "' — using defaults (will be created on first save)\n";
        return settings;
    }

    char buffer[8192];
    rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
    rapidjson::Document doc;
    doc.ParseStream(is);
    std::fclose(fp);

    if (doc.HasParseError()) {
        std::cerr << "[SettingsIO] WARNING: parse error at offset " << doc.GetErrorOffset() << ": "
                  << rapidjson::GetParseError_En(doc.GetParseError()) << " — using defaults\n";
        return settings;  // デフォルト維持
    }
    if (!doc.IsObject()) {
        std::cerr << "[SettingsIO] WARNING: root is not an object — using defaults\n";
        return settings;
    }

    // バージョンチェック (互換性は今後追加)
    int version = 0;
    if (doc.HasMember("version") && doc["version"].IsInt()) {
        version = doc["version"].GetInt();
    }
    if (version != kSettingsVersion) {
        std::cerr << "[SettingsIO] WARNING: version mismatch (file=" << version
                  << " expected=" << kSettingsVersion << ") — using defaults\n";
        return settings;
    }

    // 各フィールドを安全に読む (失敗してもデフォルトのまま続行)
    readFloat(doc, "bgmVolume", settings.bgmVolume);
    readFloat(doc, "sfxVolume", settings.sfxVolume);
    readFloat(doc, "mouseSensitivity", settings.mouseSensitivity);

    if (doc.HasMember("keyMapping") && doc["keyMapping"].IsObject()) {
        const auto& km = doc["keyMapping"];
        readBinding(km, "moveForward", settings.keyMapping.moveForward);
        readBinding(km, "moveBack", settings.keyMapping.moveBack);
        readBinding(km, "moveLeft", settings.keyMapping.moveLeft);
        readBinding(km, "moveRight", settings.keyMapping.moveRight);
        readBinding(km, "moveUp", settings.keyMapping.moveUp);
        readBinding(km, "moveDown", settings.keyMapping.moveDown);
        readBinding(km, "sprint", settings.keyMapping.sprint);
        readBinding(km, "crouch", settings.keyMapping.crouch);
        readBinding(km, "jump", settings.keyMapping.jump);
        readBinding(km, "toggleCamera", settings.keyMapping.toggleCamera);
        readBinding(km, "attack", settings.keyMapping.attack);
        readBinding(km, "strongAttack", settings.keyMapping.strongAttack);
        readBinding(km, "guard", settings.keyMapping.guard);
    }

    // 一時フラグは永続化対象外
    settings.keyMappingDirty = false;
    settings.persistDirty = false;

    // 範囲クランプ (壊れた値が入っていても安全)
    auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    settings.bgmVolume =
        clamp(settings.bgmVolume, GameSettings::kMinVolume, GameSettings::kMaxVolume);
    settings.sfxVolume =
        clamp(settings.sfxVolume, GameSettings::kMinVolume, GameSettings::kMaxVolume);
    settings.mouseSensitivity = clamp(settings.mouseSensitivity, GameSettings::kMinSensitivity,
                                      GameSettings::kMaxSensitivity);

    std::cout << "[SettingsIO] loaded from '" << path << "'\n";
    return settings;
}

bool save(const GameSettings& settings, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        std::cerr << "[SettingsIO] WARNING: failed to open '" << path
                  << "' for writing: " << std::strerror(errno) << "\n";
        return false;
    }

    char buffer[8192];
    rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> w(os);

    w.StartObject();
    {
        w.Key("version");
        w.Int(kSettingsVersion);

        w.Key("bgmVolume");
        w.Double(settings.bgmVolume);
        w.Key("sfxVolume");
        w.Double(settings.sfxVolume);
        w.Key("mouseSensitivity");
        w.Double(settings.mouseSensitivity);

        w.Key("keyMapping");
        w.StartObject();
        {
            const auto& km = settings.keyMapping;
            writeBinding(w, "moveForward", km.moveForward);
            writeBinding(w, "moveBack", km.moveBack);
            writeBinding(w, "moveLeft", km.moveLeft);
            writeBinding(w, "moveRight", km.moveRight);
            writeBinding(w, "moveUp", km.moveUp);
            writeBinding(w, "moveDown", km.moveDown);
            writeBinding(w, "sprint", km.sprint);
            writeBinding(w, "crouch", km.crouch);
            writeBinding(w, "jump", km.jump);
            writeBinding(w, "toggleCamera", km.toggleCamera);
            writeBinding(w, "attack", km.attack);
            writeBinding(w, "strongAttack", km.strongAttack);
            writeBinding(w, "guard", km.guard);
        }
        w.EndObject();
    }
    w.EndObject();

    std::fclose(fp);
    std::cout << "[SettingsIO] saved to '" << path << "'\n";
    return true;
}

}  // namespace settings_io

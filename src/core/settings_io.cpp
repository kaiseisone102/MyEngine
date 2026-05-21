// =============================================================================
// settings_io.cpp — + reflectionQuality + reflectShadows
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
std::string computeSettingsPath() {
    char* prefPath = SDL_GetPrefPath("MyEngine", "MyEngine");
    if (!prefPath) {
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
bool readFloat(const rapidjson::Value& obj, const char* key, float& out) {
    if (!obj.HasMember(key)) return false;
    const auto& v = obj[key];
    if (!v.IsNumber()) return false;
    out = static_cast<float>(v.GetDouble());
    return true;
}
bool readInt(const rapidjson::Value& obj, const char* key, int& out) {
    if (!obj.HasMember(key)) return false;
    const auto& v = obj[key];
    if (!v.IsInt()) return false;
    out = v.GetInt();
    return true;
}
bool readBool(const rapidjson::Value& obj, const char* key, bool& out) {
    if (!obj.HasMember(key)) return false;
    const auto& v = obj[key];
    if (!v.IsBool()) return false;
    out = v.GetBool();
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
    return false;
}
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
    static const std::string path = computeSettingsPath();
    return path;
}
GameSettings load(const std::string& path) {
    GameSettings settings;
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
        return settings;
    }
    if (!doc.IsObject()) {
        std::cerr << "[SettingsIO] WARNING: root is not an object — using defaults\n";
        return settings;
    }
    int version = 0;
    if (doc.HasMember("version") && doc["version"].IsInt()) {
        version = doc["version"].GetInt();
    }
    if (version != kSettingsVersion) {
        std::cerr << "[SettingsIO] WARNING: version mismatch (file=" << version
                  << " expected=" << kSettingsVersion << ") — using defaults\n";
        return settings;
    }
    readFloat(doc, "bgmVolume", settings.bgmVolume);
    readFloat(doc, "sfxVolume", settings.sfxVolume);
    readFloat(doc, "mouseSensitivity", settings.mouseSensitivity);
    readFloat(doc, "drawDistance", settings.drawDistance);

    // ─── Reflection ────
    {
        int qi = static_cast<int>(settings.reflectionQuality);
        if (readInt(doc, "reflectionQuality", qi)) {
            // 範囲外なら デフォルト (Half) にフォールバック
            if (qi < 0 || qi > 3) {
                std::cerr << "[SettingsIO] WARNING: reflectionQuality out of range (" << qi
                          << "), using default Half\n";
                qi = static_cast<int>(ReflectionQuality::Half);
            }
            settings.reflectionQuality = static_cast<ReflectionQuality>(qi);
        }
        readBool(doc, "reflectShadows", settings.reflectShadows);
        readBool(doc, "grassWind", settings.grassWind);
        readInt(doc, "shadowQuality", settings.shadowQuality);
        if (settings.shadowQuality < 0 || settings.shadowQuality > 2) settings.shadowQuality = 1;
        readBool(doc, "bloom", settings.bloom);
        {
            int ti = static_cast<int>(settings.tonemapMode);
            if (readInt(doc, "tonemapMode", ti)) {
                if (ti < 0 || ti > 2) {
                    std::cerr << "[SettingsIO] WARNING: tonemapMode out of range (" << ti
                              << "), using default ACES\n";
                    ti = static_cast<int>(TonemapMode::ACES);
                }
                settings.tonemapMode = static_cast<TonemapMode>(ti);
            }
        }
    }

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
    settings.keyMappingDirty = false;
    settings.persistDirty = false;
    settings.reflectionDirty = false;
    auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    settings.bgmVolume =
        clamp(settings.bgmVolume, GameSettings::kMinVolume, GameSettings::kMaxVolume);
    settings.sfxVolume =
        clamp(settings.sfxVolume, GameSettings::kMinVolume, GameSettings::kMaxVolume);
    settings.mouseSensitivity = clamp(settings.mouseSensitivity, GameSettings::kMinSensitivity,
                                      GameSettings::kMaxSensitivity);
    settings.drawDistance = clamp(settings.drawDistance, GameSettings::kMinDrawDistance,
                                  GameSettings::kMaxDrawDistance);
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
        w.Key("drawDistance");
        w.Double(settings.drawDistance);

        // ─── Reflection ────
        w.Key("reflectionQuality");
        w.Int(static_cast<int>(settings.reflectionQuality));
        w.Key("reflectShadows");
        w.Bool(settings.reflectShadows);
        w.Key("tonemapMode");
        w.Int(static_cast<int>(settings.tonemapMode));
        w.Key("grassWind");
        w.Bool(settings.grassWind);
        w.Key("shadowQuality");
        w.Int(settings.shadowQuality);
        w.Key("bloom");
        w.Bool(settings.bloom);

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

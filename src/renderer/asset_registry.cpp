// src/renderer/asset_registry.cpp
#include "renderer/asset_registry.h"
#include "renderer/bindless_texture_registry.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>

#include "renderer/model_loader.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

namespace {
constexpr uint32_t kMaxMaterialSets = 256;
}  // namespace

void AssetRegistry::init(VulkanContext* ctx, ResourceFactory* resources,
                         const std::string& assetDir,
                         BindlessTextureRegistry* bindless) {
    bindless_ = bindless;
    ctx_ = ctx;
    resources_ = resources;
    assetDir_ = assetDir;

    createDefaultMesh();
    createGrass();  // Phase 1F
    createMaterialSetLayout();
    createMaterialDescriptorPool();
    createDefaultTexture();
    createDefaultMaterial();

    // ─── Phase 3 段階: ステージ用テクスチャ/マテリアルを初期登録 ───
    // パスに拡張子を含めない: registerTexture が .png / .jpg / .jpeg / .bmp / .tga を
    // 順に試行して見つかったものを使う。
    // 全部見つからない場合は checkerboard に fallback する。
    const std::pair<const char*, const char*> kInitialTextures[] = {
        {"stone_wall", "textures/stone_walls/stone_wall"},
        {"wood_wall", "textures/wood_walls/wood_wall"},
        {"stone_floor", "textures/stone_floors/stone_floor"},
        {"wood_floor", "textures/wood_floors/wood_floor"},
        {"grass_field", "textures/grounds/grass_field"},
    };
    for (const auto& [name, path] : kInitialTextures) {
        registerTexture(name, path);
        registerMaterial(name, name);  // texture 名と同じ名前で material を登録
    }
}

void AssetRegistry::createDefaultMesh() {
    // 既定 cube mesh: 足元基準 [-0.5,+0.5] x [0,1] x [-0.5,+0.5] のコード生成。
    // cube.obj ファイル不要。 物理 AABB (足元基準) と完全に一致する。
    defaultMesh_.createCube(ctx_, resources_);
}

void AssetRegistry::createGrass() {
    grassMesh_.createCrossQuad(ctx_, resources_);
    const int W = 64, H = 64;
    std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4, 0);
    auto blade = [&](float cx, float halfW, float lean) {
        for (int y = 0; y < H; ++y) {
            const float t = 1.0f - float(y) / float(H - 1);
            const float w = halfW * (0.3f + 0.7f * t);
            const float center = cx + lean * (1.0f - t);
            for (int x = 0; x < W; ++x) {
                const float fx = float(x) / float(W - 1);
                if (std::fabs(fx - center) <= w) {
                    const size_t i = (static_cast<size_t>(y) * W + x) * 4;
                    px[i + 0] = static_cast<uint8_t>((0.10f + 0.25f * t) * 255.f);
                    px[i + 1] = static_cast<uint8_t>((0.30f + 0.45f * t) * 255.f);
                    px[i + 2] = static_cast<uint8_t>((0.05f + 0.10f * t) * 255.f);
                    px[i + 3] = 255;
                }
            }
        }
    };
    blade(0.30f, 0.05f, 0.06f);
    blade(0.45f, 0.06f, -0.04f);
    blade(0.55f, 0.05f, 0.03f);
    blade(0.70f, 0.05f, -0.05f);
    grassTexture_.loadFromRawRGBA(ctx_, resources_, px.data(), W, H);
    if (bindless_) {
        const uint32_t idx = bindless_->registerTexture(grassTexture_.view(), grassTexture_.sampler());
        if (idx != UINT32_MAX) {
            grassTexture_.setBindlessIndex(idx);
        std::cout << "grass texture bindless idx = " << idx << "\n";
        }
    }
}


void AssetRegistry::createMaterialSetLayout() {
    VkDescriptorSetLayoutBinding bind{};
    bind.binding = 0;
    bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bind.descriptorCount = 1;
    bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 1;
    ci.pBindings = &bind;
    if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &materialSetLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("AssetRegistry: vkCreateDescriptorSetLayout failed");
    }
}

void AssetRegistry::createMaterialDescriptorPool() {
    VkDescriptorPoolSize sz{};
    sz.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sz.descriptorCount = kMaxMaterialSets;

    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &sz;
    ci.maxSets = kMaxMaterialSets;
    if (vkCreateDescriptorPool(ctx_->device(), &ci, nullptr, &materialPool_) != VK_SUCCESS) {
        throw std::runtime_error("AssetRegistry: vkCreateDescriptorPool (material) failed");
    }
}

void AssetRegistry::createDefaultTexture() {
    defaultTexture_.loadFromFileOrCheckerboard(ctx_, resources_, assetDir_ + "texture.png");
    // Phase 1D: also register the default texture in the bindless array.
    if (bindless_) {
        const uint32_t idx = bindless_->registerTexture(defaultTexture_.view(),
                                                        defaultTexture_.sampler());
        if (idx != UINT32_MAX) {
            defaultTexture_.setBindlessIndex(idx);
            std::cout << "[AssetRegistry] default texture bindless index = " << idx << "\n";
        }
    }
}

void AssetRegistry::createDefaultMaterial() {
    defaultMaterial_.init(ctx_, materialPool_, materialSetLayout_, &defaultTexture_);
    defaultMaterial_.setBindlessIndex(defaultTexture_.bindlessIndex());
}

bool AssetRegistry::registerModel(const std::string& name, const std::string& path) {
    if (models_.find(name) != models_.end()) {
        std::cout << "[AssetRegistry] model '" << name << "' already registered, skipping\n";
        return true;
    }

    auto modelPtr = std::make_unique<Model>();
    std::vector<AnimationClip> embeddedAnims;
    const bool ok = ModelLoader::load(ctx_, resources_, *this, path, *modelPtr, embeddedAnims);
    if (!ok) {
        std::cerr << "[AssetRegistry] registerModel failed: name='" << name << "' path='" << path
                  << "'\n";
        return false;
    }

    models_[name] = std::move(modelPtr);

    if (!embeddedAnims.empty()) {
        animationLibrary_[name] = std::move(embeddedAnims);
        std::cout << "[AssetRegistry] auto-registered " << animationLibrary_[name].size()
                  << " embedded animation(s) under name='" << name << "':\n";
        // 診断: 全クリップ名と duration を出力
        for (size_t i = 0; i < animationLibrary_[name].size(); ++i) {
            const auto& clip = animationLibrary_[name][i];
            std::cout << "  [" << i << "] name='" << clip.name << "' duration=" << clip.duration
                      << "s channels=" << clip.channels.size() << "\n";
        }
    }

    if (activeModelName_.empty()) {
        activeModelName_ = name;
        std::cout << "[AssetRegistry] active model set: " << path << "\n";
    }

    return true;
}

bool AssetRegistry::registerAnimation(const std::string& name, const std::string& path) {
    if (animationLibrary_.find(name) != animationLibrary_.end()) {
        std::cout << "[AssetRegistry] animation '" << name << "' already registered, skipping\n";
        return true;
    }

    std::vector<AnimationClip> clips;
    const bool ok = ModelLoader::loadAnimationsOnly(path, clips);
    if (!ok) {
        std::cerr << "[AssetRegistry] registerAnimation failed: name='" << name << "' path='"
                  << path << "'\n";
        return false;
    }

    animationLibrary_[name] = std::move(clips);
    std::cout << "[AssetRegistry] animation registered: name='" << name
              << "' clips=" << animationLibrary_[name].size() << ":\n";
    // 診断: 全クリップ名と duration を出力
    for (size_t i = 0; i < animationLibrary_[name].size(); ++i) {
        const auto& clip = animationLibrary_[name][i];
        std::cout << "  [" << i << "] name='" << clip.name << "' duration=" << clip.duration
                  << "s channels=" << clip.channels.size() << "\n";
    }
    return true;
}

const Model* AssetRegistry::getModel(const std::string& name) const {
    auto it = models_.find(name);
    return (it != models_.end()) ? it->second.get() : nullptr;
}

const AnimationClip* AssetRegistry::getAnimation(const std::string& name) const {
    auto it = animationLibrary_.find(name);
    if (it == animationLibrary_.end() || it->second.empty()) return nullptr;
    return &it->second.front();
}

// ─── Phase 3 段階: テクスチャ/マテリアル register/get ─────────
// path の拡張子有無による挙動:
//   - 拡張子付き (例: "textures/stone_wall.png"): そのファイルを直接ロード
//   - 拡張子なし (例: "textures/stone_wall"): .png .jpg .jpeg .bmp .tga を
//     順に試行し、 最初に見つかったものをロード
//   - 全部見つからなければ Texture::loadFromFileOrCheckerboard が
//     checkerboard に fallback する。
bool AssetRegistry::registerTexture(const std::string& name, const std::string& path) {
    if (textures_.find(name) != textures_.end()) {
        std::cout << "[AssetRegistry] texture '" << name << "' already registered, skipping\n";
        return true;
    }

    // 拡張子の有無を判定 (パス末尾に '.' があり、 そこから '/' '\\'
    // まで戻っても何もなければ拡張子あり)
    bool hasExtension = false;
    {
        const size_t dotPos = path.find_last_of('.');
        const size_t sepPos = path.find_last_of("/\\");
        if (dotPos != std::string::npos && (sepPos == std::string::npos || dotPos > sepPos)) {
            hasExtension = true;
        }
    }

    std::string resolvedPath;
    if (hasExtension) {
        // 拡張子あり: そのまま使う
        resolvedPath = path;
    } else {
        // 拡張子なし: 候補リストを順に試す
        static const char* kCandidateExts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga"};
        const std::string fullBase = assetDir_ + path;
        for (const char* ext : kCandidateExts) {
            const std::string candidate = fullBase + ext;
            // FILE* で存在チェック (stb_image を呼ぶ前)
            if (FILE* fp = std::fopen(candidate.c_str(), "rb")) {
                std::fclose(fp);
                // 見つかった: 相対パス + 拡張子で resolvedPath を作る
                resolvedPath = path + ext;
                std::cout << "[AssetRegistry] texture '" << name << "' resolved extension: " << ext
                          << "\n";
                break;
            }
        }
        if (resolvedPath.empty()) {
            // 全部見つからない: 最初の候補 (.png) を渡して checkerboard fallback させる
            resolvedPath = path + ".png";
            std::cerr << "[AssetRegistry] texture '" << name
                      << "' no file found for any extension, will fall back to checkerboard\n";
        }
    }

    auto tex = std::make_unique<Texture>();
    // loadFromFileOrCheckerboard はファイル不在/読込失敗時にチェッカーボードに fallback。
        tex->loadFromFileOrCheckerboard(ctx_, resources_, assetDir_ + resolvedPath);

    // Phase 1D: register this texture in the global bindless array.
    // The returned index is stored on the Texture so Materials can look it up.
    if (bindless_) {
        const uint32_t idx = bindless_->registerTexture(tex->view(), tex->sampler());
        if (idx != UINT32_MAX) {
            tex->setBindlessIndex(idx);
            std::cout << "[AssetRegistry] texture '" << name
                      << "' bindless index = " << idx << "\n";
        }
    }

    textures_[name] = std::move(tex);
    std::cout << "[AssetRegistry] registered texture '" << name << "' (path='" << resolvedPath
              << "')\n";
    return true;
}

bool AssetRegistry::registerMaterial(const std::string& name, const std::string& textureName) {
    if (materials_.find(name) != materials_.end()) {
        std::cout << "[AssetRegistry] material '" << name << "' already registered, skipping\n";
        return true;
    }
    const Texture* tex = getTexture(textureName);
    if (!tex) {
        std::cerr << "[AssetRegistry] registerMaterial failed: texture '" << textureName
                  << "' not found\n";
        return false;
    }
    auto mat = std::make_unique<Material>();
        mat->init(ctx_, materialPool_, materialSetLayout_, tex);
    // Phase 1D: copy the bindless index from the texture to the material
    // so the renderer can pick the right slot in the bindless array.
    mat->setBindlessIndex(tex->bindlessIndex());
    materials_[name] = std::move(mat);
    std::cout << "[AssetRegistry] registered material '" << name << "' (texture='" << textureName
              << "')\n";
    return true;
}

const Texture* AssetRegistry::getTexture(const std::string& name) const {
    auto it = textures_.find(name);
    if (it == textures_.end()) return nullptr;
    return it->second.get();
}

const Material* AssetRegistry::getMaterial(const std::string& name) const {
    auto it = materials_.find(name);
    if (it == materials_.end()) return nullptr;
    return it->second.get();
}

bool AssetRegistry::loadModelFromFile(const std::string& path) {
    const size_t slashPos = path.find_last_of("/\\");
    const size_t dotPos = path.find_last_of('.');
    std::string name;
    if (slashPos != std::string::npos && dotPos != std::string::npos && dotPos > slashPos) {
        name = path.substr(slashPos + 1, dotPos - slashPos - 1);
    } else if (dotPos != std::string::npos) {
        name = path.substr(0, dotPos);
    } else {
        name = path;
    }
    return registerModel(name, path);
}

const Model* AssetRegistry::activeModel() const {
    if (activeModelName_.empty()) return nullptr;
    auto it = models_.find(activeModelName_);
    return (it != models_.end()) ? it->second.get() : nullptr;
}

void AssetRegistry::shutdown() {
    if (!ctx_ || ctx_->device() == VK_NULL_HANDLE) return;

    for (auto& [k, mptr] : models_) {
        if (mptr) mptr->destroy();
    }
    models_.clear();
    animationLibrary_.clear();
    activeModelName_.clear();

    // Phase 3 段階: テクスチャ/マテリアルを破棄
    // Material は DescriptorSet (pool 破棄で自動解放) なので destroy() は no-op。
    // Texture は VkImage/VkDeviceMemory を持つので明示破棄が必要。
    for (auto& [k, mptr] : materials_) {
        if (mptr) mptr->destroy();
    }
    materials_.clear();
    for (auto& [k, tptr] : textures_) {
        if (tptr) tptr->destroy();
    }
    textures_.clear();

    defaultMaterial_.destroy();
    defaultTexture_.destroy();

    if (materialPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->device(), materialPool_, nullptr);
        materialPool_ = VK_NULL_HANDLE;
    }
    if (materialSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx_->device(), materialSetLayout_, nullptr);
        materialSetLayout_ = VK_NULL_HANDLE;
    }

    defaultMesh_.destroy();
    ctx_ = nullptr;
    resources_ = nullptr;
}

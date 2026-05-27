// src/renderer/asset_registry.cpp
#include "renderer/asset_registry.h"

#include "renderer/deletion_queue.h"
#include "renderer/bindless_texture_registry.h"

#include <cstdio>
#include <iostream>
#include <stdexcept>

#include "renderer/model_loader.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

namespace {
}  // namespace

void AssetRegistry::init(VulkanContext* ctx, ResourceFactory* resources,
                         const std::string& assetDir,
                         BindlessTextureRegistry* bindless,
                         DeletionQueue* deletionQueue) {
    bindless_ = bindless;
    ctx_ = ctx;
    resources_ = resources;
    assetDir_ = assetDir;

    // Phase 2B PART3a: stand up the shared geometry megabuffer BEFORE any mesh is
    // created, so cube/grass/Models can allocate their geometry into it.
    if (deletionQueue) geometry_.init(ctx, resources, deletionQueue);

    createDefaultMesh();
    createGrass();  // Phase 1F
    createDefaultTexture();
    createDefaultMaterial();
    // Phase 1K-2: unified PBR material registry (SSBO + BDA)
    materialRegistry_.init(ctx, resources);
    createGrassMaterial();  // S6-b: needs registry init + grass bindless idx (createGrass ran above)

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

    // grass_field is registered above, so the shared terrain can reference it.
    createSharedFlatTerrain();
}

void AssetRegistry::createDefaultMesh() {
    // 既定 cube mesh: 足元基準 [-0.5,+0.5] x [0,1] x [-0.5,+0.5] のコード生成。
    // cube.obj ファイル不要。 物理 AABB (足元基準) と完全に一致する。
    defaultMesh_.createCube(ctx_, resources_, &geometry_);
}

void AssetRegistry::createSharedFlatTerrain() {
    // A flat grass ground for lightweight scenes (title/menu) that don't build a
    // full world. We hand-write the rectangle + a flat height function here so
    // this renderer-layer code does NOT depend on the world-layer polygon/profile
    // helpers (that dependency would point the wrong way).
    const float half = 50.f;
    const std::vector<glm::vec2> rect = {
        {-half, -half}, {half, -half}, {half, half}, {-half, half}};
    const TerrainMesh::HeightFunc flat = [](float, float) { return 0.f; };
    const Material* grassMat = getMaterial("grass_field");
    sharedFlatTerrain_.init(ctx_, resources_, rect, 0.f, flat,
                            /*cellSize=*/2.0f, /*uvScale=*/3.0f, grassMat,
                            /*geom=*/&geometry_);  // PART3c: terrain on megabuffer
}

void AssetRegistry::createGrass() {
    grassMesh_.createCrossQuad(ctx_, resources_, &geometry_);
    const int W = 128, H = 128;
    std::vector<uint8_t> px(static_cast<size_t>(W) * H * 4, 0);

    auto plot = [&](int x, int y, float r, float g, float b, float a) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        const size_t i = (static_cast<size_t>(y) * W + x) * 4;
        if (a * 255.f > px[i + 3]) {
            px[i + 0] = static_cast<uint8_t>(r * 255.f);
            px[i + 1] = static_cast<uint8_t>(g * 255.f);
            px[i + 2] = static_cast<uint8_t>(b * 255.f);
            px[i + 3] = static_cast<uint8_t>(a * 255.f);
        }
    };

    // One curved, tapered leaf. baseX/tipSpread in 0..1 of width; hgt in 0..1.
    auto leaf = [&](float baseX, float tipSpread, float hgt, float baseW, float hueShift) {
        const int yTop = static_cast<int>((1.0f - hgt) * H);
        for (int y = H - 1; y >= yTop; --y) {
            const float raw = (H - 1 - y) / float(H - 1);
            const float t = raw / (hgt > 0.01f ? hgt : 1.f);  // 0 root .. 1 tip
            const float tt = (t < 0.f) ? 0.f : (t > 1.f ? 1.f : t);
            const float curveX = baseX + tipSpread * (tt * tt);
            const float w = baseW * (1.0f - tt * 0.92f);
            const float cxPix = curveX * W;
            const float wPix = w * W;
            const float r = (0.10f + 0.28f * tt) + hueShift * 0.10f;
            const float g = (0.32f + 0.40f * tt);
            const float b = (0.04f + 0.10f * tt);
            const int x0 = static_cast<int>(cxPix - wPix);
            const int x1 = static_cast<int>(cxPix + wPix);
            for (int x = x0; x <= x1; ++x) {
                const float d = std::fabs((x + 0.5f) - cxPix) / (wPix + 0.0001f);
                const float a = (d >= 1.f) ? 0.f : 1.0f;
                if (a > 0.f) plot(x, y, r, g, b, a);
            }
        }
    };

    // A clump of thin leaves fanning out from the base, like a real grass tuft.
    const float defs[][5] = {
        {0.50f,  0.00f, 1.00f, 0.018f,  0.0f},
        {0.49f, -0.12f, 0.95f, 0.016f, -0.3f},
        {0.51f,  0.12f, 0.96f, 0.016f,  0.2f},
        {0.48f, -0.24f, 0.86f, 0.015f,  0.4f},
        {0.52f,  0.24f, 0.88f, 0.015f, -0.2f},
        {0.47f, -0.34f, 0.74f, 0.014f,  0.1f},
        {0.53f,  0.34f, 0.76f, 0.014f,  0.3f},
        {0.50f, -0.06f, 0.90f, 0.015f,  0.5f},
        {0.50f,  0.06f, 0.92f, 0.015f, -0.4f},
        {0.46f, -0.42f, 0.62f, 0.013f,  0.0f},
        {0.54f,  0.42f, 0.64f, 0.013f,  0.2f},
        {0.49f, -0.18f, 0.82f, 0.014f, -0.1f},
        {0.51f,  0.18f, 0.84f, 0.014f,  0.4f},
        {0.50f,  0.00f, 0.70f, 0.012f,  0.3f},
    };
    for (const auto& d : defs) leaf(d[0], d[1], d[2], d[3], d[4]);
    grassTexture_.loadFromRawRGBA(ctx_, resources_, px.data(), W, H);
    if (bindless_) {
        const uint32_t idx = bindless_->registerTexture(grassTexture_.view(), grassTexture_.sampler());
        if (idx != UINT32_MAX) {
            grassTexture_.setBindlessIndex(idx);
        std::cout << "grass texture bindless idx = " << idx << "\n";
        }
    }
}

void AssetRegistry::createGrassMaterial() {
    // S6-b: grass joins the unified materialId+bindless path. The grass blade
    // texture is already in the bindless array (createGrass), so register a
    // GpuMaterial whose albedoIdx points at it and keep the returned id on
    // grassMaterial_. No Material::init / descriptor set: grass draws via the
    // bindless set (set=1), like static/skinned. MaterialRegistry::upload()
    // (already wired per-frame) pushes it to the GPU.
    myengine::shared::GpuMaterial gm{};
    gm.baseColorFactor = glm::vec4(1.0f);
    gm.metallic = 0.0f;
    gm.roughness = 0.8f;
    gm.emissiveStrength = 0.0f;
    gm.albedoIdx = (grassTexture_.bindlessIndex() != UINT32_MAX)
        ? static_cast<int>(grassTexture_.bindlessIndex()) : -1;
    gm.normalIdx = -1; gm.mrIdx = -1; gm.aoIdx = -1; gm.emissiveIdx = -1;
    const uint32_t matId = materialRegistry_.add("grass_blade", gm);
    grassMaterial_.setBindlessIndex(grassTexture_.bindlessIndex());
    grassMaterial_.setMaterialId(matId);
    std::cout << "[AssetRegistry] grass_blade material id = " << matId
              << " (albedoIdx=" << gm.albedoIdx << ")\n";
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
    // S6-c: no descriptor set; default material carries only bindlessIndex/materialId
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
    // Phase 1D: copy the bindless index from the texture to the material
    // so the renderer can pick the right slot in the bindless array.
    mat->setBindlessIndex(tex->bindlessIndex());
    // Phase 1K-2 S4-d: register this named material in the SSBO too, so terrain
    // and anything using a named material can sample via materialId.
    {
        myengine::shared::GpuMaterial gm{};
        gm.baseColorFactor = glm::vec4(1.0f);
        gm.metallic = 0.0f;
        gm.roughness = 0.5f;
        gm.emissiveStrength = 0.0f;
        gm.albedoIdx = (tex->bindlessIndex() != UINT32_MAX)
            ? static_cast<int>(tex->bindlessIndex()) : -1;
        gm.normalIdx = -1; gm.mrIdx = -1; gm.aoIdx = -1; gm.emissiveIdx = -1;
        uint32_t matId = materialRegistry_.add("named:" + name, gm);
        mat->setMaterialId(matId);
    }
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

    materialRegistry_.shutdown();  // Phase 1K-2
    geometry_.shutdown();          // Phase 2B PART3a

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

    defaultMesh_.destroy();
    grassMesh_.destroy();        // Phase 1F: was leaking (init'd in createGrass, never freed)
    grassTexture_.destroy();     // Phase 1F: was leaking
    sharedFlatTerrain_.destroy();
    ctx_ = nullptr;
    resources_ = nullptr;
}

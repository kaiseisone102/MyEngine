// src/renderer/asset_registry.cpp
#include "renderer/asset_registry.h"

#include <iostream>

#include "renderer/model_loader.h"

void AssetRegistry::init(VulkanContext* ctx, ResourceFactory* resources,
                         const std::string& assetDir) {
    ctx_ = ctx;
    resources_ = resources;
    mesh_.loadFromObj(ctx, resources, assetDir + "cube.obj");
    texture_.loadFromFileOrCheckerboard(ctx, resources, assetDir + "texture.png");
}

void AssetRegistry::shutdown() {
    if (hasActiveModel_) {
        activeModel_.destroy();
        hasActiveModel_ = false;
    }
    texture_.destroy();
    mesh_.destroy();
    ctx_ = nullptr;
    resources_ = nullptr;
}

void AssetRegistry::loadModelFromFile(const std::string& path) {
    if (!ctx_ || !resources_) {
        std::cerr << "[AssetRegistry] not initialized\n";
        return;
    }
    // 既存があれば破棄
    if (hasActiveModel_) {
        activeModel_.destroy();
        hasActiveModel_ = false;
    }
    // ロード (失敗時は空 Model が返る)
    activeModel_ = ModelLoader::load(ctx_, resources_, path);
    if (activeModel_.empty()) {
        std::cerr << "[AssetRegistry] failed to load model: " << path << "\n";
        return;
    }
    hasActiveModel_ = true;
    std::cout << "[AssetRegistry] active model set: " << path << "\n";
}

void AssetRegistry::clearActiveModel() {
    if (hasActiveModel_) {
        activeModel_.destroy();
        hasActiveModel_ = false;
    }
}

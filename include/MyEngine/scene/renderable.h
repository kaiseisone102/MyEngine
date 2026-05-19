#pragma once
// =============================================================================
// renderable.h — + WaterRenderable
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/water.h"  // WaterDrawParams
#include "renderer/render_queue.h"

class Model;
class TerrainMesh;
class Material;
class WaterMesh;

namespace material {
class IMaterial;
}

namespace scene {

class IRenderable {
   public:
    virtual ~IRenderable() = default;
    virtual void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const = 0;
    virtual const material::IMaterial* material() const { return nullptr; }

    float alpha() const { return alpha_; }
    void setAlpha(float a) { alpha_ = a; }

   protected:
    float alpha_ = 1.0f;
};

class CubeRenderable : public IRenderable {
   public:
    CubeRenderable() = default;
    explicit CubeRenderable(const ::Material* mat) : material_(mat) {}

    void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const override {
        render::RenderCommand cmd;
        cmd.kind = render::RenderCommand::Kind::Cube;
        cmd.worldMatrix = worldMatrix;
        cmd.meshMaterial = material_;
        cmd.alpha = alpha_;
        queue.add(cmd);
    }

    const ::Material* materialPtr() const { return material_; }

   private:
    const ::Material* material_ = nullptr;
};

class StaticMeshRenderable : public IRenderable {
   public:
    explicit StaticMeshRenderable(const Model* model) : model_(model) {}

    void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const override {
        if (!model_) return;
        render::RenderCommand cmd;
        cmd.kind = render::RenderCommand::Kind::StaticMesh;
        cmd.worldMatrix = worldMatrix;
        cmd.model = model_;
        cmd.alpha = alpha_;
        queue.add(cmd);
    }

    const Model* sourceModel() const { return model_; }

   private:
    const Model* model_ = nullptr;
};

class SkinnedMeshRenderable : public IRenderable {
   public:
    SkinnedMeshRenderable(const Model* model, int skinOffset)
        : model_(model), skinOffset_(skinOffset) {}

    void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const override {
        if (!model_) return;
        render::RenderCommand cmd;
        cmd.kind = render::RenderCommand::Kind::SkinnedMesh;
        cmd.worldMatrix = worldMatrix;
        cmd.model = model_;
        cmd.skinOffset = skinOffset_;
        cmd.alpha = alpha_;
        queue.add(cmd);
    }

    const Model* sourceModel() const { return model_; }
    int skinOffset() const { return skinOffset_; }
    void setSkinOffset(int offset) { skinOffset_ = offset; }

   private:
    const Model* model_ = nullptr;
    int skinOffset_ = 0;
};

class TerrainRenderable : public IRenderable {
   public:
    explicit TerrainRenderable(const TerrainMesh* terrain, const ::Material* mat = nullptr)
        : terrain_(terrain), material_(mat) {}

    void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const override {
        if (!terrain_) return;
        render::RenderCommand cmd;
        cmd.kind = render::RenderCommand::Kind::Terrain;
        cmd.worldMatrix = worldMatrix;
        cmd.terrain = terrain_;
        cmd.meshMaterial = material_;
        cmd.alpha = alpha_;
        queue.add(cmd);
    }

    const TerrainMesh* terrain() const { return terrain_; }
    const ::Material* materialPtr() const { return material_; }

   private:
    const TerrainMesh* terrain_ = nullptr;
    const ::Material* material_ = nullptr;
};

class WaterRenderable : public IRenderable {
   public:
    WaterRenderable(const WaterMesh* mesh, const WaterDrawParams& params)
        : mesh_(mesh), params_(params) {}

    void enqueue(render::RenderQueue& queue, const glm::mat4& worldMatrix) const override {
        if (!mesh_) return;
        render::RenderCommand cmd;
        cmd.kind = render::RenderCommand::Kind::Water;
        cmd.worldMatrix = worldMatrix;
        cmd.waterMesh = mesh_;
        cmd.waterParams = params_;
        cmd.alpha = 1.0f;  // 水固有の alpha は params.shallow/deepColor.a で決まる
        queue.add(cmd);
    }

    const WaterMesh* mesh() const { return mesh_; }

   private:
    const WaterMesh* mesh_ = nullptr;
    WaterDrawParams params_{};
};

}  // namespace scene

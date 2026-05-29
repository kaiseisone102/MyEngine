#pragma once
// =============================================================================
// terrain_mesh.h — 多角形ベース地形メッシュ (poly2tri + spatial hash 最適化版)
// =============================================================================
// 最適化:
//   三角形を XZ 平面 grid に分配して sampleHeight / sampleNormal を高速化。
//   セル幅 = cellSize * 4、 各セルに「重なる三角形のインデックス」 を保存。
//
// 公開 API 追加:
//   worldCenter() — bbox の XZ 中心 (距離カリング用)
// =============================================================================

#include <vulkan/vulkan.h>
#include "renderer/vma_buffer.h"
#include "renderer/geometry_buffer.h"  // MeshHandle (PART3c: terrain on megabuffer)

#include <cstdint>
#include <functional>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class VulkanContext;
class ResourceFactory;
class Material;
class GeometryBuffer;

class TerrainMesh {
   public:
    using HeightFunc = std::function<float(float, float)>;

    void init(const VulkanContext* ctx, const ResourceFactory* resources,
              const std::vector<glm::vec2>& polygonXZ, float baseY,
              const HeightFunc& heightFunc, float cellSize = 1.5f,
              float uvScale = 1.f, const Material* material = nullptr,
              GeometryBuffer* geom = nullptr);  // PART3c: non-null -> megabuffer

    void destroy();

    void bind(VkCommandBuffer cmd) const;
    uint32_t indexCount() const { return indexCount_; }
    // PART3c: megabuffer range (0 on the legacy private-buffer path).
    uint32_t firstIndex() const { return firstIndex_; }
    int32_t vertexOffset() const { return vertexOffset_; }
    uint32_t blockIndex() const { return blockIndex_; }
    bool onGeometryBuffer() const { return geom_ != nullptr; }

    const Material* material() const { return material_; }

    // 物理判定 API
    float sampleHeight(float worldX, float worldZ) const;
    glm::vec3 sampleNormal(float worldX, float worldZ) const;
    bool isInsideXZ(float worldX, float worldZ) const;

    // カリング用: 多角形 bbox の中心 (XZ 平面)、 Y は baseY
    glm::vec3 worldCenter() const;
    // bbox の半径 (= bbox 対角線の半分)、 カリングの保守判定用
    float boundingRadius() const;

   private:
    const VulkanContext* ctx_ = nullptr;

    // Private DEVICE_LOCAL buffers used while terrain is on the legacy CPU path
    // (Phase 2F will move terrain to its own GeometryBuffer bucket; until then,
    // each TerrainMesh owns its own pair). VMA-managed, no raw vkAllocateMemory.
    VmaBuffer vertexBuffer_;
    VmaBuffer indexBuffer_;
    uint32_t indexCount_ = 0;

    // PART3c: when uploaded into the shared GeometryBuffer, geom_ is set and
    // firstIndex_/vertexOffset_/blockIndex_ locate this terrain in the megabuffers
    // (the private VkUnique buffers above stay empty). bind() is hybrid.
    GeometryBuffer* geom_ = nullptr;
    MeshHandle handle_{};
    uint32_t firstIndex_ = 0;
    int32_t vertexOffset_ = 0;
    uint32_t blockIndex_ = 0;

    std::vector<glm::vec2> polygon_;
    glm::vec2 bboxMin_{0.f};
    glm::vec2 bboxMax_{0.f};
    float baseY_ = 0.f;

    const Material* material_ = nullptr;

    struct CpuVertex {
        glm::vec3 pos;
        glm::vec3 normal;
    };
    std::vector<CpuVertex> cpuVerts_;
    std::vector<uint32_t> cpuIndices_;

    // ─── Spatial Hash (sampleHeight 高速化) ────────────────
    float gridCellSize_ = 0.f;
    int gridCellsX_ = 0;
    int gridCellsZ_ = 0;
    // triGrid_[cellIndex] = そのセルと交差する三角形の「三角形 ID リスト」
    // 三角形 ID t → cpuIndices_[t*3 .. t*3+2] が頂点インデックス
    std::vector<std::vector<uint32_t>> triGrid_;

    int cellIndex(int cx, int cz) const { return cz * gridCellsX_ + cx; }
    // (worldX, worldZ) を含むセルの (cx, cz) を返す。 範囲外なら -1, -1。
    void cellFromWorld(float worldX, float worldZ, int& cx, int& cz) const;
    void buildSpatialHash();

    static bool pointInPolygon(const std::vector<glm::vec2>& poly, float x, float z);
    static bool triangleContainsXZ(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                     float x, float z, glm::vec3& outBary);

    // Upload `size` bytes from `src` to a device-local VmaBuffer via a host-visible
    // staging buffer + copyBufferRegion. Used by the legacy private-buffer path
    // (when no GeometryBuffer is supplied at init).
    void uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                      VkBufferUsageFlags usage, VmaBuffer& buffer);
};

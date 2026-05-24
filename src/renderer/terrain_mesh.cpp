// =============================================================================
// terrain_mesh.cpp — 多角形ベース + spatial hash 最適化
// =============================================================================
#include "renderer/terrain_mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <poly2tri/poly2tri.h>

#include "renderer/mesh.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

namespace {

void polygonAABB(const std::vector<glm::vec2>& poly, glm::vec2& mn, glm::vec2& mx) {
    if (poly.empty()) {
        mn = {0.f, 0.f};
        mx = {0.f, 0.f};
        return;
    }
    mn = poly[0];
    mx = poly[0];
    for (const auto& p : poly) {
        mn.x = std::min(mn.x, p.x);
        mn.y = std::min(mn.y, p.y);
        mx.x = std::max(mx.x, p.x);
        mx.y = std::max(mx.y, p.y);
    }
}

bool nearlyEqual(const glm::vec2& a, const glm::vec2& b, float eps = 1e-4f) {
    return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps;
}

std::vector<glm::vec2> subdivideBoundary(const std::vector<glm::vec2>& polygon, float cellSize) {
    std::vector<glm::vec2> result;
    const int n = static_cast<int>(polygon.size());
    if (n < 3) return polygon;

    for (int i = 0; i < n; ++i) {
        const glm::vec2& a = polygon[i];
        const glm::vec2& b = polygon[(i + 1) % n];
        result.push_back(a);

        const float len = glm::length(b - a);
        const int subdiv = static_cast<int>(std::floor(len / cellSize));
        for (int k = 1; k < subdiv; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(subdiv);
            result.push_back(a + (b - a) * t);
        }
    }
    return result;
}

}  // namespace

bool TerrainMesh::pointInPolygon(const std::vector<glm::vec2>& poly, float x, float z) {
    if (poly.size() < 3) return false;
    bool inside = false;
    const int n = static_cast<int>(poly.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const glm::vec2& a = poly[i];
        const glm::vec2& b = poly[j];
        if (((a.y > z) != (b.y > z))) {
            const float t = (z - a.y) / (b.y - a.y);
            const float xIntersect = a.x + t * (b.x - a.x);
            if (x < xIntersect) inside = !inside;
        }
    }
    return inside;
}

bool TerrainMesh::triangleContainsXZ(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                       float x, float z, glm::vec3& outBary) {
    const float x0 = v0.x, z0 = v0.z;
    const float x1 = v1.x, z1 = v1.z;
    const float x2 = v2.x, z2 = v2.z;

    const float denom = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2);
    if (std::abs(denom) < 1e-12f) return false;

    const float w0 = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / denom;
    const float w1 = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / denom;
    const float w2 = 1.0f - w0 - w1;

    const float eps = -1e-5f;
    if (w0 < eps || w1 < eps || w2 < eps) return false;

    outBary = glm::vec3{w0, w1, w2};
    return true;
}

void TerrainMesh::cellFromWorld(float worldX, float worldZ, int& cx, int& cz) const {
    if (gridCellSize_ <= 0.f || gridCellsX_ <= 0 || gridCellsZ_ <= 0) {
        cx = -1; cz = -1;
        return;
    }
    cx = static_cast<int>(std::floor((worldX - bboxMin_.x) / gridCellSize_));
    cz = static_cast<int>(std::floor((worldZ - bboxMin_.y) / gridCellSize_));
    if (cx < 0 || cx >= gridCellsX_ || cz < 0 || cz >= gridCellsZ_) {
        cx = -1; cz = -1;
    }
}

void TerrainMesh::buildSpatialHash() {
    // 各三角形を「その三角形の AABB が交差する全セル」 に登録。
    triGrid_.clear();
    triGrid_.resize(gridCellsX_ * gridCellsZ_);

    const size_t triCount = cpuIndices_.size() / 3;
    for (size_t t = 0; t < triCount; ++t) {
        const glm::vec3& v0 = cpuVerts_[cpuIndices_[t * 3 + 0]].pos;
        const glm::vec3& v1 = cpuVerts_[cpuIndices_[t * 3 + 1]].pos;
        const glm::vec3& v2 = cpuVerts_[cpuIndices_[t * 3 + 2]].pos;

        // 三角形の XZ AABB
        const float minX = std::min({v0.x, v1.x, v2.x});
        const float maxX = std::max({v0.x, v1.x, v2.x});
        const float minZ = std::min({v0.z, v1.z, v2.z});
        const float maxZ = std::max({v0.z, v1.z, v2.z});

        // AABB が交差するセル範囲を計算
        int cx0 = static_cast<int>(std::floor((minX - bboxMin_.x) / gridCellSize_));
        int cz0 = static_cast<int>(std::floor((minZ - bboxMin_.y) / gridCellSize_));
        int cx1 = static_cast<int>(std::floor((maxX - bboxMin_.x) / gridCellSize_));
        int cz1 = static_cast<int>(std::floor((maxZ - bboxMin_.y) / gridCellSize_));

        cx0 = std::clamp(cx0, 0, gridCellsX_ - 1);
        cz0 = std::clamp(cz0, 0, gridCellsZ_ - 1);
        cx1 = std::clamp(cx1, 0, gridCellsX_ - 1);
        cz1 = std::clamp(cz1, 0, gridCellsZ_ - 1);

        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                triGrid_[cellIndex(cx, cz)].push_back(static_cast<uint32_t>(t));
            }
        }
    }
}

void TerrainMesh::init(const VulkanContext* ctx, const ResourceFactory* resources,
                        const std::vector<glm::vec2>& polygonXZ, float baseY,
                        const HeightFunc& heightFunc, float cellSize,
                        float uvScale, const Material* material) {
    if (!ctx || !resources) throw std::runtime_error("TerrainMesh::init: null ctx/resources");
    if (polygonXZ.size() < 3) throw std::runtime_error("TerrainMesh::init: polygon must have >= 3 vertices");
    if (cellSize <= 0.f) cellSize = 1.0f;
    if (uvScale <= 0.f) uvScale = 1.f;

    ctx_ = ctx;
    polygon_ = polygonXZ;
    baseY_ = baseY;
    material_ = material;

    polygonAABB(polygon_, bboxMin_, bboxMax_);

    std::vector<glm::vec2> boundary = subdivideBoundary(polygon_, cellSize);

    std::vector<glm::vec2> boundaryDedup;
    boundaryDedup.reserve(boundary.size());
    for (size_t i = 0; i < boundary.size(); ++i) {
        if (boundaryDedup.empty() || !nearlyEqual(boundary[i], boundaryDedup.back())) {
            boundaryDedup.push_back(boundary[i]);
        }
    }
    if (boundaryDedup.size() >= 2 && nearlyEqual(boundaryDedup.front(), boundaryDedup.back())) {
        boundaryDedup.pop_back();
    }

    std::vector<glm::vec2> interiorPoints;
    {
        const float bboxW = bboxMax_.x - bboxMin_.x;
        const float bboxH = bboxMax_.y - bboxMin_.y;
        const int gridX = std::max(2, static_cast<int>(std::floor(bboxW / cellSize)));
        const int gridZ = std::max(2, static_cast<int>(std::floor(bboxH / cellSize)));

        const float boundaryMargin = cellSize * 0.3f;
        const float marginSq = boundaryMargin * boundaryMargin;

        std::vector<std::pair<glm::vec2, glm::vec2>> edges;
        const int n = static_cast<int>(polygon_.size());
        for (int i = 0; i < n; ++i) {
            edges.emplace_back(polygon_[i], polygon_[(i + 1) % n]);
        }

        auto pointSegDistSq = [](const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
            const glm::vec2 ab = b - a;
            const glm::vec2 ap = p - a;
            const float t = glm::clamp(glm::dot(ap, ab) / glm::dot(ab, ab), 0.f, 1.f);
            const glm::vec2 closest = a + ab * t;
            const glm::vec2 d = p - closest;
            return glm::dot(d, d);
        };

        for (int j = 1; j < gridZ; ++j) {
            const float fz = bboxMin_.y + (static_cast<float>(j) / static_cast<float>(gridZ)) * bboxH;
            for (int i = 1; i < gridX; ++i) {
                const float fx = bboxMin_.x + (static_cast<float>(i) / static_cast<float>(gridX)) * bboxW;
                if (!pointInPolygon(polygon_, fx, fz)) continue;

                bool tooClose = false;
                for (const auto& e : edges) {
                    if (pointSegDistSq({fx, fz}, e.first, e.second) < marginSq) {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose) continue;

                interiorPoints.push_back({fx, fz});
            }
        }
    }

    std::vector<p2t::Point*> p2tBoundary;
    p2tBoundary.reserve(boundaryDedup.size());
    for (const auto& v : boundaryDedup) {
        p2tBoundary.push_back(new p2t::Point(v.x, v.y));
    }

    std::vector<p2t::Point*> p2tInterior;
    p2tInterior.reserve(interiorPoints.size());
    for (const auto& v : interiorPoints) {
        p2tInterior.push_back(new p2t::Point(v.x, v.y));
    }

    p2t::CDT cdt(p2tBoundary);
    for (p2t::Point* pt : p2tInterior) {
        cdt.AddPoint(pt);
    }

    cdt.Triangulate();
    std::vector<p2t::Triangle*> p2tTriangles = cdt.GetTriangles();

    std::unordered_map<p2t::Point*, uint32_t> pointToIdx;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec3 vertexColor{1.f, 1.f, 1.f};

    auto getOrAddVertex = [&](p2t::Point* p) -> uint32_t {
        auto it = pointToIdx.find(p);
        if (it != pointToIdx.end()) return it->second;

        const float wx = static_cast<float>(p->x);
        const float wz = static_cast<float>(p->y);
        const float wy = baseY_ + heightFunc(wx, wz);

        Vertex v;
        v.pos = glm::vec3{wx, wy, wz};
        v.color = vertexColor;
        v.texCoord = glm::vec2{wx / uvScale, wz / uvScale};
        v.normal = glm::vec3{0.f, 1.f, 0.f};

        const uint32_t idx = static_cast<uint32_t>(vertices.size());
        vertices.push_back(v);
        pointToIdx[p] = idx;
        return idx;
    };

    indices.reserve(p2tTriangles.size() * 3);
    for (p2t::Triangle* tri : p2tTriangles) {
        const uint32_t i0 = getOrAddVertex(tri->GetPoint(0));
        const uint32_t i1 = getOrAddVertex(tri->GetPoint(1));
        const uint32_t i2 = getOrAddVertex(tri->GetPoint(2));
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
    }

    std::vector<glm::vec3> accumNormals(vertices.size(), glm::vec3{0.f});
    for (size_t t = 0; t < indices.size(); t += 3) {
        const uint32_t a = indices[t + 0];
        const uint32_t b = indices[t + 1];
        const uint32_t c = indices[t + 2];
        const glm::vec3& pa = vertices[a].pos;
        const glm::vec3& pb = vertices[b].pos;
        const glm::vec3& pc = vertices[c].pos;
        glm::vec3 n = glm::cross(pb - pa, pc - pa);
        if (n.y < 0.f) n = -n;
        accumNormals[a] += n;
        accumNormals[b] += n;
        accumNormals[c] += n;
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        const float lenSq = glm::dot(accumNormals[i], accumNormals[i]);
        if (lenSq > 1e-8f) {
            vertices[i].normal = accumNormals[i] / std::sqrt(lenSq);
        } else {
            vertices[i].normal = glm::vec3{0.f, 1.f, 0.f};
        }
    }

    indexCount_ = static_cast<uint32_t>(indices.size());

    cpuVerts_.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        cpuVerts_[i].pos = vertices[i].pos;
        cpuVerts_[i].normal = vertices[i].normal;
    }
    cpuIndices_ = indices;

    // ─── Spatial Hash 構築 ─────────────────────────────────
    // セル幅は cellSize * 4 程度が経験上ベスト。
    // 100m × 100m の terrain で cellSize=1.5 なら grid セル幅 6m → 16×16 = 256 セル。
    // 各セル平均 ~30 三角形なら、 sampleHeight 1 回で 30 三角形だけチェック。
    gridCellSize_ = cellSize * 4.f;
    const float bboxW = bboxMax_.x - bboxMin_.x;
    const float bboxH = bboxMax_.y - bboxMin_.y;
    gridCellsX_ = std::max(1, static_cast<int>(std::ceil(bboxW / gridCellSize_)));
    gridCellsZ_ = std::max(1, static_cast<int>(std::ceil(bboxH / gridCellSize_)));
    buildSpatialHash();

    uploadBuffer(resources, vertices.data(), sizeof(Vertex) * vertices.size(),
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexBufferMemory_);
    uploadBuffer(resources, indices.data(), sizeof(uint32_t) * indices.size(),
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexBufferMemory_);

    for (p2t::Point* p : p2tBoundary) delete p;
    for (p2t::Point* p : p2tInterior) delete p;

    std::cout << "[TerrainMesh] init (polygon): " << polygon_.size() << " boundary, "
              << interiorPoints.size() << " interior, "
              << vertices.size() << " total verts, " << p2tTriangles.size() << " tris, "
              << "grid " << gridCellsX_ << "x" << gridCellsZ_ << " (cellW=" << gridCellSize_ << "m)\n";
}

void TerrainMesh::uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                                 VkBufferUsageFlags usage, VkUnique<VkBuffer>& buffer,
                                 VkUnique<VkDeviceMemory>& memory) const {
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    resources->createBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
        stagingMem);

    void* data = nullptr;
    vkMapMemory(ctx_->device(), stagingMem, 0, size, 0, &data);
    std::memcpy(data, src, static_cast<size_t>(size));
    vkUnmapMemory(ctx_->device(), stagingMem);

    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    resources->createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
    buffer = VkUnique<VkBuffer>(ctx_->device(), buf);
    memory = VkUnique<VkDeviceMemory>(ctx_->device(), mem);
    resources->copyBuffer(staging, buffer.get(), size);

    vkDestroyBuffer(ctx_->device(), staging, nullptr);
    vkFreeMemory(ctx_->device(), stagingMem, nullptr);
}

void TerrainMesh::bind(VkCommandBuffer cmd) const {
    const VkDeviceSize offset = 0;
    VkBuffer vb = vertexBuffer_.get();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.get(), 0, VK_INDEX_TYPE_UINT32);
}

void TerrainMesh::destroy() {
    // VkUnique frees each handle (no-op if empty). The auto destructor would do
    // the same if destroy() were never called.
    indexBuffer_.reset();
    indexBufferMemory_.reset();
    vertexBuffer_.reset();
    vertexBufferMemory_.reset();
    indexCount_ = 0;
    cpuVerts_.clear();
    cpuVerts_.shrink_to_fit();
    cpuIndices_.clear();
    cpuIndices_.shrink_to_fit();
    polygon_.clear();
    polygon_.shrink_to_fit();
    triGrid_.clear();
    triGrid_.shrink_to_fit();
    ctx_ = nullptr;
}

bool TerrainMesh::isInsideXZ(float worldX, float worldZ) const {
    if (worldX < bboxMin_.x || worldX > bboxMax_.x) return false;
    if (worldZ < bboxMin_.y || worldZ > bboxMax_.y) return false;
    return pointInPolygon(polygon_, worldX, worldZ);
}

glm::vec3 TerrainMesh::worldCenter() const {
    return glm::vec3{
        (bboxMin_.x + bboxMax_.x) * 0.5f,
        baseY_,
        (bboxMin_.y + bboxMax_.y) * 0.5f
    };
}

float TerrainMesh::boundingRadius() const {
    const float dx = bboxMax_.x - bboxMin_.x;
    const float dz = bboxMax_.y - bboxMin_.y;
    return 0.5f * std::sqrt(dx * dx + dz * dz);
}

float TerrainMesh::sampleHeight(float worldX, float worldZ) const {
    if (!isInsideXZ(worldX, worldZ) || cpuIndices_.empty() || triGrid_.empty()) {
        return std::numeric_limits<float>::lowest();
    }

    int cx, cz;
    cellFromWorld(worldX, worldZ, cx, cz);
    if (cx < 0 || cz < 0) return std::numeric_limits<float>::lowest();

    glm::vec3 bary;
    const auto& triList = triGrid_[cellIndex(cx, cz)];
    for (uint32_t t : triList) {
        const glm::vec3& v0 = cpuVerts_[cpuIndices_[t * 3 + 0]].pos;
        const glm::vec3& v1 = cpuVerts_[cpuIndices_[t * 3 + 1]].pos;
        const glm::vec3& v2 = cpuVerts_[cpuIndices_[t * 3 + 2]].pos;
        if (triangleContainsXZ(v0, v1, v2, worldX, worldZ, bary)) {
            return bary.x * v0.y + bary.y * v1.y + bary.z * v2.y;
        }
    }
    return baseY_;
}

glm::vec3 TerrainMesh::sampleNormal(float worldX, float worldZ) const {
    if (!isInsideXZ(worldX, worldZ) || cpuIndices_.empty() || triGrid_.empty()) {
        return glm::vec3{0.f, 1.f, 0.f};
    }

    int cx, cz;
    cellFromWorld(worldX, worldZ, cx, cz);
    if (cx < 0 || cz < 0) return glm::vec3{0.f, 1.f, 0.f};

    glm::vec3 bary;
    const auto& triList = triGrid_[cellIndex(cx, cz)];
    for (uint32_t t : triList) {
        const glm::vec3& v0 = cpuVerts_[cpuIndices_[t * 3 + 0]].pos;
        const glm::vec3& v1 = cpuVerts_[cpuIndices_[t * 3 + 1]].pos;
        const glm::vec3& v2 = cpuVerts_[cpuIndices_[t * 3 + 2]].pos;
        if (triangleContainsXZ(v0, v1, v2, worldX, worldZ, bary)) {
            const glm::vec3& n0 = cpuVerts_[cpuIndices_[t * 3 + 0]].normal;
            const glm::vec3& n1 = cpuVerts_[cpuIndices_[t * 3 + 1]].normal;
            const glm::vec3& n2 = cpuVerts_[cpuIndices_[t * 3 + 2]].normal;
            glm::vec3 n = bary.x * n0 + bary.y * n1 + bary.z * n2;
            const float lenSq = glm::dot(n, n);
            if (lenSq > 1e-8f) return n / std::sqrt(lenSq);
            return glm::vec3{0.f, 1.f, 0.f};
        }
    }
    return glm::vec3{0.f, 1.f, 0.f};
}

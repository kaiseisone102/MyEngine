// src/renderer/model_loader.cpp
#include "renderer/model_loader.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

// =============================================================================
// 内部ヘルパ
// =============================================================================
namespace {

// CPU 側の中間データ。GPU 転送前にここに集める。
struct SubMeshCpuData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialIndex = 0;
};

// aiVector3D → glm::vec3
inline glm::vec3 toVec3(const aiVector3D& v) { return {v.x, v.y, v.z}; }

// aiColor4D → glm::vec3 (RGB のみ)
inline glm::vec3 toVec3(const aiColor4D& c) { return {c.r, c.g, c.b}; }

// aiMesh から SubMeshCpuData を作る。
// 頂点フォーマットは Vertex {pos, color, texCoord, normal} に合わせる。
SubMeshCpuData buildSubMeshCpu(const aiMesh* mesh) {
    SubMeshCpuData out;
    out.materialIndex = mesh->mMaterialIndex;

    out.vertices.reserve(mesh->mNumVertices);
    for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v{};
        v.pos = toVec3(mesh->mVertices[i]);

        // 法線: aiProcess_GenSmoothNormals を入れているので必ずある想定だが、
        // 念のため有無をチェックする。
        if (mesh->HasNormals()) {
            v.normal = toVec3(mesh->mNormals[i]);
        } else {
            v.normal = {0.f, 1.f, 0.f};
        }

        // UV: 0番目のチャンネルだけ使う (大抵のモデルは 0 のみ)
        if (mesh->HasTextureCoords(0)) {
            v.texCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        } else {
            v.texCoord = {0.f, 0.f};
        }

        // 頂点カラー: 0番目のセットだけ。なければ白。
        // Mixamo モデルは普通カラー無しなので、ここはほぼ常に白。
        if (mesh->HasVertexColors(0)) {
            v.color = toVec3(mesh->mColors[0][i]);
        } else {
            v.color = {1.f, 1.f, 1.f};
        }

        out.vertices.push_back(v);
    }

    // インデックス: aiProcess_Triangulate により全て三角形。
    // 万一 4 頂点以上の面があったら警告して飛ばす (防衛的)。
    out.indices.reserve(mesh->mNumFaces * 3);
    for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            std::cerr << "[ModelLoader] non-triangle face encountered (numIndices="
                      << face.mNumIndices << "), skipping\n";
            continue;
        }
        out.indices.push_back(face.mIndices[0]);
        out.indices.push_back(face.mIndices[1]);
        out.indices.push_back(face.mIndices[2]);
    }
    return out;
}

// 段階C で実装する GPU 転送。今は前方宣言のみ。
// CPU データを SubMesh にアップロードする。
void uploadSubMeshToGpu(const VulkanContext* ctx, const ResourceFactory* resources,
                        const SubMeshCpuData& cpu, SubMesh& gpu);

}  // namespace

// =============================================================================
// probe() — Phase 1-B 診断
// =============================================================================
bool ModelLoader::probe(const std::string& path) {
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "[ModelLoader] FILE NOT OPENABLE: " << path << "\n";
            return false;
        }
    }

    Assimp::Importer importer;
    const unsigned flags = aiProcess_Triangulate;
    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) {
        std::cerr << "[ModelLoader] failed to load: " << path << "\n";
        std::cerr << "[ModelLoader] reason: '" << importer.GetErrorString() << "'\n";
        return false;
    }

    unsigned totalVertices = 0;
    unsigned totalFaces = 0;
    unsigned bonesAcrossMeshes = 0;
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        totalVertices += mesh->mNumVertices;
        totalFaces += mesh->mNumFaces;
        bonesAcrossMeshes += mesh->mNumBones;
    }

    std::cout << "[ModelLoader] loaded: " << path << "\n";
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cout << "  WARNING: scene is INCOMPLETE\n";
    }
    std::cout << "  meshes      : " << scene->mNumMeshes << "\n";
    std::cout << "  materials   : " << scene->mNumMaterials << "\n";
    std::cout << "  textures    : " << scene->mNumTextures << " (embedded)\n";
    std::cout << "  vertices    : " << totalVertices << "\n";
    std::cout << "  faces       : " << totalFaces << "\n";
    std::cout << "  bones       : " << bonesAcrossMeshes << "\n";
    std::cout << "  animations  : " << scene->mNumAnimations << "\n";
    std::cout << "  has skin    : " << (bonesAcrossMeshes > 0 ? "yes" : "no") << "\n";
    return true;
}

// =============================================================================
// load() — 本番ロード
// =============================================================================
Model ModelLoader::load(const VulkanContext* ctx, const ResourceFactory* resources,
                        const std::string& path) {
    Model model;

    if (!ctx || !resources) {
        std::cerr << "[ModelLoader::load] null ctx or resources\n";
        return model;
    }

    Assimp::Importer importer;
    // load() で使うフラグ:
    //   Triangulate           : 必須 (Vulkan は三角形以外受け付けない)
    //   GenSmoothNormals      : 法線が無い場合の保険
    //   FlipUVs               : Vulkan の UV 座標系 (上下反転)
    //   JoinIdenticalVertices : 重複頂点を統合してインデックスを最適化
    const unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
                           aiProcess_JoinIdenticalVertices;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) {
        std::cerr << "[ModelLoader::load] failed: " << path << "\n";
        std::cerr << "[ModelLoader::load] reason: '" << importer.GetErrorString() << "'\n";
        return model;
    }

    if (scene->mNumMeshes == 0) {
        std::cerr << "[ModelLoader::load] no meshes in: " << path << "\n";
        return model;
    }

    // ─── 1) 全 aiMesh を SubMeshCpuData に変換 ────────────────────
    // ノード階層は今回は無視して flat に処理する。
    // (Mixamo はメッシュがほぼ全部ルート直下なので問題ない。
    //  本格的なシーン階層は将来対応)
    std::vector<SubMeshCpuData> cpuData;
    cpuData.reserve(scene->mNumMeshes);
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        cpuData.push_back(buildSubMeshCpu(scene->mMeshes[i]));
    }

    // ─── 2) GPU バッファに転送 ────────────────────────────────────
    model.ctx_ = ctx;
    model.subMeshes_.resize(cpuData.size());
    for (size_t i = 0; i < cpuData.size(); ++i) {
        SubMesh& gpu = model.subMeshes_[i];
        gpu.materialIndex = cpuData[i].materialIndex;
        gpu.indexCount = static_cast<uint32_t>(cpuData[i].indices.size());
        uploadSubMeshToGpu(ctx, resources, cpuData[i], gpu);
    }

    std::cout << "[ModelLoader::load] " << path << ": " << model.subMeshes_.size()
              << " submeshes uploaded\n";
    return model;
}

// =============================================================================
// 段階C: GPU 転送の実装
// =============================================================================
namespace {

// staging バッファ経由で DEVICE_LOCAL バッファに転送するヘルパ。
// Mesh::uploadBuffer と同じロジック。
// (将来 ResourceFactory 側に共通化する余地あり)
void uploadBuffer(const VulkanContext* ctx, const ResourceFactory* resources, const void* src,
                  VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer,
                  VkDeviceMemory& outMemory) {
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    resources->createBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
        stagingMem);

    void* data = nullptr;
    vkMapMemory(ctx->device(), stagingMem, 0, size, 0, &data);
    std::memcpy(data, src, static_cast<size_t>(size));
    vkUnmapMemory(ctx->device(), stagingMem);

    resources->createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outBuffer, outMemory);
    resources->copyBuffer(staging, outBuffer, size);

    vkDestroyBuffer(ctx->device(), staging, nullptr);
    vkFreeMemory(ctx->device(), stagingMem, nullptr);
}

void uploadSubMeshToGpu(const VulkanContext* ctx, const ResourceFactory* resources,
                        const SubMeshCpuData& cpu, SubMesh& gpu) {
    if (cpu.vertices.empty() || cpu.indices.empty()) {
        std::cerr << "[ModelLoader] empty submesh, skipping upload\n";
        return;
    }
    uploadBuffer(ctx, resources, cpu.vertices.data(), sizeof(Vertex) * cpu.vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, gpu.vertexBuffer, gpu.vertexBufferMemory);
    uploadBuffer(ctx, resources, cpu.indices.data(), sizeof(uint32_t) * cpu.indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, gpu.indexBuffer, gpu.indexBufferMemory);
}

}  // namespace

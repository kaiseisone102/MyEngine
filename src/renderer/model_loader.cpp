// src/renderer/model_loader.cpp
// =============================================================================
// + Model::localAABB の計算
//   全 submesh の全頂点 (pos) を走査して min/max を求める。
//   スケルタル モデルは bind pose のローカル座標で計算 (= スケルトンが
//   T-pose の状態の AABB)。
// =============================================================================
#include "renderer/model_loader.h"

#include "renderer/geometry_buffer.h"
#include "renderer/asset_registry.h"

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

#include "renderer/asset_registry.h"
#include "renderer/bindless_texture_registry.h"
#include "renderer/material_registry.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"
#include "renderer/animator.h"  // Phase 2G-2b: load-time bounds reuse runtime skinning math

namespace {

struct SubMeshCpuData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialIndex = 0;
    std::vector<std::string> localBoneNames;
};

inline glm::vec3 toVec3(const aiVector3D& v) { return {v.x, v.y, v.z}; }
inline glm::vec3 toVec3(const aiColor4D& c) { return {c.r, c.g, c.b}; }

inline glm::quat toQuat(const aiQuaternion& q) {
    return glm::quat(q.w, q.x, q.y, q.z);
}

void writeJointDataToVertices(const aiMesh* mesh, std::vector<Vertex>& vertices) {
    std::vector<uint8_t> slotCount(vertices.size(), 0);

    for (unsigned b = 0; b < mesh->mNumBones; ++b) {
        const aiBone* bone = mesh->mBones[b];
        for (unsigned w = 0; w < bone->mNumWeights; ++w) {
            const aiVertexWeight& vw = bone->mWeights[w];
            const unsigned vid = vw.mVertexId;
            if (vid >= vertices.size()) continue;

            uint8_t& slot = slotCount[vid];
            if (slot >= 4) {
                Vertex& v = vertices[vid];
                int minIdx = 0;
                float minW = v.jointWeights[0];
                for (int i = 1; i < 4; ++i) {
                    if (v.jointWeights[i] < minW) {
                        minW = v.jointWeights[i];
                        minIdx = i;
                    }
                }
                if (vw.mWeight > minW) {
                    v.jointIndices[minIdx] = static_cast<int>(b);
                    v.jointWeights[minIdx] = vw.mWeight;
                }
                continue;
            }
            Vertex& v = vertices[vid];
            v.jointIndices[slot] = static_cast<int>(b);
            v.jointWeights[slot] = vw.mWeight;
            slot++;
        }
    }

    for (Vertex& v : vertices) {
        const float sum =
            v.jointWeights.x + v.jointWeights.y + v.jointWeights.z + v.jointWeights.w;
        if (sum > 1e-6f) {
            v.jointWeights /= sum;
        }
    }
}

void remapJointIndicesToGlobal(std::vector<Vertex>& vertices,
                               const std::vector<std::string>& localBoneNames,
                               const Skeleton& skeleton) {
    if (localBoneNames.empty() || skeleton.empty()) return;

    std::vector<int> localToGlobal(localBoneNames.size(), 0);
    for (size_t i = 0; i < localBoneNames.size(); ++i) {
        const int g = skeleton.findBoneByName(localBoneNames[i]);
        if (g < 0) {
            std::cerr << "[ModelLoader] WARNING: local bone '" << localBoneNames[i]
                      << "' not found in skeleton, mapping to 0\n";
            localToGlobal[i] = 0;
        } else {
            localToGlobal[i] = g;
        }
    }

    for (Vertex& v : vertices) {
        for (int k = 0; k < 4; ++k) {
            const int local = v.jointIndices[k];
            if (local < 0 || static_cast<size_t>(local) >= localToGlobal.size()) {
                v.jointIndices[k] = 0;
            } else {
                v.jointIndices[k] = localToGlobal[local];
            }
        }
    }
}

SubMeshCpuData buildSubMeshCpu(const aiMesh* mesh) {
    SubMeshCpuData out;
    out.materialIndex = mesh->mMaterialIndex;
    out.localBoneNames.reserve(mesh->mNumBones);
    for (unsigned b = 0; b < mesh->mNumBones; ++b) {
        out.localBoneNames.emplace_back(mesh->mBones[b]->mName.C_Str());
    }

    out.vertices.reserve(mesh->mNumVertices);
    for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v{};
        v.pos = toVec3(mesh->mVertices[i]);
        if (mesh->HasNormals()) {
            v.normal = toVec3(mesh->mNormals[i]);
        } else {
            v.normal = {0.f, 1.f, 0.f};
        }
        if (mesh->HasTextureCoords(0)) {
            v.texCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        } else {
            v.texCoord = {0.f, 0.f};
        }
        if (mesh->HasVertexColors(0)) {
            v.color = toVec3(mesh->mColors[0][i]);
        } else {
            v.color = {1.f, 1.f, 1.f};
        }
        out.vertices.push_back(v);
    }

    if (mesh->mNumBones > 0) {
        writeJointDataToVertices(mesh, out.vertices);
    }

    out.indices.reserve(mesh->mNumFaces * 3);
    for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            std::cerr << "[ModelLoader] non-triangle face (numIndices=" << face.mNumIndices
                      << "), skipping\n";
            continue;
        }
        out.indices.push_back(face.mIndices[0]);
        out.indices.push_back(face.mIndices[1]);
        out.indices.push_back(face.mIndices[2]);
    }
    return out;
}

void uploadSubMeshToGpu(const SubMeshCpuData& cpu, SubMesh& gpu, GeometryBuffer& geom) {
    if (cpu.vertices.empty() || cpu.indices.empty()) {
        std::cerr << "[ModelLoader] empty submesh, skipping upload\n";
        return;
    }
    const MeshHandle h = geom.alloc(cpu.vertices, cpu.indices);
    gpu.geom = &geom;
    gpu.firstIndex = h.firstIndex;
    gpu.vertexOffset = h.vertexOffset;
    gpu.blockIndex = h.blockIndex;
    gpu.indexCount = h.indexCount;    // keep consistent with the handle
    gpu.vertexCount = h.vertexCount;  // Phase 2G: needed for compute skinning dispatch
}

// linearIndices: embedded texture indices that are DATA maps (normal/MR/AO) and
// must be read as linear (UNORM); everything else is color (sRGB). Reading them
// up front in one resize avoids growing the vector later (which would force the
// caller to rely on vector reallocation not moving GPU handles).
void extractEmbeddedTextures(const VulkanContext* ctx, const ResourceFactory* resources,
                             const aiScene* scene, std::vector<Texture>& outTextures,
                             const std::set<int>& linearIndices) {
    outTextures.resize(scene->mNumTextures);
    for (unsigned i = 0; i < scene->mNumTextures; ++i) {
        const aiTexture* aitex = scene->mTextures[i];
        const bool srgb = (linearIndices.count(static_cast<int>(i)) == 0);
        if (aitex->mHeight == 0) {
            const auto* data = reinterpret_cast<const uint8_t*>(aitex->pcData);
            const size_t size = aitex->mWidth;
            outTextures[i].loadFromMemory(ctx, resources, data, size, srgb);
        } else {
            std::cerr << "[ModelLoader] uncompressed embedded texture not supported (idx=" << i
                      << "), checkerboard fallback\n";
            outTextures[i].loadFromMemory(ctx, resources, nullptr, 0, srgb);
        }
    }
}

// Normal map index (embedded *N). glTF stores it under aiTextureType_NORMALS;
// OBJ/Assimp quirk: many OBJ exporters put it under aiTextureType_HEIGHT, so we
// fall back to that. External (non-embedded) references are not supported yet.
int resolveNormalTextureIndex(const aiScene* scene, const aiMaterial* mat) {
    (void)scene;
    aiString path;
    aiReturn r = mat->GetTexture(aiTextureType_NORMALS, 0, &path);
    if (r != AI_SUCCESS) {
        r = mat->GetTexture(aiTextureType_HEIGHT, 0, &path);
    }
    if (r != AI_SUCCESS) return -1;
    const std::string s = path.C_Str();
    if (s.empty()) return -1;
    if (s[0] == '*') {
        try {
            return std::stoi(s.substr(1));
        } catch (...) {
            return -1;
        }
    }
    std::cerr << "[ModelLoader] external normal texture reference not supported: " << s << "\n";
    return -1;
}

// Metallic-roughness index (embedded *N). glTF packs roughness in G and metalness
// in B of one linear texture. assimp exposes it via aiTextureType_METALNESS (and
// aiTextureType_DIFFUSE_ROUGHNESS, usually the same image). External refs unsupported.
int resolveMRTextureIndex(const aiScene* scene, const aiMaterial* mat) {
    (void)scene;
    aiString path;
    aiReturn r = mat->GetTexture(aiTextureType_METALNESS, 0, &path);
    if (r != AI_SUCCESS) {
        r = mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    }
    if (r != AI_SUCCESS) return -1;
    const std::string s = path.C_Str();
    if (s.empty()) return -1;
    if (s[0] == '*') {
        try {
            return std::stoi(s.substr(1));
        } catch (...) {
            return -1;
        }
    }
    std::cerr << "[ModelLoader] external MR texture reference not supported: " << s << "\n";
    return -1;
}

int resolveBaseColorTextureIndex(const aiScene* scene, const aiMaterial* mat) {
    aiString path;
    aiReturn r = mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path);
    if (r != AI_SUCCESS) {
        r = mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    }
    if (r != AI_SUCCESS) return -1;
    const std::string s = path.C_Str();
    if (s.empty()) return -1;
    if (s[0] == '*') {
        try {
            return std::stoi(s.substr(1));
        } catch (...) {
            return -1;
        }
    }
    std::cerr << "[ModelLoader] external texture reference not supported: " << s << "\n";
    return -1;
}

void extractAnimationsByName(const aiScene* scene, std::vector<AnimationClip>& outAnims) {
    if (scene->mNumAnimations == 0) return;

    outAnims.reserve(outAnims.size() + scene->mNumAnimations);
    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* ai = scene->mAnimations[a];

        AnimationClip clip;
        clip.name = ai->mName.C_Str();
        const float tps =
            (ai->mTicksPerSecond > 1e-6) ? static_cast<float>(ai->mTicksPerSecond) : 25.f;
        clip.ticksPerSecond = tps;
        clip.duration = static_cast<float>(ai->mDuration) / tps;

        clip.channels.reserve(ai->mNumChannels);
        for (unsigned c = 0; c < ai->mNumChannels; ++c) {
            const aiNodeAnim* nodeAnim = ai->mChannels[c];

            AnimationChannel ch;
            ch.boneName = nodeAnim->mNodeName.C_Str();

            ch.positionKeys.reserve(nodeAnim->mNumPositionKeys);
            for (unsigned k = 0; k < nodeAnim->mNumPositionKeys; ++k) {
                const aiVectorKey& vk = nodeAnim->mPositionKeys[k];
                PositionKey pk;
                pk.time = static_cast<float>(vk.mTime) / tps;
                pk.value = toVec3(vk.mValue);
                ch.positionKeys.push_back(pk);
            }

            ch.rotationKeys.reserve(nodeAnim->mNumRotationKeys);
            for (unsigned k = 0; k < nodeAnim->mNumRotationKeys; ++k) {
                const aiQuatKey& qk = nodeAnim->mRotationKeys[k];
                RotationKey rk;
                rk.time = static_cast<float>(qk.mTime) / tps;
                rk.value = toQuat(qk.mValue);
                ch.rotationKeys.push_back(rk);
            }

            ch.scaleKeys.reserve(nodeAnim->mNumScalingKeys);
            for (unsigned k = 0; k < nodeAnim->mNumScalingKeys; ++k) {
                const aiVectorKey& vk = nodeAnim->mScalingKeys[k];
                ScaleKey sk;
                sk.time = static_cast<float>(vk.mTime) / tps;
                sk.value = toVec3(vk.mValue);
                ch.scaleKeys.push_back(sk);
            }

            clip.channels.push_back(std::move(ch));
        }

        outAnims.push_back(std::move(clip));
    }
}

// ─── 全 submesh の頂点を走査して min/max を求める ─────────
AABB computeLocalAABB(const std::vector<SubMeshCpuData>& cpuData) {
    AABB out{};
    const float inf = std::numeric_limits<float>::infinity();
    glm::vec3 mn{ inf,  inf,  inf};
    glm::vec3 mx{-inf, -inf, -inf};

    bool any = false;
    for (const auto& cpu : cpuData) {
        for (const Vertex& v : cpu.vertices) {
            mn = glm::min(mn, v.pos);
            mx = glm::max(mx, v.pos);
            any = true;
        }
    }

    if (!any) {
        return AABB::fromCenterHalf({0.f, 0.f, 0.f}, {0.5f, 0.5f, 0.5f});
    }
    return AABB::fromMinMax(mn, mx);
}

// ─── Phase 2G-2b: conservative AABB across ALL animation poses ──────────────
// Returned in MODEL-LOCAL space (same frame as localAABB). Naive sampling of
// the final mesh AABB at sparse keyframes can UNDER-bound -- Unterguggenberger
// 2021 ("Conservative Meshlet Bounds for Robust Culling of Skinned Meshes")
// warns "merely sampling along an animation interval can be insufficient" --
// which would let a swung limb poke outside the cull bounds and be wrongly
// culled. Robust approach at whole-submesh granularity:
//   1. Build each bone's BIND-POSE influence box (the verts it skins, w > 0).
//   2. Densely sample every clip (~30 poses/sec) and, per pose, transform each
//      bone's influence box by that bone's skin matrix. LBS makes a skinned
//      vertex a convex combination of S[i]*v over its influencing bones, so the
//      union of the transformed per-bone boxes conservatively contains the mesh.
//   3. Inflate by a small inter-sample margin (over-inclusion only loosens
//      culling; it can never cause a wrong cull) and union with the bind pose.
// Reuses Animator so the load-time pose math is identical to runtime skinning.
AABB computeAnimationAABB(const std::vector<SubMeshCpuData>& cpuData,
                          const Skeleton& skeleton,
                          const std::vector<AnimationClip>& clips,
                          const AABB& bindPoseAABB) {
    if (skeleton.empty() || clips.empty()) return bindPoseAABB;

    const int boneCount = skeleton.boneCount();
    const float inf = std::numeric_limits<float>::infinity();
    std::vector<glm::vec3> boneMin(boneCount, glm::vec3( inf));
    std::vector<glm::vec3> boneMax(boneCount, glm::vec3(-inf));
    std::vector<uint8_t>   boneUsed(boneCount, 0);
    for (const auto& cpu : cpuData) {
        for (const Vertex& vertex : cpu.vertices) {
            for (int k = 0; k < 4; ++k) {
                if (vertex.jointWeights[k] <= 0.f) continue;
                const int bone = vertex.jointIndices[k];  // global after remap
                if (bone < 0 || bone >= boneCount) continue;
                boneMin[bone] = glm::min(boneMin[bone], vertex.pos);
                boneMax[bone] = glm::max(boneMax[bone], vertex.pos);
                boneUsed[bone] = 1;
            }
        }
    }

    glm::vec3 worldMin( inf);
    glm::vec3 worldMax(-inf);
    bool any = false;

    Animator animator;
    std::vector<glm::mat4> skinMatrices;
    for (const AnimationClip& clip : clips) {
        animator.bind(&skeleton, &clip);
        const float duration = (clip.duration > 0.f) ? clip.duration : 0.f;
        const int samples = (duration > 0.f)
            ? std::clamp(static_cast<int>(std::ceil(duration * 30.f)), 8, 256)
            : 1;
        for (int sampleIdx = 0; sampleIdx < samples; ++sampleIdx) {
            const float t = (samples > 1)
                ? duration * (static_cast<float>(sampleIdx) / (samples - 1))
                : 0.f;
            animator.setTime(t);
            animator.computeSkinMatrices(skinMatrices);
            if (static_cast<int>(skinMatrices.size()) != boneCount) continue;
            for (int bone = 0; bone < boneCount; ++bone) {
                if (!boneUsed[bone]) continue;
                const AABB box =
                    transformAABB(skinMatrices[bone], AABB{boneMin[bone], boneMax[bone]});
                worldMin = glm::min(worldMin, box.min);
                worldMax = glm::max(worldMax, box.max);
                any = true;
            }
        }
    }

    if (!any) return bindPoseAABB;
    AABB result = AABB::fromMinMax(worldMin, worldMax);
    const float diagonal = glm::length(result.max - result.min);
    result = result.expanded(diagonal * 0.02f + 1e-3f);   // inter-sample safety margin
    result.min = glm::min(result.min, bindPoseAABB.min);  // never tighter than bind pose
    result.max = glm::max(result.max, bindPoseAABB.max);
    return result;
}

}  // namespace

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
    std::cout << "  meshes      : " << scene->mNumMeshes << "\n";
    std::cout << "  materials   : " << scene->mNumMaterials << "\n";
    std::cout << "  textures    : " << scene->mNumTextures << " (embedded)\n";
    std::cout << "  vertices    : " << totalVertices << "\n";
    std::cout << "  faces       : " << totalFaces << "\n";
    std::cout << "  bones       : " << bonesAcrossMeshes << "\n";
    std::cout << "  animations  : " << scene->mNumAnimations << "\n";
    return true;
}

bool ModelLoader::load(const VulkanContext* ctx, const ResourceFactory* resources,
                       AssetRegistry& assets, const std::string& path,
                       Model& outModel, std::vector<AnimationClip>& outAnimations) {
    if (!ctx || !resources) {
        std::cerr << "[ModelLoader::load] null ctx or resources\n";
        return false;
    }

    Assimp::Importer importer;
    const unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                           aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                           aiProcess_LimitBoneWeights;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->mRootNode) {
        std::cerr << "[ModelLoader::load] failed: " << path << "\n";
        std::cerr << "[ModelLoader::load] reason: '" << importer.GetErrorString() << "'\n";
        return false;
    }
    if (scene->mNumMeshes == 0) {
        std::cerr << "[ModelLoader::load] no meshes in: " << path << "\n";
        return false;
    }

    outModel.ctx_ = ctx;

    // Collect embedded texture indices used as normal maps so they are read as
    // linear (sRGB would skew the encoded normals). Built before extraction so
    // the textures vector is sized once and never regrown.
    std::set<int> linearTexIndices;
    for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
        const int ni = resolveNormalTextureIndex(scene, scene->mMaterials[i]);
        if (ni >= 0) linearTexIndices.insert(ni);
        const int mi = resolveMRTextureIndex(scene, scene->mMaterials[i]);
        if (mi >= 0) linearTexIndices.insert(mi);
    }
    if (scene->mNumTextures > 0) {
        extractEmbeddedTextures(ctx, resources, scene, outModel.textures_, linearTexIndices);
    }

    const Texture& fallbackTex = assets.defaultTexture();

    const unsigned matCount = (scene->mNumMaterials > 0) ? scene->mNumMaterials : 1;
    outModel.materials_.resize(matCount);
    for (unsigned i = 0; i < matCount; ++i) {
        const Texture* useTex = &fallbackTex;
        const Texture* normalTex = nullptr;
        const Texture* mrTex = nullptr;
        if (scene->mNumMaterials > 0 && i < scene->mNumMaterials) {
            const aiMaterial* aimat = scene->mMaterials[i];
            const int texIdx = resolveBaseColorTextureIndex(scene, aimat);
            if (texIdx >= 0 && static_cast<size_t>(texIdx) < outModel.textures_.size()) {
                useTex = &outModel.textures_[texIdx];
            }
            const int normalIdx = resolveNormalTextureIndex(scene, aimat);
            if (normalIdx >= 0 && static_cast<size_t>(normalIdx) < outModel.textures_.size()) {
                normalTex = &outModel.textures_[normalIdx];
            }
            const int mrIdx = resolveMRTextureIndex(scene, aimat);
            if (mrIdx >= 0 && static_cast<size_t>(mrIdx) < outModel.textures_.size()) {
                mrTex = &outModel.textures_[mrIdx];
            }
        }
        // S6-c: model materials go through materialId+bindless only (no descriptor set)
        // Phase 1K-2 S4-d: register this material in the bindless + SSBO system
        Material& mat = outModel.materials_[i];
        int albedoIdx = -1;
        if (assets.bindless() && useTex && useTex->view() != VK_NULL_HANDLE) {
            uint32_t bidx = assets.bindless()->registerTexture(useTex->view(), useTex->sampler());
            if (bidx != UINT32_MAX) {
                mat.setBindlessIndex(bidx);
                albedoIdx = static_cast<int>(bidx);
            }
        }
        int normalIdx = -1;
        if (assets.bindless() && normalTex && normalTex->view() != VK_NULL_HANDLE) {
            uint32_t nidx = assets.bindless()->registerTexture(normalTex->view(), normalTex->sampler());
            if (nidx != UINT32_MAX) {
                normalIdx = static_cast<int>(nidx);
            }
        }
        int mrIdx = -1;
        if (assets.bindless() && mrTex && mrTex->view() != VK_NULL_HANDLE) {
            uint32_t midx = assets.bindless()->registerTexture(mrTex->view(), mrTex->sampler());
            if (midx != UINT32_MAX) {
                mrIdx = static_cast<int>(midx);
            }
        }
        myengine::shared::GpuMaterial gm{};
        gm.baseColorFactor = glm::vec4(1.0f);
        gm.metallic = 0.0f;
        gm.roughness = 0.5f;
        gm.emissiveStrength = 0.0f;
        gm.albedoIdx = albedoIdx;
        gm.normalIdx = normalIdx;
        gm.mrIdx = mrIdx;
        gm.aoIdx = -1;
        gm.emissiveIdx = -1;
        const std::string matName = path + "#mat" + std::to_string(i);
        uint32_t matId = assets.materialRegistry().add(matName, gm);
        mat.setMaterialId(matId);
    }

    outModel.skeleton_.build(scene);

    std::vector<SubMeshCpuData> cpuData;
    cpuData.reserve(scene->mNumMeshes);
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        cpuData.push_back(buildSubMeshCpu(scene->mMeshes[i]));
    }

    if (!outModel.skeleton_.empty()) {
        for (auto& cpu : cpuData) {
            remapJointIndicesToGlobal(cpu.vertices, cpu.localBoneNames, outModel.skeleton_);
        }
    }

    // ─── localAABB を計算して保存 ────
    outModel.localAABB_ = computeLocalAABB(cpuData);

    outModel.subMeshes_.resize(cpuData.size());
    for (size_t i = 0; i < cpuData.size(); ++i) {
        SubMesh& gpu = outModel.subMeshes_[i];
        gpu.materialIndex =
            (cpuData[i].materialIndex < matCount) ? cpuData[i].materialIndex : 0;
        gpu.indexCount = static_cast<uint32_t>(cpuData[i].indices.size());
        uploadSubMeshToGpu(cpuData[i], gpu, assets.geometry());
    }

    extractAnimationsByName(scene, outAnimations);

    // Phase 2G-2b: conservative bound across all animation poses (model-local).
    // cpuData here already has GLOBAL joint indices (remapped above) + bind pos.
    outModel.animationAABB_ = computeAnimationAABB(cpuData, outModel.skeleton_,
                                                   outAnimations, outModel.localAABB_);

    std::cout << "[ModelLoader::load] " << path << ": " << outModel.subMeshes_.size()
              << " submeshes, " << outModel.materials_.size() << " materials, "
              << outModel.textures_.size() << " textures, " << outModel.skeleton_.boneCount()
              << " bones, " << outAnimations.size() << " animations\n";
    std::cout << "  localAABB: min=(" << outModel.localAABB_.min.x << ", "
              << outModel.localAABB_.min.y << ", " << outModel.localAABB_.min.z << ") max=("
              << outModel.localAABB_.max.x << ", " << outModel.localAABB_.max.y << ", "
              << outModel.localAABB_.max.z << ")\n";
    // Phase 2G-2b: for skinned models, log the conservative animation bound so it
    // can be eyeballed at load (should enclose localAABB; bigger where limbs swing).
    if (!outModel.skeleton_.empty()) {
        std::cout << "  animationAABB: min=(" << outModel.animationAABB_.min.x << ", "
                  << outModel.animationAABB_.min.y << ", " << outModel.animationAABB_.min.z
                  << ") max=(" << outModel.animationAABB_.max.x << ", "
                  << outModel.animationAABB_.max.y << ", " << outModel.animationAABB_.max.z
                  << ")\n";
    }

    return true;
}

bool ModelLoader::loadAnimationsOnly(const std::string& path,
                                     std::vector<AnimationClip>& outAnimations) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 0);
    if (!scene) {
        std::cerr << "[ModelLoader::loadAnimationsOnly] failed: " << path << "\n";
        std::cerr << "[ModelLoader::loadAnimationsOnly] reason: '" << importer.GetErrorString()
                  << "'\n";
        return false;
    }
    if (scene->mNumAnimations == 0) {
        std::cerr << "[ModelLoader::loadAnimationsOnly] no animations in " << path << "\n";
        return false;
    }

    extractAnimationsByName(scene, outAnimations);
    std::cout << "[ModelLoader::loadAnimationsOnly] " << path << ": " << scene->mNumAnimations
              << " animations extracted\n";
    return true;
}

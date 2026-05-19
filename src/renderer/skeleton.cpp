// src/renderer/skeleton.cpp
// =============================================================================
// Phase 5-B: findBoneByAnyName API 実装
// (起動時のボーン名 dump は今後も便利なため残す)
// =============================================================================
#include "renderer/skeleton.h"

#include <assimp/scene.h>

#include <iomanip>
#include <iostream>

namespace {

glm::mat4 toGlmMat4(const aiMatrix4x4& m) {
    return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4,
                     m.b4, m.c4, m.d4);
}

void dumpMat4(const char* label, const glm::mat4& m) {
    std::cout << "[Skeleton DEBUG] " << label << ":\n";
    std::cout << std::fixed << std::setprecision(4);
    for (int row = 0; row < 4; ++row) {
        std::cout << "  ";
        for (int col = 0; col < 4; ++col) {
            std::cout << std::setw(10) << m[col][row] << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::defaultfloat;
}

}  // namespace

void Skeleton::collectBoneNames(const aiScene* scene,
                                std::unordered_map<std::string, glm::mat4>& outOffsets) {
    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        for (unsigned b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            const std::string name = bone->mName.C_Str();
            outOffsets[name] = toGlmMat4(bone->mOffsetMatrix);
        }
    }
}

void Skeleton::buildHierarchy(const aiNode* node, int parentIndex,
                              const std::unordered_map<std::string, glm::mat4>& offsets) {
    const std::string nodeName = node->mName.C_Str();
    int myIndex = parentIndex;

    auto it = offsets.find(nodeName);
    if (it != offsets.end()) {
        Bone bone{};
        bone.name = nodeName;
        bone.parentIndex = parentIndex;
        bone.inverseBindMatrix = it->second;
        bone.localBindTransform = toGlmMat4(node->mTransformation);

        myIndex = static_cast<int>(bones_.size());
        nameToIndex_[nodeName] = myIndex;
        bones_.push_back(bone);
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        buildHierarchy(node->mChildren[i], myIndex, offsets);
    }
}

void Skeleton::build(const aiScene* scene) {
    bones_.clear();
    nameToIndex_.clear();
    globalInverseTransform_ = glm::mat4(1.f);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[Skeleton] build: invalid scene\n";
        return;
    }

    const glm::mat4 rootNodeTransform = toGlmMat4(scene->mRootNode->mTransformation);
    globalInverseTransform_ = rootNodeTransform;

    // dumpMat4("rootNode->mTransformation (used as skeleton root parent)", rootNodeTransform);

    std::unordered_map<std::string, glm::mat4> offsets;
    collectBoneNames(scene, offsets);

    if (offsets.empty()) {
        return;
    }

    buildHierarchy(scene->mRootNode, -1, offsets);

    std::cout << "[Skeleton] built: " << bones_.size()
              << " bones (offsets collected: " << offsets.size() << ")\n";

    if (bones_.size() < offsets.size()) {
        std::cerr << "[Skeleton] WARNING: " << (offsets.size() - bones_.size())
                  << " bones in mBones but not found in node tree\n";
    }

    // ボーン名一覧を出力 (装備の attach 先確認、デバッグに便利なため恒久的に残す)
    // std::cout << "[Skeleton] === Bone list (" << bones_.size() << " bones) ===\n";
    // for (size_t i = 0; i < bones_.size(); ++i) {
    //     const Bone& b = bones_[i];
    //     std::cout << "  [" << std::setw(3) << i << "] parent="
    //               << std::setw(3) << b.parentIndex
    //               << "  name='" << b.name << "'\n";
    // }
    // std::cout << "[Skeleton] === End bone list ===\n";
}

int Skeleton::findBoneByName(const std::string& name) const {
    auto it = nameToIndex_.find(name);
    return (it != nameToIndex_.end()) ? it->second : -1;
}

// Phase 5-B: 複数候補から最初に見つかったボーンを返す
int Skeleton::findBoneByAnyName(std::initializer_list<const char*> candidates) const {
    for (const char* name : candidates) {
        if (!name) continue;
        auto it = nameToIndex_.find(name);
        if (it != nameToIndex_.end()) {
            return it->second;
        }
    }
    return -1;
}

void Skeleton::computeBindPoseSkinMatrices(std::vector<glm::mat4>& out) const {
    out.assign(bones_.size(), glm::mat4(1.f));
    if (bones_.empty()) return;

    std::vector<glm::mat4> globalTransforms(bones_.size(), glm::mat4(1.f));
    for (size_t i = 0; i < bones_.size(); ++i) {
        const Bone& b = bones_[i];
        const glm::mat4& parentGlobal =
            (b.parentIndex >= 0) ? globalTransforms[b.parentIndex] : globalInverseTransform_;
        globalTransforms[i] = parentGlobal * b.localBindTransform;
        out[i] = globalTransforms[i] * b.inverseBindMatrix;
    }
}

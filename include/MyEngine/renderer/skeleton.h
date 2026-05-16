// include/MyEngine/renderer/skeleton.h
#pragma once
// =============================================================================
// skeleton.h — Phase 2 段階C
// =============================================================================
// 注意: Assimp の aiMatrix4x4 は template typedef のため、ここで前方宣言すると
// 「typedef redefinition with different types」エラーになる。
// Assimp 型の include は skeleton.cpp 側にだけ置く。
// aiScene / aiNode はポインタ受け渡しのみなので不完全型前方宣言で OK。
// =============================================================================

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct aiScene;
struct aiNode;

struct Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix{1.f};
    glm::mat4 localBindTransform{1.f};
};

class Skeleton {
   public:
    void build(const aiScene* scene);

    bool empty() const { return bones_.empty(); }
    int boneCount() const { return static_cast<int>(bones_.size()); }
    const Bone& bone(int idx) const { return bones_[idx]; }
    const std::vector<Bone>& bones() const { return bones_; }

    int findBoneByName(const std::string& name) const;

    const glm::mat4& globalInverseTransform() const { return globalInverseTransform_; }

    void computeBindPoseSkinMatrices(std::vector<glm::mat4>& out) const;

   private:
    std::vector<Bone> bones_;
    std::unordered_map<std::string, int> nameToIndex_;
    glm::mat4 globalInverseTransform_{1.f};

    // toGlmMat4 は skeleton.cpp 内部の無名 namespace に置く
    // (aiMatrix4x4 の完全型が必要なため header では宣言しない)
    void collectBoneNames(const aiScene* scene,
                          std::unordered_map<std::string, glm::mat4>& outOffsets);
    void buildHierarchy(const aiNode* node, int parentIndex,
                        const std::unordered_map<std::string, glm::mat4>& offsets);
};

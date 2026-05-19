#pragma once
// =============================================================================
// skeleton.h — Phase 5-B
// findBoneByAnyName API 追加 (複数候補ボーン名から最初の一致を返す)
// =============================================================================

#include <glm/glm.hpp>
#include <initializer_list>
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

    // Phase 5-B: 複数候補名から最初に見つかったボーンのインデックスを返す
    // モデルごとに装備用ボーン名が異なる場合 (Mixamo / 自作モデルなど) のフォールバック検索用。
    // 例: findBoneByAnyName({"mixamorig:Shield_joint", "ShieldJoint", "shield_attach"})
    // 一致するものがなければ -1 を返す。
    int findBoneByAnyName(std::initializer_list<const char*> candidates) const;

    const glm::mat4& globalInverseTransform() const { return globalInverseTransform_; }

    void computeBindPoseSkinMatrices(std::vector<glm::mat4>& out) const;

   private:
    std::vector<Bone> bones_;
    std::unordered_map<std::string, int> nameToIndex_;
    glm::mat4 globalInverseTransform_{1.f};

    void collectBoneNames(const aiScene* scene,
                          std::unordered_map<std::string, glm::mat4>& outOffsets);
    void buildHierarchy(const aiNode* node, int parentIndex,
                        const std::unordered_map<std::string, glm::mat4>& offsets);
};

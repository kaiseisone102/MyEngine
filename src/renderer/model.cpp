// src/renderer/model.cpp
// =============================================================================
// 段階A: 型の定義のみ。destroy() と SubMesh::bind() を実装。
// 実際のロード処理は段階B (model_loader.cpp) で実装する。
// =============================================================================

#include "renderer/model.h"

#include "renderer/vulkan_context.h"

void SubMesh::bind(VkCommandBuffer cmd) const {
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Model::destroy() {
    if (!ctx_) return;
    VkDevice device = ctx_->device();

    for (SubMesh& sm : subMeshes_) {
        if (sm.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, sm.indexBuffer, nullptr);
            sm.indexBuffer = VK_NULL_HANDLE;
        }
        if (sm.indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, sm.indexBufferMemory, nullptr);
            sm.indexBufferMemory = VK_NULL_HANDLE;
        }
        if (sm.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, sm.vertexBuffer, nullptr);
            sm.vertexBuffer = VK_NULL_HANDLE;
        }
        if (sm.vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, sm.vertexBufferMemory, nullptr);
            sm.vertexBufferMemory = VK_NULL_HANDLE;
        }
        sm.indexCount = 0;
    }
    subMeshes_.clear();
    ctx_ = nullptr;
}

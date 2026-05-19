// src/renderer/model.cpp
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

    for (Material& m : materials_) {
        m.destroy();
    }
    materials_.clear();

    for (Texture& t : textures_) {
        t.destroy();
    }
    textures_.clear();

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

// src/renderer/model.cpp
#include "renderer/model.h"

#include "renderer/vulkan_context.h"
#include "renderer/geometry_buffer.h"

void SubMesh::bind(VkCommandBuffer cmd) const {
    if (geom) {
        geom->bindBlock(cmd, blockIndex);  // shared megabuffer; draw uses firstIndex/vertexOffset
        return;
    }
    const VkDeviceSize offset = 0;
    VkBuffer vb = vertexBuffer.get();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);
}

void SubMesh::bindAndDraw(VkCommandBuffer cmd, uint32_t instanceCount,
                          uint32_t firstInstance) const {
    bind(cmd);
    vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset,
                     firstInstance);
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
        // VkUnique frees each handle (no-op if empty).
        sm.indexBuffer.reset();
        sm.indexBufferMemory.reset();
        sm.vertexBuffer.reset();
        sm.vertexBufferMemory.reset();
        sm.indexCount = 0;
    }
    subMeshes_.clear();

    ctx_ = nullptr;
}

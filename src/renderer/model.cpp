// src/renderer/model.cpp
#include "renderer/model.h"

#include "renderer/geometry_buffer.h"

void SubMesh::bind(VkCommandBuffer cmd) const {
    geom->bindBlock(cmd, blockIndex);  // draw uses firstIndex/vertexOffset
}

void SubMesh::bindAndDraw(VkCommandBuffer cmd, uint32_t instanceCount,
                          uint32_t firstInstance) const {
    bind(cmd);
    vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset,
                     firstInstance);
}

void Model::destroy() {
    // GeometryBuffer owns the per-SubMesh memory; just drop materials/textures
    // here (each has its own destroy()). SubMesh handles become stale ranges
    // into a megabuffer that survives this Model.
    for (Material& m : materials_) {
        m.destroy();
    }
    materials_.clear();

    for (Texture& t : textures_) {
        t.destroy();
    }
    textures_.clear();

    subMeshes_.clear();
    ctx_ = nullptr;
}

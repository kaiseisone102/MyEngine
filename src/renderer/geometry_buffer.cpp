// src/renderer/geometry_buffer.cpp
// =============================================================================
// geometry_buffer.cpp - Phase 2B PART3a: multi-block consolidated geometry.
// See the header (and Work_Protocol 5c) for the design rationale.
// =============================================================================
#include "renderer/geometry_buffer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <vk_mem_alloc.h>

#include "renderer/deletion_queue.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void GeometryBuffer::init(VulkanContext* ctx, ResourceFactory* resources,
                          DeletionQueue* deletionQueue, uint32_t blockVertices,
                          uint32_t blockIndices) {
    if (!ctx || !resources || !deletionQueue)
        throw std::runtime_error("GeometryBuffer::init: null arg");
    ctx_ = ctx;
    resources_ = resources;
    deletionQueue_ = deletionQueue;
    blockVertices_ = blockVertices;
    blockIndices_ = blockIndices;
    blocks_.clear();
    // Start with one block; more are added on demand (multi-block confirmed correct;
    // the device-loss bug was a non-power-of-two byte alignment, now fixed by using
    // element-unit virtual blocks).
    addBlock(blockVertices_, blockIndices_);
    std::cout << "[GeometryBuffer] init: block granularity " << blockVertices_ << " verts / "
              << blockIndices_ << " indices; multi-block (grows on demand)\n";
}

void GeometryBuffer::shutdown() {
    if (!ctx_) return;
    for (Block& b : blocks_) {
        // shutdown runs after vkDeviceWaitIdle, so every range is safe to drop.
        // Clear all virtual allocations first; vmaDestroyVirtualBlock asserts if
        // any remain (we keep meshes resident and never individually free them).
        if (b.vtxVirt) {
            vmaClearVirtualBlock(b.vtxVirt);
            vmaDestroyVirtualBlock(b.vtxVirt);
        }
        if (b.idxVirt) {
            vmaClearVirtualBlock(b.idxVirt);
            vmaDestroyVirtualBlock(b.idxVirt);
        }
        b.vbuf.reset();
        b.ibuf.reset();
    }
    blocks_.clear();
    ctx_ = nullptr;
    resources_ = nullptr;
    deletionQueue_ = nullptr;
}

uint32_t GeometryBuffer::addBlock(uint32_t needVertices, uint32_t needIndices) {
    Block b{};
    b.vtxCapacity = std::max(blockVertices_, needVertices);
    b.idxCapacity = std::max(blockIndices_, needIndices);

    const VkDeviceSize vbSize = static_cast<VkDeviceSize>(b.vtxCapacity) * sizeof(Vertex);
    const VkDeviceSize ibSize = static_cast<VkDeviceSize>(b.idxCapacity) * sizeof(uint32_t);

    b.vbuf = VmaBuffer::createDeviceLocal(
        ctx_, vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    b.ibuf = VmaBuffer::createDeviceLocal(
        ctx_, ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // IMPORTANT: the virtual blocks track ELEMENT units (vertices / indices), NOT
    // bytes. Vertex stride is 76 bytes (not a power of two), so a byte-based
    // virtual block returns offsets that are not multiples of 76; dividing back to
    // a vertex index truncates and the draw reads out of bounds (device loss).
    // Working in element units makes the returned offset already a clean index.
    VmaVirtualBlockCreateInfo vci{};
    vci.size = b.vtxCapacity;  // in vertices
    if (vmaCreateVirtualBlock(&vci, &b.vtxVirt) != VK_SUCCESS)
        throw std::runtime_error("GeometryBuffer::addBlock: vmaCreateVirtualBlock (vtx) failed");
    VmaVirtualBlockCreateInfo ici{};
    ici.size = b.idxCapacity;  // in indices
    if (vmaCreateVirtualBlock(&ici, &b.idxVirt) != VK_SUCCESS)
        throw std::runtime_error("GeometryBuffer::addBlock: vmaCreateVirtualBlock (idx) failed");

    blocks_.push_back(std::move(b));
    const uint32_t idx = static_cast<uint32_t>(blocks_.size() - 1);
    std::cout << "[GeometryBuffer] add block #" << idx << ": " << blocks_[idx].vtxCapacity
              << " verts (" << (vbSize / (1024 * 1024)) << " MB) + " << blocks_[idx].idxCapacity
              << " indices (" << (ibSize / (1024 * 1024)) << " MB)\n";
    return idx;
}

MeshHandle GeometryBuffer::alloc(const std::vector<Vertex>& vertices,
                                 const std::vector<uint32_t>& indices) {
    MeshHandle h{};
    if (vertices.empty() || indices.empty()) return h;  // invalid

    const uint32_t vCount = static_cast<uint32_t>(vertices.size());
    const uint32_t iCount = static_cast<uint32_t>(indices.size());
    const VkDeviceSize vBytes = static_cast<VkDeviceSize>(vCount) * sizeof(Vertex);
    const VkDeviceSize iBytes = static_cast<VkDeviceSize>(iCount) * sizeof(uint32_t);

    // Try each existing block; both vtx and idx must fit in the SAME block so a
    // draw binds exactly one block. If none fit, add a block and use it.
    uint32_t chosen = UINT32_MAX;
    VmaVirtualAllocation vAlloc = nullptr, iAlloc = nullptr;
    VkDeviceSize vOffElems = 0, iOffElems = 0;  // ELEMENT units (verts / indices)

    auto tryBlock = [&](uint32_t bi) -> bool {
        Block& b = blocks_[bi];
        VmaVirtualAllocationCreateInfo vinfo{};
        vinfo.size = vCount;       // vertices, not bytes
        vinfo.alignment = 1;       // element-granular; offset is already a vertex index
        if (vmaVirtualAllocate(b.vtxVirt, &vinfo, &vAlloc, &vOffElems) != VK_SUCCESS) {
            vAlloc = nullptr;
            return false;
        }
        VmaVirtualAllocationCreateInfo iinfo{};
        iinfo.size = iCount;       // indices, not bytes
        iinfo.alignment = 1;
        if (vmaVirtualAllocate(b.idxVirt, &iinfo, &iAlloc, &iOffElems) != VK_SUCCESS) {
            vmaVirtualFree(b.vtxVirt, vAlloc);  // roll back the vtx alloc
            vAlloc = nullptr;
            iAlloc = nullptr;
            return false;
        }
        return true;
    };

    for (uint32_t bi = 0; bi < blocks_.size(); ++bi) {
        if (tryBlock(bi)) { chosen = bi; break; }
    }
    if (chosen == UINT32_MAX) {
        const uint32_t bi = addBlock(vCount, iCount);
        if (!tryBlock(bi)) {
            std::cerr << "[GeometryBuffer] ERROR: alloc failed even in a fresh block (vtx "
                      << vCount << ", idx " << iCount << ")\n";
            return h;
        }
        chosen = bi;
    }

    // Stage vertices/indices into the chosen block's sub-ranges. Staging buffers
    // retire through the DeletionQueue.
    // copyBufferRegion submits a one-time command buffer and vkQueueWaitIdle's on
    // it (see ResourceFactory::endOneTimeCommands), so each copy is fully complete
    // when it returns. The staging buffer is therefore safe to destroy IMMEDIATELY
    // -- no DeletionQueue needed. (Routing it through the DeletionQueue was wrong:
    // alloc() runs at load time, OUTSIDE drawFrame, so collectFrame() never ran and
    // hundreds of staging buffers piled up in one bucket, exhausting VRAM and
    // causing device loss / vkQueueSubmit failure. Immediate destroy fixes that.)
    {
        VmaBuffer vStage = VmaBuffer::createMappedHostVisible(ctx_, vBytes,
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(vStage.mapped(), vertices.data(), static_cast<size_t>(vBytes));
        resources_->copyBufferRegion(vStage.buffer(), blocks_[chosen].vbuf.buffer(), 0,
                                     vOffElems * sizeof(Vertex), vBytes);
        vStage.reset();  // copy already completed (waitIdle); destroy now
    }
    {
        VmaBuffer iStage = VmaBuffer::createMappedHostVisible(ctx_, iBytes,
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(iStage.mapped(), indices.data(), static_cast<size_t>(iBytes));
        resources_->copyBufferRegion(iStage.buffer(), blocks_[chosen].ibuf.buffer(), 0,
                                     iOffElems * sizeof(uint32_t), iBytes);
        iStage.reset();  // copy already completed (waitIdle); destroy now
    }

    h.blockIndex = chosen;
    h.firstIndex = static_cast<uint32_t>(iOffElems);    // already an index count
    h.vertexOffset = static_cast<int32_t>(vOffElems);   // already a vertex index
    h.indexCount = iCount;
    h.vertexCount = vCount;
    h.vtxAlloc = vAlloc;
    h.idxAlloc = iAlloc;
    return h;
}

void GeometryBuffer::free(const MeshHandle& handle) {
    if (!handle.valid()) return;
    if (handle.blockIndex >= blocks_.size()) return;
    const uint32_t bi = handle.blockIndex;
    VmaVirtualAllocation vA = handle.vtxAlloc;
    VmaVirtualAllocation iA = handle.idxAlloc;
    // Free via the DeletionQueue so the ranges are reused only after in-flight
    // frames that referenced them have completed.
    deletionQueue_->enqueue([this, bi, vA, iA]() {
        if (bi >= blocks_.size()) return;
        if (vA) vmaVirtualFree(blocks_[bi].vtxVirt, vA);
        if (iA) vmaVirtualFree(blocks_[bi].idxVirt, iA);
    });
}

void GeometryBuffer::bindBlock(VkCommandBuffer cmd, uint32_t blockIndex) const {
    if (blockIndex >= blocks_.size()) return;
    const Block& b = blocks_[blockIndex];
    const VkDeviceSize offset = 0;
    VkBuffer vb = b.vbuf.buffer();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, b.ibuf.buffer(), 0, VK_INDEX_TYPE_UINT32);
}

VkDeviceAddress GeometryBuffer::blockVertexAddress(uint32_t blockIndex) const {
    // Phase 2G: compute skinning reads the original interleaved vertices of a
    // block via BDA. vbuf is created with SHADER_DEVICE_ADDRESS (see addBlock),
    // so deviceAddress() is populated.
    if (blockIndex >= blocks_.size()) return 0;
    return blocks_[blockIndex].vbuf.deviceAddress();
}
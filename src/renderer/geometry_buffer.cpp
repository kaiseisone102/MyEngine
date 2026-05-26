// src/renderer/geometry_buffer.cpp
// =============================================================================
// geometry_buffer.cpp - Phase 2B PART3a: consolidated geometry megabuffer.
// See the header for the design + dynamic-scene/growth rationale.
// =============================================================================
#include "renderer/geometry_buffer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "renderer/deletion_queue.h"
#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void GeometryBuffer::init(VulkanContext* ctx, ResourceFactory* resources,
                          DeletionQueue* deletionQueue, uint32_t vertexCapacity,
                          uint32_t indexCapacity) {
    if (!ctx || !resources || !deletionQueue)
        throw std::runtime_error("GeometryBuffer::init: null arg");
    ctx_ = ctx;
    resources_ = resources;
    deletionQueue_ = deletionQueue;
    vertexCapacity_ = vertexCapacity;
    indexCapacity_ = indexCapacity;

    const VkDeviceSize vbSize = static_cast<VkDeviceSize>(vertexCapacity_) * sizeof(Vertex);
    const VkDeviceSize ibSize = static_cast<VkDeviceSize>(indexCapacity_) * sizeof(uint32_t);

    // vertex buffer also gets SHADER_DEVICE_ADDRESS so a future vertex-pulling /
    // mesh-shader path can read it via BDA without recreating the megabuffer.
    vertexBuf_ = VmaBuffer::createDeviceLocal(
        ctx_, vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    indexBuf_ = VmaBuffer::createDeviceLocal(
        ctx_, ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // Whole buffer starts as one free span (element units).
    vertexFree_.clear();
    indexFree_.clear();
    vertexFree_.push_back({0, vertexCapacity_});
    indexFree_.push_back({0, indexCapacity_});

    std::cout << "[GeometryBuffer] init: " << vertexCapacity_ << " verts ("
              << (vbSize / (1024 * 1024)) << " MB) + " << indexCapacity_ << " indices ("
              << (ibSize / (1024 * 1024)) << " MB), device-local megabuffers\n";
}

void GeometryBuffer::shutdown() {
    if (!ctx_) return;
    vertexBuf_.reset();
    indexBuf_.reset();
    vertexFree_.clear();
    indexFree_.clear();
    ctx_ = nullptr;
    resources_ = nullptr;
    deletionQueue_ = nullptr;
}

// First-fit over free spans (element units). Splits the chosen span. Returns the
// allocated offset, or UINT32_MAX if no span is large enough.
uint32_t GeometryBuffer::allocSpan(std::vector<Span>& freeList, uint32_t count) {
    if (count == 0) return UINT32_MAX;
    for (size_t i = 0; i < freeList.size(); ++i) {
        if (freeList[i].count >= count) {
            const uint32_t off = freeList[i].offset;
            if (freeList[i].count == count) {
                freeList.erase(freeList.begin() + static_cast<long>(i));
            } else {
                freeList[i].offset += count;
                freeList[i].count -= count;
            }
            return off;
        }
    }
    return UINT32_MAX;
}

// Return a span and coalesce with adjacent free spans. Keeps the list sorted by
// offset so neighbour-merging is simple.
void GeometryBuffer::freeSpan(std::vector<Span>& freeList, uint32_t offset, uint32_t count) {
    if (count == 0) return;
    // insert sorted by offset
    size_t i = 0;
    while (i < freeList.size() && freeList[i].offset < offset) ++i;
    freeList.insert(freeList.begin() + static_cast<long>(i), {offset, count});
    // coalesce forward/backward
    if (i + 1 < freeList.size() &&
        freeList[i].offset + freeList[i].count == freeList[i + 1].offset) {
        freeList[i].count += freeList[i + 1].count;
        freeList.erase(freeList.begin() + static_cast<long>(i) + 1);
    }
    if (i > 0 && freeList[i - 1].offset + freeList[i - 1].count == freeList[i].offset) {
        freeList[i - 1].count += freeList[i].count;
        freeList.erase(freeList.begin() + static_cast<long>(i));
    }
}

MeshHandle GeometryBuffer::alloc(const std::vector<Vertex>& vertices,
                                 const std::vector<uint32_t>& indices) {
    MeshHandle h{};
    if (vertices.empty() || indices.empty()) return h;  // invalid (indexCount==0)

    const uint32_t vCount = static_cast<uint32_t>(vertices.size());
    const uint32_t iCount = static_cast<uint32_t>(indices.size());

    const uint32_t vOff = allocSpan(vertexFree_, vCount);
    if (vOff == UINT32_MAX) {
        std::cerr << "[GeometryBuffer] WARNING: vertex capacity exhausted (need " << vCount
                  << "). Grow policy not yet implemented.\n";
        return h;
    }
    const uint32_t iOff = allocSpan(indexFree_, iCount);
    if (iOff == UINT32_MAX) {
        std::cerr << "[GeometryBuffer] WARNING: index capacity exhausted (need " << iCount
                  << "). Grow policy not yet implemented.\n";
        freeSpan(vertexFree_, vOff, vCount);  // give back the vertex span
        return h;
    }

    // Stage vertices then indices into their megabuffer sub-ranges. The staging
    // buffers are retired through the DeletionQueue (safe even though the copy is
    // a one-time submit; this also matches the engine's deferred-destruction path).
    const VkDeviceSize vBytes = static_cast<VkDeviceSize>(vCount) * sizeof(Vertex);
    const VkDeviceSize iBytes = static_cast<VkDeviceSize>(iCount) * sizeof(uint32_t);

    {
        VmaBuffer vStage = VmaBuffer::createMappedHostVisible(ctx_, vBytes,
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(vStage.mapped(), vertices.data(), static_cast<size_t>(vBytes));
        resources_->copyBufferRegion(vStage.buffer(), vertexBuf_.buffer(), 0,
                                     static_cast<VkDeviceSize>(vOff) * sizeof(Vertex), vBytes);
        deletionQueue_->enqueueBuffer(vStage.buffer(), vStage.allocation());
        vStage.release();  // ownership handed to the DeletionQueue
    }
    {
        VmaBuffer iStage = VmaBuffer::createMappedHostVisible(ctx_, iBytes,
                                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(iStage.mapped(), indices.data(), static_cast<size_t>(iBytes));
        resources_->copyBufferRegion(iStage.buffer(), indexBuf_.buffer(), 0,
                                     static_cast<VkDeviceSize>(iOff) * sizeof(uint32_t), iBytes);
        deletionQueue_->enqueueBuffer(iStage.buffer(), iStage.allocation());
        iStage.release();
    }

    h.firstIndex = iOff;
    h.vertexOffset = static_cast<int32_t>(vOff);
    h.indexCount = iCount;
    h.vertexCount = vCount;
    return h;
}

void GeometryBuffer::free(const MeshHandle& handle) {
    if (!handle.valid()) return;
    // Return the spans to the free list THROUGH the DeletionQueue, so the region
    // is only reusable after any in-flight frame that referenced it completes.
    const uint32_t vOff = static_cast<uint32_t>(handle.vertexOffset);
    const uint32_t vCount = handle.vertexCount;
    const uint32_t iOff = handle.firstIndex;
    const uint32_t iCount = handle.indexCount;
    deletionQueue_->enqueue([this, vOff, vCount, iOff, iCount]() {
        freeSpan(vertexFree_, vOff, vCount);
        freeSpan(indexFree_, iOff, iCount);
    });
}

void GeometryBuffer::bind(VkCommandBuffer cmd) const {
    const VkDeviceSize offset = 0;
    VkBuffer vb = vertexBuf_.buffer();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuf_.buffer(), 0, VK_INDEX_TYPE_UINT32);
}
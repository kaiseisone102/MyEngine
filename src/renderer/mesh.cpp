// src/renderer/mesh.cpp

#include "renderer/mesh.h"

#include <tiny_obj_loader.h>

#include <cstring>
#include <stdexcept>
#include <unordered_map>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void Mesh::loadFromObj(const VulkanContext* ctx, const ResourceFactory* resources,
                       const std::string& path) {
    ctx_ = ctx;

    // 1) OBJ をパースして CPU 側データを組み立てる
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        throw std::runtime_error("Mesh::loadFromObj: " + warn + err);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v{};
            v.pos = {attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]};

            // OBJ は V 軸が下→上、Vulkan は上→下なので反転
            if (index.texcoord_index >= 0) {
                v.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                              1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
            }

            if (index.normal_index >= 0) {
                v.normal = {attrib.normals[3 * size_t(index.normal_index) + 0],
                            attrib.normals[3 * size_t(index.normal_index) + 1],
                            attrib.normals[3 * size_t(index.normal_index) + 2]};
            } else {
                v.normal = {0.f, 1.f, 0.f};
            }

            v.color = {1.f, 1.f, 1.f};

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }
            indices.push_back(uniqueVertices[v]);
        }
    }

    indexCount_ = static_cast<uint32_t>(indices.size());

    // 2) GPU バッファに転送
    uploadBuffer(resources, vertices.data(), sizeof(Vertex) * vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer_, vertexBufferMemory_);
    uploadBuffer(resources, indices.data(), sizeof(uint32_t) * indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_, indexBufferMemory_);

    // ここで CPU 側の vertices/indices は破棄される（自動）。
    // 後で CPU アクセスしたい場合はメンバに残す設計に変更する。
}

void Mesh::uploadBuffer(const ResourceFactory* resources, const void* src, VkDeviceSize size,
                        VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) const {
    // staging（HOST_VISIBLE）→ device-local（高速）
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    resources->createBuffer(
        size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
        stagingMem);

    void* data = nullptr;
    vkMapMemory(ctx_->device(), stagingMem, 0, size, 0, &data);
    std::memcpy(data, src, static_cast<size_t>(size));
    vkUnmapMemory(ctx_->device(), stagingMem);

    resources->createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, memory);
    resources->copyBuffer(staging, buffer, size);

    vkDestroyBuffer(ctx_->device(), staging, nullptr);
    vkFreeMemory(ctx_->device(), stagingMem, nullptr);
}

void Mesh::bind(VkCommandBuffer cmd) const {
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::destroy() {
    if (!ctx_) return;
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), indexBufferMemory_, nullptr);
        indexBufferMemory_ = VK_NULL_HANDLE;
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->device(), vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), vertexBufferMemory_, nullptr);
        vertexBufferMemory_ = VK_NULL_HANDLE;
    }
    indexCount_ = 0;
    ctx_ = nullptr;
}
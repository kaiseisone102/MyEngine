// renderer/vulkan_triangle_renderer.cpp

// =============================================================================
// vulkan_triangle_renderer.cpp — リファクタ Step 1 (VulkanContext 切り出し後)
// =============================================================================
// Step 1 で行った変更:
//   - Instance/Surface/PhysicalDevice/Device/Queues の処理は VulkanContext に移動
//   - 旧メンバ instance_/surface_/physical_/device_/graphicsQueue_/presentQueue_/
//     graphicsFamily_/presentFamily_/memoryProperties_/debugMessenger_ は
//     ctx_.xxx() 経由でアクセスするよう全置換
//   - findDepthFormat() は ctx_.findDepthFormat() に委譲
//   - 匿名 namespace から検証・デバイス選定系の関数を削除
//
// 現在このファイルが抱えている責務（Step 2 以降で分割予定）:
//   Swapchain / Depth / Render/Shadow Pass / Pipeline / Descriptor / Mesh /
//   Texture / UBO / Command/Sync / ImGui
// =============================================================================

#include "renderer/vulkan_triangle_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// stb_image: IMPLEMENTATION マクロを 1 つの .cpp でだけ定義する（二重定義厳禁）
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// tinyobjloader: vcpkg がコンパイル済みライブラリとして提供する
#include <tiny_obj_loader.h>

// ─── ステップ13: ImGui ────────────────────────────────────────────────────
// imgui.h は imgui コアの定義（ImGui::Text 等のAPI）
// imgui_impl_vulkan.h は Vulkan バックエンド（GPU に ImGui を描画する）
// imgui_impl_sdl3.h は SDL3 バックエンド（マウス/キーボード入力を ImGui に渡す）
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ─── このファイル内だけで使う補助 ────────────────────────────────────────
namespace {

// Swapchain 作成で使う。Step 2 で Swapchain クラスに移す予定。
struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupport querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &d.capabilities);
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    if (count) {
        d.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, d.formats.data());
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    if (count) {
        d.presentModes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, d.presentModes.data());
    }
    return d;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>&) {
    // VK_PRESENT_MODE_FIFO_KHR = ディスプレイの垂直同期（VSync）に合わせる。
    // GPU が際限なくフレームを生成するのを防ぎ、ノート PC の過熱・電力超過を回避する。
    // FIFO は Vulkan 仕様でサポートが必須なので必ず使える。
    return VK_PRESENT_MODE_FIFO_KHR;
}

}  // namespace

// ─── ユーティリティ ───────────────────────────────────────────────────────────

std::vector<char> VulkanTriangleRenderer::readBinaryFile(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) throw std::runtime_error(std::string("failed to open shader: ") + path);
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

VkShaderModule VulkanTriangleRenderer::createShaderModule(VkDevice device,
                                                          const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod{};
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("vkCreateShaderModule failed");
    return mod;
}

uint32_t VulkanTriangleRenderer::findMemoryType(uint32_t typeFilter,
                                                VkMemoryPropertyFlags properties) {
    const auto& memProps = ctx_.memoryProperties();
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("findMemoryType: no suitable memory type");
}

void VulkanTriangleRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                          VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                          VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx_.device(), &bi, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("createBuffer: vkCreateBuffer failed");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx_.device(), buffer, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_.device(), &ai, nullptr, &bufferMemory) != VK_SUCCESS)
        throw std::runtime_error("createBuffer: vkAllocateMemory failed");
    vkBindBufferMemory(ctx_.device(), buffer, bufferMemory, 0);
}

VkCommandBuffer VulkanTriangleRenderer::beginOneTimeCommands() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd{};
    if (vkAllocateCommandBuffers(ctx_.device(), &ai, &cmd) != VK_SUCCESS)
        throw std::runtime_error("beginOneTimeCommands failed");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VulkanTriangleRenderer::endOneTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    if (vkQueueSubmit(ctx_.graphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("endOneTimeCommands failed");
    vkQueueWaitIdle(ctx_.graphicsQueue());
    vkFreeCommandBuffers(ctx_.device(), commandPool_, 1, &cmd);
}

void VulkanTriangleRenderer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endOneTimeCommands(cmd);
}

void VulkanTriangleRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
                                         VkImageTiling tiling, VkImageUsageFlags usage,
                                         VkMemoryPropertyFlags properties, VkImage& image,
                                         VkDeviceMemory& imageMemory) {
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {width, height, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = tiling;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx_.device(), &ci, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("createImage: vkCreateImage failed");
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx_.device(), image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx_.device(), &ai, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("createImage: vkAllocateMemory failed");
    vkBindImageMemory(ctx_.device(), image, imageMemory, 0);
}

void VulkanTriangleRenderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                                                   VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage{}, dstStage{};
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("transitionImageLayout: unsupported transition");
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endOneTimeCommands(cmd);
}

void VulkanTriangleRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                               uint32_t height) {
    VkCommandBuffer cmd = beginOneTimeCommands();
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endOneTimeCommands(cmd);
}

// ─── デプスバッファ作成 ──────────────────────────────────────────────────
void VulkanTriangleRenderer::createDepthResources() {
    createImage(swapExtent_.width, swapExtent_.height, depthFormat_, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage_, depthImageMemory_);

    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = depthImage_;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = depthFormat_;
    ci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_.device(), &ci, nullptr, &depthImageView_) != VK_SUCCESS)
        throw std::runtime_error("createDepthResources: vkCreateImageView failed");
}

// ─── OBJ ロード ──────────────────────────────────────────────────────────
void VulkanTriangleRenderer::loadModel() {
    const std::string modelPath = assetDir_ + "cube.obj";

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
        throw std::runtime_error("loadModel: " + warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex v{};

            // 頂点位置（attrib.vertices は x,y,z,x,y,z... の flat array）
            v.pos = {attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]};

            // UV 座標。OBJ は V が下→上なので Vulkan 用に反転（1 - v）
            if (index.texcoord_index >= 0) {
                v.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                              1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
            }

            // ステップ10: 法線ベクトルを読み込む
            // normal_index が -1 の場合（OBJ に法線がない）は上向きをデフォルトにする
            if (index.normal_index >= 0) {
                v.normal = {attrib.normals[3 * size_t(index.normal_index) + 0],
                            attrib.normals[3 * size_t(index.normal_index) + 1],
                            attrib.normals[3 * size_t(index.normal_index) + 2]};
            } else {
                v.normal = {0.f, 1.f, 0.f};  // デフォルト: 上向き
            }

            v.color = {1.0f, 1.0f, 1.0f};

            // 初出の頂点なら追加。重複は既存インデックスを使い回す
            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(meshVertices_.size());
                meshVertices_.push_back(v);
            }
            meshIndices_.push_back(uniqueVertices[v]);
        }
    }
}

void VulkanTriangleRenderer::createVertexBuffer() {
    const VkDeviceSize bufferSize = sizeof(Vertex) * meshVertices_.size();
    VkBuffer stagingBuf{};
    VkDeviceMemory stagingMem{};
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuf, stagingMem);
    void* data = nullptr;
    vkMapMemory(ctx_.device(), stagingMem, 0, bufferSize, 0, &data);
    std::memcpy(data, meshVertices_.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(ctx_.device(), stagingMem);
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_);
    copyBuffer(stagingBuf, vertexBuffer_, bufferSize);
    vkDestroyBuffer(ctx_.device(), stagingBuf, nullptr);
    vkFreeMemory(ctx_.device(), stagingMem, nullptr);
}

void VulkanTriangleRenderer::createIndexBuffer() {
    indexCount_ = static_cast<uint32_t>(meshIndices_.size());
    const VkDeviceSize bufferSize = sizeof(uint32_t) * meshIndices_.size();
    VkBuffer stagingBuf{};
    VkDeviceMemory stagingMem{};
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuf, stagingMem);
    void* data = nullptr;
    vkMapMemory(ctx_.device(), stagingMem, 0, bufferSize, 0, &data);
    std::memcpy(data, meshIndices_.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(ctx_.device(), stagingMem);
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_);
    copyBuffer(stagingBuf, indexBuffer_, bufferSize);
    vkDestroyBuffer(ctx_.device(), stagingBuf, nullptr);
    vkFreeMemory(ctx_.device(), stagingMem, nullptr);
}

void VulkanTriangleRenderer::createUniformBuffer() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx_.physicalDevice(), &props);
    VkDeviceSize align = props.limits.minUniformBufferOffsetAlignment;
    VkDeviceSize sz = sizeof(LightingUBO);
    if (align > 0) sz = (sz + align - 1) / align * align;
    uniformBufferSize_ = sz;
    createBuffer(sz, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniformBuffer_, uniformBufferMemory_);
    if (vkMapMemory(ctx_.device(), uniformBufferMemory_, 0, sz, 0, &uniformMapped_) != VK_SUCCESS)
        throw std::runtime_error("createUniformBuffer: vkMapMemory failed");
}

void VulkanTriangleRenderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding sampler{};
    sampler.binding = 1;
    sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler.descriptorCount = 1;
    sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadowSampler{};
    shadowSampler.binding = 2;
    shadowSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowSampler.descriptorCount = 1;
    shadowSampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {ubo, sampler, shadowSampler};
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 3;
    ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(ctx_.device(), &ci, nullptr, &descriptorSetLayout_) !=
        VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

void VulkanTriangleRenderer::createDescriptorPool() {
    VkDescriptorPoolSize sizes[3]{};
    sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    sizes[2] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.poolSizeCount = 3;
    ci.pPoolSizes = sizes;
    ci.maxSets = 1;
    if (vkCreateDescriptorPool(ctx_.device(), &ci, nullptr, &descriptorPool_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorPool failed");
}

void VulkanTriangleRenderer::createDescriptorSet() {
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descriptorPool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(ctx_.device(), &ai, &descriptorSet_) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateDescriptorSets failed");

    VkDescriptorBufferInfo bi{};
    bi.buffer = uniformBuffer_;
    bi.offset = 0;
    bi.range = sizeof(LightingUBO);
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView = textureImageView_;
    ii.sampler = textureSampler_;
    VkDescriptorImageInfo si{};
    si.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    si.imageView = shadowImageView_;
    si.sampler = shadowSampler_;

    VkWriteDescriptorSet writes[3]{};
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[0].dstSet = descriptorSet_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &bi;
    writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[1].dstSet = descriptorSet_;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &ii;
    writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[2].dstSet = descriptorSet_;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &si;
    vkUpdateDescriptorSets(ctx_.device(), 3, writes, 0, nullptr);
}

void VulkanTriangleRenderer::updateUniformBuffer() {
    LightingUBO ubo{};
    ubo.vp = projMatrix_ * viewMatrix_;
    const glm::vec3 lightTarget{0.f, 0.f, 0.f};
    const glm::mat4 lightView = glm::lookAt(lightPos_, lightTarget, glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 lightProj = glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 60.f);
    ubo.lightVP = lightProj * lightView;
    ubo.lightPos = lightPos_;
    ubo._p0 = 0.f;
    ubo.lightColor = lightColor_;
    ubo._p1 = 0.f;
    ubo.viewPos = viewPos_;
    ubo._p2 = 0.f;
    ubo.ambient = ambient_;
    ubo.specular = specular_;
    ubo.shadowStrength = shadowStrength_;
    ubo.shadowBias = shadowBias_;
    std::memcpy(uniformMapped_, &ubo, sizeof(ubo));
}

// ─── テクスチャ ──────────────────────────────────────────────────────────
void VulkanTriangleRenderer::createTextureImage() {
    int texW = 0, texH = 0, texCh = 0;
    const std::string texPath = assetDir_ + "texture.png";

    // ── stb_image でファイル読み込みを試みる ─────────────────────────────────
    uint8_t* stbiPix = stbi_load(texPath.c_str(), &texW, &texH, &texCh, STBI_rgb_alpha);
    std::vector<uint8_t> ownedPix;
    uint8_t* pixels = stbiPix;

    if (!pixels) {
        // ファイルがない → 256×256 の黄×青チェッカーボードを生成
        texW = texH = 256;
        const uint32_t tile = 32;
        ownedPix.resize(static_cast<size_t>(texW * texH * 4));
        for (int y = 0; y < texH; ++y) {
            for (int x = 0; x < texW; ++x) {
                const bool even = ((x / tile) + (y / tile)) % 2 == 0;
                uint32_t idx = static_cast<uint32_t>((y * texW + x) * 4);
                ownedPix[idx + 0] = even ? 230 : 50;
                ownedPix[idx + 1] = even ? 200 : 100;
                ownedPix[idx + 2] = even ? 50 : 210;
                ownedPix[idx + 3] = 255;
            }
        }
        pixels = ownedPix.data();
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(texW * texH * 4);
    VkBuffer stagingBuf{};
    VkDeviceMemory stagingMem{};
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuf, stagingMem);
    void* data = nullptr;
    vkMapMemory(ctx_.device(), stagingMem, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(ctx_.device(), stagingMem);

    if (stbiPix) stbi_image_free(stbiPix);

    createImage(static_cast<uint32_t>(texW), static_cast<uint32_t>(texH), VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_, textureImageMemory_);

    transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuf, textureImage_, static_cast<uint32_t>(texW),
                      static_cast<uint32_t>(texH));
    transitionImageLayout(textureImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx_.device(), stagingBuf, nullptr);
    vkFreeMemory(ctx_.device(), stagingMem, nullptr);
}

void VulkanTriangleRenderer::createTextureImageView() {
    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = textureImage_;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_SRGB;
    ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_.device(), &ci, nullptr, &textureImageView_) != VK_SUCCESS)
        throw std::runtime_error("createTextureImageView failed");
}

void VulkanTriangleRenderer::createTextureSampler() {
    VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.addressModeU = ci.addressModeV = ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ci.anisotropyEnable = VK_FALSE;
    ci.maxAnisotropy = 1.f;
    ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(ctx_.device(), &ci, nullptr, &textureSampler_) != VK_SUCCESS)
        throw std::runtime_error("createTextureSampler failed");
}

// ─── スワップチェーン ────────────────────────────────────────────────────
void VulkanTriangleRenderer::createSwapchain() {
    SwapchainSupport sup = querySwapchainSupport(ctx_.physicalDevice(), ctx_.surface());
    VkSurfaceFormatKHR fmt = chooseSwapSurfaceFormat(sup.formats);
    VkPresentModeKHR mode = choosePresentMode(sup.presentModes);
    VkExtent2D extent = sup.capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        extent.width = std::clamp(static_cast<uint32_t>(w), sup.capabilities.minImageExtent.width,
                                  sup.capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h), sup.capabilities.minImageExtent.height,
                                   sup.capabilities.maxImageExtent.height);
    }
    uint32_t imgCount = sup.capabilities.minImageCount + 1;
    if (sup.capabilities.maxImageCount > 0 && imgCount > sup.capabilities.maxImageCount)
        imgCount = sup.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = ctx_.surface();
    ci.minImageCount = imgCount;
    ci.imageFormat = fmt.format;
    ci.imageColorSpace = fmt.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform = sup.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = mode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = swapchain_;
    uint32_t queueFamilyIndices[] = {ctx_.graphicsFamily(), ctx_.presentFamily()};
    if (ctx_.graphicsFamily() != ctx_.presentFamily()) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    VkSwapchainKHR newSwapchain{};
    if (vkCreateSwapchainKHR(ctx_.device(), &ci, nullptr, &newSwapchain) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSwapchainKHR failed");
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
    swapchain_ = newSwapchain;
    swapFormat_ = fmt.format;
    swapExtent_ = extent;
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &imgCount, nullptr);
    swapImages_.resize(imgCount);
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_, &imgCount, swapImages_.data());
}

void VulkanTriangleRenderer::createImageViews() {
    swapViews_.resize(swapImages_.size());
    for (size_t i = 0; i < swapImages_.size(); ++i) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = swapImages_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = swapFormat_;
        ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(ctx_.device(), &ci, nullptr, &swapViews_[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateImageView failed");
    }
}

// ─── レンダーパス ────────────────────────────────────────────────────────
void VulkanTriangleRenderer::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapFormat_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = depthFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color, depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    if (vkCreateRenderPass(ctx_.device(), &ci, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass failed");
}

void VulkanTriangleRenderer::createShadowRenderPass() {
    VkAttachmentDescription depth{};
    depth.format = shadowFormat_;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &depth;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;
    if (vkCreateRenderPass(ctx_.device(), &ci, nullptr, &shadowRenderPass_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass(shadow) failed");
}

void VulkanTriangleRenderer::createShadowResources() {
    createImage(shadowExtent_.width, shadowExtent_.height, shadowFormat_, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowImage_, shadowImageMemory_);

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = shadowImage_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = shadowFormat_;
    vi.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_.device(), &vi, nullptr, &shadowImageView_) != VK_SUCCESS)
        throw std::runtime_error("createShadowResources: imageView failed");

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.minLod = 0.f;
    si.maxLod = 1.f;
    if (vkCreateSampler(ctx_.device(), &si, nullptr, &shadowSampler_) != VK_SUCCESS)
        throw std::runtime_error("createShadowResources: sampler failed");

    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = shadowRenderPass_;
    fi.attachmentCount = 1;
    fi.pAttachments = &shadowImageView_;
    fi.width = shadowExtent_.width;
    fi.height = shadowExtent_.height;
    fi.layers = 1;
    if (vkCreateFramebuffer(ctx_.device(), &fi, nullptr, &shadowFramebuffer_) != VK_SUCCESS)
        throw std::runtime_error("createShadowResources: framebuffer failed");
}

void VulkanTriangleRenderer::createCommandPool() {
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = ctx_.graphicsFamily();
    if (vkCreateCommandPool(ctx_.device(), &ci, nullptr, &commandPool_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateCommandPool failed");
}

// ─── シャドウパイプライン ────────────────────────────────────────────────
void VulkanTriangleRenderer::createShadowPipeline() {
    auto vertCode = readBinaryFile((shaderDir_ + "shadow_vert.spv").c_str());
    VkShaderModule vert = createShaderModule(ctx_.device(), vertCode);

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = vert;
    stage.pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription posAttr{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                              static_cast<uint32_t>(offsetof(Vertex, pos))};
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 1;
    vi.pVertexAttributeDescriptions = &posAttr;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.f;
    rs.cullMode = VK_CULL_MODE_FRONT_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_TRUE;
    rs.depthBiasConstantFactor = 1.25f;
    rs.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc.offset = 0;
    pc.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1;
    lci.pSetLayouts = &descriptorSetLayout_;
    lci.pushConstantRangeCount = 1;
    lci.pPushConstantRanges = &pc;
    if (vkCreatePipelineLayout(ctx_.device(), &lci, nullptr, &shadowPipelineLayout_) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(ctx_.device(), vert, nullptr);
        throw std::runtime_error("vkCreatePipelineLayout(shadow) failed");
    }

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 1;
    pci.pStages = &stage;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pDynamicState = &dyn;
    pci.layout = shadowPipelineLayout_;
    pci.renderPass = shadowRenderPass_;
    pci.subpass = 0;
    if (vkCreateGraphicsPipelines(ctx_.device(), VK_NULL_HANDLE, 1, &pci, nullptr,
                                  &shadowPipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx_.device(), vert, nullptr);
        throw std::runtime_error("vkCreateGraphicsPipelines(shadow) failed");
    }
    vkDestroyShaderModule(ctx_.device(), vert, nullptr);
}

// ─── メインパイプライン ──────────────────────────────────────────────────
void VulkanTriangleRenderer::createGraphicsPipeline() {
    auto vertCode = readBinaryFile((shaderDir_ + "triangle_vert.spv").c_str());
    auto fragCode = readBinaryFile((shaderDir_ + "triangle_frag.spv").c_str());
    VkShaderModule vert = createShaderModule(ctx_.device(), vertCode);
    VkShaderModule frag = createShaderModule(ctx_.device(), fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[4]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, color))};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, texCoord))};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))};

    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 4;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.f;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blendAtt;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo layoutCi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCi.setLayoutCount = 1;
    layoutCi.pSetLayouts = &descriptorSetLayout_;
    layoutCi.pushConstantRangeCount = 1;
    layoutCi.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(ctx_.device(), &layoutCi, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx_.device(), vert, nullptr);
        vkDestroyShaderModule(ctx_.device(), frag, nullptr);
        throw std::runtime_error("vkCreatePipelineLayout failed");
    }

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn;
    pci.layout = pipelineLayout_;
    pci.renderPass = renderPass_;
    pci.subpass = 0;

    if (vkCreateGraphicsPipelines(ctx_.device(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(ctx_.device(), vert, nullptr);
        vkDestroyShaderModule(ctx_.device(), frag, nullptr);
        throw std::runtime_error("vkCreateGraphicsPipelines failed");
    }
    vkDestroyShaderModule(ctx_.device(), vert, nullptr);
    vkDestroyShaderModule(ctx_.device(), frag, nullptr);
}

void VulkanTriangleRenderer::createFramebuffers() {
    framebuffers_.resize(swapViews_.size());
    for (size_t i = 0; i < swapViews_.size(); ++i) {
        VkImageView attachments[] = {swapViews_[i], depthImageView_};
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = renderPass_;
        ci.attachmentCount = 2;
        ci.pAttachments = attachments;
        ci.width = swapExtent_.width;
        ci.height = swapExtent_.height;
        ci.layers = 1;
        if (vkCreateFramebuffer(ctx_.device(), &ci, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer failed");
    }
}

void VulkanTriangleRenderer::createCommandBuffers() {
    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(ctx_.device(), &ai, commandBuffers_.data()) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");
}

void VulkanTriangleRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(ctx_.device(), &si, nullptr, &imageAvailable_) != VK_SUCCESS ||
        vkCreateSemaphore(ctx_.device(), &si, nullptr, &renderFinished_) != VK_SUCCESS ||
        vkCreateFence(ctx_.device(), &fi, nullptr, &inFlight_) != VK_SUCCESS)
        throw std::runtime_error("createSyncObjects failed");
}

void VulkanTriangleRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    // 1) シャドウパス
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo shadowRp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    shadowRp.renderPass = shadowRenderPass_;
    shadowRp.framebuffer = shadowFramebuffer_;
    shadowRp.renderArea = {{0, 0}, shadowExtent_};
    shadowRp.clearValueCount = 1;
    shadowRp.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(cmd, &shadowRp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);

    VkViewport shadowViewport{
        0.f, 0.f, static_cast<float>(shadowExtent_.width), static_cast<float>(shadowExtent_.height),
        0.f, 1.f};
    vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
    VkRect2D shadowScissor{{0, 0}, shadowExtent_};
    vkCmdSetScissor(cmd, 0, 1, &shadowScissor);
    const VkDeviceSize vbOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &vbOffset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout_, 0, 1,
                            &descriptorSet_, 0, nullptr);
    for (const glm::mat4& model : drawList_) {
        vkCmdPushConstants(cmd, shadowPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(glm::mat4), &model);
        vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);

    // シャドウ画像を次パスでサンプルできるよう同期
    VkImageMemoryBarrier shadowReadBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    shadowReadBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowReadBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shadowReadBarrier.image = shadowImage_;
    shadowReadBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    shadowReadBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    shadowReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &shadowReadBarrier);

    // 2) メインパス
    VkClearValue clearValues[2]{};
    clearValues[0].color = {{0.05f, 0.06f, 0.10f, 1.f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffers_[imageIndex];
    rp.renderArea = {{0, 0}, swapExtent_};
    rp.clearValueCount = 2;
    rp.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{
        0.f, 0.f, static_cast<float>(swapExtent_.width), static_cast<float>(swapExtent_.height),
        0.f, 1.f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, swapExtent_};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &vbOffset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSet_, 0, nullptr);

    for (const glm::mat4& model : drawList_) {
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                           &model);
        vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");
}

// ─── 公開 API ─────────────────────────────────────────────────────────────────

void VulkanTriangleRenderer::init(SDL_Window* window) {
    window_ = window;
    const char* base = SDL_GetBasePath();
    if (!base) throw std::runtime_error("SDL_GetBasePath failed");
    shaderDir_ = std::string(base) + "shaders/";
    assetDir_ = std::string(base) + "assets/";

    // Step 1: Vulkan コアの初期化はコンテキストに委譲
    ctx_.init(window);
    depthFormat_ = ctx_.findDepthFormat();

    createSwapchain();
    createImageViews();
    createRenderPass();
    createShadowRenderPass();
    createCommandPool();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffer();
    createDescriptorSetLayout();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createShadowResources();
    createShadowPipeline();
    createGraphicsPipeline();
    createDepthResources();
    createDescriptorPool();
    createDescriptorSet();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
    initImGui();
}

void VulkanTriangleRenderer::cleanupSwapchain() {
    for (auto fb : framebuffers_) vkDestroyFramebuffer(ctx_.device(), fb, nullptr);
    framebuffers_.clear();
    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(ctx_.device(), commandPool_,
                             static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_.device(), depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_.device(), depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_.device(), depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }
    for (auto v : swapViews_) vkDestroyImageView(ctx_.device(), v, nullptr);
    swapViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanTriangleRenderer::recreateSwapchain() {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    while (w == 0 || h == 0) {
        SDL_Event e;
        SDL_WaitEvent(&e);
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }
    vkDeviceWaitIdle(ctx_.device());
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
}

void VulkanTriangleRenderer::onResize() {
    if (ctx_.device() != VK_NULL_HANDLE) recreateSwapchain();
}

void VulkanTriangleRenderer::drawFrame(std::function<void()> uiCallback) {
    vkWaitForFences(ctx_.device(), 1, &inFlight_, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex = 0;
    VkResult ar = vkAcquireNextImageKHR(ctx_.device(), swapchain_, UINT64_MAX, imageAvailable_,
                                        VK_NULL_HANDLE, &imageIndex);
    if (ar == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    vkResetFences(ctx_.device(), 1, &inFlight_);
    updateUniformBuffer();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (uiCallback) uiCallback();
    ImGui::Render();
    vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
    recordCommandBuffer(commandBuffers_[imageIndex], imageIndex);
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &imageAvailable_;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &commandBuffers_[imageIndex];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinished_;
    if (vkQueueSubmit(ctx_.graphicsQueue(), 1, &si, inFlight_) != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed");
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinished_;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &imageIndex;
    VkResult pr = vkQueuePresentKHR(ctx_.presentQueue(), &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
        recreateSwapchain();
    else if (pr != VK_SUCCESS)
        throw std::runtime_error("vkQueuePresentKHR failed");
}

// ─── ImGui ライフサイクル ─────────────────────────────────────────────────
void VulkanTriangleRenderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(window_);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = ctx_.instance();
    initInfo.PhysicalDevice = ctx_.physicalDevice();
    initInfo.Device = ctx_.device();
    initInfo.QueueFamily = ctx_.graphicsFamily();
    initInfo.Queue = ctx_.graphicsQueue();
    initInfo.DescriptorPoolSize = 1000;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(swapImages_.size());
    initInfo.PipelineInfoMain.RenderPass = renderPass_;

    ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanTriangleRenderer::shutdownImGui() {
    if (!ImGui::GetCurrentContext()) return;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void VulkanTriangleRenderer::shutdown() {
    if (ctx_.device() == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(ctx_.device());
    shutdownImGui();
    vkDestroySemaphore(ctx_.device(), imageAvailable_, nullptr);
    vkDestroySemaphore(ctx_.device(), renderFinished_, nullptr);
    vkDestroyFence(ctx_.device(), inFlight_, nullptr);
    cleanupSwapchain();
    if (shadowFramebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx_.device(), shadowFramebuffer_, nullptr);
        shadowFramebuffer_ = VK_NULL_HANDLE;
    }
    if (shadowSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(ctx_.device(), shadowSampler_, nullptr);
        shadowSampler_ = VK_NULL_HANDLE;
    }
    if (shadowImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_.device(), shadowImageView_, nullptr);
        shadowImageView_ = VK_NULL_HANDLE;
    }
    if (shadowImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_.device(), shadowImage_, nullptr);
        shadowImage_ = VK_NULL_HANDLE;
    }
    if (shadowImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_.device(), shadowImageMemory_, nullptr);
        shadowImageMemory_ = VK_NULL_HANDLE;
    }
    vkDestroyPipeline(ctx_.device(), pipeline_, nullptr);
    vkDestroyPipelineLayout(ctx_.device(), pipelineLayout_, nullptr);
    if (shadowPipeline_ != VK_NULL_HANDLE)
        vkDestroyPipeline(ctx_.device(), shadowPipeline_, nullptr);
    if (shadowPipelineLayout_ != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(ctx_.device(), shadowPipelineLayout_, nullptr);
    if (textureSampler_ != VK_NULL_HANDLE)
        vkDestroySampler(ctx_.device(), textureSampler_, nullptr);
    if (textureImageView_ != VK_NULL_HANDLE)
        vkDestroyImageView(ctx_.device(), textureImageView_, nullptr);
    if (textureImage_ != VK_NULL_HANDLE) vkDestroyImage(ctx_.device(), textureImage_, nullptr);
    if (textureImageMemory_ != VK_NULL_HANDLE)
        vkFreeMemory(ctx_.device(), textureImageMemory_, nullptr);
    vkDestroyDescriptorPool(ctx_.device(), descriptorPool_, nullptr);
    vkDestroyDescriptorSetLayout(ctx_.device(), descriptorSetLayout_, nullptr);
    if (uniformMapped_) vkUnmapMemory(ctx_.device(), uniformBufferMemory_);
    if (uniformBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_.device(), uniformBuffer_, nullptr);
        vkFreeMemory(ctx_.device(), uniformBufferMemory_, nullptr);
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_.device(), indexBuffer_, nullptr);
        vkFreeMemory(ctx_.device(), indexBufferMemory_, nullptr);
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_.device(), vertexBuffer_, nullptr);
        vkFreeMemory(ctx_.device(), vertexBufferMemory_, nullptr);
    }
    vkDestroyRenderPass(ctx_.device(), renderPass_, nullptr);
    if (shadowRenderPass_ != VK_NULL_HANDLE)
        vkDestroyRenderPass(ctx_.device(), shadowRenderPass_, nullptr);
    vkDestroyCommandPool(ctx_.device(), commandPool_, nullptr);

    // Step 1: Instance/Surface/Device の破棄はコンテキストに委譲
    ctx_.shutdown();
}

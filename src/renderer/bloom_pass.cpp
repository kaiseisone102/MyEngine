// src/renderer/bloom_pass.cpp
// =============================================================================
// bloom_pass.cpp - Phase 1I: compute-based mip-chain bloom (Jimenez / CoD:AW).
// First compute pass in the engine. See bloom_pass.h for the flow + contract.
//
// Conventions established here for future compute passes (2B culling, etc.):
//   - Workgroup is 8x8; dispatch groups = ceil(dst extent / 8).
//   - Storage images are written in GENERAL layout; sampled reads also use
//     GENERAL (a single layout for the whole chain keeps barriers simple).
//   - Between dispatches we insert a COMPUTE->COMPUTE barrier on the just-written
//     image: srcAccess SHADER_WRITE -> dstAccess SHADER_READ, so the next stage
//     samples completed data. The HDR input is produced by a prior graphics pass;
//     pass_chain orders bloom after MainPass so HDR is already readable.
// =============================================================================
#include "renderer/bloom_pass.h"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "renderer/barrier.h"
#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

namespace {
constexpr uint32_t kLocalSize = 8;
inline uint32_t groups(uint32_t n) { return (n + kLocalSize - 1) / kLocalSize; }
}  // namespace

// --- mip math: the ONLY place that decides count + per-mip extent -----------
std::vector<BloomPass::MipSize> BloomPass::computeMipSizes() const {
    std::vector<MipSize> sizes;
    uint32_t w = baseWidth_, h = baseHeight_;
    for (uint32_t i = 0; i < maxMips_; ++i) {
        if (w == 0 || h == 0) break;
        sizes.push_back({w, h});
        if (w == 1 && h == 1) break;
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    return sizes;
}

void BloomPass::init(const InitInfo& info) {
    if (!info.ctx) throw std::runtime_error("BloomPass::init: ctx is null");
    if (info.hdrColorView == VK_NULL_HANDLE) throw std::runtime_error("BloomPass::init: hdrColorView null");
    if (info.bloomFormat == VK_FORMAT_UNDEFINED) throw std::runtime_error("BloomPass::init: bloomFormat undefined");
    if (info.baseWidth == 0 || info.baseHeight == 0) throw std::runtime_error("BloomPass::init: zero extent");

    ctx_ = info.ctx;
    resources_ = info.resources;
    if (!resources_) throw std::runtime_error("BloomPass::init: resources is null");
    hdrColorView_ = info.hdrColorView;
    hdrColorSampler_ = info.hdrColorSampler;
    bloomFormat_ = info.bloomFormat;
    baseWidth_ = info.baseWidth;
    baseHeight_ = info.baseHeight;
    maxMips_ = std::max(2u, info.maxMips);  // need at least 2 mips for a chain
    shaderDir_ = info.shaderDir;

    createMipsAndViews();
    createDescriptorInfra();
    allocateAndWriteSets();
    createPipelines(shaderDir_);
}

void BloomPass::shutdown() {
    if (!ctx_) return;
    pipeUpsample_.reset();
    pipeDownsample_.reset();
    pipeBright_.reset();
    pipelineLayout_.reset();
    destroyMipsAndSets();
    descPool_.reset();
    descSetLayout_.reset();
}

void BloomPass::onSwapchainResized(const InitInfo& info) {
    // Extent + HDR view changed. Pipelines + layout + set layout are extent-
    // independent and kept. Rebuild mips, views, sampler, pool, and sets.
    hdrColorView_ = info.hdrColorView;
    hdrColorSampler_ = info.hdrColorSampler;
    baseWidth_ = info.baseWidth;
    baseHeight_ = info.baseHeight;

    destroyMipsAndSets();
    descPool_.reset();
    createMipsAndViews();
    createDescriptorInfra();   // recreates pool (layout kept, but pool was reset)
    allocateAndWriteSets();
}

// --- mips: each is STORAGE | SAMPLED, GENERAL layout; one view + shared sampler
void BloomPass::createMipsAndViews() {
    const auto sizes = computeMipSizes();
    if (sizes.size() < 2) throw std::runtime_error("BloomPass: extent too small for a mip chain");

    mips_.clear();
    mipViews_.clear();
    mips_.reserve(sizes.size());
    mipViews_.reserve(sizes.size());

    const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // TRANSFER_DST: clearToReadable (bloom off)
    for (const auto& s : sizes) {
        VmaImage img = VmaImage::createAttachment(ctx_, s.w, s.h, bloomFormat_, usage);

        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = img.image();
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = bloomFormat_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(ctx_->device(), &vi, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("BloomPass: vkCreateImageView failed");

        mips_.push_back(std::move(img));
        mipViews_.emplace_back(ctx_->device(), view);
    }

    // One shared sampler: linear, clamp-to-edge (avoids wrap bleeding at edges).
    VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sc.magFilter = VK_FILTER_LINEAR;
    sc.minFilter = VK_FILTER_LINEAR;
    sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sc.addressModeU = sc.addressModeV = sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sc.maxLod = 0.0f;
    VkSampler samp = VK_NULL_HANDLE;
    if (vkCreateSampler(ctx_->device(), &sc, nullptr, &samp) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreateSampler failed");
    sampler_ = VkUnique<VkSampler>(ctx_->device(), samp);

    // Transition every mip UNDEFINED -> GENERAL once, up front. Uses the shared
    // ResourceFactory helper (its generic fallback covers UNDEFINED->GENERAL).
    // Storage writes only need GENERAL layout; initial contents are irrelevant
    // because the first dispatch fully overwrites each mip.
    for (auto& img : mips_) {
        resources_->transitionImageLayout(img.image(), VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_GENERAL);
    }
}

void BloomPass::createDescriptorInfra() {
    // binding 0: sampled source (sampler2D). binding 1: storage dest (image2D).
    std::array<VkDescriptorSetLayoutBinding, 2> b{};
    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    b[1].descriptorCount = 1;
    b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    if (!descSetLayout_) {  // layout is extent-independent; create once
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = static_cast<uint32_t>(b.size());
        ci.pBindings = b.data();
        VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &dsl) != VK_SUCCESS)
            throw std::runtime_error("BloomPass: vkCreateDescriptorSetLayout failed");
        descSetLayout_ = VkUnique<VkDescriptorSetLayout>(ctx_->device(), dsl);
    }

    // Sets needed: 1 (bright) + (n-1) down + (n-1) up.
    const uint32_t n = static_cast<uint32_t>(mips_.size());
    const uint32_t setCount = 1u + (n - 1u) * 2u;

    std::array<VkDescriptorPoolSize, 2> ps{};
    ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[0].descriptorCount = setCount;
    ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ps[1].descriptorCount = setCount;

    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = setCount;
    pci.poolSizeCount = static_cast<uint32_t>(ps.size());
    pci.pPoolSizes = ps.data();
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(ctx_->device(), &pci, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreateDescriptorPool failed");
    descPool_ = VkUnique<VkDescriptorPool>(ctx_->device(), pool);
}

void BloomPass::allocateAndWriteSets() {
    const uint32_t n = static_cast<uint32_t>(mips_.size());
    const uint32_t setCount = 1u + (n - 1u) * 2u;

    std::vector<VkDescriptorSetLayout> layouts(setCount, descSetLayout_.get());
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descPool_.get();
    ai.descriptorSetCount = setCount;
    ai.pSetLayouts = layouts.data();
    std::vector<VkDescriptorSet> sets(setCount);
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, sets.data()) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkAllocateDescriptorSets failed");

    setBright_ = sets[0];
    setDown_.assign(n - 1, VK_NULL_HANDLE);
    setUp_.assign(n - 1, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < n - 1; ++i) {
        setDown_[i] = sets[1 + i];
        setUp_[i] = sets[1 + (n - 1) + i];
    }

    // Helper: write (sampledView, storageView) into a set. sampledLayout differs:
    // the HDR input is SHADER_READ_ONLY_OPTIMAL (owned by MainPass), while mips are
    // GENERAL (we read+write them as storage within this pass).
    auto writeSet = [&](VkDescriptorSet set, VkImageView sampledView, VkImageLayout sampledLayout,
                        VkImageView storageView) {
        VkDescriptorImageInfo si{};
        si.imageLayout = sampledLayout;
        si.imageView = sampledView;
        si.sampler = sampler_.get();
        VkDescriptorImageInfo di{};
        di.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        di.imageView = storageView;
        di.sampler = VK_NULL_HANDLE;

        std::array<VkWriteDescriptorSet, 2> w{};
        w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = set;
        w[0].dstBinding = 0;
        w[0].descriptorCount = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].pImageInfo = &si;
        w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[1].dstSet = set;
        w[1].dstBinding = 1;
        w[1].descriptorCount = 1;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w[1].pImageInfo = &di;
        vkUpdateDescriptorSets(ctx_->device(), 2, w.data(), 0, nullptr);
    };

    // bright: read HDR (SHADER_READ_ONLY, owned by MainPass), write mip0 (storage).
    writeSet(setBright_, hdrColorView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipViews_[0].get());
    // down[i]: read mip[i] (GENERAL), write mip[i+1] (storage).
    for (uint32_t i = 0; i < n - 1; ++i)
        writeSet(setDown_[i], mipViews_[i].get(), VK_IMAGE_LAYOUT_GENERAL, mipViews_[i + 1].get());
    // up[i]: read mip[i+1] (GENERAL), write mip[i] (storage, additive in shader).
    for (uint32_t i = 0; i < n - 1; ++i)
        writeSet(setUp_[i], mipViews_[i + 1].get(), VK_IMAGE_LAYOUT_GENERAL, mipViews_[i].get());
}

void BloomPass::createPipelines(const std::string& shaderDir) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(PushConstants);

    VkDescriptorSetLayout dsl = descSetLayout_.get();
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &dsl;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pc;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pl) != VK_SUCCESS)
        throw std::runtime_error("BloomPass: vkCreatePipelineLayout failed");
    pipelineLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), pl);

    auto buildCompute = [&](const std::string& spv) -> VkPipeline {
        VkShaderModule mod = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/" + spv);
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = mod;
        stage.pName = "main";
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage = stage;
        ci.layout = pipelineLayout_.get();
        VkPipeline p = VK_NULL_HANDLE;
        VkResult r = vkCreateComputePipelines(ctx_->device(), ctx_->pipelineCache(), 1, &ci, nullptr, &p);
        vkDestroyShaderModule(ctx_->device(), mod, nullptr);
        if (r != VK_SUCCESS) throw std::runtime_error("BloomPass: vkCreateComputePipelines failed");
        return p;
    };

    pipeBright_ = VkUnique<VkPipeline>(ctx_->device(), buildCompute("bloom_bright_comp.spv"));
    pipeDownsample_ = VkUnique<VkPipeline>(ctx_->device(), buildCompute("bloom_downsample_comp.spv"));
    pipeUpsample_ = VkUnique<VkPipeline>(ctx_->device(), buildCompute("bloom_upsample_comp.spv"));
}

void BloomPass::destroyMipsAndSets() {
    // Sets are freed implicitly when the pool is reset/destroyed by the caller.
    setBright_ = VK_NULL_HANDLE;
    setDown_.clear();
    setUp_.clear();
    mipViews_.clear();  // VkUnique views freed
    mips_.clear();      // VmaImage freed
    sampler_.reset();
}

void BloomPass::barrierWriteToRead(VkCommandBuffer cmd, VkImage img) {
    barrier::recordImage(*ctx_, cmd, barrier::ImageBarrier{
        .image = img,
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStage  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccess = VK_ACCESS_2_SHADER_READ_BIT,
    });
}

// mip0 layout flips so PostPass can sample it as SHADER_READ_ONLY while bloom
// produces it in GENERAL. Called at the start (->GENERAL) and end (->READ) of execute.
void BloomPass::transitionMip0(VkCommandBuffer cmd, VkImageLayout oldL, VkImageLayout newL,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    // The caller still passes 32-bit legacy flag types for now; widen here.
    // VK_*_2_* values overlap with the legacy bits, so the cast is exact for
    // all flags currently in use at bloom call sites.
    barrier::recordImage(*ctx_, cmd, barrier::ImageBarrier{
        .image = mips_[0].image(),
        .range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .oldLayout = oldL,
        .newLayout = newL,
        .srcStage  = static_cast<VkPipelineStageFlags2>(srcStage),
        .srcAccess = static_cast<VkAccessFlags2>(srcAccess),
        .dstStage  = static_cast<VkPipelineStageFlags2>(dstStage),
        .dstAccess = static_cast<VkAccessFlags2>(dstAccess),
    });
}

void BloomPass::recordDispatch(VkCommandBuffer cmd, VkPipeline pipe, VkDescriptorSet set,
                               uint32_t dstW, uint32_t dstH, const PushConstants& pc) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_.get(), 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);
    vkCmdDispatch(cmd, groups(dstW), groups(dstH), 1);
}

void BloomPass::execute(const ExecuteInfo& info) {
    if (!info.cmd) throw std::runtime_error("BloomPass::execute: invalid cmd");
    const auto sizes = computeMipSizes();
    const uint32_t n = static_cast<uint32_t>(mips_.size());
    if (n < 2) return;

    PushConstants pc{};
    pc.threshold = threshold_;
    pc.softKnee = softKnee_;
    pc.intensity = 1.0f;

    // mip0 was left in SHADER_READ_ONLY by the previous frame (for PostPass). Flip
    // it back to GENERAL before the bright pass writes it. UNDEFINED old layout =
    // "don't care about contents" (bright fully overwrites mip0 anyway).
    transitionMip0(info.cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                   VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // 1) bright: HDR -> mip0
    pc.param = 0.0f;
    recordDispatch(info.cmd, pipeBright_.get(), setBright_, sizes[0].w, sizes[0].h, pc);
    barrierWriteToRead(info.cmd, mips_[0].image());

    // 2) downsample chain: mip[i] -> mip[i+1]. Karis only on the first edge.
    for (uint32_t i = 0; i < n - 1; ++i) {
        pc.param = (i == 0) ? 1.0f : 0.0f;  // Karis average for mip0->mip1
        recordDispatch(info.cmd, pipeDownsample_.get(), setDown_[i], sizes[i + 1].w, sizes[i + 1].h, pc);
        barrierWriteToRead(info.cmd, mips_[i + 1].image());
    }

    // 3) upsample chain: read mip[i+1], add into mip[i]. Top-down (i = n-2 .. 0).
    pc.param = filterRadius_;
    for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
        recordDispatch(info.cmd, pipeUpsample_.get(), setUp_[i], sizes[i].w, sizes[i].h, pc);
        barrierWriteToRead(info.cmd, mips_[i].image());
    }
    // mip0 now holds the final bloom. Flip it to SHADER_READ_ONLY so PostPass (a
    // graphics pass) can sample it with the layout its descriptor expects.
    transitionMip0(info.cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void BloomPass::clearToReadable(VkCommandBuffer cmd) {
    if (mips_.empty()) return;
    // mip0 -> TRANSFER_DST (old contents don't matter), clear to black, then
    // -> SHADER_READ_ONLY for PostPass. No dispatches, so bloom contributes 0.
    transitionMip0(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkClearColorValue black{};
    black.float32[3] = 1.0f;
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, mips_[0].image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &black, 1, &range);
    transitionMip0(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}
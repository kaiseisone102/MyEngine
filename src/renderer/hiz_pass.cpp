// src/renderer/hiz_pass.cpp
// =============================================================================
// hiz_pass.cpp - PART4 4b: Hi-Z pyramid generation (SPD-style single dispatch)
// =============================================================================
// See hiz_pass.h for the algorithm and design rationale.
//
// Compute conventions inherited from BloomPass (Codebase_Guide §3):
//   - storage images are written in GENERAL layout.
//   - between dispatches that write then read the same image, insert a
//     COMPUTE->COMPUTE barrier (SHADER_WRITE -> SHADER_READ on GENERAL).
//   - SPD does its inter-mip synchronisation inside ONE dispatch via LDS and
//     an atomic counter, so the only external barriers we emit are:
//       (a) reset the atomic counter (TRANSFER_WRITE) -> dispatch reads it.
//       (b) dispatch writes pyramid -> downstream FRAGMENT_SHADER reads it.
//
// First-frame layout init:
//   The pyramid is created in UNDEFINED. Before the first dispatch we
//   transition all mips UNDEFINED -> GENERAL with a one-time helper from
//   resource_factory (same pattern bloom uses). This is recorded inside the
//   command buffer the renderer is already building, so it composes cleanly
//   with the rest of the frame.
// =============================================================================

#include "renderer/hiz_pass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "renderer/barrier.h"
#include "renderer/depth_layouts.h"
#include "renderer/resource_factory.h"
#include "renderer/shader_util.h"
#include "renderer/vulkan_context.h"

namespace {

constexpr VkFormat kPyramidFormat = VK_FORMAT_R32G32_SFLOAT;

// SPD spawns one workgroup per 64x64 source tile.
constexpr uint32_t kTileSize = 64u;
constexpr uint32_t kGroupSize = 256u;

uint32_t groupsCovering(uint32_t n, uint32_t tile) {
    return (n + tile - 1u) / tile;
}

}  // namespace

// =============================================================================
// Layout / pyramid geometry
// =============================================================================

void HiZPass::computeMipLayout() {
    // Output mip0 is half-resolution of input depth (2x2 source -> 1 mip0
    // texel). This matches the SPD reference behaviour and is what 4c
    // expects to sample. Tail mips halve until 1x1.
    mip0Width_ = std::max(1u, inputWidth_ / 2u);
    mip0Height_ = std::max(1u, inputHeight_ / 2u);

    uint32_t w = mip0Width_, h = mip0Height_;
    uint32_t count = 0;
    while (count < kMaxMips) {
        ++count;
        if (w == 1 && h == 1) break;
        w = std::max(1u, w / 2u);
        h = std::max(1u, h / 2u);
    }
    mipCount_ = count;

    numWorkGroupsTotal_ =
        groupsCovering(inputWidth_, kTileSize) * groupsCovering(inputHeight_, kTileSize);
}

// =============================================================================
// Pyramid VMA images + views + atomic counter
// =============================================================================

void HiZPass::createPyramids() {
    for (uint32_t f = 0; f < kMaxFramesInFlight; ++f) {
        PerFrame& fr = frames_[f];
        fr.pyramidInited = false;

        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = kPyramidFormat;
        ci.extent = {mip0Width_, mip0Height_, 1u};
        ci.mipLevels = mipCount_;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        fr.pyramid = VmaImage::create(ctx_, ci, /*dedicated=*/true);

        // Per-mip storage views (image2D, single mip).
        fr.mipViews.clear();
        fr.mipViews.reserve(mipCount_);
        for (uint32_t m = 0; m < mipCount_; ++m) {
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = fr.pyramid.image();
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = kPyramidFormat;
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, m, 1, 0, 1};
            VkImageView mv = VK_NULL_HANDLE;
            if (vkCreateImageView(ctx_->device(), &vi, nullptr, &mv) != VK_SUCCESS)
                throw std::runtime_error("HiZPass: vkCreateImageView (mip) failed");
            fr.mipViews.emplace_back(ctx_->device(), mv);
        }

        // Full-chain sampled view (used by 4c cull.comp; here for completeness).
        {
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = fr.pyramid.image();
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = kPyramidFormat;
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipCount_, 0, 1};
            VkImageView sv = VK_NULL_HANDLE;
            if (vkCreateImageView(ctx_->device(), &vi, nullptr, &sv) != VK_SUCCESS)
                throw std::runtime_error("HiZPass: vkCreateImageView (sampled) failed");
            fr.sampledView = VkUnique<VkImageView>(ctx_->device(), sv);
        }

        // Atomic counter: 1 uint, device-local STORAGE + TRANSFER_DST. The
        // shader does atomicAdd via the descriptor SSBO binding (no BDA / no
        // host mapping needed); the CPU only resets it via vkCmdFillBuffer
        // each frame, which is a GPU-side operation. Device-local memory
        // makes the GPU atomic stay in VRAM instead of round-tripping host
        // memory the previous createMappedStorageBDA call put it in.
        fr.atomicCounter = VmaBuffer::createDeviceLocal(
            ctx_, sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }
}

void HiZPass::destroyPyramids() {
    for (uint32_t f = 0; f < kMaxFramesInFlight; ++f) {
        PerFrame& fr = frames_[f];
        fr.set = VK_NULL_HANDLE;  // freed via pool reset/destroy
        fr.mipViews.clear();
        fr.sampledView.reset();
        fr.pyramid.reset();
        fr.atomicCounter.reset();
        fr.pyramidInited = false;
    }
}

// =============================================================================
// Descriptor set layout / pool / writes
// =============================================================================

void HiZPass::createDescriptorInfra() {
    // binding 0: combined sampler for input depth (COMBINED_IMAGE_SAMPLER).
    // binding 1: output storage image array (descriptorCount = kMaxMips).
    // binding 2: atomic counter SSBO.
    std::array<VkDescriptorSetLayoutBinding, 3> b{};
    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    b[1].descriptorCount = kMaxMips;
    b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    b[2].binding = 2;
    b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[2].descriptorCount = 1;
    b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    if (!setLayout_) {
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = static_cast<uint32_t>(b.size());
        ci.pBindings = b.data();
        VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(ctx_->device(), &ci, nullptr, &dsl) != VK_SUCCESS)
            throw std::runtime_error("HiZPass: vkCreateDescriptorSetLayout failed");
        setLayout_ = VkUnique<VkDescriptorSetLayout>(ctx_->device(), dsl);
    }

    std::array<VkDescriptorPoolSize, 3> ps{};
    ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[0].descriptorCount = kMaxFramesInFlight;
    ps[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ps[1].descriptorCount = kMaxFramesInFlight * kMaxMips;
    ps[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ps[2].descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = kMaxFramesInFlight;
    pci.poolSizeCount = static_cast<uint32_t>(ps.size());
    pci.pPoolSizes = ps.data();
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(ctx_->device(), &pci, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("HiZPass: vkCreateDescriptorPool failed");
    descPool_ = VkUnique<VkDescriptorPool>(ctx_->device(), pool);
}

void HiZPass::allocateAndWriteSets() {
    std::array<VkDescriptorSetLayout, kMaxFramesInFlight> layouts{};
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) layouts[i] = setLayout_.get();

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descPool_.get();
    ai.descriptorSetCount = kMaxFramesInFlight;
    ai.pSetLayouts = layouts.data();
    std::array<VkDescriptorSet, kMaxFramesInFlight> sets{};
    if (vkAllocateDescriptorSets(ctx_->device(), &ai, sets.data()) != VK_SUCCESS)
        throw std::runtime_error("HiZPass: vkAllocateDescriptorSets failed");

    for (uint32_t f = 0; f < kMaxFramesInFlight; ++f) {
        PerFrame& fr = frames_[f];
        fr.set = sets[f];

        VkDescriptorImageInfo depthDi{};
        // Must match the layout main_pass's post-barrier leaves the depth in.
        // depth_layouts::readOnly() returns VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
        // when separateDepthStencilLayouts is supported, and the legacy combined
        // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL otherwise. Hardcoding
        // the former trips VUID-vkCmdDispatch-imageLayout on devices lacking
        // that 1.2 optional feature.
        depthDi.imageLayout = depth_layouts::readOnly(*ctx_);
        depthDi.imageView = depthView_;
        depthDi.sampler = depthSampler_.get();

        // Storage image array (binding=1). We supply mipCount_ real views plus
        // (kMaxMips - mipCount_) padding entries pointing at the LAST real mip
        // - that's a safe no-op write target for any shader code that
        // out-of-bounds writes (it won't be reached at runtime because the
        // shader is gated by push-constant mipsToGenerate, but Vulkan needs
        // every array element to be a valid descriptor).
        std::array<VkDescriptorImageInfo, kMaxMips> mipDi{};
        const VkImageView fallbackMip = fr.mipViews.back().get();
        for (uint32_t m = 0; m < kMaxMips; ++m) {
            VkImageView v = (m < mipCount_) ? fr.mipViews[m].get() : fallbackMip;
            mipDi[m].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            mipDi[m].imageView = v;
            mipDi[m].sampler = VK_NULL_HANDLE;
        }

        VkDescriptorBufferInfo counterDi{};
        counterDi.buffer = fr.atomicCounter.buffer();
        counterDi.offset = 0;
        counterDi.range = VK_WHOLE_SIZE;

        std::array<VkWriteDescriptorSet, 3> w{};
        w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = fr.set;
        w[0].dstBinding = 0;
        w[0].descriptorCount = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].pImageInfo = &depthDi;
        w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[1].dstSet = fr.set;
        w[1].dstBinding = 1;
        w[1].descriptorCount = kMaxMips;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w[1].pImageInfo = mipDi.data();
        w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[2].dstSet = fr.set;
        w[2].dstBinding = 2;
        w[2].descriptorCount = 1;
        w[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[2].pBufferInfo = &counterDi;
        vkUpdateDescriptorSets(ctx_->device(), 3, w.data(), 0, nullptr);
    }
}

// =============================================================================
// Pipeline
// =============================================================================

void HiZPass::createPipeline(const std::string& shaderDir) {
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc.offset = 0;
    pc.size = sizeof(PushConstants);

    VkDescriptorSetLayout dsl = setLayout_.get();
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &dsl;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pc;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &pl) != VK_SUCCESS)
        throw std::runtime_error("HiZPass: vkCreatePipelineLayout failed");
    pipelineLayout_ = VkUnique<VkPipelineLayout>(ctx_->device(), pl);

    // Pick wave-ops or LDS-only SPIR-V based on queried capability.
    // The wave-ops variant uses subgroupShuffleXor(_, 16) to read the y-axis
    // neighbour in Phase C's mip1 -> mip2 reduction; that shuffle stays
    // within ONE subgroup only when subgroupSize >= 32 (16x16 thread grid).
    // Smaller subgroup sizes (Mali, some Intel) drop back to LDS-only.
    useWavePath_ = ctx_->subgroupOps() && ctx_->subgroupSize() >= 32u;
    const std::string spvName = useWavePath_
                                    ? "hiz_spd_wave_comp.spv"
                                    : "hiz_spd_comp.spv";
    VkShaderModule mod = shader_util::loadShaderModule(ctx_->device(), shaderDir + "/" + spvName);
    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName = "main";
    VkComputePipelineCreateInfo cci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cci.stage = stage;
    cci.layout = pipelineLayout_.get();
    VkPipeline p = VK_NULL_HANDLE;
    VkResult r = vkCreateComputePipelines(ctx_->device(), VK_NULL_HANDLE, 1, &cci, nullptr, &p);
    vkDestroyShaderModule(ctx_->device(), mod, nullptr);
    if (r != VK_SUCCESS) throw std::runtime_error("HiZPass: vkCreateComputePipelines failed");
    pipeline_ = VkUnique<VkPipeline>(ctx_->device(), p);
}

void HiZPass::destroyPipeline() {
    pipeline_.reset();
    pipelineLayout_.reset();
}

// =============================================================================
// init / shutdown / resize
// =============================================================================

void HiZPass::init(const InitInfo& info) {
    if (!info.ctx || !info.resources)
        throw std::runtime_error("HiZPass::init: ctx / resources is null");
    if (info.depthView == VK_NULL_HANDLE)
        throw std::runtime_error("HiZPass::init: depthView is null");
    if (info.baseWidth == 0 || info.baseHeight == 0)
        throw std::runtime_error("HiZPass::init: zero extent");

    ctx_ = info.ctx;
    resources_ = info.resources;
    depthView_ = info.depthView;
    depthFormat_ = info.depthFormat;
    inputWidth_ = info.baseWidth;
    inputHeight_ = info.baseHeight;
    shaderDir_ = info.shaderDir;

    computeMipLayout();

    // Samplers (extent-independent).
    {
        // Input depth: NEAREST + CLAMP. We use texelFetch in the shader for
        // exact min/max reduction; the sampler exists only because the
        // descriptor type is COMBINED_IMAGE_SAMPLER.
        VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sc.magFilter = VK_FILTER_NEAREST;
        sc.minFilter = VK_FILTER_NEAREST;
        sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sc.addressModeU = sc.addressModeV = sc.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSampler s = VK_NULL_HANDLE;
        if (vkCreateSampler(ctx_->device(), &sc, nullptr, &s) != VK_SUCCESS)
            throw std::runtime_error("HiZPass: vkCreateSampler (depth) failed");
        depthSampler_ = VkUnique<VkSampler>(ctx_->device(), s);
    }
    {
        // Downstream sampler (debug widget + 4c cull). LINEAR for display,
        // mipmap-NEAREST so cull.comp can pick a specific mip per AABB.
        VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sc.magFilter = VK_FILTER_LINEAR;
        sc.minFilter = VK_FILTER_LINEAR;
        sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sc.addressModeU = sc.addressModeV = sc.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sc.maxLod = static_cast<float>(kMaxMips);
        VkSampler s = VK_NULL_HANDLE;
        if (vkCreateSampler(ctx_->device(), &sc, nullptr, &s) != VK_SUCCESS)
            throw std::runtime_error("HiZPass: vkCreateSampler (pyramid) failed");
        pyramidSampler_ = VkUnique<VkSampler>(ctx_->device(), s);
    }
    {
        // PART4 4c-C: min-reduction sampler for cull.comp HZB occlusion. When
        // samplerFilterMinmax is supported a single textureLod() returns the
        // min of the 2x2 footprint (= the conservative occluder under
        // reverse-Z, .r channel of the pyramid). Without it we fall back to a
        // plain NEAREST sampler and cull.comp does 4 fetches.
        VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sc.addressModeU = sc.addressModeV = sc.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sc.maxLod = static_cast<float>(kMaxMips);
        VkSamplerReductionModeCreateInfo reductionCi{
            VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO};
        reductionCi.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
        if (ctx_->samplerFilterMinmax()) {
            // LINEAR + reduction = "MIN over the 2x2 LINEAR footprint".
            sc.magFilter = VK_FILTER_LINEAR;
            sc.minFilter = VK_FILTER_LINEAR;
            sc.pNext = &reductionCi;
            minReductionFastPath_ = true;
        } else {
            // No reduction feature: cull.comp branches on minReductionFastPath_
            // and does 4 NEAREST taps to compute min by hand.
            sc.magFilter = VK_FILTER_NEAREST;
            sc.minFilter = VK_FILTER_NEAREST;
            minReductionFastPath_ = false;
        }
        VkSampler s = VK_NULL_HANDLE;
        if (vkCreateSampler(ctx_->device(), &sc, nullptr, &s) != VK_SUCCESS)
            throw std::runtime_error("HiZPass: vkCreateSampler (minReduction) failed");
        minReductionSampler_ = VkUnique<VkSampler>(ctx_->device(), s);
    }

    createPyramids();
    createDescriptorInfra();
    allocateAndWriteSets();
    createPipeline(shaderDir_);
}

void HiZPass::shutdown() {
    if (!ctx_) return;
    destroyPipeline();
    descPool_.reset();
    setLayout_.reset();
    destroyPyramids();
    minReductionSampler_.reset();  // PART4 4c-C
    pyramidSampler_.reset();
    depthSampler_.reset();
    ctx_ = nullptr;
    resources_ = nullptr;
}

void HiZPass::onSwapchainResized(const InitInfo& info) {
    // Pipeline + descriptor layout are extent-independent. Only pyramids
    // (size changes with depth), per-mip views, descriptor pool, and writes
    // need rebuild. Depth view is re-supplied by the caller.
    depthView_ = info.depthView;
    inputWidth_ = info.baseWidth;
    inputHeight_ = info.baseHeight;

    computeMipLayout();
    destroyPyramids();
    descPool_.reset();
    createPyramids();
    createDescriptorInfra();  // recreates pool only (layout kept)
    allocateAndWriteSets();
}

// =============================================================================
// execute
// =============================================================================

void HiZPass::ensureAllSlotsInGeneral(VkCommandBuffer cmd) {
    // PART4 4c-C: pass_chain calls this once at the start of every frame so
    // cull.comp's pass1 dispatch can bind the HZB descriptor set without
    // tripping a "sampled image in UNDEFINED layout" validation error on
    // pyramids whose own execute() hasn't run yet. initialTransitionToGeneral
    // is idempotent, so this is a no-op after the first kMaxFramesInFlight
    // frames.
    for (uint32_t f = 0; f < kMaxFramesInFlight; ++f) {
        initialTransitionToGeneral(cmd, f);
    }
}

void HiZPass::initialTransitionToGeneral(VkCommandBuffer cmd, uint32_t frameIndex) {
    PerFrame& fr = frames_[frameIndex];
    if (fr.pyramidInited) return;
    // One-shot per-frame slot: UNDEFINED -> GENERAL covering ALL mips.
    // PART4 4b Obs B resolved: sync2 best practice for UNDEFINED -> X is
    // VK_PIPELINE_STAGE_2_NONE + VK_ACCESS_2_NONE on the producer side -
    // there is no prior work to synchronise against. The deprecated
    // VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT was tolerated by Vulkan for
    // back-compat but stricter validators flag it.
    barrier::ImageBarrier ib{};
    ib.image = fr.pyramid.image();
    ib.range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipCount_, 0, 1};
    ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ib.srcStage = VK_PIPELINE_STAGE_2_NONE;
    ib.srcAccess = VK_ACCESS_2_NONE;
    ib.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    ib.dstAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier::recordImage(*ctx_, cmd, ib);
    fr.pyramidInited = true;
}

void HiZPass::execute(const ExecuteInfo& info) {
    if (info.cmd == VK_NULL_HANDLE) throw std::runtime_error("HiZPass::execute: cmd null");
    if (info.frameIndex >= kMaxFramesInFlight)
        throw std::runtime_error("HiZPass::execute: frameIndex out of range");
    PerFrame& fr = frames_[info.frameIndex];

    initialTransitionToGeneral(info.cmd, info.frameIndex);

    // 1) Reset the atomic counter to 0. vkCmdFillBuffer is a TRANSFER op; we
    //    need a barrier from TRANSFER_WRITE -> COMPUTE_SHADER (read).
    vkCmdFillBuffer(info.cmd, fr.atomicCounter.buffer(), 0, sizeof(uint32_t), 0u);

    // Barrier batch:
    //   - counter: TRANSFER_WRITE -> SHADER_READ|WRITE (the shader atomicAdds it).
    //   - depth:   no transition (already DEPTH_READ_ONLY_OPTIMAL after
    //     main_pass post-barrier); we only need a memory dependency from
    //     LATE_FRAGMENT_TESTS (depth-write) -> COMPUTE_SHADER (sample).
    //     main_pass already emitted that exact image barrier in its
    //     post-barrier, so we can rely on it; we still emit a fresh memory
    //     dependency for safety.
    barrier::BufferBarrier counterBb{};
    counterBb.buffer = fr.atomicCounter.buffer();
    counterBb.offset = 0;
    counterBb.size = sizeof(uint32_t);
    counterBb.srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    counterBb.srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    counterBb.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    counterBb.dstAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier::recordBuffer(*ctx_, info.cmd, counterBb);

    // 2) Dispatch SPD: one workgroup per 64x64 source tile.
    PushConstants pc{};
    pc.mipsToGenerate = mipCount_;
    pc.numWorkGroupsTotal = numWorkGroupsTotal_;
    pc.inputWidth = inputWidth_;
    pc.inputHeight = inputHeight_;

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.get());
    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_.get(),
                             0, 1, &fr.set, 0, nullptr);
    vkCmdPushConstants(info.cmd, pipelineLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(pc), &pc);
    vkCmdDispatch(info.cmd,
                  groupsCovering(inputWidth_, kTileSize),
                  groupsCovering(inputHeight_, kTileSize),
                  1);

    // 3) Downstream (4c cull.comp, debug widget) reads pyramid as
    //    SHADER_READ; emit a COMPUTE_WRITE -> COMPUTE/FRAGMENT_READ barrier
    //    over the full mip chain. The image stays in GENERAL (we don't
    //    transition to SHADER_READ_ONLY because the debug widget samples it
    //    via ImGui's fragment shader with GENERAL allowed, and 4c will read
    //    it from compute next frame).
    barrier::ImageBarrier ib{};
    ib.image = fr.pyramid.image();
    ib.range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipCount_, 0, 1};
    ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ib.srcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    ib.srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
    ib.dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    ib.dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
    barrier::recordImage(*ctx_, info.cmd, ib);
}

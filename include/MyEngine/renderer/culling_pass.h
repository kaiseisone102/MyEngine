// include/MyEngine/renderer/culling_pass.h
#pragma once
// =============================================================================
// culling_pass.h - Phase 2B PART2 / PART4: GPU-driven frustum culling + scan
// compaction (PART4 4-前-4) + DGC / IndirectCount receptacle.
// =============================================================================
// Engine's second compute pass. Fully BDA-driven (no descriptor sets). Each
// frame the pass:
//
//   1. Uploads per-frame CullObject[] via host-mapped cullStaging_[FIF] +
//      vkCmdCopyBuffer to the persistent device-local cullBuf_.
//   2. Dispatches `cull.comp` (frustum + meshlet-ready cone test) which
//      writes a 1-bit-per-drawId predicate into the bit-packed visBuf_.
//   3. For each GeometryBuffer block range owned by static_cull::build:
//        a. scan_local.comp   (Pass A) - per-workgroup exclusive scan over the
//                                        block's predicates, write per-wg total
//                                        into workgroupTotalsBuf_.
//        b. scan_globals.comp (Pass B) - single-wg in-place scan over the
//                                        block's workgroupTotals slice, write
//                                        the per-block visible count to
//                                        countBuf1_[blockIdx].
//        c. scan_scatter.comp (Pass C) - re-run the local scan, add the per-wg
//                                        prefix, scatter the visible draw's
//                                        cmdBuf template (with instanceCount=1)
//                                        into compactCmd1Buf_ inside its
//                                        block's slot range.
//   4. Hands ownership of compactCmd1Buf_ + countBuf1_ to main_pass via the
//      indirect_exec wrapper, which picks DGC / IndirectCount / Legacy per
//      device capability.
//
// PART4 4c (two-pass occlusion) will plug in a second pass that reuses the
// same scan_compact pipeline plus the symmetrically-allocated compactCmd2Buf_
// + countBuf2_ buffers (allocated today, unused until 4c).
//
// PART4 4-前-3 persistent layout (kept):
//   * cullBuf_:        single device-local CullObject[], grown via
//                      ensureCapacity().
//   * cullStaging_[FIF]: per-frame host-mapped staging ring.
//   * visBuf_:         single device-local uint32[] bit-packed (32 obj/word).
//   * cmdBuf_[FIF]:    per-frame host-mapped DrawCmd[] (CPU writes the
//                      template, scan_scatter reads it).
//
// PART4 4-前-4 new buffers:
//   * compactCmd1Buf_/2: single device-local VkDrawIndexedIndirectCommand[],
//                      pass1 (always today) and pass2 (4c). INDIRECT_BUFFER
//                      + STORAGE + BDA + TRANSFER_DST. compactCmd2 stays
//                      empty today; cost is negligible because it's grown in
//                      lockstep with the rest.
//   * countBuf1_/2:    single device-local uint[INITIAL_BLOCKS], per-block
//                      visible count. INDIRECT_BUFFER + STORAGE + BDA + TRANSFER_DST.
//   * workgroupTotalsBuf_: scratch uint[], per-workgroup partial sum.
//                      Pass A writes, Pass B reads / writes (turns it into
//                      a prefix array), Pass C reads. STORAGE + BDA.
// =============================================================================
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "renderer/vma_buffer.h"
#include "renderer/vk_unique.h"
#include "renderer/frame_sync.h"
#include "shaders/shared/types.h"

class VulkanContext;
class DeletionQueue;

namespace static_cull { struct BlockRange; }

class CullingPass {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    // PART4 4-前-3: starting capacity per dim, NOT a hard cap. Both grow on
    // demand via ensureCapacity() / ensureBlockCount().
    static constexpr uint32_t INITIAL_CAPACITY = 4096;
    static constexpr uint32_t INITIAL_BLOCKS   = 64;
    // PART4 4-前-4: scan workgroup size. Matches NVIDIA subgroup-of-32 * 8 and
    // AMD subgroup-of-64 * 4. Each Pass A workgroup processes this many drawIds.
    static constexpr uint32_t SCAN_WG_SIZE     = 256;

    // PART4 4-前-5: one cull "set" per consumer (camera, shadow, future
    // cascades / lights). Each set has its own visBuf / compactCmd / countBuf /
    // workgroupTotals so multiple culls can run on the same shared CullObject
    // input without overwriting each other's results. Pipelines are shared.
    enum class CullSet : uint32_t {
        Camera = 0,
        Shadow = 1,
    };

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        DeletionQueue* deletionQueue = nullptr;
        std::string shaderDir;
    };

    // A single draw command template (everything except instanceCount, which
    // scan_scatter fills). Matches VkDrawIndexedIndirectCommand layout.
    struct DrawTemplate {
        uint32_t indexCount = 0;
        uint32_t firstIndex = 0;
        int32_t  vertexOffset = 0;
        uint32_t firstInstance = 0;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
        // PART4 4-前-5: which cull output set to write into. The CullObject
        // input is shared across sets; only viewProj / viewPos and the
        // output buffers differ.
        CullSet set = CullSet::Camera;
        // PART4 4-前-5: skip the CullObject staging copy when this cull set is
        // running after a same-frame Camera pass that already uploaded the
        // shared CullObject buffer. Shadow sets this to true.
        bool inputAlreadyUploaded = false;
        const std::vector<myengine::shared::CullObject>* cullObjects = nullptr;
        const std::vector<DrawTemplate>* drawTemplates = nullptr;
        // PART4 4-前-4: block-sorted ranges (from static_cull::build). Raw
        // pointer + count to avoid pulling static_cull_build.h's BlockRange
        // definition into this header.
        const static_cull::BlockRange* blockRanges = nullptr;
        uint32_t blockRangeCount = 0;
        glm::mat4 viewProj{1.0f};
        glm::vec3 viewPos{0.0f};
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    // Grow draw-side dimension (CullObject / visBuf / cmdBuf / compactCmd /
    // workgroupTotals). Doubles or jumps to `need`, whichever is larger.
    void ensureCapacity(uint32_t need);
    // Grow block-side dimension (countBuf1/2). Same doubling policy.
    void ensureBlockCount(uint32_t need);

    uint32_t capacity()    const noexcept { return capacity_;    }
    uint32_t blockCount()  const noexcept { return blockCount_;  }

    // PART4 4-前-4: BDA + handles for main_pass / shadow_pass / indirect_exec.
    // The commandBuffer pointer is the compacted draw command list (visible
    // only) for the given CullSet, and countBuffer holds per-block visible
    // counts written by scan_globals.
    VkBuffer compactCmdBuffer(CullSet set, uint32_t passIndex) const {
        const CullOutputs& o = cullOutputs_[size_t(set)];
        return passIndex == 0 ? o.compactCmd1.buffer() : o.compactCmd2.buffer();
    }
    VkBuffer countBuffer(CullSet set, uint32_t passIndex) const {
        const CullOutputs& o = cullOutputs_[size_t(set)];
        return passIndex == 0 ? o.countBuf1.buffer() : o.countBuf2.buffer();
    }

    // (Legacy 4-前-3 accessor kept for any caller still drawing from cmdBuf_;
    // main_pass will move to compactCmdBuffer / countBuffer in this commit.)
    VkBuffer commandBuffer(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? cmdBuf_[frameIndex].buffer() : VK_NULL_HANDLE;
    }
    uint32_t drawCount(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? lastCount_[frameIndex] : 0;
    }

    // Debug: count instanceCount==1 in the most recent compactCmd (post-scan).
    // gpuVisibleCount is the previous-dispatch result (frame-fence safe).
    uint32_t lastGpuVisible(uint32_t frameIndex) const {
        return (frameIndex < MAX_FRAMES_IN_FLIGHT) ? lastVisible_[frameIndex] : 0;
    }
    uint32_t lastCpuVisible() const { return lastCpuVisible_; }

    // PART4 4c-B: HizParams BDA + writer. The per-frame buffer carries two
    // slots back-to-back (passIndex == 1 / 2) so addresses are deterministic
    // without a heap lookup. cull.comp dereferences the slot specified by
    // pc.hizParamsAddr to fetch viewProj + viewport + HZB mip-chain info.
    // 4c-B leaves the field as receptacle: ExecuteInfo's default sets
    // hizParamsAddr = 0, so cull.comp's pass2 branch (dead code today)
    // never fires and there's no validity requirement on the buffer
    // contents until 4c-C wires the orchestration.
    VkDeviceAddress hizParamsAddress(uint32_t frameIndex, uint32_t passIndex) const;
    void writeHizParams(uint32_t frameIndex, uint32_t passIndex,
                        const myengine::shared::HizParams& params);

   private:
    // -- Push constants ------------------------------------------------------
    // cull.comp (PART4 4-前-4 + 4c-B): 144 bytes std430.
    //   planes[6] (96) + viewPos (16) + cullAddr (8) + visAddr (8) +
    //   objectCount (4) + _pad (4) + hizParamsAddr (8) = 144 B.
    // _pad realigns hizParamsAddr (uvec2) to its 8-byte boundary. 144B fits
    // P620's maxPushConstantsSize (Pascal+ = 256B); mobile devices with the
    // 128B spec minimum aren't a target yet (engine is desktop-first today;
    // §1.5-C will gate this when mobile lands).
    struct CullPC {
        glm::vec4  planes[6];     //  0 .. 95
        glm::vec4  viewPos;       // 96 ..111
        glm::uvec2 cullAddr;      //112 ..119
        glm::uvec2 visAddr;       //120 ..127
        uint32_t   objectCount;   //128 ..131
        uint32_t   _pad;          //132 ..135  - align hizParamsAddr to 8 B
        glm::uvec2 hizParamsAddr; //136 ..143  - PART4 4c-B HizParams BDA
                                  //               (0 = HZB read disabled =
                                  //                pass1 legacy / dead-code today)
    };
    // scan_local.comp (Pass A): 32 bytes.
    struct ScanLocalPC {
        glm::uvec2 visAddr;             // bit-packed predicate input
        glm::uvec2 workgroupTotalsAddr; // uint[] output: per-wg local total
        uint32_t   blockFirstDraw;
        uint32_t   blockDrawCount;
        uint32_t   wgFirstInBlock;      // start offset in workgroupTotals
        uint32_t   _pad;
    };
    // scan_globals.comp (Pass B): 32 bytes.
    struct ScanGlobalsPC {
        glm::uvec2 workgroupTotalsAddr; // in/out: turn totals into prefixes
        glm::uvec2 countBufAddr;        // out: per-block visible count
        uint32_t   wgFirstInBlock;
        uint32_t   numWgsInBlock;
        uint32_t   blockIdx;
        uint32_t   _pad;
    };
    // scan_scatter.comp (Pass C): 56 bytes.
    struct ScanScatterPC {
        glm::uvec2 visAddr;             // predicate input
        glm::uvec2 workgroupTotalsAddr; // per-wg prefix input (Pass B output)
        glm::uvec2 cmdTemplateAddr;     // DrawCmd[] CPU-supplied template
        glm::uvec2 compactCmdAddr;      // DrawCmd[] scan output (visible only)
        uint32_t   blockFirstDraw;
        uint32_t   blockDrawCount;
        uint32_t   wgFirstInBlock;
        uint32_t   _pad;
    };

    // -- Internals -----------------------------------------------------------
    // PART4 4-前-5: per-CullSet output buffers. cullBuf / cmdBuf / cullStaging
    // are shared (the CullObject input is the same scene draw list); only the
    // visibility result and the scan workspace differ per consumer.
    struct CullOutputs {
        VmaBuffer visBuf;             // device-local bit-packed predicate
        VmaBuffer compactCmd1;        // device-local VkDrawIndexedIndirectCommand[]
        VmaBuffer compactCmd2;        // 4c pass2 receptacle (allocated, unused today)
        VmaBuffer countBuf1;          // device-local uint[blockCount_]
        VmaBuffer countBuf2;          // 4c pass2 receptacle
        VmaBuffer workgroupTotals;    // scratch uint[scanWgsFor(capacity_)]
    };
    static constexpr size_t kNumCullSets = 2;  // Camera + Shadow today; cascades grow this.

    void createBuffers(uint32_t capacity, uint32_t blockCount);
    void destroyBuffersToDeletionQueue();
    void createPipelines(const std::string& shaderDir);
    template <typename PC>
    VkUnique<VkPipelineLayout> createComputePipelineLayout();
    VkUnique<VkPipeline> createComputePipeline(const std::string& spvPath,
                                                VkPipelineLayout layout);

    static constexpr VkDeviceSize cullStride() { return sizeof(myengine::shared::CullObject); }
    static constexpr VkDeviceSize cmdStride()  { return sizeof(VkDrawIndexedIndirectCommand); }
    static constexpr uint32_t     visWordsFor(uint32_t cap) { return (cap + 31u) >> 5; }
    static constexpr uint32_t     scanWgsFor(uint32_t cap)  { return (cap + SCAN_WG_SIZE - 1u) / SCAN_WG_SIZE; }

    VulkanContext* ctx_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_   = 0;
    uint32_t blockCount_ = 0;

    // Shared input (one upload per frame, reused by all CullSets).
    VmaBuffer cullBuf_;                                       // device-local
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cullStaging_; // host-mapped
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuf_;      // host-mapped template

    // PART4 4c-B: per-frame host-mapped HizParams[2] (pass1 / pass2 slots).
    // Receptacle today (pass2 path is dead code); 4c-C wires it.
    std::array<VmaBuffer, MAX_FRAMES_IN_FLIGHT> hizParamsBuf_;  // host-mapped, BDA

    // Per-CullSet outputs (PART4 4-前-5).
    std::array<CullOutputs, kNumCullSets> cullOutputs_;

    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastCount_{};
    std::array<uint32_t, MAX_FRAMES_IN_FLIGHT> lastVisible_{};
    uint32_t lastCpuVisible_ = 0;

    // Pipelines for cull + 3-pass scan compaction.
    VkUnique<VkPipelineLayout> cullLayout_;
    VkUnique<VkPipeline>       cullPipe_;
    VkUnique<VkPipelineLayout> scanLocalLayout_;
    VkUnique<VkPipeline>       scanLocalPipe_;
    VkUnique<VkPipelineLayout> scanGlobalsLayout_;
    VkUnique<VkPipeline>       scanGlobalsPipe_;
    VkUnique<VkPipelineLayout> scanScatterLayout_;
    VkUnique<VkPipeline>       scanScatterPipe_;
};

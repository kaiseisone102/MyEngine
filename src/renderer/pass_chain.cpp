// =============================================================================
// pass_chain.cpp — Phase 1C: ShadowPass + ReflectionPass + MainPass + Water
// =============================================================================
#include "renderer/pass_chain.h"

#include <stdexcept>
#include <iostream>

#include "scene/scene_data.h"  // SceneData::cullObjects() for the cull pass

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "renderer/asset_registry.h"
#include "renderer/debug_line_renderer.h"
#include "renderer/material.h"
#include "renderer/model.h"
#include "renderer/resource_factory.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"
#include "scene/scene_data.h"
#include "renderer/terrain_mesh.h"

namespace {

// 水面 Y を境に Y 軸を反転する行列。
// reflectedWorld = reflectMat * world (world space で水面に対して鏡映)
glm::mat4 makeReflectionMatrix(float waterY) {
    glm::mat4 m(1.0f);
    m[1][1] = -1.0f;                   // y → -y
    m[3][1] = 2.0f * waterY;            // 平面 y = waterY を保つための平行移動
    return m;
}

}  // namespace

void PassChain::init(const InitInfo& info) {
    if (!info.ctx || !info.resources || !info.swapchain || !info.frameUniforms || !info.assets) {
        throw std::runtime_error("PassChain::init: required pointer is null");
    }

    swapchain_ = info.swapchain;

    // ─── ShadowPass ──────────────────────────────────────────────
    {
        ShadowPass::InitInfo si{};
        si.ctx = info.ctx;
        si.resources = info.resources;
        si.frameSetLayout = info.frameUniforms->layout();
        si.shaderDir = info.shaderDir;
        si.extent = {2048, 2048};
        si.depthFormat = VK_FORMAT_D32_SFLOAT;
        shadowPass_.init(si);
    }

    info.frameUniforms->bindShadowMap(shadowPass_.output().view(), shadowPass_.output().sampler());
    info.frameUniforms->rebuildDescriptorSets();

    // ─── MainPass ────────────────────────────────────────────────
    {
        MainPass::InitInfo mi{};
        mi.ctx = info.ctx;
        mi.swapchain = info.swapchain;
        mi.frameSetLayout = info.frameUniforms->layout();
        mi.bindlessSetLayout = info.bindlessSetLayout;
        mi.hdrColorView = info.hdrColorView;  // Phase 1H-2
        mi.hdrColorFormat = info.hdrColorFormat;
        mi.shaderDir = info.shaderDir;
        mainPass_.init(mi);
    }

    // Phase 1E: instance matrix pool
    instancePool_.init(info.ctx, info.resources);

    // Phase 2B PART2: GPU frustum culling compute pass (BDA-only, no sets)
    {
        CullingPass::InitInfo ci{};
        ci.ctx = info.ctx;
        ci.shaderDir = info.shaderDir;
        cullingPass_.init(ci);
    }
    {
        // Phase 1I: compute mip-chain bloom (HDR -> mip chain -> mip0 = bloom)
        BloomPass::InitInfo bi{};
        bi.ctx = info.ctx;
        bi.resources = info.resources;
        bi.hdrColorView = info.hdrColorView;
        bi.hdrColorSampler = info.hdrColorSampler;
        bi.bloomFormat = info.bloomFormat;
        bi.baseWidth = info.bloomBaseWidth;
        bi.baseHeight = info.bloomBaseHeight;
        bi.maxMips = info.bloomMaxMips;
        bi.shaderDir = info.shaderDir;
        bloomPass_.init(bi);
    }
    {
        PostPass::InitInfo poi{};
        poi.ctx = info.ctx;
        poi.swapchain = info.swapchain;
        poi.hdrColorView = info.hdrColorView;
        poi.hdrColorSampler = info.hdrColorSampler;
        poi.bloomColorView = bloomPass_.bloomView();   // Phase 1I: final bloom = mip0
        poi.bloomColorSampler = bloomPass_.bloomSampler();
        poi.shaderDir = info.shaderDir;
        postPass_.init(poi);
    }


    // ─── DebugLinePass ───────────────────────────────────────────
    {
        DebugLinePass::InitInfo di{};
        di.ctx = info.ctx;
        di.resources = info.resources;
        di.swapchain = info.swapchain;
        di.mainRenderPass = mainPass_.renderPass();
        di.frameSetLayout = info.frameUniforms->layout();
        di.shaderDir = info.shaderDir;
        debugLinePass_.init(di);
    }

    // ─── ParticlePass ────────────────────────────────────────────
    {
        ParticlePass::InitInfo pi{};
        pi.ctx = info.ctx;
        pi.resources = info.resources;
        pi.swapchain = info.swapchain;
        pi.mainRenderPass = mainPass_.renderPass();
        pi.frameSetLayout = info.frameUniforms->layout();
        pi.shaderDir = info.shaderDir;
        particlePass_.init(pi);
    }

    // ─── HudPass ─────────────────────────────────────────────────
    {
        HudPass::InitInfo hi{};
        hi.ctx = info.ctx;
        hi.swapchain = info.swapchain;
        hi.mainRenderPass = mainPass_.renderPass();
        hi.shaderDir = info.shaderDir;
        hudPass_.init(hi);
    }

    // ─── WaterPass ───────────────────────────────────────────────
    {
        WaterPass::InitInfo wi{};
        wi.ctx = info.ctx;
        wi.resources = info.resources;
        wi.mainRenderPass = mainPass_.renderPass();
        wi.frameSetLayout = info.frameUniforms->layout();
        wi.shaderDir = info.shaderDir;
        waterPass_.init(wi);
    }

    // ─── ReflectionPass ──────────────────────────────────────────
    {
        ReflectionPass::InitInfo ri{};
        ri.ctx = info.ctx;
        ri.resources = info.resources;
        ri.baseWidth = info.swapchain->extent().width;
        ri.baseHeight = info.swapchain->extent().height;
        ri.colorFormat = info.swapchain->colorFormat();
        ri.depthFormat = info.swapchain->depthFormat();
        ri.quality = info.reflectionQuality;
        ri.frameSetLayout = info.frameUniforms->layout();
        ri.bindlessSetLayout = info.bindlessSetLayout;  // S4-c
        ri.shaderDir = info.shaderDir;
        reflectionPass_.init(ri);
    }

    // 反射 texture を WaterPass に bind (有効時のみ)
    if (reflectionPass_.enabled()) {
        waterPass_.bindReflectionTexture(reflectionPass_.target().color().view(),
                                          reflectionPass_.target().color().sampler());
    }

    // ─── ImGuiLayer ──────────────────────────────────────────────
    {
        ImGuiLayer::InitInfo ii{};
        ii.window = info.window;
        ii.ctx = info.ctx;
        ii.swapchainImageCount = info.swapchain->imageCount();
        ii.renderPass = mainPass_.renderPass();
        ii.minImageCount = 2;
        imgui_.init(ii);
    }
}

void PassChain::shutdown() {
    imgui_.shutdown();
    waterPass_.shutdown();
    reflectionPass_.shutdown();
    hudPass_.shutdown();
    particlePass_.shutdown();
    debugLinePass_.shutdown();
    postPass_.shutdown();  // Phase 1H-3
    bloomPass_.shutdown();  // was leaking: init'd but never shut down
    cullingPass_.shutdown();   // Phase 2B PART2
    instancePool_.shutdown();  // Phase 1E
    mainPass_.shutdown();
    shadowPass_.shutdown();
    swapchain_ = nullptr;
}

void PassChain::onReflectionQualityChanged(ReflectionQuality quality) {
    if (!swapchain_) return;
    reflectionPass_.rebuild(quality, swapchain_->extent().width, swapchain_->extent().height);
    if (reflectionPass_.enabled()) {
        waterPass_.bindReflectionTexture(reflectionPass_.target().color().view(),
                                          reflectionPass_.target().color().sampler());
    }
}

void PassChain::onSwapchainResized(VkImageView hdrColorView, VkSampler hdrColorSampler,
                                   uint32_t bloomBaseW, uint32_t bloomBaseH) {
    // Phase 1H-2: forward new HDR view to MainPass before re-creating framebuffer
    if (hdrColorView != VK_NULL_HANDLE) {
        mainPass_.setHdrColorView(hdrColorView);
    }
    mainPass_.onSwapchainResized();
    // Phase 1I: rebuild bloom mip chain at the new base extent FIRST, so its new
    // mip0 view/sampler can be forwarded to PostPass below.
    if (bloomBaseW != 0 && bloomBaseH != 0) {
        BloomPass::InitInfo bi{};
        bi.hdrColorView = hdrColorView;
        bi.hdrColorSampler = hdrColorSampler;
        bi.baseWidth = bloomBaseW;
        bi.baseHeight = bloomBaseH;
        bloomPass_.onSwapchainResized(bi);
    }
    // Phase 1H-3: forward new HDR view + sampler + new bloom mip0 to PostPass
    if (hdrColorView != VK_NULL_HANDLE) {
        postPass_.onSwapchainResized(hdrColorView, hdrColorSampler,
                                     bloomPass_.bloomView(), bloomPass_.bloomSampler());
    }
    // swapchain サイズ変更時、 反射 texture も新サイズに合わせて再作成。
    // quality は ReflectionPass が保持してる現在値を維持。
    if (swapchain_) {
        reflectionPass_.rebuild(reflectionPass_.quality(),
                                  swapchain_->extent().width, swapchain_->extent().height);
        if (reflectionPass_.enabled()) {
            waterPass_.bindReflectionTexture(reflectionPass_.target().color().view(),
                                              reflectionPass_.target().color().sampler());
        }
    }
}

void PassChain::beginUI() { imgui_.beginFrame(); }
void PassChain::endUI() { imgui_.endFrame(); }

void PassChain::recordFrame(const RecordInfo& info) {
    if (!info.scene || !info.assets || !info.frameUniforms || info.cmd == VK_NULL_HANDLE) {
        throw std::runtime_error("PassChain::recordFrame: invalid RecordInfo");
    }

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(info.cmd, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    const VkDescriptorSet frameSet = info.frameUniforms->descriptorSet(info.frameIndex);
    Mesh* mesh = const_cast<Mesh*>(&info.assets->defaultMesh());

    const auto& meshOpaque         = info.scene->meshDrawListOpaque();
    const auto& modelOpaque        = info.scene->modelDrawListOpaque();
    const auto& staticOpaque       = info.scene->staticModelDrawListOpaque();
    const auto& terrainOpaque      = info.scene->terrainDrawListOpaque();
    const auto& meshTransparent    = info.scene->meshDrawListTransparentConst();
    const auto& modelTransparent   = info.scene->modelDrawListTransparentConst();
    const auto& staticTransparent  = info.scene->staticModelDrawListTransparentConst();
    const auto& terrainTransparent = info.scene->terrainDrawListTransparentConst();
    const auto& waterList          = info.scene->waterDrawList();

    // ─── 1. ShadowPass ──────────────────────────────────────────
    {
        ShadowPass::ExecuteInfo si{};
        si.cmd = info.cmd;
        si.frameSet = frameSet;
        si.skinAddress = info.skinAddress;
        si.mesh = mesh;
        si.meshDrawList = &meshOpaque;
        si.modelDrawList = &modelOpaque;
        si.staticModelDrawList = &staticOpaque;
        shadowPass_.execute(si);
    }

    // ─── 2. ReflectionPass (有効かつ water があれば) ───────────
    glm::mat4 reflectVP(1.0f);
    bool waterUseReflection = false;
    if (reflectionPass_.enabled() && !waterList.empty()) {
        const float waterY = waterList[0].center.y;
        const glm::mat4 reflectMat = makeReflectionMatrix(waterY);
        const glm::mat4 reflectView = info.normalLighting.view * reflectMat;

        FrameUniforms::LightingUBO reflLighting = info.normalLighting;
        reflLighting.view = reflectView;
        reflLighting.viewPos = {info.normalLighting.viewPos.x,
                                  2.0f * waterY - info.normalLighting.viewPos.y,
                                  info.normalLighting.viewPos.z,
                                  info.normalLighting.viewPos.w};
        if (!info.reflectShadows) {
            reflLighting.shadowParams.x = 0.f;
        }
        info.frameUniforms->updateReflection(info.frameIndex, reflLighting);

        const VkDescriptorSet reflFrameSet =
            info.frameUniforms->descriptorSetReflection(info.frameIndex);

        ReflectionPass::ExecuteInfo ri{};
        ri.cmd = info.cmd;
        ri.frameSet = reflFrameSet;
        ri.bindlessSet = info.bindlessSet;  // S4-c: bindless texture array
        ri.skinAddress = info.skinAddress;
        ri.mesh = mesh;
        ri.meshDrawListOpaque = &meshOpaque;
        ri.staticModelDrawListOpaque = &staticOpaque;
        ri.terrainDrawListOpaque = &terrainOpaque;
        ri.modelDrawListOpaque = &modelOpaque;
        reflectionPass_.execute(ri);

        reflectVP = info.normalLighting.proj * reflectView;
        waterUseReflection = true;
    }

    // ─── 2.5 CullingPass (Phase 2B PART2): GPU frustum cull BEFORE MainPass ──
    {
        CullingPass::ExecuteInfo ce{};
        ce.cmd = info.cmd;
        ce.frameIndex = info.frameIndex;
        ce.cullObjects = &info.scene->cullObjects();
        ce.drawTemplates = nullptr;  // PART2: no real draw yet (instanceCount only)
        ce.viewProj = info.normalLighting.proj * info.normalLighting.view;
        cullingPass_.execute(ce);

        // temporary [Cull2B] verify: compare GPU visible count vs CPU total.
        // Reads this frameIndex's buffer from the PREVIOUS time it was used (its
        // GPU work has completed via the frame fence), so it's safe to read.
        static int s_cullGpuLogFrames = 0;
        if (!info.scene->cullObjects().empty() && s_cullGpuLogFrames < 30) {
            ++s_cullGpuLogFrames;
            std::cout << "[Cull2B] frame=" << info.frameIndex
                      << " gpu(prev)=" << cullingPass_.lastGpuVisible(info.frameIndex)
                      << " cpu=" << cullingPass_.lastCpuVisible()
                      << " / total=" << info.scene->cullObjects().size() << "\n";
        }
    }

    // ─── 3. MainPass ────────────────────────────────────────────
    {
        MainPass::ExecuteInfo mi{};
        mi.cmd = info.cmd;
        mi.imageIndex = info.imageIndex;
        mi.frameIndex = info.frameIndex;
        mi.frameSet = frameSet;
        mi.skinAddress = info.skinAddress;
        mi.bindlessSet = info.bindlessSet;
        mi.mesh = mesh;

        mi.meshDrawListOpaque        = &meshOpaque;
        mi.modelDrawListOpaque       = &modelOpaque;
        mi.staticModelDrawListOpaque = &staticOpaque;
        mi.terrainDrawListOpaque     = &terrainOpaque;

        mi.meshDrawListTransparent        = &meshTransparent;
        mi.modelDrawListTransparent       = &modelTransparent;
        mi.staticModelDrawListTransparent = &staticTransparent;
        mi.terrainDrawListTransparent     = &terrainTransparent;

        mi.waterPass = &waterPass_;
        mi.waterDrawList = &waterList;
        mi.waterTime = info.waterTime;
        mi.waterUseReflection = waterUseReflection;
        mi.waterReflectVP = reflectVP;

        mi.imgui = &imgui_;
        mi.debugLinePass = &debugLinePass_;
        mi.debugLines = info.debugLines;
        mi.particlePass = &particlePass_;
        mi.particles = info.particles;
        mi.particleCullingDistance = info.scene->cullingDistance();
        mi.hudPass = &hudPass_;
        mi.hud = info.hud;
        mi.screenW = info.screenW;
        mi.screenH = info.screenH;

        // === Phase 1F: instanced grass scattered on the ground, frustum-culled ===
        instancePool_.beginFrame(info.frameIndex);
        static std::vector<InstancedMeshDrawItem> grassDraw;
        grassDraw.clear();

        Frustum fr;
        fr.extract(info.normalLighting.proj * info.normalLighting.view);

        const Mesh* grassMesh = const_cast<Mesh*>(&info.assets->grassMesh());
        const auto& grassTerrains = terrainOpaque;
        auto groundHeight = [&](float wx, float wz) -> float {
            float best = -1e30f;
            for (const auto& ti : grassTerrains) {
                if (!ti.terrain) continue;
                const float hh = ti.terrain->sampleHeight(wx, wz);
                if (hh > best) best = hh;
            }
            return best;
        };
        int total = 0, visible = 0;
        {
            InstancedMeshDrawItem item;
            item.mesh = grassMesh;
            item.material = &info.assets->grassMaterial();  // S6-b: unified material path
            item.alpha = 1.0f;
            const int N = 110;         // denser, wider
            const float spacing = 1.2f;
            const float origin = -(N * spacing) * 0.5f;
            for (int z = 0; z < N; ++z) {
                for (int x = 0; x < N; ++x) {
                    ++total;
                    // deterministic pseudo-random offset per cell
                    const uint32_t h = static_cast<uint32_t>(x * 73856093) ^
                                       static_cast<uint32_t>(z * 19349663);
                    const float rx = ((h & 0xFF) / 255.0f - 0.5f) * spacing;
                    const float rz = (((h >> 8) & 0xFF) / 255.0f - 0.5f) * spacing;
                    const float sc = 0.5f + ((h >> 16) & 0xFF) / 255.0f * 0.5f;  // 0.5..1.0
                    glm::vec3 pos(origin + x * spacing + rx, 0.0f, origin + z * spacing + rz);
                    const float gy = groundHeight(pos.x, pos.z);
                    if (gy < -1e29f) continue;  // no terrain here -> no grass
                    pos.y = gy;                 // sit on the ground
                    if (!fr.sphereVisible(pos + glm::vec3(0, sc * 0.5f, 0), sc)) continue;
                    ++visible;
                    const float ang = ((h >> 24) & 0xFF) / 255.0f * 6.2831853f;
                    const float cs = std::cos(ang), sn = std::sin(ang);
                    glm::mat4 m(1.f);
                    m[0][0] = cs * sc;  m[0][2] = -sn * sc;
                    m[2][0] = sn * sc;  m[2][2] = cs * sc;
                    m[1][1] = sc;
                    m[3][0] = pos.x; m[3][1] = pos.y; m[3][2] = pos.z;

                    myengine::shared::InstanceData inst{};
                    inst.model = m;
                    // green tint variation from hash (toggle wired in G6)
                    const float cv  = ((h >> 12) & 0xFF) / 255.0f;
                    const float cv2 = ((h >>  4) & 0x0F) / 15.0f;
                    if (grassColorVariation_) {
                        float rr = 0.70f + cv * 0.45f;
                        float gg = 0.85f + cv * 0.30f;
                        float bb = 0.55f + cv * 0.30f;
                        inst.color = glm::vec4(rr, gg, bb, 1.0f);
                    } else {
                        inst.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                    }
                    inst.params = glm::vec4(0.0f, windEnabled_ ? 1.0f : 0.0f, 0.0f, 0.0f);
                    item.instances.push_back(inst);
                }
            }
            if (!item.instances.empty()) {
                item.instanceOffset = instancePool_.push(info.frameIndex, item.instances);
                grassDraw.push_back(std::move(item));
            }
        }
        lastInstancedVisible_ = visible;
        lastInstancedTotal_ = total;
        mi.grassDrawList = &grassDraw;
        mi.instanceBufferAddress = instancePool_.bufferAddress(info.frameIndex);


        mainPass_.execute(mi);
    }

    // Phase 1I: generate bloom texture from HDR (before tonemap composites it)
    if (bloomEnabled_) {
        BloomPass::ExecuteInfo be{};
        be.cmd = info.cmd;
        bloomPass_.execute(be);
    } else {
        // Bloom off: still make mip0 black + SHADER_READ_ONLY so PostPass can
        // sample it (zero contribution) without a layout mismatch.
        bloomPass_.clearToReadable(info.cmd);
    }

    // Phase 1H-3: tonemap HDR target -> swapchain
    {
        PostPass::ExecuteInfo poe{};
        poe.cmd = info.cmd;
        poe.imageIndex = info.imageIndex;
        postPass_.execute(poe);
    }

    if (vkEndCommandBuffer(info.cmd) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");
}

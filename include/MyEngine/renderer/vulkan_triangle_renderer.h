// include/MyEngine/renderer/vulkan_triangle_renderer.h

#pragma once
// =============================================================================
// vulkan_triangle_renderer.h — リファクタ Step 1 (VulkanContext 切り出し後)
// =============================================================================
// このファイルは「Renderer の全状態」を宣言するヘッダー。
// 責務分離リファクタの進行中で、現在は VulkanContext のみ切り出し済み。
//
// 切り出された責務:
//   [Step 1] VulkanContext : Instance/Surface/PhysicalDevice/Device/Queues
//
// 未切り出し (TODO):
//   Swapchain / ResourceFactory / Mesh / Texture / FrameUniforms /
//   ShadowPass / MainPass / FrameSync / ImGuiLayer
// =============================================================================

// glm の #define は h 側で統一（cpp と h で食い違うとリンクエラーになる）
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL  // gtx/hash.hpp を使うために必要
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <string>
#include <vector>

#include "vulkan_context.h"

// =============================================================================
// Vertex — CPU 側の頂点レイアウト（シェーダの location と 1 対 1 対応）
// =============================================================================
struct Vertex {
    glm::vec3 pos;       // location=0 : 3D 座標
    glm::vec3 color;     // location=1 : 頂点カラー
    glm::vec2 texCoord;  // location=2 : UV 座標
    glm::vec3 normal;    // location=3 : 法線ベクトル（ステップ10: ライティング用）

    bool operator==(const Vertex& o) const {
        return pos == o.pos && color == o.color && texCoord == o.texCoord && normal == o.normal;
    }
};

// std::unordered_map<Vertex, …> でキーとして使うためのハッシュ特殊化
namespace std {
template <>
struct hash<Vertex> {
    size_t operator()(Vertex const& v) const noexcept {
        size_t h = std::hash<glm::vec3>()(v.pos);
        h = (h ^ (std::hash<glm::vec3>()(v.color) << 1)) >> 1;
        h = (h ^ (std::hash<glm::vec2>()(v.texCoord) << 1)) >> 1;
        h = h ^ (std::hash<glm::vec3>()(v.normal) << 1);
        return h;
    }
};
}  // namespace std

// =============================================================================
class VulkanTriangleRenderer {
   public:
    void init(SDL_Window* window);

    // ─── ImGui 対応 drawFrame ────────────────────────────────────
    // uiCallback に ImGui ウィンドウを構築するラムダを渡す。
    // 何も渡さなければ ImGui ウィンドウなしで描画する。
    void drawFrame(std::function<void()> uiCallback = {});

    void onResize();
    void shutdown();

    // ─── カメラ行列を外部から受け取る ─────────────────────────────
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj) {
        viewMatrix_ = view;
        projMatrix_ = proj;
    }

    // ─── 描画オブジェクトリスト ────────────────────────────────────
    void clearObjects() { drawList_.clear(); }
    void addObject(const glm::mat4& modelMatrix) { drawList_.push_back(modelMatrix); }

    // ─── ライトパラメータを外部から設定 ───────────────────────────
    // drawFrame() を呼ぶ前に毎フレーム呼ぶ。
    //   lightPos   : ライトのワールド座標（太陽の位置など）
    //   lightColor : ライトの色（通常は白 {1,1,1}）
    //   viewPos    : カメラのワールド座標（スペキュラ計算に必要）
    //   ambient    : 環境光の強さ（0〜1、0.1〜0.3 が自然）
    //   specular   : 鏡面反射の強さ（0〜1）
    void setLightingParams(const glm::vec3& lightPos, const glm::vec3& lightColor,
                           const glm::vec3& viewPos, float ambient = 0.15f, float specular = 0.5f) {
        lightPos_ = lightPos;
        lightColor_ = lightColor;
        viewPos_ = viewPos;
        ambient_ = ambient;
        specular_ = specular;
    }

   private:
    SDL_Window* window_ = nullptr;
    std::string shaderDir_;  // exe の隣の shaders/
    std::string assetDir_;   // exe の隣の assets/

    // ─── Vulkan コア ──────────────────────────────────────────────
    VulkanContext ctx_;

    // ─── スワップチェーン（TODO: Swapchain クラスに切り出し）─────────────
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapExtent_ = {};
    std::vector<VkImage> swapImages_;
    std::vector<VkImageView> swapViews_;
    std::vector<VkFramebuffer> framebuffers_;

    // ─── デプスバッファ（TODO: Swapchain に含める）────────────────────────
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    // ─── シャドウマップ（TODO: ShadowPass クラスへ）───────────────────────
    VkExtent2D shadowExtent_ = {1024, 1024};
    VkFormat shadowFormat_ = VK_FORMAT_D32_SFLOAT;
    VkImage shadowImage_ = VK_NULL_HANDLE;
    VkDeviceMemory shadowImageMemory_ = VK_NULL_HANDLE;
    VkImageView shadowImageView_ = VK_NULL_HANDLE;
    VkSampler shadowSampler_ = VK_NULL_HANDLE;

    // ─── パイプライン（TODO: MainPass/ShadowPass へ）──────────────────────
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkRenderPass shadowRenderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline shadowPipeline_ = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_ = VK_NULL_HANDLE;

    // ─── ディスクリプタ（TODO: FrameUniforms へ）──────────────────────────
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

    // ─── 頂点・インデックス（TODO: Mesh へ）──────────────────────────────
    std::vector<Vertex> meshVertices_;   // OBJ から読んだ頂点データ
    std::vector<uint32_t> meshIndices_;  // OBJ から読んだインデックス（uint32_t！）
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    uint32_t indexCount_ = 0;

    // ─── UBO（TODO: FrameUniforms へ）─────────────────────────────────────
    VkBuffer uniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory_ = VK_NULL_HANDLE;
    void* uniformMapped_ = nullptr;
    VkDeviceSize uniformBufferSize_ = 0;

    // ─── カメラ・シーン設定 ──────────────────────────────────────────────
    glm::mat4 viewMatrix_{1.f};
    glm::mat4 projMatrix_{1.f};
    std::vector<glm::mat4> drawList_;

    // ─── ライト設定 ──────────────────────────────────────────────────────
    glm::vec3 lightPos_ = {10.f, 20.f, 10.f};  // ライトのワールド座標
    glm::vec3 lightColor_ = {1.f, 1.f, 1.f};   // ライトの色（白）
    glm::vec3 viewPos_ = {0.f, 0.f, 5.f};      // カメラのワールド座標
    float ambient_ = 0.15f;
    float specular_ = 0.5f;
    float shadowStrength_ = 0.6f;
    float shadowBias_ = 0.003f;

    // ── UBO の C++ 側レイアウト（GLSL std140 と完全一致させる）─────────────
    // vec3 は std140 で 16 バイト境界に揃えられるため alignas(16) が必要。
    // float のパディングで次の vec3 の開始を 16 バイト境界に合わせる。
    struct LightingUBO {
        glm::mat4 vp;       // 64 bytes, offset   0
        glm::mat4 lightVP;  // 64 bytes, offset  64
        alignas(16) glm::vec3 lightPos;
        float _p0;  // 16 bytes, offset 128
        alignas(16) glm::vec3 lightColor;
        float _p1;  // 16 bytes, offset 144
        alignas(16) glm::vec3 viewPos;
        float _p2;             // 16 bytes, offset 160
        float ambient;         //  4 bytes, offset 176
        float specular;        //  4 bytes, offset 180
        float shadowStrength;  //  4 bytes, offset 184
        float shadowBias;      //  4 bytes, offset 188
    };  // 合計 192 bytes

    // ─── テクスチャ（TODO: Texture へ）────────────────────────────────────
    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;

    // ─── コマンド・同期（TODO: FrameSync へ）──────────────────────────────
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;

    // ─── 初期化ステップ ──────────────────────────────────────────
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createCommandPool();
    void loadModel();  // ステップ7: OBJ → meshVertices_/meshIndices_
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffer();
    void createDescriptorSetLayout();
    void createTextureImage();  // ステップ6+7: PNG or プロシージャル
    void createTextureImageView();
    void createTextureSampler();
    void createGraphicsPipeline();
    void createShadowRenderPass();
    void createShadowResources();
    void createShadowPipeline();
    void createDepthResources();  // ステップ7: depth image/view 作成
    void createDescriptorPool();
    void createDescriptorSet();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();

    void updateUniformBuffer();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void recreateSwapchain();
    void cleanupSwapchain();

    // ─── 汎用 GPU バッファ・画像（TODO: ResourceFactory へ）──────────────
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginOneTimeCommands();
    void endOneTimeCommands(VkCommandBuffer cmd);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory);
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // ─── ステップ13: ImGui ライフサイクル ────────────────────────────────
    void initImGui();      // init() の最後に呼ぶ
    void shutdownImGui();  // shutdown() の最初に呼ぶ

    static std::vector<char> readBinaryFile(const char* path);
    static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
};

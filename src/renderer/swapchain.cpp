// \MyEngine\src\renderer\swapchain.cpp
// =============================================================================
// swapchain.cpp
// =============================================================================
// 実装内容:
//   VkSwapchainKHR の生成・破棄、 カラー image view 一式、 深度バッファ。
//
// 方針:
//   - Surface format : B8G8R8A8_SRGB を優先 (自動 gamma 補正で楽)
//   - Present mode   : VSync 固定 (FIFO は仕様上必ずサポート、 tearing なし)
//   - Min image count: caps.minImageCount + 1 (triple buffering 寄り)
//
// recreate():
//   ウィンドウ最小化 (0x0) は SDL_WaitEvent でブロック (CPU を消費しない)。
//   vkDeviceWaitIdle で描画中でないことを保証してから再生成。
//   oldSwapchain を渡してドライバ内部リソースを再利用、 リサイズを滑らかに。
// =============================================================================
#include "renderer/swapchain.h"

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace {

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
    // B8G8R8A8_SRGB + SRGB_NONLINEAR が見つかればそれを使う。
    // (swapchain image に書き込むと自動で sRGB → linear 補正されるので、
    //  shader 側で gamma を気にしなくていい。)
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>&) {
    // VSync 固定 (FIFO は仕様上サポート必須、 tearing なし)
    return VK_PRESENT_MODE_FIFO_KHR;
}

}  // namespace

// =============================================================================
// init / shutdown
// =============================================================================

void Swapchain::init(const VulkanContext* ctx, const ResourceFactory* resources,
                     SDL_Window* window, VkFormat depthFormat) {
    ctx_ = ctx;
    resources_ = resources;
    window_ = window;
    depthFormat_ = depthFormat;
    createSwapchain(VK_NULL_HANDLE);
    createImageViews();
    createDepthResources();
}

void Swapchain::shutdown() {
    if (!ctx_) return;
    destroyDepthResources();
    destroyImageViewsAndSwapchain();
    ctx_ = nullptr;
    resources_ = nullptr;
    window_ = nullptr;
}

// =============================================================================
// recreate
// =============================================================================

void Swapchain::recreate() {
    // ウィンドウが最小化されているとサイズ 0 になる。 描画できないので待つ。
    // SDL_WaitEvent でブロックすれば CPU を消費しない。
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    while (w == 0 || h == 0) {
        SDL_Event e;
        SDL_WaitEvent(&e);
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }

    vkDeviceWaitIdle(ctx_->device());

    // 古い swapchain を move で退避。createSwapchain が swapchain_ に新しい物を
    // 入れ、old.get() を oldSwapchain として渡す（ドライバの内部リソース再利用で
    // リサイズが滑らか）。old は新 swapchain 作成後にスコープを抜けて VkUnique が
    // 破棄するので、手動 vkDestroySwapchainKHR は不要。
    VkUnique<VkSwapchainKHR> old = std::move(swapchain_);

    // 旧 views と depth は新しい extent で作り直すので先に破棄
    views_.clear();  // VkUnique が各 view を破棄
    destroyDepthResources();

    createSwapchain(old.get());

    createImageViews();
    createDepthResources();
}

// =============================================================================
// createSwapchain (oldSwapchain 引数で再利用可能)
// =============================================================================

void Swapchain::createSwapchain(VkSwapchainKHR oldSwapchain) {
    SwapchainSupport sup = querySwapchainSupport(ctx_->physicalDevice(), ctx_->surface());
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
    if (sup.capabilities.maxImageCount > 0 && imgCount > sup.capabilities.maxImageCount) {
        imgCount = sup.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = ctx_->surface();
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
    ci.oldSwapchain = oldSwapchain;

    uint32_t queueFamilyIndices[] = {ctx_->graphicsFamily(), ctx_->presentFamily()};
    if (ctx_->graphicsFamily() != ctx_->presentFamily()) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR sc = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(ctx_->device(), &ci, nullptr, &sc) != VK_SUCCESS) {
        throw std::runtime_error("Swapchain::createSwapchain: vkCreateSwapchainKHR failed");
    }
    swapchain_ = VkUnique<VkSwapchainKHR>(ctx_->device(), sc);

    colorFormat_ = fmt.format;
    extent_ = extent;

    vkGetSwapchainImagesKHR(ctx_->device(), swapchain_.get(), &imgCount, nullptr);
    images_.resize(imgCount);
    vkGetSwapchainImagesKHR(ctx_->device(), swapchain_.get(), &imgCount, images_.data());
}

// =============================================================================
// createImageViews
// =============================================================================

void Swapchain::createImageViews() {
    views_.clear();
    views_.reserve(images_.size());
    for (size_t i = 0; i < images_.size(); ++i) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = images_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = colorFormat_;
        ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView v = VK_NULL_HANDLE;
        if (vkCreateImageView(ctx_->device(), &ci, nullptr, &v) != VK_SUCCESS) {
            throw std::runtime_error("Swapchain::createImageViews: vkCreateImageView failed");
        }
        views_.emplace_back(ctx_->device(), v);
    }
}

// =============================================================================
// createDepthResources
// =============================================================================

void Swapchain::createDepthResources() {
    VkImage di = VK_NULL_HANDLE;
    VkDeviceMemory dm = VK_NULL_HANDLE;
    resources_->createImage(extent_.width, extent_.height, depthFormat_, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, di, dm);
    depthImage_ = VkUnique<VkImage>(ctx_->device(), di);
    depthImageMemory_ = VkUnique<VkDeviceMemory>(ctx_->device(), dm);

    // depth view: フォーマットに stencil 成分があれば STENCIL_BIT も追加 (安全側)
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depthFormat_ == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        depthFormat_ == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = depthImage_.get();
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = depthFormat_;
    ci.subresourceRange = {aspect, 0, 1, 0, 1};
    VkImageView dv = VK_NULL_HANDLE;
    if (vkCreateImageView(ctx_->device(), &ci, nullptr, &dv) != VK_SUCCESS) {
        throw std::runtime_error("Swapchain::createDepthResources: vkCreateImageView failed");
    }
    depthView_ = VkUnique<VkImageView>(ctx_->device(), dv);
}

// =============================================================================
// destroy
// =============================================================================

void Swapchain::destroyImageViewsAndSwapchain() {
    views_.clear();      // VkUnique が各 view を破棄
    images_.clear();     // swapchain が所有していたので解放不要
    swapchain_.reset();  // vkDestroySwapchainKHR
}

void Swapchain::destroyDepthResources() {
    depthView_.reset();
    depthImage_.reset();
    depthImageMemory_.reset();
}

// src/renderer/swapchain.cpp

#include "renderer/swapchain.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

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
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>&) {
    // VSync 固定（FIFO は仕様上サポート必須）
    return VK_PRESENT_MODE_FIFO_KHR;
}

}  // namespace

void Swapchain::init(const VulkanContext* ctx, const ResourceFactory* resources, SDL_Window* window,
                     VkFormat depthFormat) {
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

void Swapchain::recreate() {
    // ウィンドウが最小化されているとサイズ 0 になる。描画できないので待つ。
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    while (w == 0 || h == 0) {
        SDL_Event e;
        SDL_WaitEvent(&e);
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }

    vkDeviceWaitIdle(ctx_->device());

    // 古い swapchain は oldSwapchain として新 swapchain 作成時に渡す。
    // こうするとドライバが内部リソースを再利用できて、リサイズが滑らか。
    VkSwapchainKHR old = swapchain_;
    swapchain_ = VK_NULL_HANDLE;  // handle を退避（createSwapchain が上書きする前に）

    // 旧 views と depth は新しい extent で作り直すので先に破棄
    for (auto v : views_) vkDestroyImageView(ctx_->device(), v, nullptr);
    views_.clear();
    destroyDepthResources();

    createSwapchain(old);

    // createSwapchain 内で old を参照し終わっているので、今安全に破棄できる
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(ctx_->device(), old, nullptr);

    createImageViews();
    createDepthResources();
}

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
    if (sup.capabilities.maxImageCount > 0 && imgCount > sup.capabilities.maxImageCount)
        imgCount = sup.capabilities.maxImageCount;

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

    if (vkCreateSwapchainKHR(ctx_->device(), &ci, nullptr, &swapchain_) != VK_SUCCESS)
        throw std::runtime_error("Swapchain::createSwapchain: vkCreateSwapchainKHR failed");

    colorFormat_ = fmt.format;
    extent_ = extent;

    vkGetSwapchainImagesKHR(ctx_->device(), swapchain_, &imgCount, nullptr);
    images_.resize(imgCount);
    vkGetSwapchainImagesKHR(ctx_->device(), swapchain_, &imgCount, images_.data());
}

void Swapchain::createImageViews() {
    views_.resize(images_.size());
    for (size_t i = 0; i < images_.size(); ++i) {
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = images_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = colorFormat_;
        ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(ctx_->device(), &ci, nullptr, &views_[i]) != VK_SUCCESS)
            throw std::runtime_error("Swapchain::createImageViews: vkCreateImageView failed");
    }
}

void Swapchain::createDepthResources() {
    resources_->createImage(extent_.width, extent_.height, depthFormat_, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage_, depthImageMemory_);

    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = depthImage_;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = depthFormat_;
    ci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_->device(), &ci, nullptr, &depthView_) != VK_SUCCESS)
        throw std::runtime_error("Swapchain::createDepthResources: vkCreateImageView failed");
}

void Swapchain::destroyImageViewsAndSwapchain() {
    for (auto v : views_) vkDestroyImageView(ctx_->device(), v, nullptr);
    views_.clear();
    images_.clear();  // swapchain が所有していたので解放不要
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx_->device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void Swapchain::destroyDepthResources() {
    if (depthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_->device(), depthView_, nullptr);
        depthView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_->device(), depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }
}
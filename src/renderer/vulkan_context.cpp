// \MyEngine\src\renderer\vulkan_context.cpp
// =============================================================================
// vulkan_context.cpp
// =============================================================================
// 実装内容:
//   Instance / Debug / Surface / PhysicalDevice / Device を作成・破棄する。
//
// バリデーションレイヤー:
//   #ifndef NDEBUG のときだけ VK_LAYER_KHRONOS_validation を有効化する。
//   SDK が入っていない環境でもクラッシュせず続行できるよう、enumerate で存在確認。
//
// デバイス機能 (features):
//   - samplerAnisotropy : テクスチャの異方性フィルタ
//   - fillModeNonSolid  : ワイヤーフレーム描画 (デバッグ用)
//   - wideLines         : 線幅指定 (デバッグライン描画用)
// =============================================================================
#include "renderer/vulkan_context.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ─── バリデーションコールバック ─────────────────────────────────────────────
#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*userdata*/) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan Validation] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}
#endif

// ─── 物理デバイス探索の補助関数 ─────────────────────────────────────────────
bool checkDeviceExtensionSupport(VkPhysicalDevice device, const char* const* names,
                                 uint32_t count) {
    uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> avail(n);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &n, avail.data());
    for (uint32_t i = 0; i < count; ++i) {
        bool found = false;
        for (const auto& ext : avail) {
            if (std::strcmp(ext.extensionName, names[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

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

std::optional<uint32_t> findGraphicsFamily(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());
    for (uint32_t i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    }
    return std::nullopt;
}

}  // namespace

// =============================================================================
// ライフサイクル
// =============================================================================

void VulkanContext::init(SDL_Window* window) {
    window_ = window;
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
        createDevice();

    // -------------------------------------------------------------------------
    // VMA (Vulkan Memory Allocator) initialization.
    // BUFFER_DEVICE_ADDRESS flag enables BDA buffers throughout the engine.
    // -------------------------------------------------------------------------
    {
        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.physicalDevice = physical_;
        allocInfo.device = device_;
        allocInfo.instance = instance_;
        allocInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        if (vmaCreateAllocator(&allocInfo, &allocator_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanContext: vmaCreateAllocator failed");
        }
        std::cout << "[MyEngine] VMA allocator created (BDA enabled, API 1.4).\n";
    }
}

void VulkanContext::shutdown() {
    // 破棄順序: Device → Surface → DebugMessenger → Instance
    if (device_ != VK_NULL_HANDLE) {
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    destroyDebugMessenger();
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    physical_ = VK_NULL_HANDLE;
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    window_ = nullptr;
}

// =============================================================================
// Instance
// =============================================================================

void VulkanContext::createInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "MyEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "MyEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    // SDL3 が要求する拡張を取得
    uint32_t sdlExtCount = 0;
    char const* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts) {
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions: ") +
                                 SDL_GetError());
    }
    std::vector<const char*> exts(sdlExts, sdlExts + sdlExtCount);
    auto addUnique = [&exts](const char* name) {
        for (const char* e : exts) {
            if (std::strcmp(e, name) == 0) return;
        }
        exts.push_back(name);
    };
#ifdef _WIN32
    addUnique("VK_KHR_surface");
    addUnique("VK_KHR_win32_surface");
#endif

    // ─── デバッグビルド: バリデーションレイヤーの利用可否を確認 ────────────
#ifndef NDEBUG
    const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
    bool validationAvailable = false;
    {
        uint32_t n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> available(n);
        vkEnumerateInstanceLayerProperties(&n, available.data());
        for (auto& l : available) {
            if (std::strcmp(l.layerName, kValidationLayer) == 0) {
                validationAvailable = true;
                break;
            }
        }
    }
    if (validationAvailable) {
        addUnique(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::cout << "[MyEngine] Vulkan validation layer enabled.\n";
    } else {
        std::cout << "[MyEngine] Vulkan validation layer not found (SDK not installed?).\n";
    }
#endif

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
#ifndef NDEBUG
    if (validationAvailable) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &kValidationLayer;
    }
#endif
    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed");
    }
}

// =============================================================================
// Debug Messenger
// =============================================================================

void VulkanContext::setupDebugMessenger() {
#ifndef NDEBUG
    // VK_EXT_debug_utils の関数は動的にロードする必要がある
    auto fnCreate = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!fnCreate) return;  // バリデーション無効環境

    VkDebugUtilsMessengerCreateInfoEXT mci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    mci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    mci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    mci.pfnUserCallback = vulkanDebugCallback;
    fnCreate(instance_, &mci, nullptr, &debugMessenger_);
#endif
}

void VulkanContext::destroyDebugMessenger() {
#ifndef NDEBUG
    if (debugMessenger_ == VK_NULL_HANDLE) return;
    auto fnDestroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (fnDestroy) fnDestroy(instance_, debugMessenger_, nullptr);
    debugMessenger_ = VK_NULL_HANDLE;
#endif
}

// =============================================================================
// Surface
// =============================================================================

void VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_)) {
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface: ") + SDL_GetError());
    }
}

// =============================================================================
// Physical Device
// =============================================================================

void VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) throw std::runtime_error("no Vulkan physical devices");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // GPU スコアリング:
    //   Discrete(外部GPU) 1000 > Integrated(内蔵) 100 > その他 10
    auto scoreDevice = [](VkPhysicalDevice d) -> int {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d, &p);
        switch (p.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return 1000;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return 100;
            default:
                return 10;
        }
    };

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;
    uint32_t bestGraphics = VK_QUEUE_FAMILY_IGNORED;
    uint32_t bestPresent = VK_QUEUE_FAMILY_IGNORED;

    for (VkPhysicalDevice d : devices) {
        auto gFam = findGraphicsFamily(d);
        if (!gFam || !checkDeviceExtensionSupport(d, deviceExts, 1)) continue;

        SwapchainSupport swap = querySwapchainSupport(d, surface_);
        if (swap.formats.empty() || swap.presentModes.empty()) continue;

        // present 可能なキューファミリーを探す (graphics と同じなら優先)
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
        uint32_t presentFam = VK_QUEUE_FAMILY_IGNORED;
        // まず graphics と同じファミリーが present できるか確認
        {
            VkBool32 ok = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, *gFam, surface_, &ok);
            if (ok) presentFam = *gFam;
        }
        // 駄目なら任意の present 可能ファミリーを探す
        if (presentFam == VK_QUEUE_FAMILY_IGNORED) {
            for (uint32_t i = 0; i < qCount; ++i) {
                VkBool32 ok = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface_, &ok);
                if (ok) {
                    presentFam = i;
                    break;
                }
            }
        }
        if (presentFam == VK_QUEUE_FAMILY_IGNORED) continue;

        int score = scoreDevice(d);
        if (score > bestScore) {
            bestDevice = d;
            bestScore = score;
            bestGraphics = *gFam;
            bestPresent = presentFam;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) throw std::runtime_error("no suitable Vulkan GPU found");

    physical_ = bestDevice;
    graphicsFamily_ = bestGraphics;
    presentFamily_ = bestPresent;
    vkGetPhysicalDeviceMemoryProperties(physical_, &memoryProperties_);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_, &props);
    const char* typeStr =
        (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)     ? "Discrete (external)"
        : (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? "Integrated (built-in)"
                                                                       : "Other";
    std::cout << "[MyEngine] GPU selected: " << props.deviceName << "  [" << typeStr << "]\n";
}

// =============================================================================
// Logical Device + Queues
// =============================================================================

void VulkanContext::createDevice() {
    const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const float priority = 1.f;

    // graphics と present が同じキューファミリーなら 1 つだけ作成
    std::set<uint32_t> uniqueFamilies = {graphicsFamily_, presentFamily_};
    std::vector<VkDeviceQueueCreateInfo> queueCis;
    for (uint32_t fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queueCis.push_back(qci);
    }

    // 必要なデバイス機能を有効化
    VkPhysicalDeviceFeatures features{};

    // Phase 2B PART3c-2: GPU-driven indirect-draw capabilities (Roadmap §3:
    // capability-check + fallback). Query the device and enable only what it
    // supports. drawIndirectFirstInstance is REQUIRED for our design: each
    // indirect command carries its DrawData slot in firstInstance, and a
    // non-zero firstInstance in an indirect draw needs this feature. Without it
    // we keep the CPU draw loop (firstInstance is unrestricted for direct draws).
    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(physical_, &supportedFeatures);
    multiDrawIndirect_ = (supportedFeatures.multiDrawIndirect == VK_TRUE);
    drawIndirectFirstInstance_ = (supportedFeatures.drawIndirectFirstInstance == VK_TRUE);
    features.multiDrawIndirect = multiDrawIndirect_ ? VK_TRUE : VK_FALSE;
    features.drawIndirectFirstInstance = drawIndirectFirstInstance_ ? VK_TRUE : VK_FALSE;

    // Vulkan13 §1 (W) + PART4 4-前-4: query Vulkan 1.2 and 1.3 core features
    // (synchronization2 / drawIndirectCount) plus the DGC extension feature
    // chain in one Features2 call.
    VkPhysicalDeviceVulkan12Features supportedVk12{};
    supportedVk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceVulkan13Features supportedVk13{};
    supportedVk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    supportedVk13.pNext = &supportedVk12;
    VkPhysicalDeviceFeatures2 supportedFeatures2{};
    supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supportedFeatures2.pNext = &supportedVk13;
    vkGetPhysicalDeviceFeatures2(physical_, &supportedFeatures2);
    synchronization2_ = (supportedVk13.synchronization2 == VK_TRUE);
    drawIndirectCount_ = (supportedVk12.drawIndirectCount == VK_TRUE);

    // PART4 4-前-4: VK_EXT_device_generated_commands is an extension; we check
    // it by name in the device-extension list. A future enable would require
    // adding the extension name to deviceExts and chaining the feature struct,
    // but the indirect_exec wrapper today only uses the capability bit and
    // falls through to the IndirectCount or Legacy path on every device,
    // including P620 which lacks DGC.
    {
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extCount, exts.data());
        for (const VkExtensionProperties& e : exts) {
            if (std::strcmp(e.extensionName, "VK_EXT_device_generated_commands") == 0) {
                deviceGeneratedCommands_ = true;
                break;
            }
        }
    }

    std::cout << "[Caps] multiDrawIndirect=" << (multiDrawIndirect_ ? 1 : 0)
              << " drawIndirectFirstInstance=" << (drawIndirectFirstInstance_ ? 1 : 0)
              << " synchronization2=" << (synchronization2_ ? 1 : 0)
              << " drawIndirectCount=" << (drawIndirectCount_ ? 1 : 0)
              << " deviceGeneratedCommands=" << (deviceGeneratedCommands_ ? 1 : 0) << "\n";
    features.samplerAnisotropy = VK_TRUE;  // テクスチャ異方性フィルタ
    features.fillModeNonSolid = VK_TRUE;   // ワイヤーフレーム描画 (デバッグ)
    features.wideLines = VK_TRUE;          // 線幅指定 (デバッグライン)
    // Vulkan 1.2+ features (enable BDA = Buffer Device Address).
    // BDA lets shaders dereference GPU memory pointers directly,
    // avoiding descriptor sets for buffers. Required for modern bindless setup.
    VkPhysicalDeviceVulkan12Features vk12Features{};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.bufferDeviceAddress = VK_TRUE;
    vk12Features.descriptorIndexing = VK_TRUE;  // bindless texture array support
    vk12Features.runtimeDescriptorArray = VK_TRUE;
    vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vk12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vk12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    // PART4 4-前-4: enable VK_KHR_draw_indirect_count (Vulkan 1.2 core feature).
    // Required for vkCmdDrawIndexedIndirectCount used by indirect_exec when
    // compaction is on.
    vk12Features.drawIndirectCount = drawIndirectCount_ ? VK_TRUE : VK_FALSE;

    // Vulkan13 §1 (W): enable synchronization2 when supported. Chained after
    // vk12Features in pNext. Future PART4 4-前-4 / 4b / 4c additions in
    // Vulkan13_Modernization receive their flags here too.
    VkPhysicalDeviceVulkan13Features vk13Features{};
    vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13Features.synchronization2 = synchronization2_ ? VK_TRUE : VK_FALSE;
    vk12Features.pNext = &vk13Features;

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.pNext = &vk12Features;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queueCis.size());
    ci.pQueueCreateInfos = queueCis.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = deviceExts;

#ifndef NDEBUG
    // 古い Vulkan 実装用に device にもレイヤーを明示（新仕様では不要だが互換のため）
    const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";
    // Device layers are deprecated since Vulkan 1.0 (only instance layers are used).
// Keeping enabledLayerCount = 0 to avoid validation warnings.
// (Instance layers already enabled in vkCreateInstance.)
#endif

    if (vkCreateDevice(physical_, &ci, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDevice failed");
    }
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
}

// =============================================================================
// Depth Format
// =============================================================================

VkFormat VulkanContext::findDepthFormat() const {
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                   VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat fmt : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physical_, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return fmt;
    }
    throw std::runtime_error("findDepthFormat: no supported depth format");
}

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
#include <fstream>
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

    // PART4 4d M1: persistent pipeline cache. Load before any vk*Pipelines
    // create call (which all happen later in renderer init); shutdown
    // saves back to disk.
    createPipelineCache();

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
        // N: Activate memory_priority so the ai.priority field on each
        // VmaAllocation actually influences eviction order. Was set to
        // 0.75 on VmaImage::createAttachment before this bit was on, but
        // VMA silently ignored it without the flag.
        if (memoryPriority_) {
            allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        }
        // I: live per-heap budget tracking so vmaGetHeapBudgets reflects
        // the driver's current usage / budget rather than VMA's internal
        // estimate. Required for any meaningful VRAM HUD or streaming
        // residency manager.
        if (memoryBudget_) {
            allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }
        if (vmaCreateAllocator(&allocInfo, &allocator_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanContext: vmaCreateAllocator failed");
        }
        std::cout << "[MyEngine] VMA allocator created (BDA enabled, API 1.4).\n";
    }
}

void VulkanContext::shutdown() {
    // 破棄順序: Pipeline cache (save+release) → VMA → Device → Surface → DebugMessenger → Instance
    if (device_ != VK_NULL_HANDLE) {
    // PART4 4d M1: persist the pipeline cache to disk + release the handle
    // BEFORE vkDestroyDevice (the cache holds device-owned data).
    savePipelineCache();
    pipelineCache_.reset();

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
    // W: opt-in to VK_LAYER_KHRONOS_synchronization_validation through the
    // VkValidationFeaturesEXT pNext chain. The validation layer detects
    // sync2 hazards (missing barriers, racy host writes, queue-family
    // ownership leaks) at run time. Pays zero cost in release because the
    // whole block is gated by NDEBUG.
    VkValidationFeaturesEXT validationFeatures{
        VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    const VkValidationFeatureEnableEXT enabledFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };
    validationFeatures.enabledValidationFeatureCount = 1;
    validationFeatures.pEnabledValidationFeatures = enabledFeatures;
    if (validationAvailable) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &kValidationLayer;
        ci.pNext = &validationFeatures;
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
    // Device extensions: swapchain is mandatory; VK_EXT_memory_priority is
    // added when queryCapabilities() detected it (N).
    std::vector<const char*> deviceExtsVec;
    deviceExtsVec.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (memoryPriority_) {
        deviceExtsVec.push_back("VK_EXT_memory_priority");
    }
    if (memoryBudget_) {
        deviceExtsVec.push_back("VK_EXT_memory_budget");  // I
    }
    // L/K/Z/Q: activated. Their feature structs (chained below in pNext)
    // set the bool to TRUE so vkCreateDevice accepts them. T (swapchain_
    // maintenance1) is held back because it requires the
    // VK_EXT_surface_maintenance1 INSTANCE extension which we did not
    // enable at instance creation; revisiting it after the instance-side
    // ext is added. D (EDS3) is held back because its feature struct has
    // ~30 individual bool fields and a proper enable wants a prior
    // VkPhysicalDeviceFeatures2 query to enable only what the driver
    // supports -- the receptacle stays queried-only until that query
    // lands.
    if (shaderObject_) {
        deviceExtsVec.push_back("VK_EXT_shader_object");  // L
    }
    if (presentId_) {
        deviceExtsVec.push_back("VK_KHR_present_id");  // K
    }
    if (presentWait_) {
        deviceExtsVec.push_back("VK_KHR_present_wait");  // K
    }
    if (imageViewMinLod_) {
        deviceExtsVec.push_back("VK_EXT_image_view_min_lod");  // Z
    }
    if (calibratedTimestamps_) {
        deviceExtsVec.push_back("VK_KHR_calibrated_timestamps");  // Q (no feature struct)
    }
    // J (host_image_copy) is enabled via vk14Features.hostImageCopy in the
    // feature chain below; the Vulkan 1.4 core promotion handles loader
    // visibility so no extension entry is needed.
    const float priority = 1.f;

    // PART4 4c-B (§3.4-V receptacle): pick a queue family that supports
    // COMPUTE without GRAPHICS so HiZPass / CullingPass.executePass2 can
    // overlap with the previous-pass main draw on a dedicated async queue.
    // NVIDIA Pascal+ exposes one (typically family index 2 on Quadro/GeForce);
    // integrated GPUs may not. Fall back to graphicsFamily_ so the API form
    // (queue argument) stays portable even on devices without async compute.
    asyncComputeFamily_ = graphicsFamily_;  // safe fallback
    transferFamily_ = graphicsFamily_;       // C: safe fallback
    {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_, &qCount, qProps.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            const VkQueueFlags f = qProps[i].queueFlags;
            if ((f & VK_QUEUE_COMPUTE_BIT) != 0 &&
                (f & VK_QUEUE_GRAPHICS_BIT) == 0) {
                asyncComputeFamily_ = i;  // first dedicated compute family wins
                break;
            }
        }
        // C (Foundations \xc2\xa72 a-2): dedicated transfer-only family for streaming
        // uploads (Phase 2F). Most discrete GPUs expose family with TRANSFER
        // bit only and no GRAPHICS / COMPUTE. Falls back to graphicsFamily_
        // (already set above) when absent so transferQueue() is always usable.
        for (uint32_t i = 0; i < qCount; ++i) {
            const VkQueueFlags f = qProps[i].queueFlags;
            if ((f & VK_QUEUE_TRANSFER_BIT) != 0 &&
                (f & VK_QUEUE_GRAPHICS_BIT) == 0 &&
                (f & VK_QUEUE_COMPUTE_BIT) == 0) {
                transferFamily_ = i;  // first dedicated transfer family wins
                break;
            }
        }
    }

    // graphics と present (と async compute, C transfer) が同じキューファミリーなら 1 つだけ作成
    std::set<uint32_t> uniqueFamilies = {graphicsFamily_, presentFamily_, asyncComputeFamily_,
                                         transferFamily_};
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

    // Vulkan13 §1 (W) + PART4 4-前-4 + 4d M3 + 4d N4: query Vulkan 1.2 / 1.3
    // / 1.4 core features in one Features2 call. The Vulkan14Features chain
    // entry covers dynamicRenderingLocalRead (was queried via the KHR-
    // suffixed extension struct in 4d M3, migrated to the 1.4 core struct
    // here) AND maintenance5 / maintenance6 (4d N4 receptacles).
    VkPhysicalDeviceVulkan12Features supportedVk12{};
    supportedVk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceVulkan13Features supportedVk13{};
    supportedVk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    supportedVk13.pNext = &supportedVk12;
    VkPhysicalDeviceVulkan14Features supportedVk14{};
    supportedVk14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    supportedVk14.pNext = &supportedVk13;
    VkPhysicalDeviceFeatures2 supportedFeatures2{};
    supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supportedFeatures2.pNext = &supportedVk14;
    vkGetPhysicalDeviceFeatures2(physical_, &supportedFeatures2);
    synchronization2_ = (supportedVk13.synchronization2 == VK_TRUE);
    drawIndirectCount_ = (supportedVk12.drawIndirectCount == VK_TRUE);
    dynamicRendering_ = (supportedVk13.dynamicRendering == VK_TRUE);
    separateDepthStencilLayouts_ = (supportedVk12.separateDepthStencilLayouts == VK_TRUE);
    // PART4 4c-B: Vulkan 1.2 core samplerFilterMinmax. cull.comp's HZB sample
    // takes the 1-tap fast path when this is true, 4-tap manual min otherwise.
    samplerFilterMinmax_ = (supportedVk12.samplerFilterMinmax == VK_TRUE);
    // PART4 4d M3: Vulkan 1.4 core dynamicRenderingLocalRead (was KHR ext
    // in M3; migrated to the 1.4 Features struct in 4d N4).
    dynamicRenderingLocalRead_ = (supportedVk14.dynamicRenderingLocalRead == VK_TRUE);
    // PART4 4d N1: Vulkan 1.3 core pipelineCreationCacheControl.
    pipelineCreationCacheControl_ = (supportedVk13.pipelineCreationCacheControl == VK_TRUE);
    // PART4 4d N4: Vulkan 1.4 core maintenance5 / maintenance6 receptacles.
    // Modern entry points (vkCmdBindIndexBuffer2KHR, getRenderingAreaGranularity,
    // vkCmdBindDescriptorSets2KHR, etc.) available for future subsystems.
    maintenance5_ = (supportedVk14.maintenance5 == VK_TRUE);
    maintenance6_ = (supportedVk14.maintenance6 == VK_TRUE);

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
        // PART4 4-前-4: VK_EXT_device_generated_commands (DGC) extension
        // receptacle. PART4 4d N2 / N3: graphics_pipeline_library +
        // pipeline_binary receptacles for the open-world streaming
        // pipeline strategy. All are queried by name only - no feature
        // enable / no add to deviceExts yet; activation lands per-Phase.
        for (const VkExtensionProperties& e : exts) {
            if (std::strcmp(e.extensionName, "VK_EXT_device_generated_commands") == 0) {
                deviceGeneratedCommands_ = true;
            } else if (std::strcmp(e.extensionName, "VK_EXT_graphics_pipeline_library") == 0) {
                graphicsPipelineLibrary_ = true;  // PART4 4d N2
            } else if (std::strcmp(e.extensionName, "VK_KHR_pipeline_binary") == 0) {
                pipelineBinary_ = true;  // PART4 4d N3
            } else if (std::strcmp(e.extensionName, "VK_EXT_memory_priority") == 0) {
                memoryPriority_ = true;  // N: VRAM eviction priority hints
            } else if (std::strcmp(e.extensionName, "VK_EXT_memory_budget") == 0) {
                memoryBudget_ = true;  // I: VRAM budget visibility (Roadmap \xc2\xa76)
            } else if (std::strcmp(e.extensionName, "VK_EXT_extended_dynamic_state3") == 0) {
                extendedDynamicState3_ = true;  // D: EDS3 (EDS1/2 are 1.3 core)
            } else if (std::strcmp(e.extensionName, "VK_EXT_shader_object") == 0) {
                shaderObject_ = true;  // L: modern triad's third extension
            } else if (std::strcmp(e.extensionName, "VK_KHR_present_id") == 0) {
                presentId_ = true;  // K
            } else if (std::strcmp(e.extensionName, "VK_KHR_present_wait") == 0) {
                presentWait_ = true;  // K
            } else if (std::strcmp(e.extensionName, "VK_EXT_swapchain_maintenance1") == 0) {
                swapchainMaintenance1_ = true;  // T: VRR / present-mode swap
            } else if (std::strcmp(e.extensionName, "VK_EXT_image_view_min_lod") == 0) {
                imageViewMinLod_ = true;  // Z: texture mip streaming bias
            } else if (std::strcmp(e.extensionName, "VK_EXT_host_image_copy") == 0) {
                hostImageCopy_ = true;  // J: staging-less host->image upload (1.4 promotion)
            } else if (std::strcmp(e.extensionName, "VK_KHR_calibrated_timestamps") == 0) {
                calibratedTimestamps_ = true;  // Q: CPU<->GPU timeline correlation
            }
        }
        // B: timelineSemaphore is Vulkan 1.2 core. We target API 1.4, so it
        // is always present; setting the flag here keeps it visible in the
        // [Caps] log alongside the extension capabilities.
        timelineSemaphore_ = true;
    }

    // PART4 4b: query subgroup properties (Vulkan 1.1 core). HiZPass's
    // wave-ops shader (hiz_spd_wave.comp) needs GL_KHR_shader_subgroup_basic
    // (subgroup builtins) plus GL_KHR_shader_subgroup_shuffle (subgroupShuffleXor)
    // in the COMPUTE stage. ARITHMETIC and QUAD ops are NOT used; querying
    // them would over-restrict the gate. If either required feature is
    // missing we fall back to the LDS-only variant (hiz_spd.comp). The
    // subgroup size is exposed separately because the wave path also needs
    // subgroupSize >= 32 for the 16x16 thread grid's y-neighbour shuffle to
    // stay inside one subgroup.
    {
        VkPhysicalDeviceSubgroupProperties subgroupProps{};
        subgroupProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &subgroupProps;
        vkGetPhysicalDeviceProperties2(physical_, &props2);
        const VkSubgroupFeatureFlags need =
            VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
        const bool stageOk =
            (subgroupProps.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
        const bool opsOk = (subgroupProps.supportedOperations & need) == need;
        subgroupOps_ = stageOk && opsOk;
        subgroupSize_ = subgroupProps.subgroupSize;
    }

    std::cout << "[Caps] multiDrawIndirect=" << (multiDrawIndirect_ ? 1 : 0)
              << " drawIndirectFirstInstance=" << (drawIndirectFirstInstance_ ? 1 : 0)
              << " synchronization2=" << (synchronization2_ ? 1 : 0)
              << " drawIndirectCount=" << (drawIndirectCount_ ? 1 : 0)
              << " deviceGeneratedCommands=" << (deviceGeneratedCommands_ ? 1 : 0)
              << " dynamicRendering=" << (dynamicRendering_ ? 1 : 0)
              << " separateDepthStencilLayouts=" << (separateDepthStencilLayouts_ ? 1 : 0)
              << " subgroupOps=" << (subgroupOps_ ? 1 : 0)
              << " subgroupSize=" << subgroupSize_
              << " samplerFilterMinmax=" << (samplerFilterMinmax_ ? 1 : 0)
              << " asyncComputeFamily=" << asyncComputeFamily_
              << " (dedicated=" << (asyncComputeFamily_ != graphicsFamily_ ? 1 : 0) << ")"
              << " transferFamily=" << transferFamily_
              << " (dedicated=" << (transferFamily_ != graphicsFamily_ ? 1 : 0) << ")"
              << " dynamicRenderingLocalRead=" << (dynamicRenderingLocalRead_ ? 1 : 0)
              << " pipelineCreationCacheControl=" << (pipelineCreationCacheControl_ ? 1 : 0)
              << " maintenance5=" << (maintenance5_ ? 1 : 0)
              << " maintenance6=" << (maintenance6_ ? 1 : 0)
              << " graphicsPipelineLibrary=" << (graphicsPipelineLibrary_ ? 1 : 0)
              << " pipelineBinary=" << (pipelineBinary_ ? 1 : 0)
              << " memoryPriority=" << (memoryPriority_ ? 1 : 0)
              << " memoryBudget=" << (memoryBudget_ ? 1 : 0)
              << " timelineSemaphore=" << (timelineSemaphore_ ? 1 : 0)
              << " extDynState3=" << (extendedDynamicState3_ ? 1 : 0)
              << " shaderObject=" << (shaderObject_ ? 1 : 0)
              << " presentId=" << (presentId_ ? 1 : 0)
              << " presentWait=" << (presentWait_ ? 1 : 0)
              << " swapchainMaint1=" << (swapchainMaintenance1_ ? 1 : 0)
              << " imageViewMinLod=" << (imageViewMinLod_ ? 1 : 0)
              << " hostImageCopy=" << (hostImageCopy_ ? 1 : 0)
              << " calibratedTimestamps=" << (calibratedTimestamps_ ? 1 : 0)
              << "\n";
    features.samplerAnisotropy = VK_TRUE;  // テクスチャ異方性フィルタ
    features.fillModeNonSolid = VK_TRUE;   // ワイヤーフレーム描画 (デバッグ)
    features.wideLines = VK_TRUE;          // 線幅指定 (デバッグライン)
    // PART4 4b: HiZPass binds the per-mip storage views as a descriptor array
    // (descriptorCount = kMaxMips) and indexes it with a loop-uniform mip
    // index inside the SPD shader. Vulkan 1.0 core; supported on Pascal+.
    features.shaderStorageImageArrayDynamicIndexing =
        (supportedFeatures.shaderStorageImageArrayDynamicIndexing == VK_TRUE)
            ? VK_TRUE
            : VK_FALSE;
    // Vulkan 1.2+ features (enable BDA = Buffer Device Address).
    // BDA lets shaders dereference GPU memory pointers directly,
    // avoiding descriptor sets for buffers. Required for modern bindless setup.
    VkPhysicalDeviceVulkan12Features vk12Features{};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12Features.bufferDeviceAddress = VK_TRUE;
    vk12Features.descriptorIndexing = VK_TRUE;  // bindless texture array support
    // B (Vulkan13 \xc2\xa76 U / INDEX U): timelineSemaphore (Vulkan 1.2 core, 2020
    // standard). Receptacle: feature is enabled so vkCreateSemaphore can
    // supply a VkSemaphoreTypeCreateInfo with VK_SEMAPHORE_TYPE_TIMELINE.
    // FrameSync still ships on binary semaphores + VkFence today; the timeline
    // migration is a Phase 2F prerequisite for value-based wait across async
    // compute and the transfer queue. Keeping the gate live now means the
    // value-based path (vkSignalSemaphore / vkWaitSemaphores / chained
    // VkTimelineSemaphoreSubmitInfo) can land without re-enabling features.
    vk12Features.timelineSemaphore = VK_TRUE;  // (already set above)
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
    // PART4 4a-2 modernization: separate depth/stencil layouts (Vulkan 1.2
    // core but OPTIONAL - enabling unconditionally trips
    // VK_ERROR_FEATURE_NOT_PRESENT on drivers that don't expose it). Gate on
    // the query above; main_pass falls back to the combined layouts when
    // the bit is false.
    vk12Features.separateDepthStencilLayouts =
        separateDepthStencilLayouts_ ? VK_TRUE : VK_FALSE;
    // PART4 4c-B: samplerFilterMinmax for the cull.comp HZB sample fast path.
    // Optional Vulkan 1.2 feature, so gate on the query.
    vk12Features.samplerFilterMinmax = samplerFilterMinmax_ ? VK_TRUE : VK_FALSE;

    // Vulkan13 §1 (W): enable synchronization2 when supported. Chained after
    // vk12Features in pNext. Future PART4 4-前-4 / 4b / 4c additions in
    // Vulkan13_Modernization receive their flags here too.
    // PART4 4a-1: enable dynamicRendering (Vulkan 1.3 core) so main_pass can
    // drop VkRenderPass / VkFramebuffer in favour of vkCmdBeginRendering.
    VkPhysicalDeviceVulkan13Features vk13Features{};
    vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13Features.synchronization2 = synchronization2_ ? VK_TRUE : VK_FALSE;
    vk13Features.dynamicRendering = dynamicRendering_ ? VK_TRUE : VK_FALSE;
    // PART4 4d N1: pipelineCreationCacheControl for streaming-friendly
    // pipeline creation (VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_
    // REQUIRED_BIT). Receptacle for Phase 2A clustered light variants +
    // Phase 3 SS effect specialisations.
    vk13Features.pipelineCreationCacheControl =
        pipelineCreationCacheControl_ ? VK_TRUE : VK_FALSE;
    vk12Features.pNext = &vk13Features;

    // PART4 4d M3 + N4: Vulkan 1.4 core features chain. Replaces the
    // KHR-suffixed VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
    // struct from M3 with the canonical 1.4 Features struct - the engine
    // already ran on a 1.4 API but had no Vulkan14Features chain entry,
    // making 1.4 promotions unreachable. N4 fixes that and adds
    // maintenance5 / maintenance6 receptacles in the same struct.
    VkPhysicalDeviceVulkan14Features vk14Features{};
    vk14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    vk14Features.dynamicRenderingLocalRead =
        dynamicRenderingLocalRead_ ? VK_TRUE : VK_FALSE;
    vk14Features.maintenance5 = maintenance5_ ? VK_TRUE : VK_FALSE;
    vk14Features.maintenance6 = maintenance6_ ? VK_TRUE : VK_FALSE;
    vk13Features.pNext = &vk14Features;

    // J: host_image_copy is Vulkan 1.4 core (promoted from
    // VK_EXT_host_image_copy). Setting the feature flag activates
    // vkCopyMemoryToImage / vkCopyImageToMemory / vkTransitionImageLayout
    // for staging-less host->image upload.
    vk14Features.hostImageCopy = hostImageCopy_ ? VK_TRUE : VK_FALSE;

    // N: VK_EXT_memory_priority feature struct. Enables VMA priority hints
    // (ai.priority on each VmaAllocation) so the driver knows which
    // allocations to keep resident under VRAM pressure.
    VkPhysicalDeviceMemoryPriorityFeaturesEXT memPriorityFeatures{};
    memPriorityFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
    memPriorityFeatures.memoryPriority = memoryPriority_ ? VK_TRUE : VK_FALSE;

    // L: VK_EXT_shader_object feature struct. Activating shaderObject lets
    // vkCreateShadersEXT / vkCmdBindShadersEXT load; together with EDS
    // (already in 1.3 core) it lets the engine drop VkPipeline objects
    // entirely on the day a caller chooses to.
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{};
    shaderObjectFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shaderObjectFeatures.shaderObject = shaderObject_ ? VK_TRUE : VK_FALSE;

    // K: VK_KHR_present_id + VK_KHR_present_wait feature structs. Pair:
    // present_id tags each present with a monotonically increasing 64-bit
    // value (set via VkPresentIdKHR in vkQueuePresentKHR's pNext);
    // present_wait blocks vkWaitForPresentKHR until the matching present
    // has reached the display. Required for any modern frame-pacing /
    // input-latency control (Reflex-equivalent open loop).
    VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{};
    presentIdFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
    presentIdFeatures.presentId = presentId_ ? VK_TRUE : VK_FALSE;

    VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{};
    presentWaitFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
    presentWaitFeatures.presentWait = presentWait_ ? VK_TRUE : VK_FALSE;

    // Z: VK_EXT_image_view_min_lod feature struct. Activating minLod lets
    // VkImageViewMinLodCreateInfoEXT chain into VkImageViewCreateInfo's
    // pNext, clamping the lowest mip the view can sample. Critical for
    // texture mip streaming: rebuild a per-texture view with minLod = N
    // and only mips >= N have to be GPU-resident.
    VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures{};
    imageViewMinLodFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT;
    imageViewMinLodFeatures.minLod = imageViewMinLod_ ? VK_TRUE : VK_FALSE;

    // Chain the activated features into pNext. Order is irrelevant as long
    // as each struct's pNext lives until vkCreateDevice returns. N stays
    // first because it was first in time; the new structs append after.
    VkBaseOutStructure* tail = nullptr;
    auto appendToChain = [&tail, &vk14Features](VkBaseOutStructure* node) {
        if (!tail) {
            tail = reinterpret_cast<VkBaseOutStructure*>(&vk14Features);
            while (tail->pNext) tail = tail->pNext;
        }
        node->pNext = nullptr;
        tail->pNext = node;
        tail = node;
    };
    if (memoryPriority_)
        appendToChain(reinterpret_cast<VkBaseOutStructure*>(&memPriorityFeatures));
    if (shaderObject_)
        appendToChain(reinterpret_cast<VkBaseOutStructure*>(&shaderObjectFeatures));
    if (presentId_)
        appendToChain(reinterpret_cast<VkBaseOutStructure*>(&presentIdFeatures));
    if (presentWait_)
        appendToChain(reinterpret_cast<VkBaseOutStructure*>(&presentWaitFeatures));
    if (imageViewMinLod_)
        appendToChain(reinterpret_cast<VkBaseOutStructure*>(&imageViewMinLodFeatures));

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.pNext = &vk12Features;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queueCis.size());
    ci.pQueueCreateInfos = queueCis.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtsVec.size());
    ci.ppEnabledExtensionNames = deviceExtsVec.data();

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
    // PART4 4c-B: async-compute queue handle. If the device has a dedicated
    // compute family we get a separate queue; otherwise asyncComputeQueue_
    // aliases graphicsQueue_ so callers can pass it without branching.
    if (asyncComputeFamily_ != graphicsFamily_) {
        vkGetDeviceQueue(device_, asyncComputeFamily_, 0, &asyncComputeQueue_);
    } else {
        asyncComputeQueue_ = graphicsQueue_;
    }
    // C: same pattern -- dedicated transfer queue when the device has a
    // transfer-only family, otherwise alias graphicsQueue_.
    if (transferFamily_ != graphicsFamily_) {
        vkGetDeviceQueue(device_, transferFamily_, 0, &transferQueue_);
    } else {
        transferQueue_ = graphicsQueue_;
    }
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

// =============================================================================
// PART4 4d M1: Persistent Pipeline Cache (Vulkan13 §3 Y)
// =============================================================================
//
// Stores compiled pipeline binaries to <SDL_GetPrefPath>/pipeline.cache so
// subsequent runs skip the shader-compile step. Vulkan ignores cached data
// whose header / pipelineCacheUUID doesn't match the current driver, so
// driver updates invalidate the cache transparently (no manual versioning).
//
// All vkCreate{Graphics,Compute}Pipelines callers pass pipelineCache_ via
// VulkanContext::pipelineCache(); the cache is created BEFORE any pipeline
// in init() (right after createDevice) and saved + released BEFORE
// vkDestroyDevice in shutdown().
//
// First run: file doesn't exist -> create empty cache; shutdown writes the
// file. Subsequent runs: load + create with initial data; shutdown overwrites.

void VulkanContext::createPipelineCache() {
    if (device_ == VK_NULL_HANDLE) return;

    // Resolve user pref path matching SettingsIO's convention.
    char* prefPath = SDL_GetPrefPath("MyEngine", "MyEngine");
    if (prefPath) {
        pipelineCachePath_ = std::string(prefPath) + "pipeline.cache";
        SDL_free(prefPath);
    } else {
        const char* basePath = SDL_GetBasePath();
        pipelineCachePath_ = (basePath ? std::string(basePath) : std::string{}) + "pipeline.cache";
        std::cerr << "[PipelineCache] WARNING: SDL_GetPrefPath failed, falling back to "
                  << pipelineCachePath_ << "\n";
    }

    // Try to load prior cache content. Vulkan validates the header (UUID,
    // device ID, vendor ID, driver version) and ignores mismatched data.
    std::vector<char> initialData;
    {
        std::ifstream in(pipelineCachePath_, std::ios::binary | std::ios::ate);
        if (in) {
            const std::streamsize size = in.tellg();
            if (size > 0) {
                in.seekg(0);
                initialData.resize(static_cast<size_t>(size));
                in.read(initialData.data(), size);
                if (!in) initialData.clear();  // partial read = treat as empty
            }
        }
    }

    VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    ci.initialDataSize = initialData.size();
    ci.pInitialData = initialData.empty() ? nullptr : initialData.data();
    VkPipelineCache cache = VK_NULL_HANDLE;
    if (vkCreatePipelineCache(device_, &ci, nullptr, &cache) != VK_SUCCESS) {
        std::cerr << "[PipelineCache] WARNING: vkCreatePipelineCache failed, continuing without cache\n";
        return;
    }
    pipelineCache_ = VkUnique<VkPipelineCache>(device_, cache);
    std::cout << "[PipelineCache] " << (initialData.empty() ? "fresh (no prior data)" : "loaded")
              << " from '" << pipelineCachePath_ << "' (" << initialData.size() << " B)\n";
}

void VulkanContext::savePipelineCache() {
    if (device_ == VK_NULL_HANDLE || !pipelineCache_ || pipelineCachePath_.empty()) return;

    // Two-call pattern: query size first, then allocate + fetch.
    size_t size = 0;
    if (vkGetPipelineCacheData(device_, pipelineCache_.get(), &size, nullptr) != VK_SUCCESS ||
        size == 0) {
        return;
    }
    std::vector<char> data(size);
    if (vkGetPipelineCacheData(device_, pipelineCache_.get(), &size, data.data()) != VK_SUCCESS) {
        return;
    }

    std::ofstream out(pipelineCachePath_, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "[PipelineCache] WARNING: failed to open '" << pipelineCachePath_
                  << "' for write\n";
        return;
    }
    out.write(data.data(), static_cast<std::streamsize>(size));
    if (out) {
        std::cout << "[PipelineCache] saved " << size << " B to '" << pipelineCachePath_ << "'\n";
    }
}

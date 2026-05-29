#pragma once
// =============================================================================
// BC7Texture -- P: GPU-block-compressed texture loading receptacle
// =============================================================================
// Today Texture::loadFromFile decodes JPEG/PNG via stb_image into RGBA8 and
// uploads RGBA8 to a VmaImage. At 1024x1024 that's 4 MB / texture; for the
// 12 Stage 1-1 textures alone the VRAM footprint is 48 MB. P620's 2 GB VRAM
// will not stretch to a streamed open-world world at that rate.
//
// BC7 (block compression 7) is the modern desktop-GPU texture format:
//   - 8 bits per texel        : 4x smaller than RGBA8.
//   - Lossy but visually lossless at the typical bitrate.
//   - Supported on every Pascal+ NVIDIA / GCN+ AMD desktop GPU (P620 has
//     textureCompressionBC = TRUE; checked via VkPhysicalDeviceFeatures).
//
// Adjacent compressed formats covered by the same receptacle:
//   - BC4 (single channel)  : heightmaps, alpha, AO masks.
//   - BC5 (two channel)     : normal maps (X+Y, derive Z).
//   - BC6H                  : HDR cubemaps / IBL.
//   - ASTC                  : mobile / Pascal -- not enabled here, but the
//                              loader's format dispatch would be the same.
//
// Receptacle shape:
//   - Asset pipeline: an offline conversion step takes the engine's source
//     .jpg/.png assets and emits .dds / .ktx2 files with BC7 (color) or
//     BC5 (normal) pre-encoded. Tools: nvtt_export, BCEnc, AMD Compressonator.
//   - Runtime: Texture::loadFromFile peeks the file header, dispatches to
//     the BC7-aware path when applicable, and uploads the BC7 blocks
//     directly to a VK_FORMAT_BC7_SRGB_BLOCK / BC7_UNORM_BLOCK VmaImage.
//     No CPU decode, no RGBA8 staging buffer.
//   - VK_EXT_host_image_copy (J receptacle) skips even the staging VkBuffer
//     once activated, so the streaming path becomes mmap -> driver upload.
//
// Capability gate: VulkanContext should query
// supportedFeatures.textureCompressionBC (Pascal+) and only take the BC7
// path when the flag is true; otherwise fall back to the existing RGBA8
// loader.
// =============================================================================
#include <cstdint>
#include <string>

namespace myengine::renderer {

struct Bc7LoadResult {
    enum class Status { LoadedRgba8, LoadedBc7, LoadedBc5, FileNotFound };
    Status status = Status::FileNotFound;
};

// Per-Phase activation lands the offline tool and the runtime loader. The
// receptacle exposes the call-site shape so the rest of the engine can
// write "auto result = bc7::load(path, ...)" against a stable surface.
namespace bc7 {

inline bool isCompressedExtension(const std::string& path) {
    if (path.size() < 5) return false;
    const std::string ext = path.substr(path.size() - 4);
    return ext == ".dds" || ext == ".ktx" || ext == ".kt2";
}

}  // namespace bc7
}  // namespace myengine::renderer

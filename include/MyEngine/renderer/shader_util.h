// include/MyEngine/renderer/shader_util.h
#pragma once
// =============================================================================
// shader_util.h — リファクタ Step 8
//   シェーダ読み込み・モジュール作成のユーティリティ。
//   将来は SPIR-V 反射やキャッシュ機構もここに集約する想定。
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace shader_util {

// SPIR-V バイナリをファイルから読み込む。
std::vector<char> readBinaryFile(const std::string& path);

// SPIR-V バイナリから VkShaderModule を作る。失敗時は throw。
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

// 上記の組み合わせ。よく使うので便利関数として提供。
VkShaderModule loadShaderModule(VkDevice device, const std::string& path);

}  // namespace shader_util

#version 450
// =============================================================================
// hud.frag — HUD 矩形描画用 fragment shader
// =============================================================================
// push constants の color をそのまま出力 (単色塗りつぶし)。
// 将来テクスチャ対応・グラデーション・シェーダエフェクトを追加する場合は
// ここに書き足す。
// =============================================================================

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
    vec2 rectMin;
    vec2 rectSize;
    vec4 color;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = pc.color;
}

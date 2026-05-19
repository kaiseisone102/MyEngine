#version 450

// =============================================================================
// particle.frag — Phase 1: 単純な円形フェード
// =============================================================================
// UV 中心からの距離で alpha 補間。 円形の柔らかい点が描画される。
// Phase 3 でノイズテクスチャ + 揺らぎに置き換える。
// =============================================================================

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vAge01;

layout(location = 0) out vec4 outColor;

void main() {
    // UV 中心からの距離
    float d = length(vUV - vec2(0.5));
    if (d > 0.5) discard;

    // 円形フェード: 中心 1.0、 縁 0.0
    float fade = 1.0 - (d * 2.0);
    fade = fade * fade;  // 緩い 2 乗カーブで中心が強調される

    outColor = vec4(vColor.rgb, vColor.a * fade);
}

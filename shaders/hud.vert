#version 450

// =============================================================================
// hud.vert — HUD 描画 (shape mode 拡張版)
// =============================================================================
// 4 頂点で四角形を描画する (TRIANGLE_STRIP)。
// 形状 (円 / リング / 扇形) はフラグメントシェーダーで discard 判定する。
// 回転矩形 (mode=3) は頂点シェーダーで bounding box を回転して生成する。
//
// PushConstants (40 + 24 = 64 bytes):
//   screenSize : ウィンドウ解像度 (px)
//   rectMin    : 矩形左上 (px、 ウィンドウ左上原点)
//   rectSize   : 矩形サイズ (w, h、 px)
//   color      : 塗り色 (RGBA)
//   shapeMode  : 0=Rect, 1=Circle, 2=Ring, 3=RotatedRect, 4=CircleSegment
//   flags      : ビットフラグ (bit0: gloss enable)
//   extraParams: 形状ごとの追加データ (vec4)
// =============================================================================

layout(push_constant) uniform PC {
    vec2 screenSize;
    vec2 rectMin;
    vec2 rectSize;
    vec4 color;
    int  shapeMode;
    int  flags;
    vec4 extraParams;
} pc;

layout(location = 0) out vec2 vLocalUV;  // 0..1 矩形内座標 (frag が形状判定に使う)

void main() {
    // 4 頂点で TRIANGLE_STRIP の四角を作る
    //   index 0: (0,0)  1: (1,0)  2: (0,1)  3: (1,1)
    vec2 corner;
    if (gl_VertexIndex == 0)      corner = vec2(0.0, 0.0);
    else if (gl_VertexIndex == 1) corner = vec2(1.0, 0.0);
    else if (gl_VertexIndex == 2) corner = vec2(0.0, 1.0);
    else                          corner = vec2(1.0, 1.0);

    vLocalUV = corner;

    // px 座標で矩形の頂点位置を計算
    vec2 posPx = pc.rectMin + corner * pc.rectSize;

    // 回転矩形 (mode=3): rectMin を中心とする回転を適用
    //   extraParams.x = 回転角 (ラジアン)
    //   extraParams.yz = 回転中心 (px)
    if (pc.shapeMode == 3) {
        float angle = pc.extraParams.x;
        vec2 center = pc.extraParams.yz;
        float c = cos(angle);
        float s = sin(angle);
        vec2 d = posPx - center;
        posPx = center + vec2(d.x * c - d.y * s, d.x * s + d.y * c);
    }

    // px → NDC (-1..1)、 Y 軸反転 (ウィンドウ左上が NDC(-1,-1))
    vec2 ndc = (posPx / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}

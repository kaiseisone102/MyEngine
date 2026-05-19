#version 450

// =============================================================================
// hud.frag — HUD 形状判定 + メカニカル装飾 (v2: 参考画像により近く)
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

layout(location = 0) in vec2 vLocalUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;

void main() {
    int mode = pc.shapeMode;

    if (mode == 0 || mode == 3) {
        outColor = pc.color;
        return;
    }

    vec2 d = vLocalUV - vec2(0.5);
    float dist = length(d);

    // ─── mode 1: 円 + 光沢 ────────────────────
    if (mode == 1) {
        if (dist > 0.5) discard;

        vec4 c = pc.color;

        if ((pc.flags & 1) != 0) {
            float strength = pc.extraParams.y;
            vec2 highlightCenter = vec2(0.35, 0.35);
            float highlightDist = length(vLocalUV - highlightCenter);
            float highlightRadius = 0.18;
            if (highlightDist < highlightRadius) {
                float t = 1.0 - (highlightDist / highlightRadius);
                t = t * t;
                c.rgb += vec3(t * strength);
            }
            float shade = smoothstep(0.3, 0.5, dist);
            c.rgb *= mix(1.0, 0.75, shade);
        }

        outColor = c;
        return;
    }

    // ─── mode 2: リング ────────────────────────
    if (mode == 2) {
        float outer = pc.extraParams.x;
        float inner = pc.extraParams.y;
        if (dist > outer || dist < inner) discard;
        outColor = pc.color;
        return;
    }

    // ─── mode 4: 扇形 ────────────────────────
    if (mode == 4) {
        float outer = pc.extraParams.x;
        float inner = pc.extraParams.y;
        if (dist > outer || dist < inner) discard;

        float ang = atan(d.x, -d.y);
        if (ang < 0.0) ang += 2.0 * PI;

        float angStart = pc.extraParams.z;
        float angEnd = pc.extraParams.w;
        if (angStart <= angEnd) {
            if (ang < angStart || ang > angEnd) discard;
        } else {
            if (ang < angStart && ang > angEnd) discard;
        }
        outColor = pc.color;
        return;
    }

    // ─── mode 5: グラデーション矩形 ───────────────
    if (mode == 5) {
        float darken = pc.extraParams.z;
        float vertical = pc.extraParams.w;
        float t = mix(abs(vLocalUV.x - 0.5) * 2.0, abs(vLocalUV.y - 0.5) * 2.0, vertical);
        vec4 c = pc.color;
        c.rgb *= mix(1.0, 1.0 - darken, t);
        outColor = c;
        return;
    }

    // ─── mode 6: ベベル矩形 ──────────────────────
    if (mode == 6) {
        float bevel = pc.extraParams.x;
        float hiStrength = pc.extraParams.y;
        float shStrength = pc.extraParams.z;

        vec4 c = pc.color;

        if (vLocalUV.y < bevel) {
            float t = 1.0 - (vLocalUV.y / bevel);
            c.rgb = mix(c.rgb, vec3(1.0), t * hiStrength);
        }
        else if (vLocalUV.y > 1.0 - bevel) {
            float t = (vLocalUV.y - (1.0 - bevel)) / bevel;
            c.rgb = mix(c.rgb, vec3(0.0), t * shStrength);
        }

        outColor = c;
        return;
    }

    // ─── mode 7: 金属枠 (v2: より太い + 立体感強化) ──
    if (mode == 7) {
        float border = pc.extraParams.x;
        if (vLocalUV.x > border && vLocalUV.x < 1.0 - border &&
            vLocalUV.y > border && vLocalUV.y < 1.0 - border) {
            discard;
        }

        vec4 c = pc.color;
        float topW = max(0.0, 1.0 - vLocalUV.y / border);
        float leftW = max(0.0, 1.0 - vLocalUV.x / border);
        float botW = max(0.0, 1.0 - (1.0 - vLocalUV.y) / border);
        float rightW = max(0.0, 1.0 - (1.0 - vLocalUV.x) / border);

        float hi = max(topW, leftW);
        float sh = max(botW, rightW);
        c.rgb = mix(c.rgb, vec3(1.0), hi * 0.40);    // 強化
        c.rgb = mix(c.rgb, vec3(0.0), sh * 0.55);    // 強化
        outColor = c;
        return;
    }

    // ─── mode 8: リベット (v2: 強めハイライト + より立体的) ──
    if (mode == 8) {
        if (dist > 0.5) discard;

        vec4 c = pc.color;
        vec2 hCenter = vec2(0.38, 0.32);
        float hDist = length(vLocalUV - hCenter);
        float hRadius = 0.25;
        if (hDist < hRadius) {
            float t = 1.0 - hDist / hRadius;
            t = pow(t, 1.5);
            c.rgb += vec3(t * 0.60);  // ハイライト強化
        }
        float edgeShade = smoothstep(0.25, 0.5, dist);
        c.rgb *= mix(1.0, 0.50, edgeShade);  // 縁シャドウ強化
        outColor = c;
        return;
    }

    // ─── mode 9: セグメント分割 fill + グラデーション (v2: 球状立体感) ─
    if (mode == 9) {
        int segCount = int(pc.extraParams.x);
        if (segCount < 1) segCount = 1;
        float gradFrac = pc.extraParams.y;
        int fullSegs = int(pc.extraParams.z);

        float segWidth = 1.0 / float(segCount);
        int segIdx = int(vLocalUV.x / segWidth);
        if (segIdx >= segCount) segIdx = segCount - 1;
        float localX = (vLocalUV.x - float(segIdx) * segWidth) / segWidth;

        bool isSeparator = (segIdx > 0 && localX < 0.025);

        bool isFilled = false;
        if (segIdx < fullSegs) {
            isFilled = true;
        } else if (segIdx == fullSegs) {
            isFilled = (localX <= gradFrac);
        }

        if (isSeparator) {
            outColor = vec4(0.05, 0.03, 0.02, pc.color.a);  // ほぼ黒
            return;
        }

        if (!isFilled) {
            // 空: 非常に暗い背景
            outColor = vec4(0.05, 0.03, 0.02, 0.95);
            return;
        }

        // 埋まっている: 球状立体感
        vec4 c = pc.color;
        
        // 垂直方向: 中央が明るく、 上下端が暗い
        float vDarken = abs(vLocalUV.y - 0.5) * 2.0;  // 0..1
        // ガンマ補正で中央を広く明るくする
        vDarken = pow(vDarken, 1.5);
        
        // 水平方向: 各セグメントの中央を明るく、 両端を暗くする (光沢感)
        float hDarken = abs(localX - 0.5) * 2.0;  // 0..1 (セグメント内)
        hDarken = pow(hDarken, 2.0);  // 中央を広く
        
        // 合成: 球状 = 縦と横の両方の暗化
        float totalDarken = max(vDarken * 0.55, hDarken * 0.35);
        c.rgb *= mix(1.0, 1.0 - totalDarken, 1.0);
        
        // 上端中央に強いハイライト (光沢の白いライン)
        if (vLocalUV.y < 0.25) {
            float vT = 1.0 - vLocalUV.y / 0.25;
            float hT = 1.0 - abs(localX - 0.5) * 2.0;
            float highlightStrength = vT * hT * 0.45;
            c.rgb = mix(c.rgb, vec3(1.0), highlightStrength);
        }
        
        outColor = c;
        return;
    }

    outColor = pc.color;
}

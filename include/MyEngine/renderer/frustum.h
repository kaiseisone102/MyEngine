// include/MyEngine/renderer/frustum.h
#pragma once
// =============================================================================
// frustum.h - Phase 1F: CPU frustum culling helper (header-only)
// =============================================================================
// Extracts the 6 view frustum planes from a view-projection matrix using the
// Gribb-Hartmann method, and tests bounding spheres for visibility.
//
// Reference: Gribb & Hartmann, "Fast Extraction of Viewing Frustum Planes
// from the World-View-Projection Matrix" (2001). The planes come directly
// from sums/differences of the matrix rows. We store them with inward-facing
// normals and normalize so that the plane equation gives true signed distance,
// which lets us test spheres by comparing against -radius.
//
// Usage:
//   Frustum fr;
//   fr.extract(proj * view);          // clip-space planes in world coords
//   if (fr.sphereVisible(center, r))  // keep this instance
//       ...
//
// NOTE on depth convention: this engine uses GLM_FORCE_DEPTH_ZERO_TO_ONE
// (Vulkan-style 0..1 clip depth). The near-plane row is therefore taken as
// just row3 (m[.][2]) rather than (row3 + row4) which is the GL -1..1 form.
// =============================================================================
#include <glm/glm.hpp>

struct Frustum {
    // Each plane: vec4(a, b, c, d) for a*x + b*y + c*z + d >= 0 when inside.
    glm::vec4 planes[6];

    // Extract planes from a view-projection matrix (column-major glm).
    // m[col][row]: row r of the matrix is (m[0][r], m[1][r], m[2][r], m[3][r]).
    void extract(const glm::mat4& m) {
        // Matrix rows (glm is column-major, so gather across columns).
        const glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
        const glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
        const glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
        const glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

        planes[0] = row3 + row0;  // left
        planes[1] = row3 - row0;  // right
        planes[2] = row3 + row1;  // bottom
        planes[3] = row3 - row1;  // top
        planes[4] = row2;         // near (0..1 depth: just row2)
        planes[5] = row3 - row2;  // far

        // Normalize each plane so d is a true signed distance.
        for (int i = 0; i < 6; ++i) {
            const float len = glm::length(glm::vec3(planes[i]));
            if (len > 0.0f) planes[i] /= len;
        }
    }

    // True if a sphere (center, radius) is at least partially inside the frustum.
    bool sphereVisible(const glm::vec3& center, float radius) const {
        for (int i = 0; i < 6; ++i) {
            const float dist = planes[i].x * center.x + planes[i].y * center.y +
                               planes[i].z * center.z + planes[i].w;
            if (dist < -radius) return false;  // fully outside this plane
        }
        return true;
    }
};

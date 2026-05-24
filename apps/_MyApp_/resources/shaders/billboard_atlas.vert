#version 410 core

// === vertex attributes — Mesh::CreatePlane VAO 의 layout ===
layout(location = 0) in vec3 a_position;   // (-0.5,-0.5,0) ~ (0.5,0.5,0) — XY 평면 quad
layout(location = 1) in vec3 a_normal;     // (0,0,1) — billboard 에서 미사용
layout(location = 2) in vec2 a_uv;         // (0,0) ~ (1,1) — quad UV, V=0 at bottom (GL convention)

// === uniforms ===
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_billboardCenter;        // 빌보드 월드 좌표 (중심)
uniform vec2 u_billboardSize;          // 빌보드 크기 (width, height)
uniform vec4 u_uvRect;                 // (uMin, vMin, uSize, vSize) — atlas 내부 sub-rect
uniform float u_flipX;                 // +1.0 또는 -1.0

// === out ===
out vec2 v_uv;

void main()
{
    // === Spherical billboard — view 행렬에서 right + up 모두 추출 ===
    // 카메라 row vector 가 곧 world space basis (orthonormal rotation 의 transpose).
    // Y-axis 고정 cylindrical 이 *실패* 한 이유: 카메라 pitch (위에서 내려다보기) 가 흡수 안 됨.
    vec3 cameraRight = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cameraUp    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    // Mesh::CreatePlane 은 XY 평면 (z=0) quad — position.xy 가 quad 의 local coord
    vec3 worldPos = u_billboardCenter
                  + cameraRight * a_position.x * u_billboardSize.x * u_flipX
                  + cameraUp    * a_position.y * u_billboardSize.y;

    v_uv = u_uvRect.xy + a_uv * u_uvRect.zw;
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
}

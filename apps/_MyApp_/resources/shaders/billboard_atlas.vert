#version 410 core

// === vertex attributes ===
layout(location = 0) in vec2 a_quad;   // (-0.5,-0.5) ~ (0.5,0.5) — quad 6 정점 (2 triangles)
layout(location = 1) in vec2 a_uv;     // (0,0) ~ (1,1)            — quad UV

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
    // View 행렬에서 카메라 right 벡터 추출 (Y축 고정 cylindrical billboard)
    vec3 cameraRight = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cameraUp    = vec3(0.0, 1.0, 0.0);    // Y축 고정

    vec3 worldPos = u_billboardCenter
                  + cameraRight * a_quad.x * u_billboardSize.x * u_flipX
                  + cameraUp    * a_quad.y * u_billboardSize.y;

    v_uv = u_uvRect.xy + a_uv * u_uvRect.zw;
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
}

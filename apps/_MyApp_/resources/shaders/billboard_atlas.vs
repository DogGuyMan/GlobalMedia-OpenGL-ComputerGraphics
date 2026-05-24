#version 410 core

// === vertex attributes — Mesh::CreatePlane VAO 의 layout (lighting.vs 컨벤션 일치) ===
layout(location = 0) in vec3 aPos; // (-0.5,-0.5,0) ~ (0.5,0.5,0) — XY 평면 quad
layout(location = 1) in vec3 aNormal; // (0,0,1) — billboard 에서 미사용
layout(location = 2) in vec2 aTexCoord; // (0,0) ~ (1,1) — quad UV, V=0 at bottom

// === uniforms — uModel 이 빌보드 center (Translate) + size (Scale) 흡수 ===
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec4 uUvRect; // (uMin, vMin, uSize, vSize) — atlas 내부 sub-rect
uniform float uFlipX; // +1.0 or -1.0

out vec2 vUv;

void main()
{
        // === Spherical billboard — view 행렬에서 right + up 모두 추출 ===
        // 카메라 row vector 가 곧 world space basis (orthonormal rotation 의 transpose).
        vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
        vec3 cameraUp = vec3(uView[0][1], uView[1][1], uView[2][1]);

        // === uModel 에서 빌보드 center + size 흡수 ===
        // center = origin 을 model space → world space 로 변환
        // sx/sy = uModel 의 column 0/1 길이 (Scale.x / Scale.y) — Transform.Scale 이 그대로 반영됨
        vec3 center = (uModel * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
        float sx = length(uModel[0].xyz);
        float sy = length(uModel[1].xyz);

        vec3 worldPos = center
                        + cameraRight * aPos.x * sx * uFlipX
                        + cameraUp * aPos.y * sy;

        vUv = uUvRect.xy + aTexCoord * uUvRect.zw;
        gl_Position = uProj * uView * vec4(worldPos, 1.0);
}

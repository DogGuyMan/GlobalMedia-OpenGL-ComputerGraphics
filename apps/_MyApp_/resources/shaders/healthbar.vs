#version 410 core

// Geometry::Plane XY quad: aPos (-0.5,-0.5,0)~(0.5,0.5,0), V=0 at bottom
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // 미사용
layout(location = 2) in vec2 aTexCoord; // (0,0)~(1,1)

uniform mat4 uModel;       // 액터 world transform (center + scale 흡수)
uniform mat4 uView;
uniform mat4 uProj;
uniform float uHeadOffset; // 화면상 '위'(cameraUp) 로 띄우는 거리 (월드 단위)

out vec2 vUv;

void main()
{
    // 카메라 right/up = view 행렬의 row vector (orthonormal rotation transpose)
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    // uModel 에서 center + size 흡수
    vec3 center = (uModel * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    float sx = length(uModel[0].xyz); // Transform.Scale.x → 바 가로폭
    float sy = length(uModel[1].xyz); // Transform.Scale.y → 바 세로높이

    // 머리 위 앵커: 화면상 cameraUp 방향으로 오프셋 (카메라각 독립)
    vec3 anchor   = center + cameraUp * uHeadOffset;
    vec3 worldPos = anchor + cameraRight * aPos.x * sx + cameraUp * aPos.y * sy;

    vUv = aTexCoord;
    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}

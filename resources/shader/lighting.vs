#version 410 core

// SP5 Lighting vertex shader — lighting.fs 의 in 변수 (vsNormal/vsPosition/vsTexCoord) 와 1:1 매칭.
// SJH::Vertex 레이아웃: 0=position(vec3), 1=normal(vec3), 2=texCoord(vec2).
// uniform 컨벤션은 simple.vs / postprocess.vs 와 일치 — uModel/uView/uProj 별개.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

void main()
{
    // world-space position — Phong 라이팅이 world space 에서 lightPos / viewPos 와 비교.
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vsPosition = worldPos.xyz;

    // world-space normal — 비균등 scale 대응을 위해 transpose(inverse(mat3(uModel))) 사용.
    // mat3(uModel) 직접 사용은 scale 균등 가정 — 일반화는 더 비싸지만 학습용으로 정확함이 우선.
    vsNormal = mat3(transpose(inverse(uModel))) * aNormal;

    vsTexCoord = aTexCoord;

    gl_Position = uProj * uView * worldPos;
}

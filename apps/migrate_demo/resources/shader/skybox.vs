#version 410 core

// migrate_demo Skybox VS — Pass::Kind::Skybox 셰이더 컨벤션.
// 핵심 트릭:
//  1. view 의 translation 성분 제거 (mat3 cast) — 카메라가 움직여도 skybox 가 *고정 배경* 처럼.
//  2. gl_Position = pos.xyww -> NDC z = w/w = 1.0 강제. cleared depth(1.0) 와 *동등* (LEQUAL 통과).
//     Pass::Kind::Skybox 가 자동으로 GL_LEQUAL + DepthWrite off + CullFront 적용.
//
// SJH::Vertex 레이아웃: 0=position(vec3), 1=normal, 2=texCoord — skybox 는 position 만 사용.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // unused — Vertex 레이아웃 호환만
layout(location = 2) in vec2 aTexCoord; // unused

uniform mat4 uView;
uniform mat4 uProj;

out vec3 vsLocalDir; // cube local-space direction — fragment 가 그라데이션/sampler 방향으로 사용

void main()
{
        vsLocalDir = aPos; // cube box 의 vertex 자체가 *중심에서 본 방향*

        // view translation 제거 — skybox 는 *카메라 응시 방향* 만 영향, 위치 무관.
        mat4 viewNoTrans = mat4(mat3(uView));

        vec4 pos = uProj * viewNoTrans * vec4(aPos, 1.0);
        gl_Position = pos.xyww; // * z = w -> perspective divide 후 NDC z = 1.0
}

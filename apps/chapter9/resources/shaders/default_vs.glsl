#version 410 core

// vertex color 제거 : 색은 Material에서 uniform으로 내려옴 (fragment 의존)
layout(location = 0) in vec4 positions;
layout(location = 1) in vec4 colors;
layout(location = 2) in vec2 uvCoords;

uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;

// Material uniforms : UV 변환
uniform vec2 uvOffset;
uniform vec2 uvRatio;

out VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} vs_out;

void main(void) {
        vec4 mPos = modelMat * positions;
        vec4 vPos = viewMat * mPos;
        vec4 pPos = projMat * vPos;

        gl_Position = pPos;

        vec2 rUv = vec2(
                        uvCoords.x * uvRatio.x,
                        uvCoords.y * uvRatio.y
                );
        vs_out.vsColor = colors;
        vs_out.vsTexCoord = rUv + uvOffset;
}

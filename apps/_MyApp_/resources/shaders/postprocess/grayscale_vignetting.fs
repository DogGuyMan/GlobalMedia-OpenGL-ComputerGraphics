#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;

// 그레이스케일 제어 (0.0 = 완전 무채색, 1.0 = 원본 색상)
uniform float uGrayscaleAmount = 1.0;

// 비네팅 제어 (0.0 = 효과 없음, 1.0 = 강한 비네팅)
uniform float uVignetteAmount = 0.5;
uniform vec3 uVignetteColor = vec3(0.0, 0.0, 0.0);

void main()
{
        // 1. 원본 씬(Scene) 색상 가져오기
        vec3 finalColor = texture(uScene, vUV).rgb;

        // 2. 그레이스케일(Grayscale) 효과 적용
        // 휘도(Luminance)를 계산하여 무채색 생성
        float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
        vec3 grayscaleColor = vec3(luminance);
        // uGrayscaleAmount 값에 따라 원본색과 무채색을 섞음
        finalColor = mix(grayscaleColor, finalColor, uGrayscaleAmount);

        // 3. 비네팅(Vignetting) 효과 적용
        // 화면 중앙(0.5, 0.5)으로부터의 거리 계산
        float dist = distance(vUV, vec2(0.5));
        // uVignetteAmount를 가우시안 함수의 강도로 사용하여 비네팅 계수 계산
        float intensity = uVignetteAmount * 20.0; // 0~1 범위를 적절한 강도(0~20)로 변환
        float vignetteFactor = exp(-dist * dist * intensity);
        // 비네팅 색상과 현재 색상을 섞음 (중앙은 원본, 외곽은 비네팅 색)
        finalColor = mix(uVignetteColor, finalColor, vignetteFactor);

        fragColor = vec4(finalColor, 1.0);
}

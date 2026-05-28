#version 410 core

in vec2 vUv;

uniform sampler2D uAtlas;
uniform vec4 uTint;

// 디졸브(사망) 효과 관련 Uniforms
uniform bool uEnableDissolve;
uniform sampler2D uDissolveTex;
uniform float uDissolveThreshold; // 0.0 ~ 1.0 (사라지는 정도)
uniform vec3 uDissolveOutlineColor; // 에미시브 색상
uniform float uDissolveOutlineThickness; // 아웃라인 두께

// 피격(Hit) 깜빡임 효과 관련 Uniforms
uniform bool uEnableHit;
uniform float uTime; // 깜빡임을 제어하기 위한 앱 구동 시간

out vec4 fragColor;

void main()
{
        vec4 c = texture(uAtlas, vUv);
        if (c.a < 0.01) discard; // alpha-test (sorting 회피, spec §10.2)

        // 1. 피격 효과 (빨강/하양 극단적 깜빡임)
        if (uEnableHit) {
                // sin 함수와 step을 이용해 중간 단계 없이 0.0과 1.0을 매우 빠르게(30.0배속) 오가도록 설정
                float blink = step(0.0, sin(uTime * 30.0));
                vec3 hitColor = mix(vec3(1.0, 1.0, 1.0), vec3(1.0, 0.0, 0.0), blink);
                c.rgb = hitColor;
        }

        vec4 finalColor = c * uTint;

        // 2. 디졸브 효과 (사망 시 사라짐)
        if (uEnableDissolve) {
                float dissolveValue = texture(uDissolveTex, vUv).r;
                if (dissolveValue < uDissolveThreshold) discard; // 임계치 미만은 픽셀 버림

                // 경계선(테두리 두께) 내부에 들어오는 픽셀은 에미시브(지정된 단색) 컬러 덮어쓰기
                float edge = step(dissolveValue, uDissolveThreshold + uDissolveOutlineThickness);
                finalColor.rgb = mix(finalColor.rgb, uDissolveOutlineColor, edge);
        }

        fragColor = finalColor;
}

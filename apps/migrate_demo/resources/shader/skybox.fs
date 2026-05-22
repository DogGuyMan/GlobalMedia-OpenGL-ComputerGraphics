#version 410 core

// migrate_demo Skybox FS — procedural gradient (cube map / equirectangular 인프라 미도입).
// 향후 samplerCube 또는 equirectangular sampler2D 로 확장 가능 (셰이더만 교체).

in vec3 vsLocalDir;
out vec4 fragColor;

// 학습용 단색 그라데이션 — 천정(zenith) ↔ 수평선(horizon) ↔ 지면(ground) 3구간.
// 향후 Material 의 properties bag 으로 색을 노출하면 챕터에서 SetVec3 로 조절 가능.
void main()
{
    vec3 dir = normalize(vsLocalDir);

    // 색상 팔레트 — 새벽 하늘 톤.
    const vec3 ZENITH  = vec3(0.20, 0.40, 0.75);   // 천정 — 진한 파랑
    const vec3 HORIZON = vec3(0.80, 0.85, 0.95);   // 수평선 — 밝은 회청색
    const vec3 GROUND  = vec3(0.25, 0.20, 0.18);   // 지면 — 어두운 갈색

    vec3 sky;
    if (dir.y >= 0.0)
    {
        // 위쪽 반구 — horizon → zenith
        float t = smoothstep(0.0, 1.0, dir.y);
        sky = mix(HORIZON, ZENITH, t);
    }
    else
    {
        // 아래쪽 반구 — horizon → ground
        float t = smoothstep(0.0, 1.0, -dir.y);
        sky = mix(HORIZON, GROUND, t);
    }

    fragColor = vec4(sky, 1.0);
}

#ifndef __ENGINE_LIGHTING_H__
#define __ENGINE_LIGHTING_H__

#include "GL/gl3w.h"
#include "vmath.h"

namespace Engine::Lighting
{
	// ────────────────────────────────────────────────────────────────────
	// 광원 데이터 컨테이너 — 자유 함수 (engine/uniforms.h Engine::Uniforms::*) 가
	// 셰이더 uniform 으로 푸시한다. 클래스 내부 멤버 함수 보유 X (Apply 류는 자유 함수).
	// ────────────────────────────────────────────────────────────────────

	// 공통 — 모든 modern 광원이 갖는 ambient/diffuse/specular 채널 (RGB 각각 vec3)
	struct LightChannels
	{
		vmath::vec3 ambient = vmath::vec3(0.1f, 0.1f, 0.1f);
		vmath::vec3 diffuse = vmath::vec3(0.8f, 0.8f, 0.8f);
		vmath::vec3 specular = vmath::vec3(1.0f, 1.0f, 1.0f);
	};

	// 방향광 — 무한히 먼 광원 (태양 등). 위치 없음 / 방향만.
	struct DirLight : LightChannels
	{
		vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f);
	};

	// 점광원 — 위치 + 거리 감쇠 (c1 = linear, c2 = quadratic, attenuation = 1/(1 + c1*d + c2*d^2))
	struct PointLight : LightChannels
	{
		vmath::vec3 position = vmath::vec3(0.0f, 0.0f, 0.0f);
		float c1 = 0.09f;
		float c2 = 0.032f;
	};

	// 스포트라이트 — 위치 + 방향 + 원뿔(cutOff/outerCutOff 코사인 값) + 거리 감쇠.
	struct SpotLight : LightChannels
	{
		vmath::vec3 position = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 direction = vmath::vec3(0.0f, -1.0f, 0.0f);
		float cutOff = 0.0f;
		float outerCutOff = 0.0f;
		float c1 = 0.09f;
		float c2 = 0.032f;
	};

	// 레거시 — chapter7 단일 Phong 광원 (texture_fs.glsl 셰이더 계약).
	// Color 단일 채널 (RGB intensity) + scalar AmbientStrength / SpecularStrength.
	// 위 LightChannels 식의 modern 광원과 별개의 셰이더 파라메트리제이션.
	struct Light
	{
		vmath::vec3 Position = vmath::vec3(2.0f, 2.0f, 2.0f);
		vmath::vec3 Color = vmath::vec3(1.0f, 1.0f, 1.0f);
		float AmbientStrength = 0.1f;
		float SpecularStrength = 0.5f;
		float Shininess = 32.0f;
	};
} // namespace Engine::Lighting

#endif // __ENGINE_LIGHTING_H__

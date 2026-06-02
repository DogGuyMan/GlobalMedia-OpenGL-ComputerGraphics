#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__

#include <vmath.h>

namespace SJH::Scene
{
	class Actor;
} // namespace SJH::Scene

namespace TopdownShooter::HUD
{
	/// @brief 머리 위 분절형 체력바 외형/배치 설정 (전부 기본값 보유).
	struct HealthBarConfig
	{
		vmath::vec4 fillColor      = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 채워진 조각 (레퍼런스 녹색)
		vmath::vec4 bgColor        = vmath::vec4(0.0f, 0.0f, 0.0f, 0.55f); // 빈 조각 트랙
		float       segmentCount   = 5.0f;
		float       segmentSpacing = 0.08f;
		float       headOffset     = 0.2f;                     // cameraUp 방향 머리 위 거리 (1.2→0.2, 1.0 하향)
		vmath::vec2 size           = vmath::vec2(1.2f, 0.18f); // 바 가로×세로
	};

	/// @brief target 의 Life(ILivable) 에 묶인 체력바 자식 Actor 를 생성·부착.
	///        Program/Mesh 는 ResourceRegistry 공유 캐시, Material 은 per-instance.
	///        target 에 ILivable 이 없거나 셰이더/메시 확보 실패 시 no-op (silent).
	void AttachHealthBar(SJH::Scene::Actor &target, const HealthBarConfig &cfg = {});
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__

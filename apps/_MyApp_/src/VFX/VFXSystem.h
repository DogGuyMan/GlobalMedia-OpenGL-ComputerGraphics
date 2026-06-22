/**
 * @file VFXSystem.h
 * @brief Effekseer 엔진 부트스트랩 + 매 프레임 Update/Draw 를 총괄하는 파티클 시스템 owner.
 *
 * @details
 *  ### 책임
 *  - @c Effekseer::ManagerRef 와 @c EffekseerRendererGL::RendererRef 를 *소유* (전체 1개).
 *  - 부트스트랩 - GL 디바이스/렌더러 생성 + 5종 SubRenderer + 4종 Loader 주입 (@c Init).
 *  - 매 프레임 시뮬레이션 전진 (@c Update) + 렌더 패스 (@c Draw).
 *  - 종료 시 Manager/Renderer Reset (@c Shutdown).
 *
 *  ### 비-책임
 *  - [X] 개별 이펙트 재생 lifecycle - @c EffekseerPlayable leaf 가 빌린 Manager 로 Play/Stop.
 *  - [X] .efk 에셋 로드/캐시 - @c SJH::ResourceRegistry / @c SJH::Effect 담당.
 *  - [X] 렌더 stage 통합 - @c ParticlePass 가 본 시스템의 Draw 를 호출.
 *
 *  ### 정통 매핑
 *  - Cocos2D `ParticleSystem` 엔진 계층 - *엔진/에셋/인스턴스* 3-tier 분리의 엔진 tier.
 *
 * @note 책임 3분할 - VFXSystem(엔진 owner) / SJH::Effect(에셋 캐시) / EffekseerPlayable(재생 인스턴스).
 *       자세한 통합 패턴은 doc/EffekseerAPI.md 0절 참조.
 */
#ifndef _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__
#define _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__

#include <Effekseer.h>
#include <EffekseerRendererGL.h>

namespace TopdownShooter::VFX
{
	/**
	 * @brief Effekseer Manager + EffekseerRendererGL Renderer owner (싱글턴급, Director 멤버).
	 * @details
	 *  Manager/Renderer 를 각각 1개씩만 보유한다 (파티클 1개당이 아니라 *전체 1개*).
	 *  @c Update(dt) 는 main update 단계, @c Draw(view, proj) 는 render 단계에서 호출.
	 *  복사 금지 - 단일 소유 시스템.
	 */
	class VFXSystem
	{
	  public:
		VFXSystem()  = default;
		~VFXSystem() = default;
		VFXSystem(const VFXSystem &)            = delete;
		VFXSystem &operator=(const VFXSystem &) = delete;

		/// @brief Effekseer 엔진 부트스트랩 - GL 디바이스/렌더러/매니저 생성 + SubRenderer 5종 + Loader 4종 주입.
		/// @param maxSprites 동시 스프라이트/인스턴스 풀 상한 (기본 8000).
		void Init(int maxSprites = 8000);

		/// @brief 파티클 시뮬레이션을 한 스텝 전진. @c manager->Update(dt * 60.0f).
		/// @details Effekseer Update 인자는 *초가 아니라 프레임* (60fps 기준 deltaFrame).
		///          그냥 dt(초)를 넘기면 파티클이 60배 느려진다.
		/// @param dt 직전 프레임 경과 시간(초). 내부에서 *60 하여 deltaFrame 으로 변환.
		void Update(float dt);

		/// @brief 한 프레임 파티클 렌더 패스 - view/proj 를 렌더러에 먹인 뒤 BeginRendering/Draw/EndRendering.
		/// @details 씬을 먼저 그린 뒤 호출해야 반투명 파티클이 위에 얹힌다.
		/// @param viewMat 16 float view 행렬 포인터 (SJH/vmath - memcpy 로 Effekseer Matrix44 에 복사).
		/// @param projMat 16 float projection 행렬 포인터.
		void Draw(const float *viewMat, const float *projMat);

		/// @brief Manager 먼저, Renderer 나중 순서로 Reset (Ref 타입 - 명시 delete 불요).
		void Shutdown();

		/// @brief 소유한 Manager 핸들 반환 - leaf Playable 이 ctor 로 빌릴 때 사용.
		::Effekseer::ManagerRef            GetManager()  { return mManager; }
		/// @brief 소유한 Renderer 핸들 반환.
		::EffekseerRendererGL::RendererRef GetRenderer() { return mRenderer; }

	  private:
		::EffekseerRendererGL::RendererRef mRenderer;  ///< GL 백엔드 렌더러 (Ref - 참조카운트 스마트 포인터).
		::Effekseer::ManagerRef            mManager;   ///< 파티클 매니저 (Ref - 참조카운트 스마트 포인터).
	};
}

#endif // _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__

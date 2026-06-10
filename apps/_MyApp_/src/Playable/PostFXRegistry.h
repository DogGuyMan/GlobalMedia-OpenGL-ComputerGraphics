/**
 * @file PostFXRegistry.h
 * @brief PostFX 패스 Material 의 이름->포인터 레지스트리 (Meyer's 싱글톤).
 *
 * @details
 *  ### 책임
 *  - 패스 이름을 키로 @c SJH::Material* 를 저장/조회하는 전역 비소유 관찰자 레지스트리.
 *  - startup 에서 @c Register 로 등록된 Material 을 이후 연출 트랙
 *    (@c PostFXTweenPlayable / @c HpGrayscalePostFX) 이 @c Material(name) 로 조회한다.
 *  ### 비-책임
 *  - [X] Material 소유권 -- PassComponent 가 소유. 본 레지스트리는 포인터만 보관.
 *  - [X] 셰이더 컴파일/패스 생성 -- @c RenderPipeline / PassComponent 담당.
 *  - [X] uniform 값 갱신 -- @c PostFXTweenPlayable / @c HpGrayscalePostFX 담당.
 *  ### 정통 매핑
 *  - Unity @c PostProcessingProfile 전역 룩업.
 *  - Cocos2D @c Director::getInstance() 패턴 (Meyer's 싱글톤 + 전역 서비스 로케이터).
 * @note Material 소유권은 PassComponent 에 있다. 레지스트리 파괴 시 포인터가 dangling 될 수 있으므로
 *       PassComponent 보다 수명이 짧게 유지되도록 한다 (Application 스코프에서 동일 수명).
 */
#ifndef __TOPDOWNSHOOTER_PLAYABLE_POSTFX_REGISTRY_H__
#define __TOPDOWNSHOOTER_PLAYABLE_POSTFX_REGISTRY_H__

#include <map>
#include <string>

// fwd — Material* 만 보유/반환하므로 전방 선언으로 충분 (헤더 경량 유지).
namespace SJH
{
	class Material;
}

namespace TopdownShooter::Playable
{
	/**
	 * @brief PostFX 패스 이름 -> @c SJH::Material* 비소유 레지스트리 (Meyer's 싱글톤).
	 * @details
	 *  main.cpp startup 에서 PassComponent 의 Material 을 @c Register 로 등록하고,
	 *  이후 연출 트랙(@c PostFXTweenPlayable / @c HpGrayscalePostFX) 이
	 *  @c Material(passName) 으로 조회해 @c Properties.Floats 에 직접 기록한다.
	 *
	 *  복사/대입 금지 -- Meyer's 싱글톤. Material 소유권은 PassComponent 에 있다.
	 */
	class PostFXRegistry
	{
	  public:
		/// @brief Meyer's 싱글톤 접근. GL context 활성 상태에서만 호출 유효.
		/// @return 싱글톤 레퍼런스.
		static PostFXRegistry &Get(); // Meyer's

		/// @brief @p passName 패스의 Material 포인터를 등록.
		/// @details nullptr 도 그대로 저장된다 -- 조회 측(@c PostFXTweenPlayable 등)이 nullptr 가드 책임.
		///          재등록(resize/재생성) 시 덮어쓰기 허용.
		/// @param passName 패스 식별 이름 (예: "grayscale_vignetting").
		/// @param mat      등록할 @c SJH::Material 포인터 (비소유). nullptr 허용.
		void Register(const std::string &passName, SJH::Material *mat);

		/// @brief 등록된 Material 포인터를 반환.
		/// @param passName 조회할 패스 이름.
		/// @return 등록된 @c SJH::Material* -- 등록되지 않았으면 nullptr.
		SJH::Material *Material(const std::string &passName) const;

	  private:
		PostFXRegistry()                                  = default;
		PostFXRegistry(const PostFXRegistry &)            = delete;
		PostFXRegistry &operator=(const PostFXRegistry &) = delete;

		std::map<std::string, SJH::Material *> mMats;  ///< passName -> Material* 비소유 매핑.
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_POSTFX_REGISTRY_H__

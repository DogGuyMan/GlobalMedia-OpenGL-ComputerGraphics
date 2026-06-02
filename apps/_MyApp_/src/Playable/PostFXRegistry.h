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
	/// @brief PostFX 패스 Material 의 이름→포인터 레지스트리 (Meyer's 싱글톤).
	///        main.cpp 가 startup 에서 PassComponent 의 Material 을 등록하고,
	///        이후 연출 트랙(hit-FX 의 PostFXTweenPlayable 등)이 셰이더 uniform 에 도달할 때
	///        FindPassMaterial 대신 PostFXRegistry::Get().Material(name) 로 조회한다.
	///        보유 Material 의 소유권은 PassComponent 에 있고 본 레지스트리는 비소유 관찰자.
	class PostFXRegistry
	{
	  public:
		static PostFXRegistry &Get(); // Meyer's

		/// @brief @p passName 패스의 Material 을 등록. nullptr 도 그대로 저장(조회 측이 가드).
		void Register(const std::string &passName, SJH::Material *mat);

		/// @brief 등록된 Material 반환 — 없으면 nullptr.
		SJH::Material *Material(const std::string &passName) const;

	  private:
		PostFXRegistry()                                  = default;
		PostFXRegistry(const PostFXRegistry &)            = delete;
		PostFXRegistry &operator=(const PostFXRegistry &) = delete;

		std::map<std::string, SJH::Material *> mMats;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_POSTFX_REGISTRY_H__

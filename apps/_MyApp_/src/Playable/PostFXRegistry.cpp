/**
 * @file PostFXRegistry.cpp
 * @brief PostFXRegistry Meyer's 싱글톤 구현 -- Register / Material 조회 2 메서드.
 *
 * @details
 *  - @c Get()     : static local instance 로 Meyer's 싱글톤 보장.
 *  - @c Register  : mMats[passName] = mat 덮어쓰기. 비소유 포인터 단순 저장.
 *  - @c Material  : std::map::find -- 없으면 nullptr 반환.
 */
#include "Playable/PostFXRegistry.h"

#include <string>

namespace TopdownShooter::Playable
{
	PostFXRegistry &PostFXRegistry::Get()
	{
		static PostFXRegistry instance;
		return instance;
	}

	void PostFXRegistry::Register(const std::string &passName, SJH::Material *mat)
	{
		mMats[passName] = mat; // 비소유 - 덮어쓰기 허용 (resize 시 재등록 등).
	}

	SJH::Material *PostFXRegistry::Material(const std::string &passName) const
	{
		auto it = mMats.find(passName);
		return (it == mMats.end()) ? nullptr : it->second;
	}
}

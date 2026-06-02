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
		mMats[passName] = mat; // 비소유 — 덮어쓰기 허용 (resize 시 재등록 등).
	}

	SJH::Material *PostFXRegistry::Material(const std::string &passName) const
	{
		auto it = mMats.find(passName);
		return (it == mMats.end()) ? nullptr : it->second;
	}
}

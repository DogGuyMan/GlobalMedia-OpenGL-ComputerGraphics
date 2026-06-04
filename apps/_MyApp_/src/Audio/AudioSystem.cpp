#include "AudioSystem.h"

#include <fmod/fmod_common.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vmath.h>

namespace TopdownShooter::Audio
{
	namespace
	{
		bool ck(FMOD_RESULT r, const char *what)
		{
			if (r != FMOD_OK)
			{
				spdlog::error("[AudioSystem] {} FMOD_RESULT={}", what, int(r));
				return false;
			}
			return true;
		}
	}

	void AudioSystem::Init()
	{
		if (!ck(::FMOD::Studio::System::create(&mStudioSystem), "Studio::System::create")) return;
		if (!ck(mStudioSystem->initialize(512,
		                                  FMOD_STUDIO_INIT_NORMAL,
		                                  FMOD_INIT_NORMAL,
		                                  nullptr),
		        "Studio::System::initialize")) return;
		if (!ck(mStudioSystem->getCoreSystem(&mSystem), "Studio::System::getCoreSystem")) return;
		spdlog::info("[AudioSystem] init OK (Studio + Core)");
	}

	void AudioSystem::Update(float /*dt*/)
	{
		if (mStudioSystem) mStudioSystem->update();   // FMOD 는 자체 dt 추적
	}

	void AudioSystem::Shutdown()
	{
		if (mStudioSystem)
		{
			for (auto *bank : mBanks) if (bank) bank->unload();
			mBanks.clear();
			mEventCache.clear();
			mStudioSystem->release();
			mStudioSystem = nullptr;
			mSystem       = nullptr;   // Studio 가 Core 소유 — getCoreSystem 으로 받은 ptr 은 별도 release 불요
		}
		spdlog::info("[AudioSystem] shutdown OK");
	}

	void AudioSystem::LoadBank(const std::string &path)
	{
		if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadBank] Studio 미초기화"); return; }
		::FMOD::Studio::Bank *bank = nullptr;
		if (!ck(mStudioSystem->loadBankFile(path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank),
		        ("loadBankFile " + path).c_str()))
			return;
		mBanks.push_back(bank);
		spdlog::info("[AudioSystem::LoadBank] OK {}", path);
	}

	::FMOD::Studio::EventDescription *AudioSystem::LoadEvent(const std::string &eventPath)
	{
		if (auto it = mEventCache.find(eventPath); it != mEventCache.end()) return it->second;
		if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadEvent] Studio 미초기화"); return nullptr; }

		::FMOD::Studio::EventDescription *desc = nullptr;
		FMOD_RESULT r = mStudioSystem->getEvent(eventPath.c_str(), &desc);
		if (r != FMOD_OK || !desc)
		{
			spdlog::warn("[AudioSystem::LoadEvent] 미존재 {} FMOD_RESULT={}", eventPath, int(r));
			return nullptr;
		}
		mEventCache[eventPath] = desc;
		return desc;
	}

	void AudioSystem::SetGlobalParameter(const std::string &name, float value)
	{
		if (mStudioSystem) mStudioSystem->setParameterByName(name.c_str(), value);
	}

	void AudioSystem::SetListener(const vmath::vec3 &pos, const vmath::vec3 &forward, const vmath::vec3 &up)
	{
		FMOD_3D_ATTRIBUTES attr = {};
		attr.position = {pos[0], pos[1], pos[2]};
		attr.velocity = {0.0f, 0.0f, 0.0f};
		attr.forward  = {forward[0], forward[1], forward[2]};
		attr.up       = {up[0], up[1], up[2]};
		if (mStudioSystem) mStudioSystem->setListenerAttributes(0, &attr);

		if (mSystem)
		{
			FMOD_VECTOR p = {pos[0], pos[1], pos[2]};
			FMOD_VECTOR v = {0.0f, 0.0f, 0.0f};
			FMOD_VECTOR f = {forward[0], forward[1], forward[2]};
			FMOD_VECTOR u = {up[0], up[1], up[2]};
			mSystem->set3DListenerAttributes(0, &p, &v, &f, &u);
		}
	}
}

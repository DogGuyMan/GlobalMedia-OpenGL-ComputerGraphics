/**
 * @file AudioSystem.cpp
 * @brief AudioSystem 구현 - Studio+Core 부트/셧다운, bank/event 로드, listener/parameter/bus 제어.
 *
 * @details 모든 FMOD 호출은 @c SJH_HAS_FMOD 가드 안에 있다. 가드 밖(FMOD 미빌드)에서는
 *  로그/no-op 로 컴파일되어 오디오만 비활성화된다 (FMOD 헤더 의존 없이 빌드 성립).
 * @note FMOD 헤더(@c fmod.hpp / @c fmod_studio.hpp)는 본 .cpp 안에서만 include - 헤더 의존 격리.
 */
#include "AudioSystem.h"

#ifdef SJH_HAS_FMOD
#include <fmod/fmod_common.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
#endif
#include <spdlog/spdlog.h>
#include <string>
#include <glm/glm.hpp>

namespace TopdownShooter::Audio
{
#ifdef SJH_HAS_FMOD
	namespace
	{
		/// @brief FMOD_RESULT 체크 헬퍼 - 실패 시 에러 로그만 남기고 false 반환 (graceful degradation).
		/// @param r    FMOD 호출 반환 코드.
		/// @param what 실패 지점 라벨 (로그용).
		/// @return @c FMOD_OK 면 true, 그 외 false.
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
#endif

	void AudioSystem::Init()
	{
#ifdef SJH_HAS_FMOD
		if (!ck(::FMOD::Studio::System::create(&mStudioSystem), "Studio::System::create")) return;
		if (!ck(mStudioSystem->initialize(512,
		                                  FMOD_STUDIO_INIT_NORMAL,
		                                  FMOD_INIT_NORMAL,
		                                  nullptr),
		        "Studio::System::initialize")) return;
		// Studio init 직후 1회 Core System 핸들 캐시 - 이후 FmodPlayable 의 createSound/playSound 에 사용.
		if (!ck(mStudioSystem->getCoreSystem(&mSystem), "Studio::System::getCoreSystem")) return;
		spdlog::info("[AudioSystem] init OK (Studio + Core)");
#else
		spdlog::warn("[AudioSystem] FMOD 미빌드 - 오디오 비활성 (no-op)");
#endif
	}

	void AudioSystem::Update(float /*dt*/)
	{
#ifdef SJH_HAS_FMOD
		if (mStudioSystem) mStudioSystem->update();   // FMOD 는 자체 dt 추적
#endif
	}

	void AudioSystem::Shutdown()
	{
#ifdef SJH_HAS_FMOD
		if (mStudioSystem)
		{
			for (auto *bank : mBanks) if (bank) bank->unload();   // bank 일괄 언로드 (FMODAPI.md sec.4)
			mBanks.clear();
			mEventCache.clear();                                  // EventDescription 은 bank 소유 - 포인터만 비움
			mStudioSystem->release();                             // Studio release 가 Core + 잔여 인스턴스까지 정리
			mStudioSystem = nullptr;
			mSystem       = nullptr;   // Studio 가 Core 소유 - getCoreSystem 으로 받은 ptr 은 별도 release 불요
		}
		spdlog::info("[AudioSystem] shutdown OK");
#endif
	}

	void AudioSystem::LoadBank(const std::string &path)
	{
#ifdef SJH_HAS_FMOD
		if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadBank] Studio 미초기화"); return; }
		::FMOD::Studio::Bank *bank = nullptr;
		if (!ck(mStudioSystem->loadBankFile(path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank),
		        ("loadBankFile " + path).c_str()))
			return;
		mBanks.push_back(bank);
		spdlog::info("[AudioSystem::LoadBank] OK {}", path);
#else
		(void)path;
#endif
	}

	::FMOD::Studio::EventDescription *AudioSystem::LoadEvent(const std::string &eventPath)
	{
		if (auto it = mEventCache.find(eventPath); it != mEventCache.end()) return it->second;
#ifdef SJH_HAS_FMOD
		if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadEvent] Studio 미초기화"); return nullptr; }

		::FMOD::Studio::EventDescription *desc = nullptr;
		// getEvent 는 오타/.strings.bank 미로드 시 ERR_EVENT_NOTFOUND 를 *조용히* 반환 (FMODAPI.md sec.11).
		FMOD_RESULT r = mStudioSystem->getEvent(eventPath.c_str(), &desc);
		if (r != FMOD_OK || !desc)
		{
			spdlog::warn("[AudioSystem::LoadEvent] 미존재 {} FMOD_RESULT={}", eventPath, int(r));
			return nullptr;
		}
		mEventCache[eventPath] = desc;
		return desc;
#else
		return nullptr;
#endif
	}

	void AudioSystem::SetGlobalParameter(const std::string &name, float value)
	{
#ifdef SJH_HAS_FMOD
		if (mStudioSystem) mStudioSystem->setParameterByName(name.c_str(), value);
#else
		(void)name;
		(void)value;
#endif
	}

	void AudioSystem::SetBusVolume(const std::string &busPath, float volume)
	{
#ifdef SJH_HAS_FMOD
		if (!mStudioSystem) return;
		::FMOD::Studio::Bus *bus = nullptr;
		if (mStudioSystem->getBus(busPath.c_str(), &bus) == FMOD_OK && bus)
			bus->setVolume(volume);
#else
		(void)busPath;
		(void)volume;
#endif
	}

	float AudioSystem::GetBusVolume(const std::string &busPath)
	{
#ifdef SJH_HAS_FMOD
		if (!mStudioSystem) return 1.0f;
		::FMOD::Studio::Bus *bus = nullptr;
		float vol = 1.0f, finalVol = 1.0f; // finalVol = 페이드/automation 반영 - 슬라이더엔 raw vol 사용
		if (mStudioSystem->getBus(busPath.c_str(), &bus) == FMOD_OK && bus)
			bus->getVolume(&vol, &finalVol);
		return vol;
#else
		(void)busPath;
		return 1.0f;
#endif
	}

	void AudioSystem::SetListener(const glm::vec3 &pos, const glm::vec3 &forward, const glm::vec3 &up)
	{
#ifdef SJH_HAS_FMOD
		// Studio listener - position/velocity/forward/up 4벡터를 FMOD_3D_ATTRIBUTES 한 struct 로 묶어 송신.
		FMOD_3D_ATTRIBUTES attr = {};
		attr.position = {pos[0], pos[1], pos[2]};
		attr.velocity = {0.0f, 0.0f, 0.0f};                 // velocity=0 -> 도플러 비활성 (탑다운엔 불필요)
		attr.forward  = {forward[0], forward[1], forward[2]};
		attr.up       = {up[0], up[1], up[2]};
		if (mStudioSystem) mStudioSystem->setListenerAttributes(0, &attr);   // listener index 0

		// Core listener - 같은 값을 FMOD_VECTOR 4개로 분리 전달 (Core playSound 사운드용, FMODAPI.md sec.14).
		if (mSystem)
		{
			FMOD_VECTOR p = {pos[0], pos[1], pos[2]};
			FMOD_VECTOR v = {0.0f, 0.0f, 0.0f};
			FMOD_VECTOR f = {forward[0], forward[1], forward[2]};
			FMOD_VECTOR u = {up[0], up[1], up[2]};
			mSystem->set3DListenerAttributes(0, &p, &v, &f, &u);
		}
#else
		(void)pos;
		(void)forward;
		(void)up;
#endif
	}
}

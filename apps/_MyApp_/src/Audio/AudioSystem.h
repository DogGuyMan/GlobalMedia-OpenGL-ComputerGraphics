#ifndef _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__
#define _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__

#include <string>
#include <unordered_map>
#include <vector>
#include <vmath.h>

// fwd — FMOD 헤더는 .cpp 안에서만
namespace FMOD
{
	class System;
	namespace Studio
	{
		class System;
		class Bank;
		class EventDescription;
	}
}

namespace TopdownShooter::Audio
{
	/// @brief FMOD Core (System) + Studio (System + Bank + EventDescription 캐시) owner.
	/// @details Director 멤버로 거주. main 의 startup 에서 Init, render 마다 Update(dt), shutdown 에서 Shutdown.
	///   Studio::System::getCoreSystem 으로 Core System ptr 도 함께 얻어 FmodPlayable 에서 사용.
	class AudioSystem
	{
	  public:
		AudioSystem()  = default;
		~AudioSystem() = default;
		AudioSystem(const AudioSystem &)            = delete;
		AudioSystem &operator=(const AudioSystem &) = delete;

		void Init();
		void Update(float dt);
		void Shutdown();

		::FMOD::System         *GetSystem()       { return mSystem; }
		::FMOD::Studio::System *GetStudioSystem() { return mStudioSystem; }

		/// @brief listener 위치/방향 갱신 — render 마다 카메라 Transform 으로 호출 (velocity=0 -> doppler 없음).
		void SetListener(const vmath::vec3 &pos,
		                 const vmath::vec3 &forward = vmath::vec3(0.0f, 0.0f, -1.0f),
		                 const vmath::vec3 &up      = vmath::vec3(0.0f, 1.0f, 0.0f));

		/// @brief @p path (예: "resources/banks/Master.bank") 의 bank 를 *로드*.
		void LoadBank(const std::string &path);

		/// @brief @p eventPath (예: "event:/BGM") 의 EventDescription 을 *조회/캐시* 후 반환.
		///        Studio bank 가 사전에 LoadBank 로 로드된 상태여야 한다. 못 찾으면 nullptr.
		::FMOD::Studio::EventDescription *LoadEvent(const std::string &eventPath);

		/// @brief Studio global parameter 설정 (System 스코프 — 예: "Health"). 미초기화 시 no-op.
		void SetGlobalParameter(const std::string &name, float value);

	  private:
		::FMOD::System         *mSystem       = nullptr;
		::FMOD::Studio::System *mStudioSystem = nullptr;
		std::vector<::FMOD::Studio::Bank *> mBanks;
		std::unordered_map<std::string, ::FMOD::Studio::EventDescription *> mEventCache;
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__

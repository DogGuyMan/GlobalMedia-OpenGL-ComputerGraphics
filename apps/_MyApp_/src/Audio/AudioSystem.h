/**
 * @file AudioSystem.h
 * @brief FMOD Studio + Core 의 single owner - System/Bank/EventDescription 캐시 + listener 갱신.
 *
 * @details
 *  ### 책임
 *  - FMOD Studio System 생성/초기화 + @c getCoreSystem 으로 Core System 핸들 동시 보유 (FMODAPI.md sec.12).
 *  - @c .bank 파일 로드 (@c LoadBank) + EventDescription 경로 lookup 캐시 (@c LoadEvent).
 *  - 매 프레임 @c Update 에서 Studio @c update() 구동 (Core update 는 Studio 가 내부 구동 - 별도 호출 안 함).
 *  - 카메라 Transform 으로 3D listener 갱신 (@c SetListener) + global parameter / bus 볼륨 제어.
 *
 *  ### 비-책임
 *  - [X] 실제 재생 - leaf Playable (@c FmodPlayable Core / @c FmodStudioPlayable Studio) 담당.
 *  - [X] Core @c Sound 캐시 - @c SJH::ResourceRegistry (CreateSound/FindSound) 담당.
 *  - [X] Core System @c release() - Studio 가 Core 를 소유하므로 Studio release 가 함께 정리.
 *
 *  ### 정통 매핑
 *  - Unreal `FAudioDevice` - 오디오 엔진 진입점 (System 보유 + listener 갱신).
 *  - Cocos2D `AudioEngine` - 사운드 로드/재생 facade.
 *
 * @note 모든 FMOD 호출은 @c SJH_HAS_FMOD 컴파일 가드 안에 있다. FMOD 미빌드 환경에서는
 *       전 메서드가 no-op 으로 컴파일되어 (헤더 의존 없이) 오디오만 비활성화된다.
 * @note FMOD 헤더는 @c .cpp 안에서만 include - 헤더는 전방 선언만 노출해 의존을 격리.
 */
#ifndef _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__
#define _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__

#include <string>
#include <unordered_map>
#include <vector>
#include <vmath.h>

// fwd - FMOD 헤더는 .cpp 안에서만
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
	/**
	 * @brief FMOD Core (System) + Studio (System + Bank + EventDescription 캐시) owner.
	 * @details
	 *  Director 멤버로 거주. main 의 startup 에서 @c Init, render 마다 @c Update(dt),
	 *  shutdown 에서 @c Shutdown 호출. @c Studio::System::getCoreSystem 으로 Core System ptr 도
	 *  함께 얻어 @c FmodPlayable 의 Core 직접 재생(@c playSound)에 사용 (FMODAPI.md sec.12).
	 *
	 *  복사 금지 (단일 owner) - 이동도 미정의. FMOD 자원 lifetime 을 @c Init / @c Shutdown 쌍이 관리.
	 */
	class AudioSystem
	{
	  public:
		AudioSystem()  = default;
		~AudioSystem() = default;
		AudioSystem(const AudioSystem &)            = delete;
		AudioSystem &operator=(const AudioSystem &) = delete;

		/// @brief Studio System 생성 + 초기화 후 @c getCoreSystem 으로 Core System 핸들 캐시.
		/// @details @c maxchannels=512, @c FMOD_STUDIO_INIT_NORMAL + @c FMOD_INIT_NORMAL.
		///          한 단계라도 실패하면 즉시 return (오디오 비활성, 게임은 계속 진행).
		///          FMOD 미빌드 시 warn 로그만 남기는 no-op.
		void Init();
		/// @brief 매 프레임 Studio @c update() 1회 - 누락 시 사운드가 끊기거나 안 들림 (FMODAPI.md sec.3).
		/// @param dt 미사용 - FMOD 가 자체 dt 를 추적하므로 인자는 전달만 받고 무시.
		void Update(float dt);
		/// @brief 모든 bank 언로드 + 캐시 clear + Studio @c release() (Core 까지 함께 정리).
		/// @details Core @c mSystem 은 Studio 소유 핸들이라 별도 release 없이 nullify 만.
		void Shutdown();

		/// @brief Core System 핸들 반환 (@c FmodPlayable 가 @c playSound 에 사용). 미초기화 시 nullptr.
		/// @return Studio 가 소유한 Core System 포인터 - 빌려 쓰기만, release 금지.
		::FMOD::System         *GetSystem()       { return mSystem; }
		/// @brief Studio System 핸들 반환. 미초기화 시 nullptr.
		::FMOD::Studio::System *GetStudioSystem() { return mStudioSystem; }

		/// @brief listener 위치/방향 갱신 - render 마다 카메라 Transform 으로 호출 (velocity=0 -> doppler 없음).
		/// @details Studio (@c setListenerAttributes) + Core (@c set3DListenerAttributes) listener 를 둘 다 갱신.
		/// @param pos     listener(카메라) 월드 좌표.
		/// @param forward listener 바라보는 방향 (단위 벡터 권장).
		/// @param up      listener up 벡터 (forward 와 수직 + 단위 길이 권장).
		void SetListener(const vmath::vec3 &pos,
		                 const vmath::vec3 &forward = vmath::vec3(0.0f, 0.0f, -1.0f),
		                 const vmath::vec3 &up      = vmath::vec3(0.0f, 1.0f, 0.0f));

		/// @brief @p path (예: "resources/banks/Master.bank") 의 bank 를 *로드*.
		/// @details 로드된 bank 는 @c mBanks 에 보관되어 @c Shutdown 시 일괄 언로드. 실패 시 push 안 함.
		/// @param path bank 파일 경로 (실행 파일 기준 상대경로).
		void LoadBank(const std::string &path);

		/// @brief @p eventPath (예: "event:/BGM") 의 EventDescription 을 *조회/캐시* 후 반환.
		///        Studio bank 가 사전에 LoadBank 로 로드된 상태여야 한다. 못 찾으면 nullptr.
		/// @details 캐시 hit 시 즉시 반환. EventDescription 은 bank 소유라 release 하지 않는다 (FMODAPI.md sec.5).
		///          @c .strings.bank 누락/경로 오타 시 조용히 nullptr - 호출부가 별도 가드 없이 안전.
		/// @param eventPath "event:/<name>" 형식 경로 (대소문자/공백 정확히 일치해야 함).
		/// @return 캐시된 EventDescription 포인터, 미존재 시 nullptr.
		::FMOD::Studio::EventDescription *LoadEvent(const std::string &eventPath);

		/// @brief Studio global parameter 설정 (System 스코프 - 예: "Health"). 미초기화 시 no-op.
		/// @param name  global parameter 이름.
		/// @param value 설정할 값.
		void SetGlobalParameter(const std::string &name, float value);

		/// @brief bus (예: "bus:/BGM Bus") 볼륨 설정 [0..1]. 미초기화/미존재 시 no-op.
		/// @param busPath "bus:/<name>" 형식 경로 (공백 포함 이름 그대로).
		/// @param volume  0.0(무음)~1.0(원본). 1.0 초과는 부스트 (clipping 위험).
		void  SetBusVolume(const std::string &busPath, float volume);
		/// @brief bus 현재 볼륨 조회 (슬라이더 초기값). 미초기화/미존재 시 1.0.
		/// @param busPath "bus:/<name>" 형식 경로.
		/// @return raw 볼륨 값 (페이드/automation 반영 전 - 슬라이더 표시용).
		float GetBusVolume(const std::string &busPath);

	  private:
		::FMOD::System         *mSystem       = nullptr;  ///< Core System - Studio 소유 빌림 핸들 (release 금지).
		::FMOD::Studio::System *mStudioSystem = nullptr;  ///< Studio System - 본 클래스가 lifetime owner.
		std::vector<::FMOD::Studio::Bank *> mBanks;        ///< 로드된 bank 목록 - Shutdown 시 일괄 언로드.
		std::unordered_map<std::string, ::FMOD::Studio::EventDescription *> mEventCache;  ///< eventPath -> EventDescription 캐시 (bank 소유, release X).
	};
}

#endif // _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__

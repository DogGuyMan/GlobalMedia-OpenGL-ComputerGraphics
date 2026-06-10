/**
 * @file Constants.h
 * @brief VFX 이펙트 에셋 카탈로그 - registry 키와 .efk 경로(char16_t) 정적 테이블.
 *
 * @details
 *  ### 책임
 *  - @c EffectAsset 자료형 정의 (캐시 키 + .efk 파일 경로 한 쌍).
 *  - 게임이 로드하는 모든 Effekseer 에셋의 단일 카탈로그 (muzzle + 테스트 드롭다운).
 *
 *  ### 비-책임
 *  - [X] 에셋 로드 - @c SJH::ResourceRegistry::CreateEffect 가 경로를 받아 수행.
 *  - [X] 재생 - @c EffekseerPlayable leaf Playable 담당.
 *
 * @note 경로 리터럴은 반드시 @c u"..." (char16_t / UTF-16) - Effekseer EFK_CHAR 규약.
 *       일반 @c "..." 는 컴파일 에러.
 * @note .efk 는 1.7 에디터로 export 해야 한다. 포맷 버전이 런타임 SupportBinaryVersion(1710)
 *       보다 높으면 @c Effect::Create 가 *조용히* nullptr 을 돌려준다 (예외/로그 없음).
 */
#ifndef _TOPDOWNSHOOTER_VFX_CONSTANTS__
#define _TOPDOWNSHOOTER_VFX_CONSTANTS__

namespace TopdownShooter::VFX
{
	/// @brief Effekseer 이펙트 자원 1개 - registry 키 + .efk 경로(char16_t).
	struct EffectAsset
	{
		const char     *key;   ///< ResourceRegistry 캐시 키 (영문 식별자).
		const char16_t *path;  ///< .efk 파일 경로. char16_t 필수 (EFK_CHAR 규약).
	};

	/// @brief 단발 muzzle 이펙트 (좌클릭 발사 연출 - startup 에서 CreateEffect).
	const EffectAsset MUZZLE_EFFECT = {"muzzle", u"resources/vfx/distortion.efk"};

	/// @brief VFX 테스트 드롭다운용 이펙트 목록 - 1.7 에디터 export (.efk 포맷 1710, 런타임 SupportBinaryVersion 과 일치).
	/// @note .efk 가 참조하는 텍스처는 .efk 파일이 있는 디렉토리 기준 상대경로로 해석된다.
	///       resources/vfx/ 레이아웃을 에디터 export 기준과 맞추지 않으면 텍스처가 깨진다.
	const EffectAsset TEST_EFFECTS[] = {
	    {"dust", u"resources/vfx/170/01_Pierre01/Dust.efk"},
	    {"hit", u"resources/vfx/170/03_Hanmado01/Effect/Signlehit.efk"},
	    {"laser", u"resources/vfx/170/01_AndrewFM01/blue_laser.efk"},
	    {"orbital_background", u"resources/vfx/170/01_AndrewFM01/orbital_background.efk"},
	    {"slash", u"resources/vfx/170/Slash/slash_weak.efk"},
	    {"summon", u"resources/vfx/170/01_NextSoft01/summon.efk"},
	    {"gunshoot", u"resources/vfx/170/gunshoot/gunshoot.efk"},
	};
} // namespace TopdownShooter::VFX

#endif //_TOPDOWNSHOOTER_VFX_CONSTANTS__

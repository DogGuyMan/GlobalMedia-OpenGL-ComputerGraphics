#ifndef _TOPDOWNSHOOTER_VFX_CONSTANTS__
#define _TOPDOWNSHOOTER_VFX_CONSTANTS__

namespace TopdownShooter::VFX
{
	/// @brief Effekseer 이펙트 자원 1개 — registry 키 + .efk 경로(char16_t).
	struct EffectAsset
	{
		const char     *key;
		const char16_t *path;
	};

	// 단발 muzzle 이펙트 (좌클릭 발사 연출 — startup 에서 CreateEffect).
	const EffectAsset MUZZLE_EFFECT = {"muzzle", u"resources/vfx/distortion.efk"};

	// VFX 테스트 드롭다운 — 1.7 에디터 export (.efk 포맷 1710 — 런타임 SupportBinaryVersion 과 일치).
	// ※ .efk 가 참조하는 텍스처가 resources/vfx/ 아래에 있어야 실제로 보인다.
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

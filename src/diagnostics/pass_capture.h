/**
 * @file pass_capture.h
 * @brief PassIterator 변형별 골든 캡처 runner. 각 변형 = 패스 Enabled 오버라이드 +
 *        DebugPassIndex + World queue-range + 캡처 대상(backbuffer/worldFbo).
 * @note diagnostics = cycle-exempt 예외 모듈 (CLAUDE.md): SJH::render(PassIterator/WorldPass)+buffer
 *       상향 링크. render->diagnostics 와 순환 - CMake 가 STATIC lib 순환을 link-line 반복으로 해소.
 */
#ifndef __SJH_PASS_CAPTURE_H__
#define __SJH_PASS_CAPTURE_H__

#include <climits>
#include <string>
#include <utility>
#include <vector>

namespace SJH
{
	class PassIterator;
	class DeviceContext;
	class RenderTarget;
	class WorldPass;
}

namespace SJH::Diagnostics
{
	/// @brief 캡처 변형 1개 명세(순수 데이터). 상태 저장/복원은 runner 가 책임.
	struct CaptureVariant
	{
		std::string                               outName;      ///< 출력 파일명(확장자 제외).
		std::vector<std::pair<std::string, bool>> passOverride; ///< {passKey, Enabled} 오버라이드.
		int                                       stopAtPass = -1;        ///< DebugPassIndex (-1=전체).
		int                                       worldQueueMin = INT_MIN; ///< World 큐 필터 [min,max) 하한.
		int                                       worldQueueMax = INT_MAX; ///< 상한.
		enum Target
		{
			Backbuffer,
			WorldFbo
		} target = Backbuffer;
	};

	/// @brief 변형 목록을 순회하며 각 캡처. 각 변형 전후로 상태 저장/복원.
	/// @param it         Pass 반복자(등록/실행 소유).
	/// @param rec        GL facade(DeviceContext).
	/// @param backbuffer 최종 출력(Execute 인자).
	/// @param worldFbo   World FBO(RenderTexture) - WorldFbo target 캡처 대상. nullptr 이면 그 변형 skip.
	/// @param worldPass  World 큐 필터 대상. nullptr 이면 큐 필터 무시.
	/// @param variants   캡처 변형 목록.
	/// @param outDir     출력 디렉토리(호출자가 생성 보장).
	/// @param w,h        프레임 크기.
	void RunCaptureVariants(PassIterator &it, DeviceContext &rec, RenderTarget &backbuffer,
	                        RenderTarget *worldFbo, WorldPass *worldPass,
	                        const std::vector<CaptureVariant> &variants,
	                        const std::string &outDir, int w, int h);
}

#endif // __SJH_PASS_CAPTURE_H__

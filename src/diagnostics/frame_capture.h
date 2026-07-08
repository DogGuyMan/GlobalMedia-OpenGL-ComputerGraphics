/**
 * @file frame_capture.h
 * @brief 프레임 readback -> PNG (골든 캡처). glReadPixels + 수직 flip + stb_write.
 * @note diagnostics = cycle-exempt 예외 모듈 (CLAUDE.md). GL 호출은 .cpp 내부만.
 *       백엔드 무관 시그니처(경로/크기) -- 후일 Metal readback 교체점.
 */
#ifndef __SJH_FRAME_CAPTURE_H__
#define __SJH_FRAME_CAPTURE_H__

#include <string>

namespace SJH
{
	class RenderTarget;
}

namespace SJH::Diagnostics
{
	/// @brief 백버퍼(FBO 0)를 PNG 로 저장. @return 성공 여부.
	bool CaptureBackbufferToPng(const std::string &path, int w, int h);

	/// @brief 임의 RenderTarget(FBO)을 Bind 후 PNG 로 저장 (raw World FBO 등, PostFX 전).
	bool CaptureTargetToPng(RenderTarget &target, const std::string &path, int w, int h);
}

#endif // __SJH_FRAME_CAPTURE_H__

/**
 * @file frame_capture.cpp
 * @brief frame_capture 구현. STB_IMAGE_WRITE_IMPLEMENTATION 을 이 TU 에서만 정의
 *        (image.cpp 의 STB_IMAGE_IMPLEMENTATION 과 별개 매크로 - 충돌 없음).
 *        구 apps/_MyApp_/src/Capture/golden_capture.cpp 로직을 엔진 이관한 것.
 */

// stb_image_write 구현체를 이 TU 에서 단 한 번만 생성.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "diagnostics/frame_capture.h"
#include "buffer/render_target.h"

#include <GL/gl3w.h>

#include <cstring> // std::memcpy
#include <spdlog/spdlog.h>
#include <vector>

namespace SJH::Diagnostics
{
	namespace
	{
		/// @brief 현재 READ FBO 에서 [w,h] RGBA 를 읽어 수직 flip 후 PNG 저장.
		bool ReadFlipWrite(const std::string &path, int w, int h)
		{
			if (w <= 0 || h <= 0)
			{
				spdlog::error("[FrameCapture] 유효하지 않은 크기: {}x{}", w, h);
				return false;
			}

			// GL 원점은 좌하단 -> PNG 좌상단과 행 순서가 반대. 4바이트 RGBA.
			const int stride = w * 4;
			std::vector<unsigned char> raw(static_cast<std::size_t>(h) * stride);
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

			const GLenum err = glGetError();
			if (err != GL_NO_ERROR)
			{
				spdlog::error("[FrameCapture] glReadPixels 실패 0x{:X}", err);
				return false;
			}

			// 상하 반전 방지: GL 프레임버퍼 원점=좌하단, PNG 원점=좌상단 이라 행 순서가 반대다.
			//   glReadPixels 결과를 그대로 저장하면 이미지가 위아래로 뒤집힌다 - 행을 역순 복사해 바로 세운다.
			//   (육안 검증이 똑바로 보이고, flip 상태로 커밋된 기존 골든과 bit-일치하기 위해 필수.)
			std::vector<unsigned char> flipped(static_cast<std::size_t>(h) * stride);
			for (int row = 0; row < h; ++row)
			{
				std::memcpy(flipped.data() + row * stride,
				            raw.data() + (h - 1 - row) * stride,
				            static_cast<std::size_t>(stride));
			}

			if (stbi_write_png(path.c_str(), w, h, 4, flipped.data(), stride) == 0)
			{
				spdlog::error("[FrameCapture] stbi_write_png 실패: {}", path);
				return false;
			}

			spdlog::info("[FrameCapture] PNG 저장: {} ({}x{})", path, w, h);
			return true;
		}
	} // namespace

	bool CaptureBackbufferToPng(const std::string &path, int w, int h)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0); // 백버퍼(FBO 0) 명시 바인드
		return ReadFlipWrite(path, w, h);
	}

	bool CaptureTargetToPng(RenderTarget &target, const std::string &path, int w, int h)
	{
		target.Bind(); // glBindFramebuffer(target FBO) + glViewport
		return ReadFlipWrite(path, w, h);
	}
} // namespace SJH::Diagnostics

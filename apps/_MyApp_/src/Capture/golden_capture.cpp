/**
 * @file golden_capture.cpp
 * @brief 골든 이미지 캡처 구현 -- glReadPixels + 행 flip + stb_image_write PNG 저장.
 *
 * STB_IMAGE_WRITE_IMPLEMENTATION 을 이 TU(번역 단위)에서만 정의.
 * src/resource_registry/image.cpp 의 STB_IMAGE_IMPLEMENTATION 과는 별개 매크로 -- 충돌 없음.
 */

// stb_image_write 구현체를 이 TU 에서 단 한 번만 생성.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <GL/gl3w.h>

#include <cstring>  // std::memcpy
#include <spdlog/spdlog.h>
#include <vector>

#include "Capture/golden_capture.h"

namespace TopdownShooter::Capture
{

bool CaptureBackbufferToPng(const std::string &path, int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        spdlog::error("[GoldenCapture] 유효하지 않은 프레임버퍼 크기: {}x{}", w, h);
        return false;
    }

    // ── 1. glReadPixels 로 백버퍼(FBO 0)에서 RGBA 픽셀 읽기 ───────────────────
    // GL 원점은 좌하단 -> PNG/이미지 좌상단과 행 순서가 반대. 4바이트 RGBA.
    const int stride = w * 4;
    std::vector<unsigned char> raw(static_cast<std::size_t>(h) * stride);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 백버퍼(FBO 0) 명시 바인드
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        spdlog::error("[GoldenCapture] glReadPixels 실패, GL 에러: 0x{:X}", err);
        return false;
    }

    // ── 2. 행 수직 flip (GL 좌하단 -> PNG 좌상단) ────────────────────────────
    std::vector<unsigned char> flipped(static_cast<std::size_t>(h) * stride);
    for (int row = 0; row < h; ++row)
    {
        const unsigned char *src = raw.data() + (h - 1 - row) * stride;
        unsigned char       *dst = flipped.data() + row * stride;
        std::memcpy(dst, src, static_cast<std::size_t>(stride));
    }

    // ── 3. PNG 파일로 저장 ───────────────────────────────────────────────────
    const int ret = stbi_write_png(path.c_str(), w, h, 4, flipped.data(), stride);
    if (ret == 0)
    {
        spdlog::error("[GoldenCapture] stbi_write_png 실패: {}", path);
        return false;
    }

    spdlog::info("[GoldenCapture] PNG 저장 완료: {} ({}x{})", path, w, h);
    return true;
}

} // namespace TopdownShooter::Capture

/**
 * @file golden_capture.h
 * @brief 골든 이미지 캡처 헬퍼 -- 백버퍼를 PNG 파일로 저장.
 *
 * 사용법:
 *   CaptureBackbufferToPng("test/golden/golden_full.png", width, height);
 *
 * 주의:
 *   - glReadPixels 로 백버퍼(FBO 0) 를 읽고, 상하 flip 후 stb_image_write 로 PNG 저장.
 *   - 이 함수는 GL 컨텍스트가 유효한 상태(render 루프 내)에서만 호출해야 한다.
 */

#ifndef __GOLDEN_CAPTURE_H__
#define __GOLDEN_CAPTURE_H__

#include <string>

namespace TopdownShooter::Capture
{

/**
 * @brief 현재 백버퍼(FBO 0)를 PNG 파일로 저장한다.
 *
 * @param path  저장할 PNG 파일 경로 (디렉토리는 호출 전에 존재해야 함).
 * @param w     프레임버퍼 너비 (픽셀, glfwGetFramebufferSize 값).
 * @param h     프레임버퍼 높이 (픽셀, glfwGetFramebufferSize 값).
 * @return      저장 성공이면 true, 실패이면 false.
 */
bool CaptureBackbufferToPng(const std::string &path, int w, int h);

} // namespace TopdownShooter::Capture

#endif // __GOLDEN_CAPTURE_H__

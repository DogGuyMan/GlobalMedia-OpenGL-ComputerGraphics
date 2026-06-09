/**
 * @file common.cpp
 * @brief @ref common.h 에 선언된 비-인라인 함수 구현 - @c LoadTextFile, @c CrossPlatformDir.
 *
 * @details
 *  ### 책임
 *  - @c LoadTextFile : @c std::ifstream 전체 읽기 -> @c std::optional<std::string> 반환.
 *    실패 시 @c spdlog::error 로그 후 @c std::nullopt.
 *  - @c CrossPlatformDir : macOS @c _NSGetExecutablePath -> @c dirname -> @c chdir 로
 *    GLFW 3.0.4 의 @c _GLFW_USE_CHDIR 부수효과(번들 Resources CWD 변경)를 실행파일 디렉토리로 복원.
 *    macOS 외(@c #ifndef __APPLE__)는 아무 작업 없음(no-op).
 *
 *  ### 비-책임
 *  - [X] GL 의존 없음 - 순수 파일시스템 + 플랫폼 POSIX.
 */
#include "common.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <cstdint>
#include <cstring>
#include <libgen.h>      // dirname
#include <limits.h>      // PATH_MAX
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <unistd.h>      // chdir
#endif

namespace SJH {
	std::optional<std::string> LoadTextFile(const std::string& filename) {
		// ifstream 으로 전체 내용을 stringstream 에 흘려넣는 표준 패턴
		std::ifstream fin(filename);
		if(!fin.is_open()) {
			spdlog::error("failed to open file: {}", filename);
			return {};
		}
		std::stringstream text;
		text << fin.rdbuf();
		return text.str();
	}

	void CrossPlatformDir()
	{
	#ifdef __APPLE__
		char exePath[PATH_MAX] = {};
		uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
		if (_NSGetExecutablePath(exePath, &exeSize) == 0)
		{
			char exePathCopy[PATH_MAX] = {};
			std::strncpy(exePathCopy, exePath, PATH_MAX - 1);
			chdir(dirname(exePathCopy));
		}
	#endif
	}
}

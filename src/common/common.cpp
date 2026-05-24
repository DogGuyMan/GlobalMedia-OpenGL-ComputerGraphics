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

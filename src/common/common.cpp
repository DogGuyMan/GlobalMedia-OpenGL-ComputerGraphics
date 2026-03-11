#include <common/common.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace SJH::Common
{
	std::optional<std::string> LoadTextFile(const std::string &filename)
	{
		// cpp 스타일의 파일 로딩 방식이다.
		std::ifstream fin(filename);
		if (!fin.is_open())
		{
			std::cout << "failed to open file: {}" << std::endl;
			return {};
		}
		std::stringstream text;
		text << fin.rdbuf();
		return text.str();
	}
}

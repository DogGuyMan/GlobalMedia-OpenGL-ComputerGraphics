#ifndef __SJH_COMMON_H__
#define __SJH_COMMON_H__

#include <memory>
#include <string>
#include <optional>

namespace SJH::Common
{
	std::optional<std::string> LoadTextFile(const std::string &filename);
}

#endif //__SJH_COMMON_H__

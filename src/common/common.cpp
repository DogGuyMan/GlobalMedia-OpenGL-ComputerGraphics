#include "common.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

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
}

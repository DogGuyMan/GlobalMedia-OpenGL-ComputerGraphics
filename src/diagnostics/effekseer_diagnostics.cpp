#include "diagnostics/effekseer_diagnostics.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace SJH::Diagnostics
{
    namespace
    {
        /// 확장자(소문자, ASCII) 일치 검사 — 텍스처/모델/머티리얼 참조만 추림.
        bool HasResourceExtension(const std::string &s)
        {
            std::string lower = s;
            for (char &c : lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            static constexpr std::string_view kExts[] = {".png", ".efkefc", ".efkmat", ".efkmodel"};
            for (std::string_view e : kExts)
            {
                if (lower.size() >= e.size() &&
                    lower.compare(lower.size() - e.size(), e.size(), e) == 0)
                    return true;
            }
            return false;
        }
    } // namespace

    int EffekseerDiagnostics::CheckEffectTextures(std::string_view efkPath, std::string_view baseDir)
    {
        const std::string pathStr(efkPath);
        std::ifstream ifs(pathStr, std::ios::binary);
        if (!ifs)
        {
            spdlog::warn("[EfkDiag] .efk 열기 실패: {}", pathStr);
            return -1;
        }
        const std::vector<unsigned char> data((std::istreambuf_iterator<char>(ifs)),
                                              std::istreambuf_iterator<char>());

        // magic 'SKFE' (Effekseer 바이너리)
        if (data.size() < 4 || data[0] != 'S' || data[1] != 'K' || data[2] != 'F' || data[3] != 'E')
        {
            spdlog::warn("[EfkDiag] magic 'SKFE' 불일치 (.efk 아님?): {}", pathStr);
            return -2;
        }

        // UTF-16LE 런 스캔 — (ascii, 0x00) 쌍이 3자 이상 연속인 구간.
        std::vector<std::string> refs;
        std::string cur;
        auto flush = [&]() {
            if (cur.size() >= 3 && HasResourceExtension(cur))
            {
                for (char &c : cur)
                    if (c == '\\') c = '/'; // 슬래시 통일
                if (std::find(refs.begin(), refs.end(), cur) == refs.end())
                    refs.push_back(cur);
            }
            cur.clear();
        };
        for (std::size_t i = 0; i + 1 < data.size(); i += 2)
        {
            const unsigned char lo = data[i];
            const unsigned char hi = data[i + 1];
            if (hi == 0x00 && lo >= 0x20 && lo <= 0x7e)
                cur.push_back(static_cast<char>(lo));
            else
                flush();
        }
        flush();

        const fs::path base = baseDir.empty()
                                  ? fs::path(pathStr).parent_path()
                                  : fs::path(std::string(baseDir));
        const std::string efkName = fs::path(pathStr).filename().string();

        int missing = 0;
        for (const std::string &ref : refs)
        {
            const fs::path  resolved = base / ref;
            std::error_code ec;
            if (!fs::exists(resolved, ec))
            {
                spdlog::warn("[EfkDiag] {} 텍스처 누락: {} -> {}", efkName, ref, resolved.string());
                ++missing;
            }
        }
        if (missing == 0)
            spdlog::info("[EfkDiag] {}: {} refs 전부 OK", efkName, refs.size());
        else
            spdlog::warn("[EfkDiag] {}: {}/{} refs 누락", efkName, missing, refs.size());
        return missing;
    }

    bool EffekseerDiagnostics::CheckPlayHandle(int32_t handle, std::string_view tag)
    {
        if (handle < 0)
        {
            spdlog::warn("[EfkDiag] Play 실패 (handle={}) tag={} — manager/effect null 또는 maxSprites 초과 가능",
                         handle, tag);
            return false;
        }
        return true;
    }

    bool EffekseerDiagnostics::CheckHandleAlive(int32_t handle, bool exists, std::string_view tag)
    {
        if (handle < 0)
            return false; // CheckPlayHandle 이 이미 보고
        if (!exists)
        {
            spdlog::warn("[EfkDiag] handle={} tag={} 가 Play 직후 즉시 종료 — 빈 이펙트/텍스처 전무 의심",
                         handle, tag);
            return false;
        }
        return true;
    }
}

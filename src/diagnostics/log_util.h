#ifndef __SJH_DIAGNOSTICS_LOG_UTIL_H__
#define __SJH_DIAGNOSTICS_LOG_UTIL_H__

// diagnostics 모듈 *내부 전용* 헤더 — 공개 API 아님.
// 이 프로젝트는 vcpkg 미사용이라 spdlog/fmt 가 없다. 그래서 진단 출력에 쓰던
// spdlog::{info,warn,error} / fmt::format / fmt::join 를 표준 라이브러리만으로
// 흉내 내는 미니 구현을 둔다.
//
//  - Log::Format("...{}...{:x}...", a, b)  → fmt::format 대체 (지원 스펙: "{}", "{:x}", "{:X}", "{:.Nf}")
//    "{{" / "}}" 이스케이프 지원. 그 외 스펙은 무시하고 기본 변환.
//  - Log::Join(range, ", ")                → fmt::join 대체
//  - Log::Info / Log::Warn / Log::Error    → spdlog::* 대체 (stderr 에 "[diag][level] msg\n")
//
// CMakeLists.txt 의 PRIVATE include 경로(${CMAKE_CURRENT_SOURCE_DIR})에서만 보이므로
// 외부 타겟으로 전파되지 않는다.

#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

namespace SJH::Diagnostics::Log
{
    // ====== 값 1개 → 문자열 (spec 은 '{:' 와 '}' 가 제거된 상태로 전달됨) ======
    inline std::string Stringify(std::string_view /*spec*/, const std::string &v) { return v; }
    inline std::string Stringify(std::string_view /*spec*/, std::string_view v) { return std::string(v); }
    inline std::string Stringify(std::string_view /*spec*/, const char *v) { return v ? std::string(v) : std::string("(null)"); }
    inline std::string Stringify(std::string_view /*spec*/, char v) { return std::string(1, v); }
    inline std::string Stringify(std::string_view /*spec*/, bool v) { return v ? std::string("true") : std::string("false"); }

    // 산술 타입 (bool / char 는 위 비-템플릿 오버로드가 우선)
    template <typename T>
    std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value && !std::is_same<T, char>::value, std::string>
    Stringify(std::string_view spec, T v)
    {
        char buf[64] = {};
        if constexpr (std::is_floating_point<T>::value)
        {
            int prec = 6; // 스펙 미지정 시 fmt 기본과 유사
            if (spec.size() >= 2 && spec.front() == '.' && spec.back() == 'f')
            {
                prec = 0;
                for (std::size_t i = 1; i + 1 < spec.size(); ++i)
                    prec = prec * 10 + (spec[i] - '0');
            }
            std::snprintf(buf, sizeof buf, "%.*f", prec, static_cast<double>(v));
        }
        else
        {
            const char kind = spec.empty() ? '\0' : spec.back();
            if (kind == 'x' || kind == 'X')
            {
                std::snprintf(buf, sizeof buf, (kind == 'x') ? "%llx" : "%llX",
                              static_cast<unsigned long long>(static_cast<typename std::make_unsigned<T>::type>(v)));
            }
            else
            {
                return std::to_string(v);
            }
        }
        return buf;
    }

    namespace detail_fmt
    {
        // pat 의 리터럴 부분을 ("{{"→"{", "}}"→"}" 언이스케이프하며) out 에 복사하다가
        // 진짜 "{...}" 플레이스홀더를 만나면 spec/rest 를 채우고 true. 끝까지 없으면 false.
        inline bool NextPlaceholder(std::string &out, std::string_view pat,
                                    std::string_view &spec, std::string_view &rest)
        {
            for (std::size_t i = 0; i < pat.size(); ++i)
            {
                const char c = pat[i];
                if (c == '{')
                {
                    if (i + 1 < pat.size() && pat[i + 1] == '{')
                    {
                        out.push_back('{');
                        ++i;
                        continue;
                    }
                    const std::size_t close = pat.find('}', i + 1);
                    if (close == std::string_view::npos)
                    {
                        out.append(pat.substr(i)); // 망가진 패턴 — 그대로 흘림
                        return false;
                    }
                    spec = pat.substr(i + 1, close - (i + 1));
                    if (!spec.empty() && spec.front() == ':')
                        spec.remove_prefix(1);
                    rest = pat.substr(close + 1);
                    return true;
                }
                if (c == '}' && i + 1 < pat.size() && pat[i + 1] == '}')
                {
                    out.push_back('}');
                    ++i;
                    continue;
                }
                out.push_back(c);
            }
            return false;
        }

        inline void FormatTo(std::string &out, std::string_view pat)
        {
            // 남은 인자 없음 — 플레이스홀더가 더 있어도 빈칸으로 흘려보낸다 (fmt 처럼 throw 하지 않음).
            std::string_view spec, rest;
            while (NextPlaceholder(out, pat, spec, rest))
                pat = rest;
        }

        template <typename T, typename... Rest>
        void FormatTo(std::string &out, std::string_view pat, const T &v, const Rest &...rest)
        {
            std::string_view spec, tail;
            if (!NextPlaceholder(out, pat, spec, tail))
                return; // 플레이스홀더가 없으면 남은 인자는 버린다
            out.append(Stringify(spec, v));
            FormatTo(out, tail, rest...);
        }
    }

    template <typename... Args>
    std::string Format(std::string_view pat, const Args &...args)
    {
        std::string out;
        out.reserve(pat.size() + 32u);
        detail_fmt::FormatTo(out, pat, args...);
        return out;
    }

    // ====== fmt::join 대체 ======
    template <typename Range>
    std::string Join(const Range &range, std::string_view sep)
    {
        std::string out;
        bool first = true;
        for (const auto &e : range)
        {
            if (!first)
                out.append(sep);
            first = false;
            out.append(Stringify(std::string_view{}, e));
        }
        return out;
    }

    // ====== 레벨 로깅 (spdlog 기본 sink 는 stdout 이지만, 진단 메시지라 stderr 로 통일) ======
    enum class Level
    {
        Info,
        Warn,
        Error
    };

    inline void Emit(Level level, const std::string &msg)
    {
        const char *prefix = (level == Level::Error) ? "[diag][error] "
                             : (level == Level::Warn)  ? "[diag][warn]  "
                                                       : "[diag][info]  ";
        std::fputs(prefix, stderr);
        std::fputs(msg.c_str(), stderr);
        std::fputc('\n', stderr);
    }

    template <typename... Args>
    void Info(std::string_view pat, const Args &...args) { Emit(Level::Info, Format(pat, args...)); }
    template <typename... Args>
    void Warn(std::string_view pat, const Args &...args) { Emit(Level::Warn, Format(pat, args...)); }
    template <typename... Args>
    void Error(std::string_view pat, const Args &...args) { Emit(Level::Error, Format(pat, args...)); }
}

#endif // __SJH_DIAGNOSTICS_LOG_UTIL_H__

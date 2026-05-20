#include <catch2/catch_test_macros.hpp>

#include "common/common.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// 테스트 종료 시 자동 삭제되는 임시 파일.
// fs::temp_directory_path() 아래에 chrono+counter 로 고유 이름 생성.
class TempFile {
public:
    explicit TempFile(const std::string& contents) : mPath(makePath()) {
        std::ofstream out(mPath, std::ios::binary);
        out << contents;
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    ~TempFile() {
        std::error_code ec;
        fs::remove(mPath, ec);
    }

    std::string path_string() const { return mPath.string(); }

private:
    fs::path mPath;

    static fs::path makePath() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::high_resolution_clock::now()
                               .time_since_epoch()
                               .count();
        const auto name = "sjh_test_" + std::to_string(stamp) +
                          "_" + std::to_string(counter.fetch_add(1)) + ".txt";
        return fs::temp_directory_path() / name;
    }
};

} // namespace

TEST_CASE("LoadTextFile reads existing file content", "[common][LoadTextFile]") {
    const std::string expected = "hello world\nsecond line\n";
    TempFile file(expected);

    auto result = SJH::LoadTextFile(file.path_string());

    REQUIRE(result.has_value());
    REQUIRE(*result == expected);
}

TEST_CASE("LoadTextFile returns nullopt for missing file", "[common][LoadTextFile]") {
    const auto missing =
        fs::temp_directory_path() / "sjh_definitely_missing_404_xyz.txt";
    std::error_code ec;
    fs::remove(missing, ec); // 만약 남아있다면 정리 — 진짜로 존재하지 않게 보장

    auto result = SJH::LoadTextFile(missing.string());

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("LoadTextFile returns empty string for empty file", "[common][LoadTextFile]") {
    TempFile file("");

    auto result = SJH::LoadTextFile(file.path_string());

    // 현재 동작 동결 (Characterization):
    //   is_open() 성공 -> rdbuf() 빈 스트림 -> optional 에 빈 문자열.
    //   nullopt 가 아님에 주의.
    REQUIRE(result.has_value());
    REQUIRE(result->empty());
}

// SJH::Diagnostics 신규 구조화 반환 API characterization - 순수 CPU 만 (GL 컨텍스트 불필요).
// 대상 3종: CheckIndicesDetailed (Cat A) / ClassifyInfoLog (Cat F 분류) / DiffStates (상태 diff).
// 계약: 세 함수 모두 GL 호출 0 - 입력 POD/vector/string 만으로 결정적 결과를 낸다.
#include <catch2/catch_test_macros.hpp>

#include "diagnostics/gl_validate.h"
#include "diagnostics/gl_state_fields.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
    namespace gv = SJH::Diagnostics::GLValidate;
    using SJH::Diagnostics::DiffStates;
    using SJH::Diagnostics::FieldChange;
    using SJH::Diagnostics::GLStateFields;

    // findings 안에 특정 kind 가 존재하는지 헬퍼.
    bool HasKind(const gv::DiagResult &result, gv::IndexFindingKind kind)
    {
        return std::any_of(result.findings.begin(), result.findings.end(),
                           [kind](const gv::DiagFinding &f) { return f.kind == kind; });
    }

    // changes 안에 특정 필드명이 존재하는지 헬퍼.
    bool HasField(const std::vector<FieldChange> &changes, const std::string &field)
    {
        return std::any_of(changes.begin(), changes.end(),
                           [&field](const FieldChange &c) { return c.field == field; });
    }
}

// ───────────────────────────────────────────────────────────────────────────
// 1. CheckIndicesDetailed (Cat A) - 순수 CPU EBO 인덱스 검사
// ───────────────────────────────────────────────────────────────────────────

TEST_CASE("CheckIndicesDetailed: OOB 인덱스는 OutOfBounds finding 을 만든다", "[diag][cat-a]")
{
    // vertexCount=3 인데 99 는 범위 밖 -> 해당 삼각형 OOB.
    const std::vector<uint32_t> indices{0, 1, 99};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "oob");

    REQUIRE(HasKind(result, gv::IndexFindingKind::OutOfBounds));
    REQUIRE_FALSE(result.Clean());
}

TEST_CASE("CheckIndicesDetailed: 두 인덱스 동일은 Degenerate finding 을 만든다", "[diag][cat-a]")
{
    // (0,1,1) - b==c 인 zero-area 삼각형.
    const std::vector<uint32_t> indices{0, 1, 1};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "degen");

    REQUIRE(HasKind(result, gv::IndexFindingKind::Degenerate));
    // OOB 가 아니므로 degenerate 분류로만 잡혀야 한다.
    REQUIRE_FALSE(HasKind(result, gv::IndexFindingKind::OutOfBounds));
}

TEST_CASE("CheckIndicesDetailed: 정렬 트리플 동일은 Duplicate finding 을 만든다", "[diag][cat-a]")
{
    // (0,1,2) 와 (2,1,0) 은 정렬하면 같은 트리플 -> 두 번째가 중복.
    const std::vector<uint32_t> indices{0, 1, 2, 2, 1, 0};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "dup");

    REQUIRE(HasKind(result, gv::IndexFindingKind::Duplicate));
    // 정확히 1건 (두 번째 삼각형만) 위반이어야 한다.
    REQUIRE(result.Count() == 1);
}

TEST_CASE("CheckIndicesDetailed: 정상 삼각형은 clean", "[diag][cat-a]")
{
    const std::vector<uint32_t> indices{0, 1, 2};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "clean");

    REQUIRE(result.Clean());
    REQUIRE(result.Count() == 0);
}

TEST_CASE("CheckIndicesDetailed: 빈 입력은 Empty finding (배타적)", "[diag][cat-a]")
{
    // 헤더 계약: 빈 입력은 Empty 1건만 push 후 즉시 반환 (early return).
    const std::vector<uint32_t> indices{};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "empty");

    REQUIRE(HasKind(result, gv::IndexFindingKind::Empty));
    REQUIRE(result.Count() == 1);
}

TEST_CASE("CheckIndicesDetailed: size %% 3 != 0 은 NotMultipleOf3 finding", "[diag][cat-a]")
{
    // 4 개 - 3 의 배수 아님. 구현은 NotMultipleOf3 를 push 한 뒤에도 size/3=1 삼각형을 순회한다.
    // (0,1,2) 는 정상이므로 NotMultipleOf3 1건만 남는다.
    const std::vector<uint32_t> indices{0, 1, 2, 0};
    const gv::DiagResult result = gv::CheckIndicesDetailed(indices, 3, "notmul3");

    REQUIRE(HasKind(result, gv::IndexFindingKind::NotMultipleOf3));
    REQUIRE(result.Count() == 1);
}

TEST_CASE("CheckIndices wrapper 는 CheckIndicesDetailed().Count() 와 일치", "[diag][cat-a]")
{
    // 헤더 계약: 기존 size_t 반환 CheckIndices 는 Detailed 의 Count() wrapper.
    const std::vector<uint32_t> indices{0, 1, 99, 3, 3, 4}; // OOB 1 + degenerate 1
    const size_t wrapped = gv::CheckIndices(indices, 3, "wrap");
    const size_t detailed = gv::CheckIndicesDetailed(indices, 3, "wrap").Count();

    REQUIRE(wrapped == detailed);
}

// ───────────────────────────────────────────────────────────────────────────
// 2. ClassifyInfoLog (Cat F 분류) - 순수 CPU 키워드 판정
// ───────────────────────────────────────────────────────────────────────────

TEST_CASE("ClassifyInfoLog: 'ERROR:' 포함은 Error", "[diag][cat-f]")
{
    REQUIRE(gv::ClassifyInfoLog("ERROR: 0:12: undefined") == gv::InfoLogSeverity::Error);
}

TEST_CASE("ClassifyInfoLog: 'WARNING' 만 포함은 Warning", "[diag][cat-f]")
{
    REQUIRE(gv::ClassifyInfoLog("WARNING: implicit cast") == gv::InfoLogSeverity::Warning);
}

TEST_CASE("ClassifyInfoLog: 빈 로그는 Clean", "[diag][cat-f]")
{
    REQUIRE(gv::ClassifyInfoLog("") == gv::InfoLogSeverity::Clean);
}

TEST_CASE("ClassifyInfoLog: 소문자 'error' 도 대소문자 무시로 Error", "[diag][cat-f]")
{
    REQUIRE(gv::ClassifyInfoLog("some error happened") == gv::InfoLogSeverity::Error);
}

TEST_CASE("ClassifyInfoLog: error 와 warning 동시 포함 시 Error 우선", "[diag][cat-f]")
{
    REQUIRE(gv::ClassifyInfoLog("warning: x\nerror: y") == gv::InfoLogSeverity::Error);
}

// ───────────────────────────────────────────────────────────────────────────
// 3. DiffStates - 두 GLStateFields POD 의 순수 CPU diff (GL 호출 없음)
// ───────────────────────────────────────────────────────────────────────────

TEST_CASE("DiffStates: 동일 두 상태는 빈 벡터", "[diag][diff]")
{
    // 기본 생성 POD 두 개 - 모든 필드 동일.
    const GLStateFields before;
    const GLStateFields after;
    const std::vector<FieldChange> changes = DiffStates(before, after);

    REQUIRE(changes.empty());
}

TEST_CASE("DiffStates: program 바인딩 1필드 변경은 program/카테고리 B 1건", "[diag][diff]")
{
    GLStateFields before;
    GLStateFields after;
    after.program = 7; // 핸들 변경 - 카테고리 B.

    const std::vector<FieldChange> changes = DiffStates(before, after);

    REQUIRE(changes.size() == 1);
    REQUIRE(changes[0].field == "program");
    REQUIRE(changes[0].category == 'B');
    REQUIRE(changes[0].before == "0");
    REQUIRE(changes[0].after == "7");
}

TEST_CASE("DiffStates: blend + depth 2필드 변경은 정확히 2건 + 카테고리 D", "[diag][diff]")
{
    GLStateFields before;
    GLStateFields after;
    after.blend_enabled = true;       // 카테고리 D (픽셀 파이프라인)
    after.depth_test_enabled = true;  // 카테고리 D

    const std::vector<FieldChange> changes = DiffStates(before, after);

    REQUIRE(changes.size() == 2);
    REQUIRE(HasField(changes, "blend_enabled"));
    REQUIRE(HasField(changes, "depth_test_enabled"));
    // 두 변경 모두 픽셀 파이프라인 -> 카테고리 D.
    for (const FieldChange &c : changes)
        REQUIRE(c.category == 'D');
}

TEST_CASE("DiffStates: blend factor enum 변경은 SymbolicName 으로 표기", "[diag][diff]")
{
    GLStateFields before; // blend_src_rgb 기본 = GL_ONE
    GLStateFields after;
    after.blend_src_rgb = GL_SRC_ALPHA;

    const std::vector<FieldChange> changes = DiffStates(before, after);

    REQUIRE(changes.size() == 1);
    REQUIRE(changes[0].field == "blend_src_rgb");
    REQUIRE(changes[0].category == 'D');
    // enum 필드는 raw 정수가 아니라 SymbolicName 문자열.
    REQUIRE(changes[0].before == "GL_ONE");
    REQUIRE(changes[0].after == "GL_SRC_ALPHA");
}

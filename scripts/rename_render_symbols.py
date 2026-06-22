#!/usr/bin/env python3
r"""render 심볼 *순수 rename* 일괄 치환 (파일 내용만; 파일/폴더 이동은 별도 에이전트).

배경:
  render 서브시스템의 심볼 명명을 정리한다. 의미는 그대로, 이름만 바꾼다.
    - `Pass::Kind`     -> `Pass::RenderQueue`   (enum 명 변경)
    - `PipelineState`  -> `RenderStateBlock`    (struct/메서드/주석)
    - `IRenderStage`   -> `IRenderPassable`     (인터페이스 명)
    - 헤더가드 / 소문자 파일토큰 `render_stage` -> `render_passable`

  파일/폴더의 물리적 git mv 는 *다른 에이전트* 담당. 본 스크립트는 *내용만* 친다.

치환 순서가 중요한 이유 (순진한 s/Kind/.../g 절대 금지):
  - `DrawCommand::Kind` (별개 enum {WorldMesh, ScreenQuad}, mesh_pass_processor.h)
    는 *건드리면 안 된다*. 그래서 전역으로는 `Pass::Kind` 리터럴만 친다.
  - bare `Kind` -> `RenderQueue` 는 `enum class Kind` 정의가 있는 `src/material/pass.h`
    *한 파일에만* 적용한다. 다른 파일의 bare `Kind`(=DrawCommand::Kind 등)는 보존.

사후 assert (실패 시 sys.exit(1)):
  1. `DrawCommand::Kind` 출현 수가 before==after (불변).
  2. `RenderQueue::WorldMesh` / `RenderQueue::ScreenQuad` 가 0 (DrawCommand 오염 검출).
  3. pass.h *외* 파일의 bare `Kind`(예 mesh_pass_processor.h 의 `Kind kind = Kind::WorldMesh`)
     가 그대로 유지 (개수 불변).

기본은 dry-run(미리보기). 실제 기록은 --apply.

사용법:
    python3 scripts/rename_render_symbols.py            # dry-run 리포트
    python3 scripts/rename_render_symbols.py --apply    # in-place 치환
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent

# ── 대상 파일 수집 ────────────────────────────────────────────────────────────
#   src/**/*.{h,cpp}, apps/_MyApp_/**/*.{h,cpp,slang}, 그리고 명시 CMake 2종.


def collect_targets():
    targets = []
    seen = set()

    def add(path):
        p = path.resolve()
        if p.is_file() and p not in seen:
            seen.add(p)
            targets.append(p)

    src = PROJECT_ROOT / "src"
    for ext in ("*.h", "*.cpp"):
        for p in src.rglob(ext):
            add(p)

    myapp = PROJECT_ROOT / "apps" / "_MyApp_"
    for ext in ("*.h", "*.cpp", "*.slang"):
        for p in myapp.rglob(ext):
            add(p)

    # 명시 CMake 2종 (그 외 CMakeLists 는 대상 아님)
    add(PROJECT_ROOT / "src" / "render" / "CMakeLists.txt")
    add(PROJECT_ROOT / "src" / "render" / "render_stage" / "CMakeLists.txt")

    return sorted(targets)


# ── 규칙 정의 ────────────────────────────────────────────────────────────────
#   각 규칙은 (이름, 적용함수). 적용함수는 (텍스트) -> (새텍스트, 치환수).
#   순서가 곧 적용 순서다.

PASS_H = (PROJECT_ROOT / "src" / "material" / "pass.h").resolve()


def _count_sub(pattern, repl, text, is_regex):
    """치환 수를 세면서 치환. is_regex=False 면 리터럴 substring."""
    if is_regex:
        new, n = re.subn(pattern, repl, text)
        return new, n
    n = text.count(pattern)
    if n:
        text = text.replace(pattern, repl)
    return text, n


# 규칙 1: 리터럴 `Pass::Kind` -> `Pass::RenderQueue`
#   (SJH::Pass::Kind, Pass::Kind::Opaque 등은 이 substring 으로 자연 포함)
def rule1_pass_kind(text, path):
    return _count_sub("Pass::Kind", "Pass::RenderQueue", text, is_regex=False)


# 규칙 2: substring `PipelineState` -> `RenderStateBlock`
#   (Pass::PipelineState 타입, ApplyPipelineState, DefaultPipelineStateOf, 주석 일괄.
#    InvalidateStateCache / mStateInitialized 는 'PipelineState' 미포함이라 자동 안전.)
def rule2_pipeline_state(text, path):
    return _count_sub("PipelineState", "RenderStateBlock", text, is_regex=False)


# 규칙 3: 단어 `IRenderStage` -> `IRenderPassable` (word-boundary)
def rule3_irenderstage(text, path):
    return _count_sub(r"\bIRenderStage\b", "IRenderPassable", text, is_regex=True)


# 규칙 4: 헤더가드(대문자)
def rule4_header_guards(text, path):
    total = 0
    text, n = _count_sub("__SJH_IRENDER_STAGE_H__", "__SJH_IRENDER_PASSABLE_H__", text, is_regex=False)
    total += n
    text, n = _count_sub("__SJH_RENDER_STAGE_IMPLS_H__", "__SJH_RENDER_PASSABLE_IMPLS_H__", text, is_regex=False)
    total += n
    return text, total


# 규칙 5: 소문자 파일/폴더 토큰 substring `render_stage` -> `render_passable`
#   (include 경로 / CMake add_subdirectory / target_sources / 주석 일괄.
#    대문자 RENDER_STAGE 는 규칙4 가 별도 처리하므로 여기선 소문자만 걸린다.)
def rule5_render_stage_token(text, path):
    return _count_sub("render_stage", "render_passable", text, is_regex=False)


# 규칙 6: 파일-스코프 (pass.h 한정) bare `\bKind\b` -> `RenderQueue`
#   pass.h 의 enum class Kind / case Kind::* / const Kind k / QueueOf(Kind::*) 만.
#   pass.h *외* 파일엔 절대 적용 금지.
def rule6_bare_kind(text, path):
    if path.resolve() != PASS_H:
        return text, 0
    return _count_sub(r"\bKind\b", "RenderQueue", text, is_regex=True)


RULES = [
    ("rule1 Pass::Kind->Pass::RenderQueue", rule1_pass_kind),
    ("rule2 PipelineState->RenderStateBlock", rule2_pipeline_state),
    ("rule3 IRenderStage->IRenderPassable", rule3_irenderstage),
    ("rule4 header guards (UPPER)", rule4_header_guards),
    ("rule5 render_stage->render_passable", rule5_render_stage_token),
    ("rule6 bare Kind->RenderQueue (pass.h only)", rule6_bare_kind),
]


# ── 사후 assert 헬퍼 ─────────────────────────────────────────────────────────
def count_token(token, texts):
    """texts(파일경로->내용) 전체에서 리터럴 token 출현 수."""
    return sum(t.count(token) for t in texts.values())


def count_bare_kind_outside_passh(texts):
    r"""pass.h 외 파일의 *DrawCommand 류* bare `Kind` 총 출현 수.

    주의: `Pass::Kind` 의 `Kind` 는 규칙1 이 의도적으로 바꾸므로 invariant 에서 제외한다.
    (regex `\bKind\b` 는 `Pass::Kind` 의 Kind 도 잡으므로, `Pass::` 뒤가 아닌 것만 센다.)
    여기서 세는 것은 `DrawCommand::Kind` / `Kind kind` / `Kind::WorldMesh` 류 -
    이들은 치환 전후로 반드시 보존돼야 한다.
    """
    pat = re.compile(r"(?<!Pass::)\bKind\b")
    total = 0
    for path, t in texts.items():
        if Path(path).resolve() == PASS_H:
            continue
        total += len(pat.findall(t))
    return total


def main():
    ap = argparse.ArgumentParser(description="render 심볼 순수 rename 일괄 치환")
    ap.add_argument("--apply", action="store_true", help="실제 파일에 기록 (기본은 dry-run)")
    args = ap.parse_args()

    targets = collect_targets()

    # 원본 로드
    before = {}
    for p in targets:
        try:
            before[str(p)] = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            print(f"[skip] UTF-8 디코드 실패: {p}")

    # ── before 측정 (assert 기준값) ──
    before_drawcommand_kind = count_token("DrawCommand::Kind", before)
    before_bare_kind_outside = count_bare_kind_outside_passh(before)

    # ── 치환 적용 (메모리상) ──
    after = {}
    rule_totals = defaultdict(int)        # 규칙별 총 치환 수
    file_rule_counts = {}                  # 파일별 규칙별 치환 수
    for path, text in before.items():
        counts = {}
        for name, fn in RULES:
            text, n = fn(text, Path(path))
            if n:
                counts[name] = n
                rule_totals[name] += n
        after[path] = text
        if counts:
            file_rule_counts[path] = counts

    # ── 리포트 출력 ──
    mode = "APPLY" if args.apply else "DRY-RUN"
    print(f"=== rename_render_symbols [{mode}] ===")
    print(f"대상 파일 수: {len(before)}\n")

    print("--- 파일별 치환 수 ---")
    for path in sorted(file_rule_counts):
        rel = Path(path).relative_to(PROJECT_ROOT)
        parts = ", ".join(f"{name.split()[0]}={n}" for name, n in file_rule_counts[path].items())
        total = sum(file_rule_counts[path].values())
        print(f"  {rel}  (총 {total})  [{parts}]")
    if not file_rule_counts:
        print("  (변경 없음)")

    print("\n--- 규칙별 총 치환 수 ---")
    grand = 0
    for name, _ in RULES:
        n = rule_totals[name]
        grand += n
        print(f"  {name}: {n}")
    print(f"  ====================")
    print(f"  합계: {grand}")

    # ── 사후 assert (after 텍스트 기준) ──
    after_drawcommand_kind = count_token("DrawCommand::Kind", after)
    after_renderqueue_worldmesh = count_token("RenderQueue::WorldMesh", after)
    after_renderqueue_screenquad = count_token("RenderQueue::ScreenQuad", after)
    after_bare_kind_outside = count_bare_kind_outside_passh(after)

    print("\n--- 사후 assert ---")
    ok = True

    a1 = before_drawcommand_kind == after_drawcommand_kind
    ok &= a1
    print(f"  [{'OK' if a1 else 'FAIL'}] DrawCommand::Kind 불변: before={before_drawcommand_kind} after={after_drawcommand_kind}")

    a2 = (after_renderqueue_worldmesh == 0) and (after_renderqueue_screenquad == 0)
    ok &= a2
    print(f"  [{'OK' if a2 else 'FAIL'}] RenderQueue::WorldMesh={after_renderqueue_worldmesh} / RenderQueue::ScreenQuad={after_renderqueue_screenquad} (둘 다 0 이어야 함)")

    a3 = before_bare_kind_outside == after_bare_kind_outside
    ok &= a3
    print(f"  [{'OK' if a3 else 'FAIL'}] pass.h 외 bare Kind 불변: before={before_bare_kind_outside} after={after_bare_kind_outside}")

    if not ok:
        print("\n사후 assert 실패 - 기록하지 않고 종료.")
        sys.exit(1)

    # ── 기록 ──
    if args.apply:
        written = 0
        for path, text in after.items():
            if text != before[path]:
                Path(path).write_text(text, encoding="utf-8")
                written += 1
        print(f"\n[APPLY] {written} 파일 기록 완료.")
    else:
        print("\n[DRY-RUN] 기록하지 않음. 적용하려면 --apply.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""명명 규칙(컨벤션) 위반 전수조사기.

검사 범위: src/  +  apps/_MyApp_/src   (.cpp / .h)
검사하는 컨벤션 (memory: naming_convention_philosophy):
  - 멤버(private/protected)  : m + PascalCase   (bool 이면 mIs*, 포인터면 *Ptr)
  - 지역변수                 : 순수 camelCase
  - 클래스/함수/메서드        : PascalCase

이 스크립트는 위반 "후보" 를 신뢰도 등급과 함께 보고한다. 코드를 수정하지 않는다.
정규식·휴리스틱 기반이라 등급별 오탐률이 다르므로 사람 검토가 전제다.
(완전 정확한 강제는 clang-tidy readability-identifier-naming 가 정답 — 본 도구는 가벼운 보조.)

  A) trailing underscore 멤버    [HIGH] — 옛 Google 스타일 `xxx_` 잔존. 거의 확정.
  B) m 없는 멤버 변수            [MED]  — class private/protected 본문의 데이터 멤버 중 m/k/ALLCAPS 아님.
  C) 지역변수 PascalCase/snake_  [LOW]  — 함수 본문 내 초기화 선언이 camelCase 아님. 오탐 가능.

사용:
  python3 scripts/find_naming_violations.py            # 전체 (A/B/C 위반 후보)
  python3 scripts/find_naming_violations.py A          # A 등급만 (A/B/C/AB.. 조합)
  python3 scripts/find_naming_violations.py --quiet    # 요약만
  python3 scripts/find_naming_violations.py --vocab    # 멤버 어휘 사전 → 오타/불일치 육안 검출

[--vocab 모드] 구조적 regex 의 사각지대(이미 m 으로 시작하는 *오타*, 예 mIextInterval)를 메운다.
  \\bm[A-Z] 로 멤버명을 전수 dedupe·정렬 → mIext… 가 mInterval… 옆에서 오드볼로 튄다.
  ※ 앞 공백 ' m[A-Z]' 패턴은 .clang-format(Microsoft) 의 `Type *mPtr` 같은 *직결을 놓쳐서 \\b 채택.
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).parent.parent
SCAN_DIRS = [ROOT / "src", ROOT / "apps/_MyApp_/src"]
EXTS = {".cpp", ".h"}

# ── 주석/문자열/문자 리터럴 마스킹 ───────────────────────────────────────────
# 식별자가 주석·문자열 안에 있으면 오탐이므로 코드만 남긴다.
def strip_noise(line: str, in_block: bool):
    """라인에서 //주석, /*..*/, "..", '..' 을 공백으로 치환. (lineno 보존 위해 길이 유지)
       in_block: 진입 시점에 블록주석 안인지. 반환 (code_line, still_in_block)."""
    out = []
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if in_block:
            if c == "*" and i + 1 < n and line[i + 1] == "/":
                out.append("  "); i += 2; in_block = False
            else:
                out.append(" "); i += 1
            continue
        # 라인 주석
        if c == "/" and i + 1 < n and line[i + 1] == "/":
            out.append(" " * (n - i)); break
        # 블록 주석 시작
        if c == "/" and i + 1 < n and line[i + 1] == "*":
            out.append("  "); i += 2; in_block = True
            continue
        # 문자열 / 문자 리터럴
        if c in ('"', "'"):
            quote = c
            out.append(" "); i += 1
            while i < n:
                if line[i] == "\\" and i + 1 < n:
                    out.append("  "); i += 2; continue
                if line[i] == quote:
                    out.append(" "); i += 1; break
                out.append(" "); i += 1
            continue
        out.append(c); i += 1
    return "".join(out), in_block

# ── 패턴 ─────────────────────────────────────────────────────────────────────
# A) trailing underscore: 소문자 시작 식별자가 단일 '_' 로 끝남 (size_t/int32_t 등 중간 '_' 는 제외)
RE_TRAILING = re.compile(r"\b([a-z][A-Za-z0-9]*_)(?![A-Za-z0-9_])")
# C++ 예약어를 식별자명으로 쓰기 위한 trailing '_' 는 정당한 예외 (template_ 등) → A 에서 제외
RESERVED_TRAILING = {
    "template_", "class_", "struct_", "operator_", "new_", "delete_",
    "this_", "namespace_", "typename_", "default_", "case_", "for_",
}

# 클래스/구조체 선언 (전방선언 ; 으로 끝나는 것은 별도 제외)
RE_CLASS = re.compile(r"\b(class|struct)\b\s+([A-Za-z_]\w*)")
RE_ACCESS = re.compile(r"^\s*(public|protected|private)\s*:")

# 데이터 멤버 선언 후보: 세미콜론으로 끝, '(' 없음(함수 아님), = 또는 {} 초기화 허용.
# 마지막 식별자(=선언명)를 잡는다.
RE_DECL_NAME = re.compile(
    r"^[\s\w:<>,\*&\[\]]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*$"
)
# 제외 키워드로 시작하는 라인 (멤버/지역 선언 아님)
RE_SKIP_DECL = re.compile(
    r"^\s*(using|typedef|friend|return|static_assert|namespace|template|"
    r"public|protected|private|case|goto|delete|else|do)\b"
)
# 이미 규칙을 지키거나 별도 컨벤션인 이름 (멤버 검사에서 통과 처리)
RE_OK_MEMBER = re.compile(r"^(m[A-Z]|k[A-Z]|[A-Z0-9_]+$|s_|g_)")

# 지역 초기화 선언: `Type name = ...;`  또는  `auto name = ...;`
RE_LOCAL = re.compile(
    r"^\s*(?:const\s+|constexpr\s+|static\s+)*"
    r"(?:auto|[A-Za-z_][\w:]*(?:<[^;{}]*>)?)[\s\*&]+([A-Za-z_]\w*)\s*=[^=]"
)
RE_PASCAL = re.compile(r"^[A-Z][a-z0-9]")          # PascalCase 후보
RE_SNAKE  = re.compile(r"^[a-z]+_[a-z]")            # snake_case 후보 (trailing 과 구분: 중간 '_')

GRADES = {"A": "HIGH ", "B": "MED  ", "C": "LOW  "}

# 멤버 어휘 추출 — 단어경계 mXxx (앞 공백 패턴은 *mPtr/(mFoo/!mFoo 를 놓치므로 \b 사용)
RE_MEMBER_TOKEN = re.compile(r"\bm[A-Z][A-Za-z0-9]*\b")


def scan_file(path: Path):
    """파일 1개에서 (grade, lineno, name, raw) 위반 후보 리스트 반환."""
    raw_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    hits = []

    depth = 0                 # 현재 중괄호 깊이 (라인 처리 후)
    class_stack = []          # [{"body_depth": int, "access": str}]
    pending_class = None      # (kind) — 선언은 봤고 여는 { 대기 (Allman 스타일)

    in_block = False
    for no, raw in enumerate(raw_lines, 1):
        code, in_block_next = strip_noise(raw, in_block)
        was_in_block = in_block
        in_block = in_block_next
        if was_in_block and not code.strip():
            continue

        depth_before = depth

        # access 지정자 갱신
        m_acc = RE_ACCESS.match(code)
        if m_acc and class_stack:
            class_stack[-1]["access"] = m_acc.group(1)

        # 멤버 레벨 여부 = 클래스 본문 깊이에 직접 위치
        at_member_level = bool(class_stack) and depth_before == class_stack[-1]["body_depth"]
        member_access = class_stack[-1]["access"] if class_stack else None

        # ── A) trailing underscore (어디서든) ──
        for m in RE_TRAILING.finditer(code):
            if m.group(1) in RESERVED_TRAILING:
                continue  # 예약어 회피 — 정당한 예외
            hits.append(("A", no, m.group(1), raw.rstrip()))

        # ── B) m 없는 멤버 ──
        if at_member_level and member_access in ("private", "protected") \
           and not RE_SKIP_DECL.match(code) and "(" not in code:
            md = RE_DECL_NAME.match(code)
            if md:
                name = md.group(1)
                if not RE_OK_MEMBER.match(name) and not name.endswith("_"):
                    hits.append(("B", no, name, raw.rstrip()))

        # ── C) 지역변수 PascalCase/snake_case ──
        # 함수 본문 = 어떤 { } 안이지만 멤버 레벨/네임스페이스 직속이 아님 (depth>0 && !멤버레벨)
        if depth_before > 0 and not at_member_level and not RE_SKIP_DECL.match(code):
            ml = RE_LOCAL.match(code)
            if ml:
                name = ml.group(1)
                if RE_PASCAL.match(name) or RE_SNAKE.match(name):
                    hits.append(("C", no, name, raw.rstrip()))

        # ── 클래스 컨텍스트 / 깊이 추적 ──
        # 클래스/구조체 선언 감지 (전방선언 제외: 같은 줄에서 '{' 없이 ';' 로 끝)
        mc = RE_CLASS.search(code)
        if mc and not re.search(r"\b" + mc.group(1) + r"\b[^{;]*;", code):
            pending_class = mc.group(1)

        # 중괄호를 좌→우로 처리하며 깊이/클래스 push·pop
        for ch in code:
            if ch == "{":
                depth += 1
                if pending_class is not None:
                    default_access = "private" if pending_class == "class" else "public"
                    class_stack.append({"body_depth": depth, "access": default_access})
                    pending_class = None
            elif ch == "}":
                if class_stack and depth == class_stack[-1]["body_depth"]:
                    class_stack.pop()
                depth = max(0, depth - 1)

    return hits


def collect_member_tokens(path: Path):
    """파일에서 코드 영역(주석/문자열 제외)의 mXxx 멤버 토큰을 수집 → {name: count}."""
    raw_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    counts = defaultdict(int)
    in_block = False
    for raw in raw_lines:
        code, in_block = strip_noise(raw, in_block)
        for tok in RE_MEMBER_TOKEN.findall(code):
            counts[tok] += 1
    return counts


def run_vocab(files):
    """멤버 어휘 사전 — \\bm[A-Z] 전수 dedupe. 정렬 시 오타/오드볼이 인접 클러스터에서 튄다.
       (예: mIextInterval 이 mIntervalTime 옆에 떠서 사람이 즉시 식별)."""
    total = defaultdict(int)               # name -> 총 등장 횟수
    where = defaultdict(set)               # name -> {파일}
    for fpath in files:
        rel = str(fpath.relative_to(ROOT))
        for name, c in collect_member_tokens(fpath).items():
            total[name] += c
            where[name].add(rel)

    names = sorted(total)
    rare = [n for n in names if total[n] == 1]   # 1회만 등장 = 오타 1순위 후보
    print("=" * 76)
    print(f"  멤버 어휘 사전 (\\bm[A-Z])   고유 멤버명 {len(names)}개   1회성 {len(rare)}개")
    print("  정렬 인접 클러스터에서 오타/불일치를 육안 검출 (예: mIext… vs mInterval…)")
    print("=" * 76)
    prev_prefix = ""
    for n in names:
        prefix = n[:4]                      # 접두 4글자 바뀌면 빈 줄로 클러스터 구분
        if prefix != prev_prefix:
            print()
            prev_prefix = prefix
        flag = "  ⚠1회" if total[n] == 1 else ""
        locs = ", ".join(sorted(where[n])[:2]) + ("…" if len(where[n]) > 2 else "")
        print(f"  {n:<28} {total[n]:>4}회{flag:<6}  {locs}")
    print("\n" + "=" * 76)
    print(f"  ⚠1회 = 단 한 곳에서만 쓰인 멤버 — 오타 1순위 후보로 우선 점검 권장")
    print("=" * 76)
    return 0


def main():
    args = [a for a in sys.argv[1:]]
    quiet = "--quiet" in args
    if "--vocab" in args:
        files = sorted(p for d in SCAN_DIRS for p in d.rglob("*") if p.suffix in EXTS)
        return run_vocab(files)
    grade_filter = "".join(a.upper() for a in args if a.upper() in ("A", "B", "C", "AB", "AC", "BC", "ABC"))
    grade_filter = set(grade_filter) if grade_filter else {"A", "B", "C"}

    files = sorted(p for d in SCAN_DIRS for p in d.rglob("*") if p.suffix in EXTS)

    per_file = defaultdict(list)
    grade_total = defaultdict(int)
    for fpath in files:
        try:
            hits = scan_file(fpath)
        except Exception as e:  # noqa: BLE001
            print(f"[ERR] {fpath}: {e}", file=sys.stderr)
            continue
        rel = str(fpath.relative_to(ROOT))
        for g, no, name, raw in hits:
            if g not in grade_filter:
                continue
            per_file[rel].append((g, no, name, raw))
            grade_total[g] += 1

    total = sum(grade_total.values())
    print("=" * 76)
    print(f"  명명 규칙 위반 전수조사   대상 {len(files)}개 파일   후보 {total}건")
    print(f"  [A] trailing underscore 멤버 : {grade_total['A']:>4}건  (HIGH 신뢰)")
    print(f"  [B] m 없는 멤버 변수         : {grade_total['B']:>4}건  (MED  휴리스틱)")
    print(f"  [C] 지역변수 비-camelCase    : {grade_total['C']:>4}건  (LOW  오탐 가능)")
    print("=" * 76)

    if quiet:
        return 1 if total else 0

    for rel, entries in sorted(per_file.items()):
        print(f"\n── {rel}  ({len(entries)}건) ──")
        for g, no, name, raw in entries:
            disp = raw if len(raw) <= 100 else raw[:97] + "..."
            print(f"  {no:5d}  [{GRADES[g]}{g}]  {name:<24}  {disp.strip()}")

    print("\n" + "=" * 76)
    print(f"  총 {total}건 / {len(per_file)}개 파일.  A=거의확정, B=검토권장, C=오탐주의")
    print("=" * 76)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())

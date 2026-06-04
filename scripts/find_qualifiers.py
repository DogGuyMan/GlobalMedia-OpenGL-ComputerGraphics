#!/usr/bin/env python3
"""apps/_MyApp_/src 내 .cpp/.h 파일에서 const/static/constexpr 사용 위치를 집계한다."""

import re
import sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).parent.parent / "apps/_MyApp_/src"
EXTS = {".cpp", ".h"}

# 대상 키워드 (우선순위: constexpr > static constexpr > static > const)
KEYWORDS = ["constexpr", "static", "const"]

# 행 전체가 주석이거나 빈 줄은 건너뜀
RE_LINE_COMMENT = re.compile(r"^\s*//")
RE_BLANK        = re.compile(r"^\s*$")

# 각 키워드 검출 패턴 (단어 경계)
PATTERNS = {kw: re.compile(rf"\b{kw}\b") for kw in KEYWORDS}

def classify(line: str) -> list[str]:
    """한 줄에서 검출된 키워드 집합(중복 제거, 순서 유지)."""
    found = []
    for kw in KEYWORDS:
        if PATTERNS[kw].search(line):
            found.append(kw)
    return found

def main():
    # file → [(lineno, matched_keywords, raw_line)]
    hits: dict[str, list] = defaultdict(list)
    keyword_total: dict[str, int] = defaultdict(int)

    files = sorted(p for p in ROOT.rglob("*") if p.suffix in EXTS)

    for fpath in files:
        try:
            lines = fpath.read_text(encoding="utf-8", errors="replace").splitlines()
        except Exception as e:
            print(f"[ERR] {fpath}: {e}", file=sys.stderr)
            continue

        rel = fpath.relative_to(ROOT.parent.parent)
        for no, raw in enumerate(lines, 1):
            if RE_LINE_COMMENT.match(raw) or RE_BLANK.match(raw):
                continue
            kws = classify(raw)
            if kws:
                hits[str(rel)].append((no, kws, raw.rstrip()))
                for k in kws:
                    keyword_total[k] += 1

    # ── 출력 ──────────────────────────────────────────────────────────────
    total_lines = sum(len(v) for v in hits.values())
    print(f"{'='*72}")
    print(f"  검색 루트: {ROOT}")
    print(f"  대상 파일: {len(files)}개   히트 행: {total_lines}개")
    for kw in KEYWORDS:
        print(f"    {kw:12s}: {keyword_total[kw]:>5}회")
    print(f"{'='*72}\n")

    for fpath_str, entries in hits.items():
        print(f"── {fpath_str}  ({len(entries)}줄) ──")
        for no, kws, raw in entries:
            tag = "+".join(kws)
            # 긴 줄은 120자 이하로 잘라 가독성 확보
            display = raw if len(raw) <= 120 else raw[:117] + "..."
            print(f"  {no:5d}  [{tag:<20}]  {display}")
        print()

    print(f"{'='*72}")
    print(f"  총 {total_lines}개 행 / {len(hits)}개 파일에서 키워드 검출 완료")
    print(f"{'='*72}")

if __name__ == "__main__":
    main()

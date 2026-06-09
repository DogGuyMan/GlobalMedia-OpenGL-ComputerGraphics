#!/usr/bin/env python3
r"""ASCII·한글 외 특수문자를 *의미 보존 ASCII 등가물* 로 교체(replace)한다.

`find_special_chars.py` 는 `--strip`(삭제)만 제공하지만, 본 도구는 *치환 매핑* 으로
주석 가독성을 유지하면서 소스를 ASCII(+한글)화한다. 화살표(→)는 `->`, 비교(≤)는 `<=`,
원형숫자(①)는 `(1)`, 괘선(─/│)은 `-`/`|`, em-dash(—)는 `-` 로 바꾼다.

설계 원칙 (무손실):
  - 매핑에 *있는* 문자만 교체한다.
  - 매핑에 *없는* 특수문자는 **건드리지 않고** 경고로 보고한다 (silent loss 금지).
    → 보고된 미매핑 문자는 MAP 에 추가한 뒤 재실행.

대상: 기본 `src/` (인자로 경로 지정 가능). 코드/주석/문자열 구분 없이 전부 치환한다
(특수문자는 사실상 주석·문자열 장식에만 존재 — C++ 토큰은 ASCII).

사용법:
    python3 scripts/replace_special_chars.py                # src/ dry-run 리포트
    python3 scripts/replace_special_chars.py --apply        # src/ in-place 교체
    python3 scripts/replace_special_chars.py src apps --apply
"""

import argparse
import re
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
DEFAULT_TARGETS = ["src"]
SCAN_EXTS = {".cpp", ".h", ".hpp", ".c", ".cc", ".glsl", ".vert", ".frag"}

# ── ASCII·한글 외 전부를 잡는 정규식 (find_special_chars.py 와 동일 클래스) ──────
RE_TARGET = re.compile(
    "[^"
    "\x00-\x7f"      # ASCII
    "가-힣"          # 한글 완성형
    "ᄀ-ᇿ"           # 한글 자모
    "㄰-㆏"          # 한글 호환 자모
    "ꥠ-꥿"           # 한글 자모 확장-A
    "ힰ-퟿"          # 한글 자모 확장-B
    "]"
)

# ── 다중문자(digraph) 우선 치환 — 단일문자 치환 전에 적용 ──────────────────────
DIGRAPH = [
    ("⁻¹", "^-1"),   # ⁻¹  superscript -1  →  ^-1
]

# ── 단일문자 치환 매핑 ────────────────────────────────────────────────────────
SINGLE = {
    # 괘선(box-drawing) → ASCII 라인 아트
    "─": "-",  "│": "|",  "┼": "+",  "┬": "+",  "┴": "+",
    "┌": "+",  "┐": "+",  "└": "+",  "┘": "+",  "├": "+",  "┤": "+",
    # 대시류
    "—": "-",  "–": "-",  "―": "-",
    # 화살표
    "→": "->", "←": "<-", "↑": "^",  "↓": "v",
    "↔": "<->","⇄": "<->","⟺": "<=>","⇒": "=>", "⇔": "<=>",
    # 구두점·기호
    "·": "/",  "…": "...","※": "*",  "★": "*",  "☆": "*",
    "§": "sec.",
    # 수학 기호
    "×": "x",  "÷": "/",  "±": "+/-","≈": "~=", "≡": "==",
    "≠": "!=", "≤": "<=", "≥": ">=", "∈": "in", "∪": "U",
    "√": "sqrt", "∑": "sum", "∏": "prod", "∩": "&",
    # 위첨자
    "⁻": "-",  "¹": "1",  "²": "2",  "³": "3",
    # 그리스 문자 (자주 나오는 것만)
    "θ": "theta", "π": "pi", "α": "alpha", "β": "beta",
    "λ": "lambda", "Δ": "delta", "δ": "delta",
    # 체크/엑스 마크 (책임/비-책임 표식)
    "❌": "[X]", "✅": "[O]", "⭕": "[O]", "✔": "[O]", "✖": "[X]",
    # 원형 숫자 ①~⑳
    "①": "(1)", "②": "(2)", "③": "(3)", "④": "(4)", "⑤": "(5)",
    "⑥": "(6)", "⑦": "(7)", "⑧": "(8)", "⑨": "(9)", "⑩": "(10)",
    "⑪": "(11)","⑫": "(12)","⑬": "(13)","⑭": "(14)","⑮": "(15)",
    "⑯": "(16)","⑰": "(17)","⑱": "(18)","⑲": "(19)","⑳": "(20)",
    # 흑색 원형숫자 ❶~❿
    "❶": "(1)", "❷": "(2)", "❸": "(3)", "❹": "(4)", "❺": "(5)",
    "❻": "(6)", "❼": "(7)", "❽": "(8)", "❾": "(9)", "❿": "(10)",
}


def transform(text: str):
    """치환 적용. 반환 (new_text, replaced_count, unmapped_counter)."""
    replaced = 0
    # 1) digraph 우선
    for src, dst in DIGRAPH:
        cnt = text.count(src)
        if cnt:
            text = text.replace(src, dst)
            replaced += cnt
    # 2) 미매핑 특수문자 수집 (교체 전 기준 — digraph 처리 후 잔여 대상)
    unmapped = defaultdict(int)
    out = []
    for ch in text:
        if ch in SINGLE:
            out.append(SINGLE[ch])
            replaced += 1
        elif RE_TARGET.match(ch):
            # 매핑에 없는 특수문자 → 건드리지 않고 보고
            unmapped[ch] += 1
            out.append(ch)
        else:
            out.append(ch)
    return "".join(out), replaced, unmapped


def collect_files(targets):
    files = []
    for t in targets:
        p = (PROJECT_ROOT / t) if not Path(t).is_absolute() else Path(t)
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            files.extend(q for q in p.rglob("*") if q.suffix in SCAN_EXTS)
        else:
            print(f"[WARN] 대상 없음: {p}", file=sys.stderr)
    return sorted(set(files))


def rel(p):
    try:
        return str(p.relative_to(PROJECT_ROOT))
    except ValueError:
        return str(p)


def main():
    ap = argparse.ArgumentParser(description="특수문자 → ASCII 의미보존 치환")
    ap.add_argument("targets", nargs="*", help="대상 경로 (생략 시 src/)")
    ap.add_argument("--apply", action="store_true", help="in-place 교체 (미지정 시 dry-run)")
    args = ap.parse_args()

    targets = args.targets if args.targets else DEFAULT_TARGETS
    files = collect_files(targets)
    if not files:
        print("스캔할 파일이 없습니다.", file=sys.stderr); sys.exit(1)

    total_replaced = 0
    changed_files = 0
    global_unmapped = defaultdict(int)
    per_file = []

    for fpath in files:
        try:
            text = fpath.read_text(encoding="utf-8", errors="replace")
        except Exception as e:  # noqa: BLE001
            print(f"[ERR] {fpath}: {e}", file=sys.stderr); continue
        new_text, n, unmapped = transform(text)
        for ch, c in unmapped.items():
            global_unmapped[ch] += c
        if n > 0:
            per_file.append((rel(fpath), n))
            total_replaced += n
            if args.apply and new_text != text:
                fpath.write_text(new_text, encoding="utf-8")
                changed_files += 1

    mode = "APPLY (in-place 교체됨)" if args.apply else "DRY-RUN (변경 없음 — --apply 로 적용)"
    print("=" * 78)
    print(f"  특수문자 → ASCII 치환   [{mode}]")
    print(f"  스캔 {len(files)}개 파일   치환 대상 {len(per_file)}개 파일   총 {total_replaced}자")
    print("=" * 78)
    for r, n in sorted(per_file, key=lambda x: -x[1]):
        print(f"    {n:>5}자  {r}")

    if global_unmapped:
        print("\n" + "!" * 78)
        print("  [경고] 매핑에 없는 특수문자 — 교체하지 않고 *그대로 유지* 함:")
        for ch, c in sorted(global_unmapped.items(), key=lambda kv: -kv[1]):
            try:
                uname = unicodedata.name(ch)
            except ValueError:
                uname = "<unnamed>"
            print(f"    U+{ord(ch):04X} '{ch}'  {c}회   {uname}  → SINGLE 매핑에 추가 후 재실행")
        print("!" * 78)
    else:
        print("\n  매핑 누락 없음 — 모든 특수문자가 ASCII 등가물로 처리됨.")

    print("\n  검토: git diff 로 확인 후 경로 지정 partial commit 권장.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

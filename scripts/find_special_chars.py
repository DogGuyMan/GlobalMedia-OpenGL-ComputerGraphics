#!/usr/bin/env python3
r"""ASCII·한글을 제외한 문자(기호/이모지/화살표/·/한자/가나 등)를 Regex로 추출하고 라인 단위로 분석한다.

대상은 코드(apps/_MyApp_/src, src)와 문서(doc/*.md). 기본은 *읽기 전용 리포트* —
어떤 특수문자가 어디(파일:라인:열)에 몇 번 나오는지 집계한다.
`--strip` 을 주면 대상 파일에서 해당 문자를 제거(in-place)한다.

표준 라이브러리만 사용 — 파이썬 `re` 는 `\p{Hangul}` 미지원이라 한글 유니코드 범위를
명시적으로 부정 문자클래스에 넣었다 (정규식 `[^\x00-\x7F가-힣...]`).
파이썬3 문자열은 코드포인트 단위라 BMP 밖 이모지(U+1F600 등)도 한 문자로 잡힌다.

사용법:
    python3 scripts/find_special_chars.py                 # 기본 대상 분석 리포트
    python3 scripts/find_special_chars.py --no-detail      # 라인별 상세 생략(요약만)
    python3 scripts/find_special_chars.py src apps          # 대상 경로 직접 지정
    python3 scripts/find_special_chars.py --strip src       # src 하위에서 특수문자 제거
"""

import argparse
import re
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent

# ── 기본 대상 (사용자 지정: apps/_MyApp_/src, main.cpp, src, doc 4종) ──────────
DEFAULT_TARGETS = [
    "apps/_MyApp_/src",
    "apps/_MyApp_/main.cpp",
    "src",
    "doc/FMOD_Setup.md",
    "doc/EngineAPI.md",
    "doc/Box2DAPI.md",
    "doc/FMODAPI.md",
]

# 디렉토리를 재귀 스캔할 때 대상으로 삼는 확장자
SCAN_EXTS = {".cpp", ".h", ".hpp", ".c", ".cc", ".md", ".glsl", ".vert", ".frag"}

# ── 핵심 정규식 — ASCII + 한글(완성형/자모/호환자모/확장 A·B) 외 전부 ───────────
RE_TARGET = re.compile(
    "[^"
    "\x00-\x7f"      # ASCII
    "가-힣"  # 한글 완성형 (가~힣)
    "ᄀ-ᇿ"  # 한글 자모
    "㄰-㆏"  # 한글 호환 자모 (ㄱ, ㅏ)
    "ꥠ-꥿"  # 한글 자모 확장-A
    "ힰ-퟿"  # 한글 자모 확장-B
    "]"
)


def categorize(ch: str) -> str:
    """문자를 사람이 읽을 분류 라벨로 변환."""
    cp = ord(ch)
    try:
        name = unicodedata.name(ch)
    except ValueError:
        name = ""
    cat = unicodedata.category(ch)  # Po, Sm, So, Lo ...

    if cp in (0x00B7, 0x2022, 0x2027, 0x2219, 0x22C5, 0x30FB, 0xFF65):
        return "middle-dot(·)"
    if 0x2190 <= cp <= 0x21FF or 0x2900 <= cp <= 0x297F or 0x27F0 <= cp <= 0x27FF:
        return "arrow(화살표)"
    if 0x2500 <= cp <= 0x257F:
        return "box-drawing(괘선)"
    # 원형 숫자/문자 — ①②③④⑤⑥…⑳ (Enclosed Alphanumerics) + ❶❷ (Dingbat negative) + ⓿ 등.
    # emoji 범위(0x2600~0x27BF)와 겹치는 딩벳 원형숫자(0x2776~)보다 먼저 분기.
    if (0x2460 <= cp <= 0x24FF) or (0x2776 <= cp <= 0x2793) or (0x3251 <= cp <= 0x32BF):
        return "circled-num(원형숫자)"
    if cp >= 0x1F000 or 0x2600 <= cp <= 0x27BF or 0x2B00 <= cp <= 0x2BFF or 0xFE00 <= cp <= 0xFE0F:
        return "emoji(이모지)"
    if name.startswith("CJK") or "IDEOGRAPH" in name:
        return "cjk-han(한자)"
    if name.startswith("HIRAGANA") or name.startswith("KATAKANA"):
        return "kana(가나)"
    if cat.startswith("P"):
        return "punctuation(구두점)"
    if cat.startswith("S"):
        return "symbol(기호)"
    return f"other({cat})"


def collect_files(targets: list[str]) -> list[Path]:
    """대상 경로 목록(파일/디렉토리 혼합)을 실제 파일 경로 리스트로 펼친다."""
    files: list[Path] = []
    for t in targets:
        p = (PROJECT_ROOT / t) if not Path(t).is_absolute() else Path(t)
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            files.extend(q for q in p.rglob("*") if q.suffix in SCAN_EXTS)
        else:
            print(f"[WARN] 대상 없음: {p}", file=sys.stderr)
    # 중복 제거 + 정렬
    return sorted(set(files))


def analyze(files: list[Path]):
    """파일들을 스캔해 (per-file hit list, char freq, category freq) 반환."""
    # file → [(lineno, col, ch, category, raw_line)]
    hits: dict[Path, list] = defaultdict(list)
    char_count: dict[str, int] = defaultdict(int)
    char_files: dict[str, set] = defaultdict(set)
    cat_count: dict[str, int] = defaultdict(int)

    for fpath in files:
        try:
            text = fpath.read_text(encoding="utf-8", errors="replace")
        except Exception as e:
            print(f"[ERR] {fpath}: {e}", file=sys.stderr)
            continue

        for no, raw in enumerate(text.splitlines(), 1):
            for m in RE_TARGET.finditer(raw):
                ch = m.group()
                col = m.start() + 1
                cat = categorize(ch)
                hits[fpath].append((no, col, ch, cat, raw.rstrip()))
                char_count[ch] += 1
                char_files[ch].add(fpath)
                cat_count[cat] += 1

    return hits, char_count, char_files, cat_count


def rel(p: Path) -> str:
    try:
        return str(p.relative_to(PROJECT_ROOT))
    except ValueError:
        return str(p)


def report(files, hits, char_count, char_files, cat_count, show_detail: bool):
    total = sum(len(v) for v in hits.values())

    # ── 헤더 ────────────────────────────────────────────────────────────────
    print("=" * 78)
    print("  특수문자 추출 리포트 (ASCII·한글 제외)")
    print(f"  스캔 파일: {len(files)}개   히트 파일: {len(hits)}개   총 매치: {total}개")
    print("=" * 78)
    if total == 0:
        print("\n  ASCII·한글 외 문자가 없습니다. (깨끗함)\n")
        return

    # ── 카테고리별 집계 ───────────────────────────────────────────────────────
    print("\n[ 카테고리별 ]")
    for cat, cnt in sorted(cat_count.items(), key=lambda kv: -kv[1]):
        print(f"    {cat:<22} {cnt:>6}회")

    # ── 문자별 빈도 ───────────────────────────────────────────────────────────
    print("\n[ 문자별 빈도 ]  (코드포인트 / 문자 / 카테고리 / 횟수 / 출현 파일수)")
    print("    " + "-" * 70)
    for ch, cnt in sorted(char_count.items(), key=lambda kv: -kv[1]):
        cp = f"U+{ord(ch):04X}"
        # 제어/조합 문자는 표시 깨짐 방지로 공백 대체
        glyph = ch if unicodedata.category(ch)[0] not in ("C", "M", "Z") else "·"
        try:
            uname = unicodedata.name(ch)
        except ValueError:
            uname = "<unnamed>"
        print(f"    {cp:<8} '{glyph}'  {categorize(ch):<20} {cnt:>5}회  "
              f"{len(char_files[ch]):>2}파일   {uname}")

    # ── 파일/라인별 상세 ─────────────────────────────────────────────────────
    if show_detail:
        print("\n[ 파일·라인별 상세 ]")
        for fpath, entries in sorted(hits.items()):
            print(f"\n── {rel(fpath)}  ({len(entries)}건) ──")
            for no, col, ch, cat, raw in entries:
                glyph = ch if unicodedata.category(ch)[0] not in ("C", "M", "Z") else "·"
                ctx = raw if len(raw) <= 90 else raw[:87] + "..."
                print(f"  {no:5d}:{col:<3d}  U+{ord(ch):04X} '{glyph}' [{cat}]")
                print(f"           | {ctx}")

    # ── 파일별 요약 ───────────────────────────────────────────────────────────
    print("\n[ 파일별 요약 ]")
    for fpath, entries in sorted(hits.items(), key=lambda kv: -len(kv[1])):
        line_set = {e[0] for e in entries}
        print(f"    {len(entries):>5}건 / {len(line_set):>4}줄   {rel(fpath)}")

    print("\n" + "=" * 78)
    print(f"  총 {total}개 매치 / {len(hits)}개 파일 / {len(char_count)}종 문자")
    print("=" * 78)


def strip_files(hits) -> None:
    """대상 파일에서 매치된 문자를 제거(in-place). git이 백업이라는 전제."""
    print("\n[ --strip ] 특수문자 제거 (in-place) — git diff 로 반드시 검토하세요.\n")
    changed = 0
    for fpath in sorted(hits.keys()):
        try:
            text = fpath.read_text(encoding="utf-8", errors="replace")
        except Exception as e:
            print(f"[ERR] {fpath}: {e}", file=sys.stderr)
            continue
        new_text, n = RE_TARGET.subn("", text)
        if n > 0:
            fpath.write_text(new_text, encoding="utf-8")
            print(f"    제거 {n:>4}자  {rel(fpath)}")
            changed += 1
    print(f"\n  {changed}개 파일 수정 완료. `git diff` 로 검토 후 경로 지정 partial commit 하세요.")


def main():
    ap = argparse.ArgumentParser(description="ASCII·한글 외 특수문자 추출 / 라인 분석 / (옵션)제거")
    ap.add_argument("targets", nargs="*", help="분석 대상 파일/디렉토리 (생략 시 기본 대상)")
    ap.add_argument("--no-detail", action="store_true", help="라인별 상세 생략(요약만)")
    ap.add_argument("--strip", action="store_true", help="매치 문자를 in-place 제거 (주의)")
    args = ap.parse_args()

    targets = args.targets if args.targets else DEFAULT_TARGETS
    files = collect_files(targets)
    if not files:
        print("스캔할 파일이 없습니다.", file=sys.stderr)
        sys.exit(1)

    hits, char_count, char_files, cat_count = analyze(files)
    report(files, hits, char_count, char_files, cat_count, show_detail=not args.no_detail)

    if args.strip:
        if not hits:
            print("\n제거할 문자가 없습니다.")
        else:
            strip_files(hits)


if __name__ == "__main__":
    main()

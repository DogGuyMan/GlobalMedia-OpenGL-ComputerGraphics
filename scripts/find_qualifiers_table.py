#!/usr/bin/env python3
"""qualifier 검출 결과를 파일 단위 집계 테이블로 출력한다."""

import re, sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).parent.parent / "apps/_MyApp_/src"
EXTS = {".cpp", ".h"}
KEYWORDS = ["constexpr", "static", "const"]
RE_LINE_COMMENT = re.compile(r"^\s*//")
RE_BLANK        = re.compile(r"^\s*$")
PATTERNS = {kw: re.compile(rf"\b{kw}\b") for kw in KEYWORDS}

def classify(line):
    return [kw for kw in KEYWORDS if PATTERNS[kw].search(line)]

files = sorted(p for p in ROOT.rglob("*") if p.suffix in EXTS)

rows = []   # (rel_path, total, constexpr_cnt, static_cnt, const_cnt, tags)
for fpath in files:
    try:
        lines = fpath.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception as e:
        print(f"[ERR] {fpath}: {e}", file=sys.stderr); continue

    cnt = defaultdict(int)
    for raw in lines:
        if RE_LINE_COMMENT.match(raw) or RE_BLANK.match(raw): continue
        for kw in classify(raw): cnt[kw] += 1

    total = sum(cnt.values())
    if total == 0: continue

    rel = str(fpath.relative_to(ROOT))
    # 특이 패턴 태그
    tags = []
    if cnt["constexpr"] and cnt["static"]: tags.append("static constexpr")
    elif cnt["constexpr"]:                  tags.append("constexpr")
    elif cnt["static"]:                     tags.append("static")
    if cnt["const"] > 10:                   tags.append(f"const×{cnt['const']}")

    rows.append((rel, total, cnt["constexpr"], cnt["static"], cnt["const"], tags))

# ── 출력 ─────────────────────────────────────────────────────────────────
# 칸 너비 계산
W_FILE  = max(len(r[0]) for r in rows) + 2
W_TOTAL = 7
W_KW    = 11

HDR = (f"{'파일':<{W_FILE}} {'합계':>{W_TOTAL}} {'constexpr':>{W_KW}} "
       f"{'static':>{W_KW}} {'const':>{W_KW}}  {'주요 패턴'}")
SEP = "-" * (W_FILE + W_TOTAL + W_KW*3 + 20)

# 디렉토리 그룹화
from itertools import groupby
rows_sorted = sorted(rows, key=lambda r: (r[0].rsplit("/",1)[0] if "/" in r[0] else "", r[0]))

print(HDR)
print(SEP)
prev_dir = None
for rel, total, ce, st, co, tags in rows_sorted:
    cur_dir = rel.rsplit("/",1)[0] if "/" in rel else "(root)"
    if cur_dir != prev_dir:
        if prev_dir is not None: print()
        print(f"  [{cur_dir}/]")
        prev_dir = cur_dir
    tag_str = ", ".join(tags) if tags else ""
    print(f"    {rel:<{W_FILE-4}} {total:>{W_TOTAL}} {ce:>{W_KW}} {st:>{W_KW}} {co:>{W_KW}}  {tag_str}")

print(SEP)
print(f"  총 {len(rows)}개 파일  /  "
      f"constexpr {sum(r[2] for r in rows)}  "
      f"static {sum(r[3] for r in rows)}  "
      f"const {sum(r[4] for r in rows)}")

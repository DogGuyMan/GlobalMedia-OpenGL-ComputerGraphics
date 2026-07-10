#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""traversal_git.py — game/main 게임/엔진 전환기 작업 히스토리 추출기.

두 단계를 한 스크립트로 수행한다.

  (1) 깃 히스토리 추출
      `28fbc73 [init] : game project` (2026-05-19) 부터 HEAD 까지의
      first-parent 선형 커밋을 시간순으로 순회하며
      날짜 / 해시 / 작성자 / 커밋 메시지 / 파일 변경 통계 / 코드 diff 발췌를 수집한다.

  (2) 일자(1일) 단위 통합
      같은 날에 묶인 커밋들을 모아 `doc/work_history.md` 를 자동 생성한다.

diff 발췌 대상은 `.cpp` / `.h` 소스와 `.gitmodules`(서브모듈) 뿐이다.
png 같은 리소스, CMake, 셰이더, 문서 등 나머지 데이터는 노이즈로 보고 diff 에서 제외한다.
(변경 사실 자체는 "비코드 N개 파일" 한 줄로만 남겨 누락을 방지한다.)

사용:
    python3 scripts/traversal_git.py                 # doc/work_history.md 생성
    python3 scripts/traversal_git.py --json out.json # 추출 원본(JSON)도 함께 덤프
    python3 scripts/traversal_git.py --limit 5       # 앞 5개 커밋만(점검용)
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import date
from pathlib import Path

# ----------------------------------------------------------------------------
# 설정 상수
# ----------------------------------------------------------------------------
SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parents[1]  # scripts/ 의 부모 디렉토리 = 저장소 루트
DEFAULT_BRANCH = "game/main"
# '[init] : game project' — 게임/엔진 전환기의 명시적 시작 커밋(2026-05-19).
# 그 이전(exercise7/chapter8/9 코스워크)은 범위에서 제외한다.
DEFAULT_START = "28fbc73"
DEFAULT_OUTPUT = REPO_ROOT / "doc" / "work_history.md"

# diff 발췌 대상: .cpp/.h 소스 + .gitmodules(서브모듈 포인터)만.
# 그 외(리소스/빌드/셰이더/문서)는 노이즈로 보고 제외한다.
DIFF_EXTENSIONS = {".cpp", ".h"}
DIFF_EXACT_NAMES = {".gitmodules"}
# 벤더/사전빌드 트리는 .cpp/.h 라도 우리 코드가 아니므로 통계·diff 모두에서 제외한다.
# (diff 는 pathspec 으로, 통계는 is_code_file 로 동일 기준 적용 → 수치 일관성 유지)
VENDORED_PREFIXES = ("include/", "lib/", "extern/")

# diff 발췌 분량 캡 (한 커밋 섹션이 과도하게 길어지지 않도록 가독성 유지)
MAX_DIFF_FILES_PER_COMMIT = 12
MAX_DIFF_LINES_PER_FILE = 80
MAX_DIFF_LINES_PER_COMMIT = 400

UNIT_SEP = "\x1f"  # git --format 필드 구분자(Unit Separator)
WEEKDAY_KR = ["월", "화", "수", "목", "금", "토", "일"]  # date.weekday(): 0=월


# ----------------------------------------------------------------------------
# git 래퍼
# ----------------------------------------------------------------------------
def run_git(args, *, repo=REPO_ROOT):
    """git 명령을 실행하고 stdout 을 UTF-8 문자열로 반환한다.

    core.quotepath=false 로 한글/유니코드 경로를 8진 이스케이프 없이 그대로 받는다.
    """
    proc = subprocess.run(
        ["git", "-c", "core.quotepath=false", "-C", str(repo), *args],
        capture_output=True,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr.decode("utf-8", "replace"))
        raise SystemExit(f"git 실패: git {' '.join(args)}")
    return proc.stdout.decode("utf-8", "replace")


# ----------------------------------------------------------------------------
# 경로/분류 유틸
# ----------------------------------------------------------------------------
def normalize_path(path: str) -> str:
    """numstat 의 rename 표기(`a => b`, `dir/{old => new}/f`)를 새 경로로 정규화한다."""
    if "=>" not in path:
        return path
    if "{" in path and "}" in path:
        prefix, rest = path.split("{", 1)
        middle, suffix = rest.split("}", 1)
        new = middle.split("=>")[-1].strip()
        return (prefix + new + suffix).replace("//", "/")
    return path.split("=>")[-1].strip()


def strip_ab(path: str) -> str:
    """diff 헤더의 `a/` `b/` 접두사를 제거한다."""
    if path.startswith(("a/", "b/")):
        return path[2:]
    return path


def is_code_file(path: str) -> bool:
    """diff 발췌 대상(.cpp/.h/.gitmodules)인지 판정한다. 벤더 트리는 제외."""
    if path.startswith(VENDORED_PREFIXES):
        return False
    name = path.rsplit("/", 1)[-1]
    if name in DIFF_EXACT_NAMES:
        return True
    dot = name.rfind(".")
    return dot != -1 and name[dot:] in DIFF_EXTENSIONS


def bucket_of(path: str) -> str:
    """파일 경로를 일자 요약용 '작업 영역' 버킷으로 매핑한다."""
    if path.startswith("apps/_MyApp_"):
        return "_MyApp_(게임)"
    if path.startswith("apps/"):
        seg = path.split("/")
        return f"apps/{seg[1]}" if len(seg) > 1 else "apps"
    if path.startswith("src/"):
        seg = path.split("/")
        return f"src/{seg[1]}" if len(seg) > 1 else "src"
    if path.startswith(("doc/", "doc/")):
        return "문서"
    if path.startswith(("include/", "lib/", "extern/")):
        return "벤더/deps"
    if path.startswith("samples/"):
        return "구 샘플"
    if path.startswith("test/"):
        return "테스트"
    if path.startswith("cmake/"):
        return "빌드(cmake)"
    if path.startswith(("shell/", "scripts/")):
        return "스크립트"
    if path.startswith("resources/"):
        return "리소스"
    if path.startswith(".github/"):
        return "CI"
    if "/" not in path:
        return "설정/루트"  # .gitmodules, .gitignore, .clang-format, README 등
    return "기타"


def slugify(heading: str) -> str:
    """GitHub 스타일 헤딩 앵커 슬러그(소문자/공백→하이픈/문장부호 제거)."""
    out = []
    for ch in heading.lower():
        if ch.isalnum() or ch in "-_가-힣":  # 한글/영숫자 유지
            out.append(ch)
        elif ch.isspace():
            out.append("-")
        # 그 외 문장부호는 제거
    return "".join(out)


# ----------------------------------------------------------------------------
# 추출 (단계 1)
# ----------------------------------------------------------------------------
def get_commit_hashes(rev_range: str) -> list[str]:
    """범위 내 first-parent 커밋 해시를 시간순(오래된→최신)으로 반환한다."""
    out = run_git(["log", "--first-parent", "--reverse", "--format=%H", rev_range])
    return out.split()


def get_meta(commit: str) -> dict:
    """커밋 한 건의 메타데이터(해시/작성자/날짜/제목/본문)를 반환한다."""
    fmt = UNIT_SEP.join(["%H", "%h", "%an", "%ad", "%s", "%b"])
    out = run_git(["show", "-s", "--date=short", f"--format={fmt}", commit])
    parts = out.split(UNIT_SEP)
    # 본문(%b)은 여러 줄일 수 있으므로 마지막 필드로 둔다.
    full, short, author, day, subject, body = (parts + [""] * 6)[:6]
    return {
        "hash": full.strip(),
        "short": short.strip(),
        "author": author.strip(),
        "date": day.strip(),
        "subject": subject.strip(),
        "body": body.strip("\n").rstrip(),
    }


def get_numstat(commit: str) -> list[dict]:
    """커밋의 파일별 증감 통계를 반환한다. 바이너리는 add/del 이 None."""
    out = run_git(["show", "--first-parent", "-M", "--numstat", "--format=", commit])
    files = []
    for line in out.splitlines():
        if not line.strip():
            continue
        cols = line.split("\t")
        if len(cols) < 3:
            continue
        add_s, del_s = cols[0], cols[1]
        path = normalize_path("\t".join(cols[2:]))
        add = None if add_s == "-" else int(add_s)
        dele = None if del_s == "-" else int(del_s)
        files.append(
            {
                "path": path,
                "add": add,
                "del": dele,
                "binary": add is None,
                "code": is_code_file(path),
            }
        )
    return files


def get_code_patch(commit: str) -> list[dict]:
    """코드 파일(.cpp/.h/.gitmodules)에 한정한 diff 를 파일별로 파싱해 반환한다.

    pathspec 으로 벤더 트리(include/lib/extern)를 배제하고 코드 확장자만 받는다.
    """
    pathspec = [
        "--",
        "*.cpp",
        "*.h",
        ".gitmodules",
        ":(exclude)include/**",
        ":(exclude)lib/**",
        ":(exclude)extern/**",
    ]
    out = run_git(
        ["show", "--first-parent", "-M", "--format=", "--unified=3", commit, *pathspec]
    )
    return parse_patch(out)


def parse_patch(text: str) -> list[dict]:
    """`git show` 패치 텍스트를 [{path, lines:[...]}] 형태로 분해한다.

    diff 헤더(index/mode/rename/Binary 등)는 버리고 hunk(@@~) 이하 본문만 남긴다.
    """
    files: list[dict] = []
    cur: dict | None = None
    skip_prefixes = (
        "index ",
        "new file mode",
        "deleted file mode",
        "old mode",
        "new mode",
        "similarity index",
        "dissimilarity index",
        "rename from",
        "rename to",
        "copy from",
        "copy to",
        "Binary files",
        "GIT binary patch",
    )
    for line in text.splitlines():
        if line.startswith("diff --git "):
            if cur is not None:
                files.append(cur)
            cur = {"path": None, "lines": []}
            continue
        if cur is None:
            continue
        if line.startswith("--- "):
            p = line[4:].strip()
            if cur["path"] is None and p != "/dev/null":
                cur["path"] = strip_ab(p)
            continue
        if line.startswith("+++ "):
            p = line[4:].strip()
            if p != "/dev/null":
                cur["path"] = strip_ab(p)
            continue
        if line.startswith(skip_prefixes):
            continue
        cur["lines"].append(line)
    if cur is not None:
        files.append(cur)
    return [f for f in files if f["path"] and f["lines"]]


def collect(rev_range: str, limit: int | None) -> list[dict]:
    """범위 내 모든 커밋을 추출해 구조화 리스트로 반환한다(단계 1)."""
    hashes = get_commit_hashes(rev_range)
    if limit is not None:
        hashes = hashes[:limit]
    total = len(hashes)
    commits = []
    for idx, h in enumerate(hashes, start=1):
        meta = get_meta(h)
        files = get_numstat(h)
        code_files = [f for f in files if f["code"]]
        noncode_files = [f for f in files if not f["code"]]
        meta["files"] = files
        meta["code_files"] = code_files
        meta["noncode_count"] = len(noncode_files)
        meta["code_add"] = sum(f["add"] or 0 for f in code_files)
        meta["code_del"] = sum(f["del"] or 0 for f in code_files)
        meta["patch"] = get_code_patch(h)
        commits.append(meta)
        if idx % 25 == 0 or idx == total:
            sys.stderr.write(f"  ... {idx}/{total} 커밋 추출\n")
    return commits


# ----------------------------------------------------------------------------
# 일자 단위 통합 + 마크다운 렌더 (단계 2)
# ----------------------------------------------------------------------------
def group_by_day(commits: list[dict]) -> list[tuple[str, list[dict]]]:
    """커밋(시간순)을 날짜별로 묶는다. 입력 순서를 보존한다."""
    days: dict[str, list[dict]] = {}
    order: list[str] = []
    for c in commits:
        d = c["date"]
        if d not in days:
            days[d] = []
            order.append(d)
        days[d].append(c)
    return [(d, days[d]) for d in order]


def top_areas(commits: list[dict], topn: int = 6) -> str:
    """하루치 커밋들의 작업 영역 버킷 히스토그램 상위 N개를 문자열로."""
    counter: dict[str, int] = {}
    for c in commits:
        seen = set()
        for f in c["files"]:
            b = bucket_of(f["path"])
            # 한 커밋 안에서 같은 버킷 중복 카운트 방지(파일 수가 아니라 '건드린 커밋 영역' 가중)
            if b in seen:
                continue
            seen.add(b)
            counter[b] = counter.get(b, 0) + 1
    items = sorted(counter.items(), key=lambda kv: (-kv[1], kv[0]))[:topn]
    return ", ".join(f"{name}×{n}" for name, n in items)


def render_diff_block(commit: dict, lines_out: list[str]) -> None:
    """코드 diff 발췌를 캡(파일 수/파일별 줄 수/커밋 총 줄 수) 안에서 렌더한다."""
    patch = commit["patch"]
    if not patch:
        return
    shown_files = 0
    used_lines = 0
    for f in patch:
        if shown_files >= MAX_DIFF_FILES_PER_COMMIT:
            remaining = len(patch) - shown_files
            lines_out.append(f"_… diff 생략: 코드 파일 {remaining}개 더 (파일 수 캡)_")
            lines_out.append("")
            break
        if used_lines >= MAX_DIFF_LINES_PER_COMMIT:
            remaining = len(patch) - shown_files
            lines_out.append(f"_… diff 생략: 코드 파일 {remaining}개 더 (커밋 줄 수 캡)_")
            lines_out.append("")
            break

        body = f["lines"]
        budget = min(
            MAX_DIFF_LINES_PER_FILE, MAX_DIFF_LINES_PER_COMMIT - used_lines
        )
        clipped = body[:budget]
        omitted = len(body) - len(clipped)

        lines_out.append(f"`{f['path']}`")
        lines_out.append("```diff")
        lines_out.extend(clipped)
        if omitted > 0:
            lines_out.append(f"… (+{omitted}줄 생략)")
        lines_out.append("```")
        lines_out.append("")

        shown_files += 1
        used_lines += len(clipped)


def render_markdown(commits: list[dict], rev_range: str, start: str, branch: str) -> str:
    """추출 결과를 일자별로 통합한 work_history.md 본문을 생성한다(단계 2)."""
    grouped = group_by_day(commits)
    days = [d for d, _ in grouped]
    span = f"{days[0]} ~ {days[-1]}" if days else "(없음)"

    L: list[str] = []
    L.append("# game/main 작업 히스토리 — 게임/엔진 전환기")
    L.append("")
    L.append(
        "이 문서는 `scripts/traversal_git.py` 가 자동 생성한다. "
        "수기 편집 금지 — 갱신은 스크립트 재실행."
    )
    L.append("")
    L.append(f"- **브랜치**: `{branch}`")
    L.append(f"- **범위**: `{rev_range}` (first-parent, 시간순)")
    L.append(
        f"- **시작 커밋**: `{start}` `[init] : game project` "
        "— SuperBible 코스워크 이후, Core/Client 분리 + SJH::engine 전환기"
    )
    L.append(f"- **기간**: {span} · **작업일 {len(days)}일** · **커밋 {len(commits)}개**")
    L.append(
        "- **diff 발췌 범위**: `.cpp` / `.h` / `.gitmodules` 만. "
        "리소스(png 등) · CMake · 셰이더 · 문서는 노이즈로 제외하고 "
        "'비코드 N개 파일' 표기로만 남김."
    )
    L.append(f"- **생성일**: {date.today().isoformat()}")
    L.append("")
    L.append("---")
    L.append("")

    # 목차
    L.append("## 목차")
    L.append("")
    for d, cs in grouped:
        wd = WEEKDAY_KR[date.fromisoformat(d).weekday()]
        heading = f"{d} ({wd})"
        L.append(f"- [{heading}](#{slugify(heading)}) — 커밋 {len(cs)}개")
    L.append("")
    L.append("---")
    L.append("")

    # 일자별 본문
    for d, cs in grouped:
        wd = WEEKDAY_KR[date.fromisoformat(d).weekday()]
        L.append(f"## {d} ({wd})")
        L.append("")

        code_add = sum(c["code_add"] for c in cs)
        code_del = sum(c["code_del"] for c in cs)
        noncode = sum(c["noncode_count"] for c in cs)
        L.append(
            f"**커밋 {len(cs)}개 · 코드 +{code_add} / -{code_del} 줄 · 비코드 {noncode} 파일**"
        )
        L.append("")
        L.append(f"주요 영역: {top_areas(cs)}")
        L.append("")

        # 그날 커밋 한눈에 보기
        L.append("그날 커밋:")
        for c in cs:
            L.append(f"- `{c['short']}` {c['subject']}")
        L.append("")

        # 커밋별 상세
        for c in cs:
            L.append(f"### `{c['short']}` {c['subject']}")
            L.append("")
            if c["body"]:
                for bl in c["body"].splitlines():
                    L.append(f"> {bl}" if bl.strip() else ">")
                L.append("")

            # 내용이 바뀐 코드 파일과 순수 이동(rename, 0/0)을 분리한다.
            changed = [
                f
                for f in c["code_files"]
                if f["binary"] or (f["add"] or 0) or (f["del"] or 0)
            ]
            renamed = [
                f
                for f in c["code_files"]
                if not f["binary"] and not (f["add"] or 0) and not (f["del"] or 0)
            ]
            if changed:
                L.append("| 코드 파일 | + | - |")
                L.append("|---|---:|---:|")
                for f in changed:
                    add = "—" if f["binary"] else f["add"]
                    dele = "—" if f["binary"] else f["del"]
                    L.append(f"| `{f['path']}` | {add} | {dele} |")
                L.append("")
            if renamed:
                L.append(f"_코드 파일 {len(renamed)}개 단순 이동/무변경(rename) — 목록 생략._")
                L.append("")
            if not changed and not renamed:
                L.append("_코드(.cpp/.h/.gitmodules) 변경 없음._")
                L.append("")

            if c["noncode_count"]:
                L.append(
                    f"_비코드 {c['noncode_count']}개 파일(리소스/빌드/셰이더/문서) 변경 — diff 생략._"
                )
                L.append("")

            render_diff_block(c, L)

        L.append("---")
        L.append("")

    return "\n".join(L).rstrip() + "\n"


# ----------------------------------------------------------------------------
# 엔트리포인트
# ----------------------------------------------------------------------------
def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--branch", default=DEFAULT_BRANCH, help="대상 브랜치")
    parser.add_argument("--start", default=DEFAULT_START, help="시작 커밋(이 커밋부터 포함)")
    parser.add_argument(
        "--output", default=str(DEFAULT_OUTPUT), help="생성할 work_history.md 경로"
    )
    parser.add_argument("--json", dest="json_path", default=None, help="추출 원본 JSON 덤프 경로")
    parser.add_argument("--limit", type=int, default=None, help="앞 N개 커밋만 처리(점검용)")
    args = parser.parse_args(argv)

    rev_range = f"{args.start}^..{args.branch}"
    sys.stderr.write(f"[1/2] 깃 히스토리 추출: {rev_range}\n")
    commits = collect(rev_range, args.limit)
    sys.stderr.write(f"      커밋 {len(commits)}개 추출 완료\n")

    if args.json_path:
        Path(args.json_path).write_text(
            json.dumps(commits, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        sys.stderr.write(f"      추출 원본 JSON → {args.json_path}\n")

    sys.stderr.write("[2/2] 일자 단위 통합 → work_history.md 렌더\n")
    md = render_markdown(commits, rev_range, args.start, args.branch)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(md, encoding="utf-8")

    days = len({c["date"] for c in commits})
    sys.stderr.write(
        f"      완료: {out_path} ({len(md.splitlines())}줄, "
        f"커밋 {len(commits)}개 / 작업일 {days}일)\n"
    )


if __name__ == "__main__":
    main()

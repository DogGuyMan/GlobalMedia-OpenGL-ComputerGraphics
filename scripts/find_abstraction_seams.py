#!/usr/bin/env python3
"""그래픽 API / 수학 라이브러리 추상화 경계(seam) 전수조사기.

목적은 *교체 가능성* 확보다 — 두 개의 독립된 seam 을 정확히 찾아 표시한다:

  [RHI seam]  순수 OpenGL API.  언젠가 Vulkan/Metal 백엔드로 교체하려면
              이 지점들이 backend-중립 인터페이스 뒤로 숨어야 한다.
  [MATH seam] glm 수학 라이브러리.  그래픽 API 독립 수학 라이브러리(예: Eigen)
              로 교체하려면 이 지점들이 중립 타입 뒤로 숨어야 한다.

함수 호출만 보면 경계를 놓친다. 헤더 인터페이스에 새는 `GLuint` 핸들,
`GL_*` enum, `#include <GL/..>` / `#include <glm/..>` 도 전부 교체 대상이라
*카테고리별로* 태깅해 보고한다. 코드는 수정하지 않는다.

카테고리 (각 매치 앞에 [태그] 표기):
  gl-call     호출  : `\\bgl[A-Z]\\w*\\s*\\(`  예) glClear( , glBindBuffer(   ← 둘째 글자 대문자라 glm/glfw 자연 배제
  gl-type     타입  : GLuint/GLenum/GLint ...   인터페이스로 새는 핸들/스칼라 (RHI 누수의 핵심)
  gl-const    상수  : `\\bGL_[A-Z0-9_]+`        예) GL_TRIANGLES, GL_DEPTH_TEST
  gl-include  헤더  : #include <GL/gl3w.h> 류
  glm         사용  : `\\bglm::\\w+`            예) glm::vec3, glm::mat4, glm::radians
  glm-include 헤더  : #include <glm/..>

seam 묶음 플래그:
  (기본)        gl-call 만           — RHI 핵심(부수효과 호출)
  --rhi         gl-call + type + const + include  — RHI seam *전체* (교체 영향 범위)
  --math        glm + glm-include                 — MATH seam 전체
  --all         두 seam 전부

granular: --types / --constants / --includes / --glm 으로 개별 토글도 가능.

검사 범위 (기본): src/  +  apps/_MyApp_/src   (.cpp / .h / .hpp / .cc)
주석(// , /* */)과 문자열 리터럴 안의 매치는 best-effort 로 제거한다.

사용:
  python3 scripts/find_gl_calls.py --rhi --summary    # RHI seam 카테고리별 집계
  python3 scripts/find_gl_calls.py --math --by-file   # MATH seam 파일별 건수
  python3 scripts/find_gl_calls.py --all              # 두 seam 라인 단위 전수
  python3 scripts/find_gl_calls.py --rhi --path src/program   # 특정 모듈만
  python3 scripts/find_gl_calls.py --rhi --exclude device_context   # 이미 추상화된 곳 제외

종료 코드: 매치가 하나라도 있으면 1, 없으면 0 (CI/seam-gate 용도).
정규식 휴리스틱이라 100% 정확하진 않다 — seam 추출의 출발점이며 육안 검토가 전제.
"""

import argparse
import os
import re
import sys

# 기본 검사 루트 (다른 스크립트와 동일 컨벤션)
DEFAULT_ROOTS = ["src", "apps/_MyApp_/src"]
SOURCE_EXTS = (".cpp", ".h", ".hpp", ".cc", ".cxx", ".hh")

# (카테고리, 정규식) — 둘째 글자 대문자 규칙이 glm::/glfw*/glad* 를 gl-call 에서 배제
PATTERNS = {
    "gl-call": re.compile(r"\bgl[A-Z]\w*(?=\s*\()"),
    "gl-type": re.compile(
        r"\bGL(?:uint|int|enum|float|double|boolean|bitfield|sizeiptr|sizei|intptr|ubyte|byte|ushort|short|char|clampf|clampd|void|fixed|half|sync|u?int64)\b"
    ),
    "gl-const": re.compile(r"\bGL_[A-Z0-9_]+"),
    "gl-include": re.compile(r'#\s*include\s*[<"][^>"]*(?:gl3w|glcorearb|glext|GL/gl|OpenGL/)[^>"]*[>"]'),
    "glm": re.compile(r"\bglm::\w+"),
    "glm-include": re.compile(r'#\s*include\s*[<"]glm/[^>"]*[>"]'),
}

RHI_CATS = ["gl-call", "gl-type", "gl-const", "gl-include"]
MATH_CATS = ["glm", "glm-include"]


def strip_comments_and_strings(line, in_block):
    """한 줄에서 // 주석, /* */ 블록, "문자열" 을 공백으로 치환.

    멀티라인 블록 주석 상태(in_block)를 이어받아 갱신해 반환한다.
    완벽한 파서는 아니지만(중첩/이스케이프 코너케이스) 오탐 제거엔 충분.
    주의: #include 줄은 < > 안이 문자열이 아니므로 보존해야 해서 통째로 살린다.
    """
    if line.lstrip().startswith("#"):
        return line, in_block
    out = []
    i = 0
    n = len(line)
    while i < n:
        if in_block:
            end = line.find("*/", i)
            if end == -1:
                out.append(" " * (n - i))
                i = n
            else:
                out.append(" " * (end + 2 - i))
                i = end + 2
                in_block = False
            continue

        two = line[i : i + 2]
        if two == "//":
            out.append(" " * (n - i))
            break
        if two == "/*":
            out.append("  ")
            i += 2
            in_block = True
            continue
        ch = line[i]
        if ch == '"' or ch == "'":
            quote = ch
            out.append(" ")
            i += 1
            while i < n:
                if line[i] == "\\":  # 이스케이프 한 글자 건너뜀
                    out.append("  ")
                    i += 2
                    continue
                if line[i] == quote:
                    out.append(" ")
                    i += 1
                    break
                out.append(" ")
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out), in_block


def iter_source_files(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [
                d
                for d in dirnames
                if not d.startswith(".")
                and not d.startswith("build")
                and d not in ("extern", "lib", "include", "__pycache__")
            ]
            for fn in filenames:
                if fn.endswith(SOURCE_EXTS):
                    yield os.path.join(dirpath, fn)


def scan_file(path, cats, strip):
    """파일에서 매치를 (line_no, col, cat, name, raw_line) 리스트로 반환."""
    hits = []
    in_block = False
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for lineno, raw in enumerate(f, 1):
                raw = raw.rstrip("\n")
                if strip:
                    scan_text, in_block = strip_comments_and_strings(raw, in_block)
                else:
                    scan_text = raw
                for cat in cats:
                    for m in PATTERNS[cat].finditer(scan_text):
                        hits.append((lineno, m.start() + 1, cat, m.group(0), raw.strip()))
    except OSError as e:
        print(f"[warn] {path} 읽기 실패: {e}", file=sys.stderr)
    return hits


def main():
    ap = argparse.ArgumentParser(
        description="그래픽 API(OpenGL) / 수학(glm) 추상화 경계 전수조사 — 코드 미수정, 카테고리별 보고.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--path", action="append", dest="paths", metavar="ROOT", help="검사 루트(파일/디렉토리). 반복 가능. 기본: src, apps/_MyApp_/src")
    ap.add_argument("--exclude", action="append", default=[], metavar="SUBSTR", help="경로에 이 부분문자열이 들어가면 제외. 반복 가능.")
    # seam 묶음
    ap.add_argument("--rhi", action="store_true", help="RHI seam 전체(gl 호출+타입+상수+헤더)")
    ap.add_argument("--math", action="store_true", help="MATH seam 전체(glm 사용+헤더)")
    ap.add_argument("--all", action="store_true", help="RHI + MATH 두 seam 전부")
    # granular 토글
    ap.add_argument("--glm", action="store_true", help="glm:: 사용처 추가")
    ap.add_argument("--types", action="store_true", help="GLuint/GLenum 등 GL 타입 추가")
    ap.add_argument("--constants", action="store_true", help="GL_* 상수 추가")
    ap.add_argument("--includes", action="store_true", help="GL/glm 헤더 include 추가")
    # 출력/동작
    ap.add_argument("--no-strip-comments", action="store_true", help="주석/문자열 내부 매치 제거를 끈다")
    ap.add_argument("--summary", action="store_true", help="카테고리+이름별 집계만")
    ap.add_argument("--by-file", action="store_true", help="파일별 건수 내림차순 요약만")
    args = ap.parse_args()

    roots = args.paths if args.paths else DEFAULT_ROOTS
    roots = [r for r in roots if os.path.exists(r)]
    if not roots:
        print("[error] 검사할 경로가 없다.", file=sys.stderr)
        return 2

    # 활성 카테고리 결정
    cats = set()
    if args.all:
        cats.update(RHI_CATS + MATH_CATS)
    if args.rhi:
        cats.update(RHI_CATS)
    if args.math:
        cats.update(MATH_CATS)
    if args.glm:
        cats.add("glm")
    if args.types:
        cats.add("gl-type")
    if args.constants:
        cats.add("gl-const")
    if args.includes:
        cats.update(["gl-include", "glm-include"])
    if not cats:
        cats.add("gl-call")  # 기본: RHI 핵심 호출만
    # 보고/순회 안정 순서
    order = ["gl-call", "gl-type", "gl-const", "gl-include", "glm", "glm-include"]
    cats = [c for c in order if c in cats]
    strip = not args.no_strip_comments

    per_file = {}                       # path -> [hit, ...]
    cat_total = {c: 0 for c in cats}    # cat -> count
    name_count = {}                     # (cat, name) -> count
    total = 0

    for path in iter_source_files(roots):
        if any(ex in path for ex in args.exclude):
            continue
        hits = scan_file(path, cats, strip)
        if not hits:
            continue
        per_file[path] = hits
        total += len(hits)
        for _, _, cat, name, _ in hits:
            cat_total[cat] += 1
            key = (cat, name)
            name_count[key] = name_count.get(key, 0) + 1

    def cat_breakdown():
        parts = [f"{c}={cat_total[c]}" for c in cats if cat_total[c]]
        return "  ".join(parts) if parts else "(없음)"

    # ---- 출력 ----
    if args.summary:
        for (cat, name), cnt in sorted(name_count.items(), key=lambda kv: (kv[0][0], -kv[1], kv[0][1])):
            print(f"{cnt:5d}  [{cat}] {name}")
        print(f"\n카테고리: {cat_breakdown()}")
        print(f"총 {total}건 / 파일 {len(per_file)}개")
    elif args.by_file:
        for path, hits in sorted(per_file.items(), key=lambda kv: (-len(kv[1]), kv[0])):
            print(f"{len(hits):5d}  {path}")
        print(f"\n카테고리: {cat_breakdown()}")
        print(f"총 {total}건 / 파일 {len(per_file)}개")
    else:
        for path in sorted(per_file):
            hits = per_file[path]
            print(f"\n=== {path}  ({len(hits)}건) ===")
            for lineno, col, cat, name, text in hits:
                print(f"  {path}:{lineno}:{col}: [{cat}] {name}    | {text}")
        print(f"\n카테고리: {cat_breakdown()}")
        print(f"총 {total}건 / 파일 {len(per_file)}개")

    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())

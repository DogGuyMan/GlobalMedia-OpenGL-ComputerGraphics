#!/usr/bin/env python3
r"""vmath -> glm 자유함수/identity *안전 1:1* 일괄 치환 (allowlist 방식).

배경:
  본 브랜치는 `<vmath.h>` 의존을 전량 glm 으로 전환 중이다. 벡터/행렬 *타입* 은
  이미 `glm::vec*/mat*` 로 바뀌었고, 남은 잔재는 `vmath::` *자유 함수* 와
  vmath-ism 누수(`glm::matN::identity()` - glm 엔 없는 메서드) 뿐이다.

이 스크립트가 하는 일 (시그니처/단위가 vmath 와 *완전히 동일* 한 것만):
  1. vmath::{normalize,dot,cross,length,radians,degrees,max}  -> glm::동일이름
  2. glm::{mat2,mat3,mat4,dmat2,dmat3,dmat4}::identity()       -> glm::동일타입(1.0f)

이 스크립트가 *절대 건드리지 않는* 것 (수동 수정 대상 - 시그니처/단위 다름):
  - vmath::translate(v)        -> glm::translate(glm::mat4(1.0f), v)        [행렬 1st 인자]
  - vmath::scale(v)            -> glm::scale(glm::mat4(1.0f), v)            [행렬 1st 인자]
  - vmath::rotate(deg,x,y,z)   -> glm::rotate(mat4(1.0f), radians(deg), v) [행렬+radian]
  - vmath::perspective(degFov) -> glm::perspective(glm::radians(degFov),…) [radian]
  - vmath::lookat              -> glm::lookAt                              [이름 변경]
  - vmath::ortho/frustum 등
  이들은 치환하지 않고 *잔존 목록* 으로 보고만 한다 (silent 오변환 금지).
  주의: 위 빌더 사용 파일은 `#include <glm/gtc/matrix_transform.hpp>` 가 추가로 필요하다.

transpose 안전성:
  vmath 와 glm 은 둘 다 column-major 이고 operator* / m[col][row] 인덱싱 규약이
  동일하다. 따라서 행렬 곱 *순서* 는 보존된다 (T*R*S 그대로). 본 스크립트가 다루는
  자유 함수(dot/cross/normalize/...)는 행렬 전치와 무관하다.

대상 파일:
  인자 없으면 *현재 git unstaged 변경 파일* (`git diff --name-only`) 중 C/C++ 소스.
  경로를 인자로 주면 해당 경로(파일/디렉토리)만 스캔.

기본은 dry-run(미리보기). 실제 기록은 --apply.

사용법:
    python3 scripts/migrate_vmath_to_glm.py                 # unstaged 변경 파일 dry-run
    python3 scripts/migrate_vmath_to_glm.py --apply         # unstaged 변경 파일 in-place
    python3 scripts/migrate_vmath_to_glm.py src apps --apply # 지정 경로 in-place
"""

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
SCAN_EXTS = {".cpp", ".h", ".hpp", ".c", ".cc"}

# ── [규칙 1] 안전 1:1 자유 함수: vmath::FN -> glm::FN ──────────────────────────
#   시그니처·단위·반환형이 glm 과 완전히 동일한 것만 allowlist 로 명시.
#   (translate/scale/rotate/perspective/lookat/ortho/frustum 등은 *의도적 제외*)
SAFE_FUNCS = (
    "normalize",
    "dot",
    "cross",
    "length",
    "radians",
    "degrees",
    "max",
)
RE_SAFE_FUNC = re.compile(r"\bvmath::(" + "|".join(SAFE_FUNCS) + r")\b")

# ── [규칙 2] glm::matN::identity() vmath-ism 누수 -> glm::matN(1.0f) ──────────
#   glm 에는 정적 identity() 가 없다. identity = 대각 1 생성자.
RE_IDENTITY = re.compile(r"\bglm::(d?mat[234](?:x[234])?)::identity\(\)")

# ── 잔존 탐지: 치환 후에도 남는 vmath:: 토큰 (= 수동 수정 대상 후보) ───────────
RE_ANY_VMATH = re.compile(r"\bvmath::([A-Za-z_]\w*)")


def apply_rules(text: str):
    """규칙 1,2 를 적용한 새 텍스트와 (규칙별 치환 횟수) 를 반환."""
    counts = defaultdict(int)

    def sub_func(m):
        counts["func:" + m.group(1)] += 1
        return "glm::" + m.group(1)

    def sub_identity(m):
        counts["identity:" + m.group(1)] += 1
        return "glm::" + m.group(1) + "(1.0f)"

    text = RE_SAFE_FUNC.sub(sub_func, text)
    text = RE_IDENTITY.sub(sub_identity, text)
    return text, counts


def changed_files_from_git():
    """git unstaged 변경 파일(working tree vs index) 중 C/C++ 소스 경로 목록."""
    try:
        out = subprocess.check_output(
            ["git", "-C", str(PROJECT_ROOT), "diff", "--name-only"],
            text=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(f"[!] git diff 실패: {exc}", file=sys.stderr)
        return []
    files = []
    for line in out.splitlines():
        line = line.strip()
        if not line:
            continue
        p = PROJECT_ROOT / line
        if p.suffix in SCAN_EXTS and p.is_file():
            files.append(p)
    return files


def gather_files(targets):
    """명시 경로(파일/디렉토리)를 C/C++ 소스 파일 목록으로 전개."""
    files = []
    for t in targets:
        p = Path(t)
        if not p.is_absolute():
            p = PROJECT_ROOT / p
        if p.is_file() and p.suffix in SCAN_EXTS:
            files.append(p)
        elif p.is_dir():
            for ext in SCAN_EXTS:
                files.extend(p.rglob(f"*{ext}"))
        else:
            print(f"[!] 건너뜀(존재하지 않거나 비대상): {t}", file=sys.stderr)
    # 중복 제거 + 안정 정렬
    return sorted(set(files))


def rel(p: Path) -> str:
    try:
        return str(p.relative_to(PROJECT_ROOT))
    except ValueError:
        return str(p)


def main():
    ap = argparse.ArgumentParser(
        description="vmath -> glm 안전 1:1 일괄 치환 (행렬 빌더는 수동 대상으로 보고만)."
    )
    ap.add_argument(
        "targets",
        nargs="*",
        help="스캔할 파일/디렉토리. 생략 시 git unstaged 변경 파일.",
    )
    ap.add_argument("--apply", action="store_true", help="실제 파일에 기록 (생략 시 dry-run).")
    args = ap.parse_args()

    if args.targets:
        files = gather_files(args.targets)
        source_desc = "지정 경로"
    else:
        files = changed_files_from_git()
        source_desc = "git unstaged 변경 파일"

    if not files:
        print(f"[i] 대상 파일 없음 ({source_desc}).")
        return 0

    total_counts = defaultdict(int)
    changed_files = []
    leftover = defaultdict(list)  # func_name -> [ "path:line" ]

    for path in files:
        try:
            original = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError) as exc:
            print(f"[!] 읽기 실패 {rel(path)}: {exc}", file=sys.stderr)
            continue

        new_text, counts = apply_rules(original)

        if counts:
            changed_files.append((path, counts))
            for k, v in counts.items():
                total_counts[k] += v
            if args.apply and new_text != original:
                path.write_text(new_text, encoding="utf-8")

        # 치환 *후* 텍스트 기준으로 잔존 vmath:: 탐지 (= 수동 수정 대상)
        for i, line in enumerate(new_text.splitlines(), 1):
            for m in RE_ANY_VMATH.finditer(line):
                leftover[m.group(1)].append(f"{rel(path)}:{i}")

    # ── 리포트 ────────────────────────────────────────────────────────────────
    mode = "APPLY (기록함)" if args.apply else "DRY-RUN (미리보기 - 기록 안 함)"
    print(f"=== vmath -> glm 마이그레이션 [{mode}] ===")
    print(f"대상: {source_desc} / 스캔 {len(files)} 파일\n")

    if changed_files:
        print(f"-- 치환 대상 파일 {len(changed_files)} 개 --")
        for path, counts in changed_files:
            detail = ", ".join(f"{k}x{v}" for k, v in sorted(counts.items()))
            print(f"  {rel(path)}  [{detail}]")
        print("\n-- 규칙별 합계 --")
        for k, v in sorted(total_counts.items()):
            print(f"  {k}: {v}")
    else:
        print("치환할 안전 1:1 패턴 없음.")

    # 잔존 vmath:: (SAFE_FUNCS 로 치환되지 못한 것 = 행렬 빌더 등 수동 대상)
    manual = {fn: locs for fn, locs in leftover.items()}
    print()
    if manual:
        print("!! 수동 수정 필요 - 남은 vmath:: (시그니처/단위 차이로 자동 치환 제외) !!")
        for fn, locs in sorted(manual.items()):
            print(f"  vmath::{fn}  ({len(locs)})")
            for loc in locs:
                print(f"      {loc}")
        print("\n  행렬 빌더 변환 규칙:")
        print("    translate(v)      -> glm::translate(glm::mat4(1.0f), v)")
        print("    scale(v)          -> glm::scale(glm::mat4(1.0f), v)")
        print("    rotate(deg,x,y,z) -> glm::rotate(glm::mat4(1.0f), glm::radians(deg), glm::vec3(x,y,z))")
        print("    perspective(deg…) -> glm::perspective(glm::radians(deg), …)")
        print("    lookat            -> glm::lookAt")
        print("    + 해당 파일에 #include <glm/gtc/matrix_transform.hpp> 추가")
    else:
        print("[OK] 잔존 vmath:: 없음 - 모든 vmath 자유함수가 처리되었습니다.")

    if not args.apply and changed_files:
        print("\n(미리보기였습니다. 적용하려면 --apply 를 붙여 다시 실행하세요.)")
    if args.apply and changed_files:
        print("\n[검증 권장] git diff 로 변경을 확인하고, 빌드로 회귀를 확인하세요.")

    return 0


if __name__ == "__main__":
    sys.exit(main())

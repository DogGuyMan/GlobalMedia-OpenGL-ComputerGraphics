#!/usr/bin/env python3
"""문서 경로 참조 감사기 (docs path-reference auditor).

검사 대상: 마크다운 문서가 인용하는 파일 경로가 실제로 존재하는지 (hallucinated path 탐지)
          + 문서가 참조하는 코드보다 30일 이상 낡았는지 (SUPERSEDED 후보 제시).

이 스크립트는 발견 사항을 신뢰도 등급과 함께 보고한다. 문서/코드를 수정하지 않는다.
drift(낡음) 판정은 mtime 휴리스틱이라 사람 검토가 전제다 — 자동 마킹 금지, 후보 제시만.

신뢰도 등급:
  [Auto]      경로 존재 검증 — 결정론적. broken 이면 거의 확정
              (단 compound 표기 / external(extern·fmod·include·build_ninja·.vscode) 경로는 별도 트랙).
  [Heuristic] drift SUPERSEDED 후보 — mtime 기반 추정. 판단 필요.

사용:
  python3 scripts/audit_docs.py                                # 기본 scope (doc/superpowers/plans + specs)
  python3 scripts/audit_docs.py --scope .claude doc/superpowers/plans
  python3 scripts/audit_docs.py --json /tmp/audit_report.json  # 구조화 전체 결과 저장
  python3 scripts/audit_docs.py --quiet                        # 콘솔 요약 억제 (액션 리스트만)

종료 코드: 순수 broken(존재-실패, compound·external 제외) 1건 이상이면 1, 아니면 0.
          (pre-commit/CI 게이트가 이 종료 코드를 재사용한다.)

※ doc/report/score_cpp.py 의 E1 검증 로직과 동일 규약을 자체 상수로 재정의한다.
   (scripts/ 와 doc/report/ 는 배포 단위가 다르므로 cross-import 금지.)
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parent.parent

# ── 상수 (doc/report/score_cpp.py 와 동일 규약 복제) ─────────────────────────
IGNORE_DIRS = {
    "node_modules", ".venv", "venv", ".git", ".next", "dist", "build",
    "__pycache__", ".turbo", ".ruff_cache", ".pytest_cache", ".mypy_cache",
    "target", "out", "coverage", ".cache", ".idea", ".vscode",
    "extern", "resources", "lib", "include", "doxygen", "html",
    "build_ninja", "build_ninja-release", "build_msvc", "build_extern",
    "skills_repo-main", "_workspace",
    # fmod/ — FMOD 독점 SDK 헤더용 최상위 경로 표기(archival plan 문서, 이후 include/fmod/ 로 정착).
    # 라이선스상 저장소에 커밋 불가라 fresh checkout 에선 영구 부재 — broken 판정 제외.
    "fmod",
}

RE_PATH_REF = re.compile(
    r"(?<![A-Za-z0-9_/.\-])"
    r"((?:\./|\.?[A-Za-z0-9_-]+/)[A-Za-z0-9_./-]+\.(?:py|ts|tsx|js|jsx|md|sql|json|yaml|yml|toml|html|css|sh|go|rs|java|kt|rb|php|h|hpp|cpp|cc|inl|cmake|slang|vert|frag|glsl|geom|comp|dot)(?![A-Za-z0-9_]))"
)

# `Foo.h/.cpp` 류 복합 표기 — 단일 파일로 존재 검증이 불가능한 저자 관용구 (별도 트랙으로 스킵)
RE_COMPOUND_REF = re.compile(r"\.\w+/\.\w+$")

# 문서가 스스로 "낡음" 을 표시했는지 런타임 감지 (하드코딩 리스트 금지)
RE_SUPERSEDED_MARK = re.compile(r"SUPERSEDED|폐기|deprecated", re.IGNORECASE)

DRIFT_THRESHOLD_SECONDS = 30 * 86400  # 문서가 코드보다 30일 이상 낡으면 SUPERSEDED 후보


# ── 수집/추출 ────────────────────────────────────────────────────────────────
def discover_markdown_files(scope, repo):
    """scope 각 항목이 디렉토리면 IGNORE_DIRS 를 건너뛰며 재귀적으로 *.md 전부 수집,
    파일이면 그대로 포함. (날짜 기반 임의 파일명이 대상이므로 파일명 필터 없음 — 모든 .md)"""
    found = []
    for entry in scope:
        path = (repo / entry) if not Path(entry).is_absolute() else Path(entry)
        if path.is_file():
            found.append(path)
        elif path.is_dir():
            for md in sorted(path.rglob("*.md")):
                if any(part in IGNORE_DIRS for part in md.relative_to(repo).parts):
                    continue
                found.append(md)
        else:
            print(f"[warn] scope 항목이 존재하지 않음 — 건너뜀: {entry}", file=sys.stderr)
    return found


def extract_refs(text):
    """경로형 참조 추출. 동일 경로 중복 참조는 1건으로 카운트 (score_cpp.py 동일 관례)."""
    return set(RE_PATH_REF.findall(text))


def classify_ref(ref):
    """`Foo.h/.cpp` 복합 표기면 "compound", 아니면 "normal"."""
    return "compound" if RE_COMPOUND_REF.search(ref) else "normal"


def is_external_ref(ref):
    """참조 최상위 세그먼트가 IGNORE_DIRS 소속이면 True.
    서브모듈 미체크아웃(extern/) · 독점 SDK 미설치(fmod/, include/) · 빌드 산출물 미생성(build_ninja/)
    · 로컬 IDE 설정(.vscode/) 등 — "정상적인 fresh checkout" 조차 만족 못 시키는 경로라
    broken 판정에서 제외한다 (문서 오류가 아니라 환경 의존적 부재)."""
    first = ref.split("/", 1)[0]
    return first in IGNORE_DIRS


def _resolve_candidates(ref, doc_path, repo):
    """score_cpp.py E1 과 동일한 4-후보(repo-상대 / 문서-상대 / include/ / src/) +
    apps/*/src/ (멀티앱 중첩 소스 레이아웃 — 앱별 코드가 최상위 src/ 가 아니라
    apps/<앱이름>/src/ 아래 있으므로, 그 경로를 인용하는 문서가 오탐(false positive)되는
    것을 막는다)."""
    candidates = [repo / ref, doc_path.parent / ref, repo / "include" / ref, repo / "src" / ref]
    apps_dir = repo / "apps"
    if apps_dir.is_dir():
        candidates.extend((app_src / ref) for app_src in sorted(apps_dir.glob("*/src")))
    return candidates


def resolve_ref(ref, doc_path, repo):
    """후보 4개 중 하나라도 실존하면 True."""
    return any(c.exists() for c in _resolve_candidates(ref, doc_path, repo))


def _resolve_path(ref, doc_path, repo):
    """실존하는 첫 후보 Path 반환 (drift 계산용). 없으면 None."""
    for c in _resolve_candidates(ref, doc_path, repo):
        if c.exists():
            return c
    return None


# ── drift(낡음) 휴리스틱 ─────────────────────────────────────────────────────
def compute_drift(doc_path, valid_refs, repo):
    """문서 mtime + 30일 < 참조 코드 max mtime 이면 SUPERSEDED 후보 반환 (자동 마킹 금지).
    문서가 이미 스스로 낡음 표시(SUPERSEDED/폐기/deprecated)했으면 None (스킵).
    참조 없는 문서도 None (drift 판정 대상 제외 — 중립)."""
    if not valid_refs:
        return None
    try:
        text = doc_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    if RE_SUPERSEDED_MARK.search(text):
        return None

    newest_file, newest_mtime = None, None
    for ref in valid_refs:
        resolved = _resolve_path(ref, doc_path, repo)
        if resolved is None or not resolved.is_file():
            continue
        mtime = resolved.stat().st_mtime
        if newest_mtime is None or mtime > newest_mtime:
            newest_file, newest_mtime = resolved, mtime
    if newest_mtime is None:
        return None

    doc_mtime = doc_path.stat().st_mtime
    if doc_mtime + DRIFT_THRESHOLD_SECONDS < newest_mtime:
        lag_days = int((newest_mtime - doc_mtime) / 86400)
        return {
            "reason": f"문서가 참조 코드보다 {lag_days}일 낡음 (임계 30일)",
            "code_file": str(newest_file.relative_to(repo)) if newest_file.is_relative_to(repo) else str(newest_file),
            "confidence": "Heuristic",
        }
    return None


# ── 문서 단위 / 전체 감사 ────────────────────────────────────────────────────
def audit_document(doc_path, repo):
    """한 문서의 참조를 전수 검증. refs_total == 0 이면 중립 (broken 0, 경고 없음)."""
    try:
        text = doc_path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        return {
            "path": str(doc_path.relative_to(repo)),
            "refs_total": 0,
            "refs_broken": [],
            "refs_skipped_compound": [],
            "refs_skipped_external": [],
            "drift_candidate": None,
            "read_error": str(e),
        }

    refs = sorted(extract_refs(text))
    broken, skipped_compound, skipped_external, valid = [], [], [], []
    for ref in refs:
        if classify_ref(ref) == "compound":
            skipped_compound.append({"ref": ref, "confidence": "Auto"})
        elif is_external_ref(ref):
            skipped_external.append({"ref": ref, "confidence": "Auto"})
        elif resolve_ref(ref, doc_path, repo):
            valid.append(ref)
        else:
            broken.append({"ref": ref, "confidence": "Auto"})

    return {
        "path": str(doc_path.relative_to(repo)),
        "refs_total": len(refs),
        "refs_broken": broken,
        "refs_skipped_compound": skipped_compound,
        "refs_skipped_external": skipped_external,
        "drift_candidate": compute_drift(doc_path, valid, repo),
    }


def run_audit(scope, repo):
    docs = discover_markdown_files(scope, repo)
    documents = [audit_document(d, repo) for d in docs]
    return {
        "documents": documents,
        "summary": {
            "total_docs": len(documents),
            "total_refs": sum(d["refs_total"] for d in documents),
            "total_broken": sum(len(d["refs_broken"]) for d in documents),
            "total_skipped_compound": sum(len(d["refs_skipped_compound"]) for d in documents),
            "total_skipped_external": sum(len(d["refs_skipped_external"]) for d in documents),
            "drift_candidates": sum(1 for d in documents if d["drift_candidate"]),
        },
    }


# ── 출력 3종 ─────────────────────────────────────────────────────────────────
def write_json(report, path):
    Path(path).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def print_console_summary(report):
    s = report["summary"]
    print("── 문서 경로 참조 감사 요약 ──")
    print(f"  문서 수            : {s['total_docs']}")
    print(f"  참조 수 (dedupe)   : {s['total_refs']}")
    print(f"  broken [Auto]      : {s['total_broken']}")
    print(f"  compound 스킵 [Auto]: {s['total_skipped_compound']}  (Foo.h/.cpp 류 — 존재 검증 불가 표기)")
    print(f"  external 스킵 [Auto]: {s['total_skipped_external']}  (extern/fmod/include/build_ninja/.vscode 등 — 환경 의존적 부재)")
    print(f"  drift 후보 [Heuristic]: {s['drift_candidates']}  (SUPERSEDED 후보 제시만 — 마킹은 사람 몫)")
    for d in report["documents"]:
        dc = d["drift_candidate"]
        if dc:
            print(f"    [Heuristic] {d['path']} — {dc['reason']} (최신 코드: {dc['code_file']})")


def print_action_list(report):
    lines = []
    for d in report["documents"]:
        for b in d["refs_broken"]:
            lines.append(f"{d['path']}: {b['ref']}")
    if lines:
        print("── broken 참조 액션 리스트 ──")
        for line in lines:
            print(line)


def main():
    parser = argparse.ArgumentParser(description="마크다운 문서의 경로 참조 존재 검증 + drift 후보 보고 (문서를 수정하지 않음)")
    # 기본 scope 에서 doc/handoffs 는 제외한다 — 핸드오프는 "아직 없는 파일을 이렇게 만들어라" 식의
    # 의도적 예시 경로를 포함하므로 green 화 자체가 부적절한 디렉토리(정책 정본 = doc/CLAUDE.md Gotchas).
    # .github/workflows/docs-validation.yml 의 CI scope 및 .husky/pre-commit 의 제외 규칙과 동일 정책.
    parser.add_argument("--scope", nargs="+", default=["doc/superpowers/plans", "doc/superpowers/specs"],
                        help="감사 대상 디렉토리/파일 (레포 루트 상대). 기본: doc/superpowers/plans doc/superpowers/specs")
    parser.add_argument("--json", metavar="PATH", help="구조화 전체 결과 JSON 출력 경로")
    parser.add_argument("--quiet", action="store_true", help="콘솔 요약 억제")
    args = parser.parse_args()

    report = run_audit(args.scope, ROOT)

    if args.json:
        write_json(report, args.json)
    if not args.quiet:
        print_console_summary(report)
    print_action_list(report)

    # 종료 코드 게이트: 순수 broken(존재-실패) 1건 이상이면 1 (drift 후보는 영향 없음)
    sys.exit(1 if report["summary"]["total_broken"] >= 1 else 0)


if __name__ == "__main__":
    main()

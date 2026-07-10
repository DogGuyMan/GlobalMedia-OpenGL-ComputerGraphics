#!/usr/bin/env python3
"""번복(rollback) 로그 집계기.

입력: doc/번복기록/rollback-log.jsonl (JSONL, 1줄=1건).
출력: trigger_type 분포 / severity 분포 / turns_estimate 평균(+표본수) 요약.

이 스크립트는 코드를 수정하지 않는다(읽기 전용 집계).

신뢰도 메모:
  - by_trigger_type / by_severity / total 은 로그 파일 내용을 그대로 센 실측 집계다.
  - turns_estimate 는 원본 두 보고서(2026-07-05-번복기록-분석보고서.md,
    AI설계판단오류-분석보고서.md) 모두 대화 턴수를 기록하지 않아 13건 중
    대부분(현재는 전부) null 이다. 평균은 non-null 표본에 대해서만 계산하며,
    표본 수를 반드시 함께 표기한다 — 표본이 적을수록 평균의 대표성이 낮다.
  - trigger_type_ratio 는 "로그된 13건 내부에서의 proxy 비율"일 뿐, 세션 전체
    turn 수 대비 진짜 base rate 가 아니다(전체 세션 수를 아직 추적하지 않음).
    이 비율만으로 "AI가 T1 유형을 몇 % 빈도로 저지른다" 라고 일반화하지 말 것.
"""

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).parent.parent
DEFAULT_LOG = ROOT / "doc/번복기록/rollback-log.jsonl"

TRIGGER_TYPES = ("T1", "T2", "T3", "T4")


def load_rows(log_path: Path):
    """JSONL 을 줄 단위로 읽어 dict 리스트로 반환. 실패 시 sys.exit(1)."""
    if not log_path.exists():
        print(f"[에러] 로그 파일이 없습니다: {log_path}", file=sys.stderr)
        sys.exit(1)
    rows = []
    try:
        text = log_path.read_text(encoding="utf-8")
        for lineno, line in enumerate(text.splitlines(), 1):
            if not line.strip():
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as e:
                print(f"[에러] {log_path}:{lineno} JSON 파싱 실패: {e}", file=sys.stderr)
                sys.exit(1)
    except OSError as e:
        print(f"[에러] {log_path} 읽기 실패: {e}", file=sys.stderr)
        sys.exit(1)
    return rows


def aggregate(rows):
    """rows -> 집계 dict. total/by_trigger_type/trigger_type_ratio/turns_estimate/by_severity."""
    total = len(rows)

    trigger_counts = Counter(r.get("trigger_type") for r in rows)
    by_trigger_type = {t: trigger_counts.get(t, 0) for t in TRIGGER_TYPES}
    trigger_type_ratio = {
        t: (by_trigger_type[t] / total if total else 0.0) for t in TRIGGER_TYPES
    }

    turns_values = [r.get("turns_estimate") for r in rows if r.get("turns_estimate") is not None]
    turns_sample = len(turns_values)
    turns_avg = (sum(turns_values) / turns_sample) if turns_sample else None

    severity_counts = Counter()
    for r in rows:
        sev = r.get("severity")
        severity_counts["null(등급없음)" if sev is None else sev] += 1

    return {
        "total": total,
        "by_trigger_type": by_trigger_type,
        "trigger_type_ratio": trigger_type_ratio,
        "trigger_type_ratio_note": "13건 내부 proxy 비율 — 세션 전체 base rate 아님(전체 세션 수 미추적)",
        "turns_estimate": {
            "average": turns_avg,
            "sample_size": turns_sample,
            "note": "원본에 턴수 기록 없어 대부분 null. 표본 0이면 average=null",
        },
        "by_severity": dict(severity_counts),
    }


def print_summary(stats):
    """사람이 읽는 한국어 콘솔 요약."""
    print("=" * 64)
    print(f"  번복 로그 집계   총 {stats['total']}건")
    print("=" * 64)

    print("\n[trigger_type 분포]")
    for t in TRIGGER_TYPES:
        cnt = stats["by_trigger_type"][t]
        ratio = stats["trigger_type_ratio"][t]
        print(f"  {t}: {cnt:>2}건  ({ratio:.1%})")
    print(f"  * {stats['trigger_type_ratio_note']}")

    te = stats["turns_estimate"]
    print("\n[turns_estimate]")
    if te["sample_size"] == 0:
        print(f"  평균: null (표본 0)")
    else:
        print(f"  평균: {te['average']:.2f}  (표본 {te['sample_size']}건)")
    print(f"  * {te['note']}")

    print("\n[severity 분포]")
    for sev, cnt in sorted(stats["by_severity"].items(), key=lambda kv: kv[0]):
        print(f"  {sev}: {cnt}건")

    print("\n" + "=" * 64)


def main():
    parser = argparse.ArgumentParser(description="번복(rollback) 로그 집계기 (읽기 전용)")
    parser.add_argument(
        "--log",
        default=str(DEFAULT_LOG),
        help=f"rollback-log.jsonl 경로 (기본: {DEFAULT_LOG.relative_to(ROOT)})",
    )
    parser.add_argument(
        "--json",
        default=None,
        help="집계 결과를 JSON 으로 저장할 경로 (선택)",
    )
    args = parser.parse_args()

    rows = load_rows(Path(args.log))
    stats = aggregate(rows)
    print_summary(stats)

    if args.json:
        out_path = Path(args.json)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(
            json.dumps(stats, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        print(f"\n[JSON 저장] {out_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())

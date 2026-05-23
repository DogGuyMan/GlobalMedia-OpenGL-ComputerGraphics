#!/usr/bin/env python3
# macOS `leaks` 도구가 만든 leaklog.txt 를 정제/필터링한다.
# - GLFW 3.0.4 (sb7 vendored, extern/sb7code) macOS 백엔드 시스템 누수와
#   사용자 코드 누수를 콜스택 기준으로 분리
# - 시스템 누수는 분류별 카운트 + 대표 콜스택 1건만 요약
# - 사용자 코드 누수는 원문 그대로 출력 (조사가 필요한 진짜 누수)
#
# Usage:
#   python3 shell/leakloghandler.py [leaklog.txt] [--out filtered.txt]
#   인자 생략 시 ./leaklog.txt 를 읽고 stdout 으로 출력.

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

# 콜스택에 아래 심볼 중 하나라도 포함되면 GLFW vendored 시스템 누수로 분류.
# (extern/sb7code/extern/glfw-3.0.4/src/cocoa_*.m 에서 발생하는 알려진 누수)
SYSTEM_LEAK_MARKERS = (
    "_glfwInitJoysticks",
    "_glfwPlatformGetMonitors",
    "getDisplayName",
    "cocoa_joystick.m",
    "cocoa_monitor.m",
)

# "Leak: 0xADDR  size=N  zone: ZONE  설명문자열"
LEAK_HEADER_RE = re.compile(r"^Leak:\s+\S+\s+size=(\d+)\s+zone:\s*\S+\s+(.+?)\s*$")


def parse_leaks(text):
    """leaklog 본문에서 (header_line, callstack_line, size, desc) 레코드 리스트 추출."""
    leaks = []
    lines = text.splitlines()
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        m = LEAK_HEADER_RE.match(line)
        if not m:
            i += 1
            continue
        size = int(m.group(1))
        desc = m.group(2).strip()
        callstack = ""
        # 다음 비어있지 않은 줄을 Call stack 으로 채집
        j = i + 1
        while j < n:
            stripped = lines[j].strip()
            if stripped.startswith("Call stack:"):
                callstack = lines[j]
                break
            if stripped.startswith("Leak:") or stripped == "":
                # 다음 Leak 블록 또는 빈줄 — 콜스택 없음
                if stripped.startswith("Leak:"):
                    break
            j += 1
        leaks.append((line, callstack, size, desc))
        i = (j + 1) if callstack else (i + 1)
    return leaks


def is_system_leak(callstack):
    return any(marker in callstack for marker in SYSTEM_LEAK_MARKERS)


def render(leaks, source_path):
    """정제 리포트를 문자열로 반환."""
    system_by_desc = defaultdict(lambda: {"count": 0, "size": 0, "sample": None})
    user_leaks = []
    total_size = 0

    for header, callstack, size, desc in leaks:
        total_size += size
        if is_system_leak(callstack):
            entry = system_by_desc[desc]
            entry["count"] += 1
            entry["size"] += size
            if entry["sample"] is None:
                entry["sample"] = (header, callstack)
        else:
            user_leaks.append((header, callstack, size))

    sys_count = sum(v["count"] for v in system_by_desc.values())
    sys_size = sum(v["size"] for v in system_by_desc.values())
    usr_size = sum(s for _, _, s in user_leaks)

    out = []
    out.append("=== leaklog 정제 보고서 ===")
    out.append(f"입력 파일       : {source_path}")
    out.append(f"총 누수 건수    : {len(leaks):,}")
    out.append(f"총 누수 바이트  : {total_size:,}")
    out.append(f"  시스템 누수   : {sys_count:,}건 / {sys_size:,} B  (GLFW 3.0.4 vendored)")
    out.append(f"  사용자 누수   : {len(user_leaks):,}건 / {usr_size:,} B")
    out.append("")

    out.append("=== 시스템 누수 분류 요약 (GLFW 3.0.4 vendored, sb7code) ===")
    if not system_by_desc:
        out.append("(해당 없음)")
    else:
        for desc, info in sorted(system_by_desc.items(), key=lambda kv: -kv[1]["count"]):
            out.append(f"  [{info['count']:5d}건 / {info['size']:>10,} B] {desc}")
        out.append("")
        out.append("--- 분류별 대표 콜스택 1건 ---")
        for desc, info in sorted(system_by_desc.items(), key=lambda kv: -kv[1]["count"]):
            out.append(f"# {desc}")
            out.append(info["sample"][0])
            out.append(info["sample"][1])
            out.append("")

    out.append("=== 사용자 코드 누수 (조사 필요) ===")
    if not user_leaks:
        out.append("(사용자 코드 누수 0건 — 모든 누수가 vendored 라이브러리에서 발생)")
    else:
        for header, callstack, _ in user_leaks:
            out.append(header)
            out.append(callstack)
            out.append("")

    return "\n".join(out) + "\n"


def main(argv):
    ap = argparse.ArgumentParser(description="leaklog.txt 정제기 (GLFW vendored 시스템 누수 분리)")
    ap.add_argument("input", nargs="?", default="leaklog.txt", help="leaks 도구가 만든 텍스트 파일 경로")
    ap.add_argument("--out", "-o", default=None, help="결과를 파일로 저장 (생략 시 stdout)")
    args = ap.parse_args(argv[1:])

    path = Path(args.input)
    if not path.exists():
        print(f"입력 파일 없음: {path}", file=sys.stderr)
        return 1

    text = path.read_text(encoding="utf-8", errors="replace")
    leaks = parse_leaks(text)
    report = render(leaks, path)

    if args.out:
        Path(args.out).write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

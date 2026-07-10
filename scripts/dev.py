#!/usr/bin/env python3
"""dev.py — 프로젝트 개발 작업 통합 CLI (크로스 플랫폼).

기존 shell/ 의 sh/bat/ps1 삼중 스크립트를 Python 단일 진입점으로 통합한 것.
플랫폼 감지(macOS/Linux=Ninja, Windows=MSVC)를 한 곳에서 처리한다.

사용 예:
    python3 scripts/dev.py all debug _MyApp_       # clean -> configure -> build -> run
    python3 scripts/dev.py configure release
    python3 scripts/dev.py build debug _MyApp_
    python3 scripts/dev.py run debug _MyApp_ --leaks
    python3 scripts/dev.py prepare
    python3 scripts/dev.py extern                   # extern 라이브러리 재빌드
    python3 scripts/dev.py doxygen 8000
    python3 scripts/dev.py move-shaders
    python3 scripts/dev.py copy-skills
    python3 scripts/dev.py resume-claude
    python3 scripts/dev.py schedule-resume --at 05:41

서브커맨드 목록: python3 scripts/dev.py --help
"""

import argparse
import os
import shutil
import subprocess
import sys
import webbrowser
from datetime import datetime, timedelta
from pathlib import Path

# ============================================================
# 공통 상수 / 플랫폼 감지
# ============================================================

IS_WIN = sys.platform.startswith("win")
IS_MAC = sys.platform == "darwin"
IS_LINUX = sys.platform.startswith("linux")

# 이 파일은 scripts/ 에 있고 프로젝트 루트는 그 부모.
ROOT = Path(__file__).resolve().parent.parent


def log(msg: str = "") -> None:
    print(msg, flush=True)


def die(msg: str, code: int = 1) -> "NoReturn":  # type: ignore[name-defined]
    print(msg, file=sys.stderr, flush=True)
    raise SystemExit(code)


def run(cmd, cwd=None, env=None, check=True):
    """명령을 그대로 스트리밍 실행. check=True 면 실패 시 예외 종료."""
    printable = " ".join(str(c) for c in cmd)
    log(f"$ {printable}")
    result = subprocess.run(cmd, cwd=str(cwd) if cwd else None, env=env)
    if check and result.returncode != 0:
        die(f"명령 실패 (exit {result.returncode}): {printable}", result.returncode)
    return result.returncode


# ============================================================
# 빌드 파이프라인: prepare / configure / build / run / all
# ============================================================


def cmd_prepare(_args) -> int:
    """build* 디렉토리 전체 삭제 (구 CMakePrepare)."""
    removed = 0
    for path in sorted(ROOT.glob("build*")):
        if path.is_dir():
            log(f"Removing {path}")
            shutil.rmtree(path, ignore_errors=True)
            removed += 1
    if removed == 0:
        log("삭제할 build* 디렉토리가 없습니다.")
    return 0


def _configure_preset(build_type: str) -> str:
    if IS_WIN:
        # MSVC 프리셋은 Debug/Release 가 동일한 configurePreset 을 공유 (build 시 --config 로 분기)
        return "msvc-2022"
    return "ninja" if build_type == "debug" else "ninja-release"


def cmd_configure(args) -> int:
    """cmake configure (구 CMakeConfigureAndGenerate)."""
    preset = _configure_preset(args.build_type)
    return run(["cmake", "--preset", preset, "-S", str(ROOT)])


def _build_dir(build_type: str) -> Path:
    return ROOT / ("build_ninja" if build_type == "debug" else "build_ninja-release")


def cmd_build(args) -> int:
    """cmake build (구 CMakeBuild)."""
    target = args.target
    if IS_WIN:
        preset = "msvc-2022" if args.build_type == "debug" else "msvc-2022-release"
        cmd = ["cmake", "--build", "--preset", preset]
        if target:
            cmd += ["--target", target]
        return run(cmd)

    build_dir = _build_dir(args.build_type)
    if target:
        # 중첩 경로(box2d_demo/demo1)는 '/' -> '_' 로 변환해 cmake 고유 타겟명 생성.
        target_name = target.replace("/", "_")
        return run(["cmake", "--build", str(build_dir), "--target", target_name])
    return run(["cmake", "--build", str(build_dir)])


def _tee_run(cmd, cwd, log_path: Path, env=None) -> int:
    """cmd 를 실행하며 stdout/stderr 를 콘솔과 log_path 에 동시 기록 (tee)."""
    proc = subprocess.Popen(
        cmd,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        universal_newlines=True,
    )
    with open(log_path, "w", encoding="utf-8", errors="replace") as fp:
        assert proc.stdout is not None
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            fp.write(line)
    return proc.wait()


def cmd_run(args) -> int:
    """빌드 산출물 실행 (구 CMakeExecute). macOS 는 --leaks 지원."""
    target = args.target
    build_type = args.build_type

    if IS_WIN:
        config_dir = "Debug" if build_type == "debug" else "Release"
        exec_dir = ROOT / "build_msvc" / "apps" / target / config_dir
        exe_name = f"{target}.exe"
        exe_path = exec_dir / exe_name
        if not exe_path.exists():
            die(f"실행 파일을 찾을 수 없습니다: {exe_path}")
        # 리소스 상대경로 해결을 위해 실행 파일 디렉토리에서 실행
        return run([str(exe_path)], cwd=exec_dir)

    # macOS / Linux (Ninja)
    exec_basename = Path(target).name  # 중첩경로 box2d_demo/demo1 -> demo1
    exec_dir = _build_dir(build_type) / "apps" / target
    exe_path = exec_dir / exec_basename

    if not exec_dir.is_dir():
        release_suffix = "-release" if build_type == "release" else ""
        die(
            f"빌드 디렉토리가 없습니다: {exec_dir}\n"
            f"먼저 빌드하세요:\n"
            f"  python3 scripts/dev.py build {build_type} {target}\n"
            f"  (또는 cmake --build --preset ninja{release_suffix} --target {exec_basename})"
        )
    if not (exe_path.exists() and os.access(exe_path, os.X_OK)):
        die(
            f"실행 파일이 없습니다: {exe_path}\n"
            f"타겟이 apps/CMakeLists.txt 에서 활성화(주석 해제)되어 있는지 확인하세요."
        )

    if args.leaks:
        if not IS_MAC:
            die(f"--leaks 옵션은 macOS 에서만 지원됩니다. (현재: {sys.platform})")
        log(f"---------메모리 누수 검사 실행 ({exec_basename})---------")
        leaklog = ROOT / "leaklog.txt"
        env = dict(os.environ)
        env["MallocStackLogging"] = "1"
        env["MallocStackLoggingNoCompact"] = "1"
        with open(leaklog, "w", encoding="utf-8", errors="replace") as fp:
            proc = subprocess.run(
                ["leaks", "--atExit", "--list", "--", f"./{exec_basename}"],
                cwd=str(exec_dir),
                env=env,
                stdout=fp,
                stderr=subprocess.STDOUT,
            )
        log(f"누수 검사 결과: {leaklog}")
        handler = ROOT / "scripts" / "leakloghandler.py"
        if handler.exists():
            log("")
            run([sys.executable, str(handler), str(leaklog)], check=False)
        return proc.returncode

    log(f"---------일반 실행 ({exec_basename})---------")
    log_path = ROOT / "log.txt"
    rc = _tee_run([f"./{exec_basename}"], cwd=exec_dir, log_path=log_path)
    log(f"실행 로그 저장: {log_path}")
    return rc


def cmd_all(args) -> int:
    """prepare -> configure -> build -> run (구 CMakeALL)."""
    cmd_prepare(args)
    rc = cmd_configure(args)
    if rc:
        return rc
    rc = cmd_build(args)
    if rc:
        return rc
    return cmd_run(args)


# ============================================================
# extern 라이브러리 빌드 (구 BuildExternLibs)
# ============================================================


def _find_first(root: Path, pattern: str, contains: str = None) -> Path:
    """root 하위에서 pattern 에 맞는 첫 파일. contains 지정 시 경로에 포함돼야 함."""
    matches = sorted(root.rglob(pattern))
    if contains:
        cl = contains.lower()
        matches = [m for m in matches if cl in str(m).lower()]
    return matches[0] if matches else None


def _copy_headers_tree(src: Path, dst: Path, exts=(".h",)) -> None:
    """src 하위의 지정 확장자 헤더를 디렉토리 구조 보존하여 dst 로 복사 (rsync 대응)."""
    dst.mkdir(parents=True, exist_ok=True)
    for path in src.rglob("*"):
        if path.is_file() and path.suffix in exts:
            rel = path.relative_to(src)
            target = dst / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)


def _copy_headers_flat(src: Path, dst: Path, exts=(".h",)) -> None:
    """src 하위 헤더를 dst 로 평탄화 복사 (Windows xcopy for /R 대응)."""
    dst.mkdir(parents=True, exist_ok=True)
    for path in src.rglob("*"):
        if path.is_file() and path.suffix in exts:
            shutil.copy2(path, dst / path.name)


SB7_CMAKELISTS = """cmake_minimum_required(VERSION 3.14)
project(sb7_build LANGUAGES C CXX)

set(SB7CODE_DIR "" CACHE PATH "sb7code 소스 경로")
set(GLFW_DIR "" CACHE PATH "GLFW 소스 경로")

file(GLOB SB7_SOURCES ${SB7CODE_DIR}/src/sb7/*.cpp ${SB7CODE_DIR}/src/sb7/*.c)
add_library(sb7 STATIC ${SB7_SOURCES})
target_include_directories(sb7 PRIVATE
    ${SB7CODE_DIR}/include
    ${GLFW_DIR}/include)
target_compile_options(sb7 PRIVATE %(warn_flag)s)

if(APPLE)
    target_compile_definitions(sb7 PRIVATE __glext_h_)
endif()
"""


def _patch_file(path: Path, old: str, new: str) -> bool:
    """path 텍스트에서 old 를 new 로 치환 (멱등). 변경 시 True."""
    text = path.read_text(encoding="utf-8", errors="replace")
    if old not in text:
        return False
    path.write_text(text.replace(old, new), encoding="utf-8")
    return True


def _extern_unix() -> int:
    """macOS/Linux extern 빌드 (Ninja)."""
    sb7code = ROOT / "extern" / "sb7code"
    glfw = sb7code / "extern" / "glfw-3.0.4"
    build = ROOT / "build_extern"
    output = build / "output"
    lib_dir = output / ("macos" if IS_MAC else "linux")
    include_dir = output / "include"

    log("=========================================")
    log(f" extern 라이브러리 빌드 ({'macOS' if IS_MAC else 'Linux'})")
    log(f" 출력: {lib_dir}")
    log(f"       {include_dir}")
    log("=========================================")

    for d in (lib_dir, include_dir, build):
        d.mkdir(parents=True, exist_ok=True)

    # ---- 헤더 복사 ----
    log("[1/3] 헤더 복사...")
    shutil.copytree(sb7code / "include", include_dir, dirs_exist_ok=True)
    (include_dir / "GLFW").mkdir(parents=True, exist_ok=True)
    shutil.copytree(glfw / "include" / "GLFW", include_dir / "GLFW", dirs_exist_ok=True)
    # macOS: gl3w.h 가 이미 OpenGL 로드 -> GLFW 가 gl3.h 중복 include 안 하도록 패치
    _patch_file(
        include_dir / "sb7.h",
        "#define GLFW_INCLUDE_GLCOREARB 1",
        "#define GLFW_INCLUDE_NONE 1",
    )
    log(f"  -> {include_dir} 에 복사 완료")

    common = [
        "-G", "Ninja",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    ]

    def configure_build(src, bdir, extra, build_type, targets=None):
        run(["cmake", "-S", str(src), "-B", str(bdir), *common,
             f"-DCMAKE_BUILD_TYPE={build_type}", *extra])
        cmd = ["cmake", "--build", str(bdir)]
        if targets:
            cmd += ["--target", *targets]
        run(cmd)

    # ---- glfw3 (Release + Debug) ----
    log("[2/3] glfw3 빌드...")
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        gb = build / f"glfw_{bt}"
        configure_build(glfw, gb, [
            "-DGLFW_BUILD_EXAMPLES=OFF", "-DGLFW_BUILD_TESTS=OFF",
            "-DGLFW_BUILD_DOCS=OFF", "-DGLFW_INSTALL=OFF",
        ], bt)
        shutil.copy2(gb / "src" / "libglfw3.a", lib_dir / f"libglfw3{suffix}.a")
    log("  -> glfw3 빌드 완료 (Release + Debug)")

    # ---- sb7 (Release + Debug) ----
    log("[3/3] sb7 빌드...")
    sb7_src = build / "sb7_src"
    sb7_src.mkdir(parents=True, exist_ok=True)
    (sb7_src / "CMakeLists.txt").write_text(
        SB7_CMAKELISTS % {"warn_flag": "-w"}, encoding="utf-8"
    )
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        sb = build / f"sb7_{bt}"
        configure_build(sb7_src, sb, [
            f"-DSB7CODE_DIR={sb7code}", f"-DGLFW_DIR={glfw}",
        ], bt)
        shutil.copy2(sb / "libsb7.a", lib_dir / f"libsb7{suffix}.a")
    log("  -> sb7 빌드 완료 (Release + Debug)")

    # ---- Box2D (Release + Debug) ----
    log("[+] Box2D v2.4.1 빌드...")
    box2d = ROOT / "extern" / "box2d"
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        bb = build / f"box2d_{bt}"
        configure_build(box2d, bb, [
            "-DBOX2D_BUILD_TESTBED=OFF", "-DBOX2D_BUILD_UNIT_TESTS=OFF",
            "-DBOX2D_BUILD_DOCS=OFF",
        ], bt)
        art = _find_first(bb, "libbox2d.a")
        if not art:
            die("ERROR: libbox2d.a 를 찾을 수 없음")
        shutil.copy2(art, lib_dir / f"libbox2d{suffix}.a")
    shutil.copytree(box2d / "include" / "box2d", include_dir / "box2d", dirs_exist_ok=True)
    log("  -> Box2D 빌드 완료 (Release + Debug)")

    # ---- Effekseer (Release + Debug) ----
    log("[+] Effekseer 빌드...")
    efk = ROOT / "extern" / "Effekseer"
    efk_hdr = efk / "Dev" / "Cpp"
    efk_opts = [
        # 신형 AppleClang libc++ 가 size_t 를 암묵 전파하지 않아 SIMD 헤더가 깨짐 -> 강제 인클루드 우회
        "-DCMAKE_CXX_FLAGS=-include cstddef",
        "-DBUILD_GL=ON", "-DBUILD_VULKAN=OFF", "-DBUILD_METAL=OFF",
        "-DBUILD_DX9=OFF", "-DBUILD_DX11=OFF", "-DBUILD_DX12=OFF",
        "-DBUILD_VIEWER=OFF", "-DBUILD_EDITOR=OFF", "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_TEST=OFF", "-DBUILD_UNITYPLUGIN=OFF",
        "-DUSE_LIBPNG_LOADER=OFF", "-DNETWORK_ENABLED=OFF",
    ]
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        eb = build / f"effekseer_{bt}"
        configure_build(efk, eb, efk_opts, bt, targets=["Effekseer", "EffekseerRendererGL"])
        efk_lib = _find_first(eb, "libEffekseer.a")
        efk_gl = _find_first(eb, "libEffekseerRendererGL.a")
        if not efk_lib or not efk_gl:
            die("ERROR: Effekseer 정적 라이브러리를 찾을 수 없음")
        shutil.copy2(efk_lib, lib_dir / f"libEffekseer{suffix}.a")
        shutil.copy2(efk_gl, lib_dir / f"libEffekseerRendererGL{suffix}.a")
    # 공개 헤더만(.h) 구조 보존 복사
    _copy_headers_tree(efk_hdr / "Effekseer", include_dir / "Effekseer")
    _copy_headers_tree(efk_hdr / "EffekseerRendererGL", include_dir / "Effekseer")
    log("  -> Effekseer 빌드 완료 (Release + Debug)")

    # ---- assimp (Release + Debug) ----
    log("[+] assimp v5.4.3 빌드...")
    assimp = ROOT / "extern" / "assimp"
    zutil = assimp / "contrib" / "zlib" / "zutil.h"
    # 신형 macOS SDK 에서 TARGET_OS_MAC 가 정의되어 레거시 fdopen 매크로가 활성화되는 것을 우회 (멱등)
    if zutil.exists():
        if _patch_file(
            zutil,
            "#if defined(MACOS) || defined(TARGET_OS_MAC)",
            "#if defined(MACOS)",
        ):
            log("  -> zutil.h 패치 적용 (TARGET_OS_MAC fdopen 매크로 우회)")
    assimp_opts = [
        "-DBUILD_SHARED_LIBS=OFF", "-DASSIMP_BUILD_TESTS=OFF",
        "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF", "-DASSIMP_BUILD_SAMPLES=OFF",
        "-DASSIMP_INSTALL=OFF", "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
        "-DASSIMP_BUILD_ZLIB=ON",
    ]
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        ab = build / f"assimp_{bt}"
        configure_build(assimp, ab, assimp_opts, bt)
        assimp_lib = _find_first(ab, "libassimp*.a")
        if not assimp_lib:
            die("ERROR: libassimp 정적 라이브러리를 찾을 수 없음")
        shutil.copy2(assimp_lib, lib_dir / f"libassimp{suffix}.a")
        zlib_lib = _find_first(ab, "libzlibstatic*.a")
        if zlib_lib:
            shutil.copy2(zlib_lib, lib_dir / f"libzlibstatic{suffix}.a")
        else:
            log("  주의: 번들 zlib 정적 라이브러리를 찾지 못함")
    # 서브모듈을 깨끗한 상태로 되돌림 (패치는 멱등이라 다음 실행 시 자동 재적용)
    subprocess.run(["git", "-C", str(assimp), "checkout", "--", "contrib/zlib/zutil.h"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    shutil.copytree(assimp / "include" / "assimp", include_dir / "assimp", dirs_exist_ok=True)
    # config.h / revision.h 는 소스 트리에 없고 Release 빌드 트리에서 생성됨
    for gen in ("config.h", "revision.h"):
        src = build / "assimp_Release" / "include" / "assimp" / gen
        if src.exists():
            shutil.copy2(src, include_dir / "assimp" / gen)
    log("  -> assimp v5.4.3 빌드 완료 (Release + Debug)")

    # ---- spdlog (Release + Debug) ----
    log("[+] spdlog v1.17.0 빌드...")
    spdlog = ROOT / "extern" / "spdlog"
    for bt, suffix in (("Release", ""), ("Debug", "_d")):
        sb = build / f"spdlog_{bt}"
        configure_build(spdlog, sb, [
            "-DSPDLOG_BUILD_EXAMPLE=OFF", "-DSPDLOG_BUILD_TESTS=OFF",
            "-DSPDLOG_BUILD_BENCH=OFF",
        ], bt)
        spdlog_lib = _find_first(sb, "libspdlog*.a")
        if not spdlog_lib:
            die("ERROR: libspdlog 정적 라이브러리를 찾을 수 없음")
        shutil.copy2(spdlog_lib, lib_dir / f"libspdlog{suffix}.a")
    shutil.copytree(spdlog / "include" / "spdlog", include_dir / "spdlog", dirs_exist_ok=True)
    log("  -> spdlog v1.17.0 빌드 완료 (Release + Debug)")

    log("")
    log("=========================================")
    log(" 빌드 완료!")
    log(f" 라이브러리: {lib_dir}")
    log(f" 헤더:      {include_dir}")
    log("")
    target_lib = "lib/macos" if IS_MAC else "lib/linux"
    log(f" {target_lib}/, include/ 로 필요한 파일을 직접 복사하세요:")
    log(f"   cp {lib_dir}/* {ROOT / target_lib}/")
    log(f"   cp -r {include_dir}/* {ROOT / 'include'}/")
    log("=========================================")
    for f in sorted(lib_dir.iterdir()):
        log(f"  {f.name}")
    return 0


def _extern_windows() -> int:
    """Windows extern 빌드 (Visual Studio 17 2022)."""
    sb7code = ROOT / "extern" / "sb7code"
    glfw = sb7code / "extern" / "glfw-3.0.4"
    build = ROOT / "build_extern"
    output = build / "output"
    lib_dir = output / "windows"
    include_dir = output / "include"

    # 메인 프로젝트와 CRT 일치: /MT (Release), /MTd (Debug)
    crt = [
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>",
    ]
    gen = ["-G", "Visual Studio 17 2022", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"]

    log("=========================================")
    log(" extern 라이브러리 빌드 (Windows)")
    log(f" 출력: {lib_dir}")
    log(f"       {include_dir}")
    log("=========================================")

    for d in (lib_dir, include_dir, build):
        d.mkdir(parents=True, exist_ok=True)

    # ---- 헤더 복사 ----
    log("[1/3] 헤더 복사...")
    shutil.copytree(sb7code / "include", include_dir, dirs_exist_ok=True)
    (include_dir / "GLFW").mkdir(parents=True, exist_ok=True)
    shutil.copytree(glfw / "include" / "GLFW", include_dir / "GLFW", dirs_exist_ok=True)
    log(f"  -> {include_dir} 에 복사 완료")

    def configure(src, bdir, extra):
        run(["cmake", "-S", str(src), "-B", str(bdir), *gen, *crt, *extra])

    def build_config(bdir, config, targets=None):
        cmd = ["cmake", "--build", str(bdir), "--config", config]
        if targets:
            cmd += ["--target", *targets]
        run(cmd)

    def copy_artifact(bdir, pattern, config, out_name):
        art = _find_first(bdir, pattern, contains=config)
        if not art:
            die(f"ERROR: {pattern} ({config}) 를 찾을 수 없음")
        shutil.copy2(art, lib_dir / out_name)

    # ---- glfw3 (Release + Debug) ----
    log("[2/3] glfw3 빌드...")
    gb = build / "glfw"
    configure(glfw, gb, [
        "-DGLFW_BUILD_EXAMPLES=OFF", "-DGLFW_BUILD_TESTS=OFF",
        "-DGLFW_BUILD_DOCS=OFF", "-DGLFW_INSTALL=OFF",
    ])
    build_config(gb, "Release")
    copy_artifact(gb, "glfw3.lib", "Release", "glfw3.lib")
    build_config(gb, "Debug")
    copy_artifact(gb, "glfw3.lib", "Debug", "glfw3_d.lib")
    log("  -> glfw3 빌드 완료 (Release + Debug)")

    # ---- sb7 (Debug only) ----
    log("[3/3] sb7 빌드...")
    sb7_build = build / "sb7"
    sb7_build.mkdir(parents=True, exist_ok=True)
    (sb7_build / "CMakeLists.txt").write_text(
        SB7_CMAKELISTS % {"warn_flag": "/w"}, encoding="utf-8"
    )
    configure(sb7_build, sb7_build / "build", [
        f"-DSB7CODE_DIR={sb7code}", f"-DGLFW_DIR={glfw}",
    ])
    build_config(sb7_build / "build", "Debug")
    copy_artifact(sb7_build / "build", "sb7.lib", "Debug", "sb7_d.lib")
    pdb = _find_first(sb7_build / "build", "sb7.pdb", contains="Debug")
    if pdb:
        shutil.copy2(pdb, lib_dir / "sb7_d.pdb")
    log("  -> sb7 빌드 완료 (Debug)")

    # ---- Box2D (Release + Debug) ----
    log("[+] Box2D v2.4.1 빌드...")
    box2d = ROOT / "extern" / "box2d"
    bb = build / "box2d"
    configure(box2d, bb, [
        "-DBOX2D_BUILD_TESTBED=OFF", "-DBOX2D_BUILD_UNIT_TESTS=OFF",
        "-DBOX2D_BUILD_DOCS=OFF",
    ])
    build_config(bb, "Release")
    copy_artifact(bb, "box2d.lib", "Release", "box2d.lib")
    build_config(bb, "Debug")
    copy_artifact(bb, "box2d.lib", "Debug", "box2d_d.lib")
    shutil.copytree(box2d / "include" / "box2d", include_dir / "box2d", dirs_exist_ok=True)
    log("  -> Box2D 빌드 완료 (Release + Debug)")

    # ---- Effekseer (Release + Debug) ----
    log("[+] Effekseer 빌드...")
    efk = ROOT / "extern" / "Effekseer"
    eb = build / "effekseer"
    configure(efk, eb, [
        "-DBUILD_GL=ON", "-DBUILD_VULKAN=OFF", "-DBUILD_METAL=OFF",
        "-DBUILD_DX9=OFF", "-DBUILD_DX11=OFF", "-DBUILD_DX12=OFF",
        "-DBUILD_VIEWER=OFF", "-DBUILD_EDITOR=OFF", "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_TEST=OFF", "-DBUILD_UNITYPLUGIN=OFF",
        "-DUSE_LIBPNG_LOADER=OFF", "-DNETWORK_ENABLED=OFF",
    ])
    efk_targets = ["Effekseer", "EffekseerRendererGL"]
    build_config(eb, "Release", targets=efk_targets)
    copy_artifact(eb, "Effekseer.lib", "Release", "Effekseer.lib")
    copy_artifact(eb, "EffekseerRendererGL.lib", "Release", "EffekseerRendererGL.lib")
    build_config(eb, "Debug", targets=efk_targets)
    copy_artifact(eb, "Effekseer.lib", "Debug", "Effekseer_d.lib")
    copy_artifact(eb, "EffekseerRendererGL.lib", "Debug", "EffekseerRendererGL_d.lib")
    # 공개 헤더만(.h) 평탄화 복사 (기존 .bat 동작)
    _copy_headers_flat(efk / "Dev" / "Cpp" / "Effekseer", include_dir / "Effekseer")
    _copy_headers_flat(efk / "Dev" / "Cpp" / "EffekseerRendererGL", include_dir / "Effekseer")
    log("  -> Effekseer 빌드 완료 (Release + Debug)")

    # ---- assimp (Release + Debug) ----
    log("[+] assimp v5.4.3 빌드...")
    assimp = ROOT / "extern" / "assimp"
    ab = build / "assimp"
    configure(assimp, ab, [
        "-DBUILD_SHARED_LIBS=OFF", "-DASSIMP_BUILD_TESTS=OFF",
        "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF", "-DASSIMP_BUILD_SAMPLES=OFF",
        "-DASSIMP_INSTALL=OFF", "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
        "-DASSIMP_BUILD_ZLIB=ON",
    ])
    build_config(ab, "Release")
    copy_artifact(ab, "assimp*.lib", "Release", "assimp.lib")
    copy_artifact(ab, "zlibstatic.lib", "Release", "zlibstatic.lib")
    build_config(ab, "Debug")
    copy_artifact(ab, "assimp*.lib", "Debug", "assimp_d.lib")
    copy_artifact(ab, "zlibstaticd.lib", "Debug", "zlibstatic_d.lib")
    _copy_headers_flat(assimp / "include" / "assimp", include_dir / "assimp",
                       exts=(".h", ".hpp", ".inl"))
    _copy_headers_flat(ab / "include" / "assimp", include_dir / "assimp")
    log("  -> assimp v5.4.3 빌드 완료 (Release + Debug)")

    # ---- spdlog (Release + Debug) ----
    log("[+] spdlog v1.17.0 빌드...")
    spdlog = ROOT / "extern" / "spdlog"
    sb = build / "spdlog"
    configure(spdlog, sb, [
        "-DSPDLOG_BUILD_EXAMPLE=OFF", "-DSPDLOG_BUILD_TESTS=OFF",
        "-DSPDLOG_BUILD_BENCH=OFF",
    ])
    build_config(sb, "Release")
    copy_artifact(sb, "spdlog.lib", "Release", "spdlog.lib")
    build_config(sb, "Debug")
    copy_artifact(sb, "spdlogd.lib", "Debug", "spdlog_d.lib")
    _copy_headers_flat(spdlog / "include" / "spdlog", include_dir / "spdlog",
                       exts=(".h", ".hpp", ".inl"))
    log("  -> spdlog v1.17.0 빌드 완료 (Release + Debug)")

    # ---- 자동 복사: build_extern/output/* -> lib/windows/, include/ ----
    log("")
    log("[+] lib/windows/ 및 include/ 로 자동 복사 중...")
    (ROOT / "lib" / "windows").mkdir(parents=True, exist_ok=True)
    for f in lib_dir.iterdir():
        if f.is_file():
            shutil.copy2(f, ROOT / "lib" / "windows" / f.name)
    shutil.copytree(include_dir, ROOT / "include", dirs_exist_ok=True)
    log("  -> 복사 완료")

    log("")
    log("=========================================")
    log(" 빌드 완료!")
    log(f" -> {ROOT / 'lib' / 'windows'} 와 {ROOT / 'include'} 로 자동 복사됨")
    log("=========================================")
    return 0


def cmd_extern(_args) -> int:
    """extern 라이브러리(glfw3/sb7/box2d/effekseer/assimp/spdlog) 재빌드."""
    if IS_WIN:
        return _extern_windows()
    return _extern_unix()


# ============================================================
# doxygen 문서 빌드 + 로컬 서빙 (구 Doxygen.sh)
# ============================================================


def cmd_doxygen(args) -> int:
    port = args.port
    build_dir = ROOT / "build_ninja"
    html_dir = ROOT / "doxygen" / "html"
    url = f"http://localhost:{port}"

    # 1) configure (build_ninja 미구성 시)
    if not (build_dir / "CMakeCache.txt").exists():
        log("[doxygen] build_ninja 미구성 — cmake --preset ninja")
        run(["cmake", "--preset", "ninja", "-S", str(ROOT)])

    # 2) doxygen 타겟 빌드
    log("[doxygen] doxygen 문서 생성...")
    run(["cmake", "--build", str(build_dir), "--target", "doxygen"])

    # 3) 산출물 확인
    if not (html_dir / "index.html").exists():
        die(f"[doxygen] ERROR: {html_dir}/index.html 없음 — doxygen 빌드 실패?")

    # 4) 로컬 서버 (in-process ThreadingHTTPServer)
    import functools
    from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

    handler = functools.partial(SimpleHTTPRequestHandler, directory=str(html_dir))
    try:
        httpd = ThreadingHTTPServer(("", port), handler)
    except OSError:
        die(f"[doxygen] ERROR: 포트 {port} 이미 사용 중일 수 있음. 다른 포트: "
            f"python3 scripts/dev.py doxygen 9000")

    log(f"[doxygen] Serving {html_dir}  ->  {url}")
    webbrowser.open(url)
    log("[doxygen] 서버 실행 중. 종료하려면 Ctrl-C.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        log("\n[doxygen] 서버 종료.")
    finally:
        httpd.server_close()
    return 0


# ============================================================
# move-shaders (구 MoveShaders.sh)
# ============================================================


def cmd_move_shaders(_args) -> int:
    """apps/*/shaders/ 를 같은 계층 resources/shaders/ 로 이동."""
    apps = ROOT / "apps"
    moved = 0
    for shader_dir in sorted(apps.glob("*/shaders")):
        if not shader_dir.is_dir():
            continue
        parent = shader_dir.parent
        resource_dir = parent / "resources"
        target_dir = resource_dir / "shaders"
        app_name = parent.name

        if not resource_dir.is_dir():
            resource_dir.mkdir(parents=True, exist_ok=True)
            log(f"[{app_name}] resources/ 디렉토리 생성")
        if target_dir.is_dir():
            shutil.rmtree(target_dir)
        shutil.move(str(shader_dir), str(target_dir))
        log(f"[{app_name}] shaders/ -> resources/shaders/ 이동 완료")
        moved += 1

    if moved == 0:
        log("이동할 shaders/ 디렉토리가 없습니다.")
    else:
        log("완료: 모든 셰이더가 resources/shaders/로 이동되었습니다.")
    return 0


# ============================================================
# copy-skills (구 CopyGlobalSkills.sh)
# ============================================================

GLOBAL_SKILLS = (
    "architecture-design-workflow",
    "agent-orchestration-anti-gaming",
    "benchmark-research-method",
    "code-design-review-lenses",
    "confidence-and-sourcing",
    "design-decision-discipline",
    "response-quality-calibration",
    "modular-build-discipline",
    "socratic-tutor",
    "personal-naming-conventions",
)


def cmd_copy_skills(_args) -> int:
    """전역 ~/.claude/skills 의 범용 Skill 10종을 프로젝트 .claude/skills 로 복사."""
    src = Path.home() / ".claude" / "skills"
    dst = ROOT / ".claude" / "skills"
    dst.mkdir(parents=True, exist_ok=True)
    log(f"SRC = {src}")
    log(f"DST = {dst}")
    log("----------------------------------------")
    copied = 0
    for s in GLOBAL_SKILLS:
        src_skill = src / s
        if src_skill.is_dir():
            dst_skill = dst / s
            if dst_skill.exists():
                shutil.rmtree(dst_skill)
            shutil.copytree(src_skill, dst_skill)
            log(f"  [OK]   {s}")
            copied += 1
        else:
            log(f"  [SKIP] {s} (소스 없음)")
    log("----------------------------------------")
    log(f"복사 완료: {copied}/{len(GLOBAL_SKILLS)}")
    return 0


# ============================================================
# claude 세션 재개 자동화 (구 ResumeClaude.sh / ScheduleResumeOnce.sh)
# ============================================================

RESUME_SESSION = "4d9fa574-5dc7-48f1-a358-998d709043f2"
RESUME_PROMPT = "지금까지 작업했던 내용의 손실 없이 끊어진 세션을 이어서 작업을 진행해"


def cmd_resume_claude(args) -> int:
    """끊긴 claude 세션을 이어서 진행 (구 ResumeClaude.sh)."""
    log_path = Path.home() / ".claude-resume.log"
    env = dict(os.environ)
    # cron 은 로그인 셸 프로파일을 안 읽으므로 PATH 를 직접 보강
    extra_path = ["/opt/homebrew/bin", "/usr/local/bin", str(Path.home() / ".local" / "bin")]
    env["PATH"] = os.pathsep.join(extra_path + [env.get("PATH", "")])
    # 함정 방지: 이 변수가 있으면 구독이 아니라 API 로 과금됨
    env.pop("ANTHROPIC_API_KEY", None)

    session = args.session or RESUME_SESSION
    with open(log_path, "a", encoding="utf-8") as fp:
        fp.write(f"===== {datetime.now()} 재개 시작 (session={session}) =====\n")
        fp.flush()
        rc = subprocess.run(
            ["claude", "--resume", session, "-p", RESUME_PROMPT,
             "--permission-mode", "acceptEdits", "--max-turns", "40"],
            cwd=str(ROOT), env=env, stdout=fp, stderr=subprocess.STDOUT,
        ).returncode
        fp.write(f"===== {datetime.now()} 재개 종료 =====\n")
    log(f"재개 완료 (로그: {log_path})")
    return rc


def cmd_schedule_resume(args) -> int:
    """지정 시각(기본 05:41)에 resume-claude 를 1회 예약 실행 (구 ScheduleResumeOnce.sh)."""
    at_log = Path.home() / ".claude-at.log"
    hh, mm = (int(x) for x in args.at.split(":"))
    now = datetime.now()
    target = now.replace(hour=hh, minute=mm, second=0, microsecond=0)
    if target <= now:
        target += timedelta(days=1)
    wait = int((target - now).total_seconds())

    with open(at_log, "a", encoding="utf-8") as fp:
        fp.write(f"{now} 예약 등록 -> {target} 실행 예정 ({wait}초 대기)\n")

    # 백그라운드 detached 프로세스: wait 초 후 resume-claude 실행
    kwargs = {}
    if IS_WIN:
        kwargs["creationflags"] = 0x00000008 | 0x00000200  # DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP
    else:
        kwargs["start_new_session"] = True

    launcher = (
        f"import time,subprocess,sys;"
        f"time.sleep({wait});"
        f"subprocess.run([sys.executable, r'{Path(__file__).resolve()}', 'resume-claude'])"
    )
    with open(at_log, "a", encoding="utf-8") as fp:
        proc = subprocess.Popen([sys.executable, "-c", launcher],
                                stdout=fp, stderr=subprocess.STDOUT, **kwargs)
    log(f"예약 완료: {target} 에 실행됩니다. (PID {proc.pid})")
    log(f"취소하려면:  kill {proc.pid}")
    return 0


# ============================================================
# argparse
# ============================================================


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="dev.py",
        description="프로젝트 개발 작업 통합 CLI (구 shell/ 스크립트 통합, 크로스 플랫폼)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = p.add_subparsers(dest="command", required=True)

    def add_bt(sp, default=None):
        sp.add_argument("build_type", nargs="?" if default else None,
                        choices=["debug", "release"], default=default,
                        help="빌드 타입 (debug|release)")

    # prepare
    sp = sub.add_parser("prepare", help="build* 디렉토리 전체 삭제")
    sp.set_defaults(func=cmd_prepare)

    # configure
    sp = sub.add_parser("configure", help="cmake configure (mac/linux=ninja, win=msvc-2022)")
    add_bt(sp, default="debug")
    sp.set_defaults(func=cmd_configure)

    # build
    sp = sub.add_parser("build", help="cmake build [타겟]")
    add_bt(sp, default="debug")
    sp.add_argument("target", nargs="?", default=None, help="빌드 타겟 (생략 시 전체)")
    sp.set_defaults(func=cmd_build)

    # run
    sp = sub.add_parser("run", help="빌드 산출물 실행")
    add_bt(sp, default="debug")
    sp.add_argument("target", help="실행 타겟 (예: _MyApp_, box2d_demo/demo1)")
    sp.add_argument("--leaks", action="store_true", help="메모리 누수 검사 (macOS leaks)")
    sp.set_defaults(func=cmd_run)

    # all
    sp = sub.add_parser("all", help="prepare -> configure -> build -> run")
    add_bt(sp, default="debug")
    sp.add_argument("target", help="타겟명")
    sp.add_argument("--leaks", action="store_true", help="실행 시 누수 검사 (macOS)")
    sp.set_defaults(func=cmd_all)

    # extern
    sp = sub.add_parser("extern", help="extern 라이브러리 재빌드 (glfw3/sb7/box2d/effekseer/assimp/spdlog)")
    sp.set_defaults(func=cmd_extern)

    # doxygen
    sp = sub.add_parser("doxygen", help="doxygen 문서 빌드 + 로컬 서빙 + 브라우저 오픈")
    sp.add_argument("port", nargs="?", type=int, default=8000, help="서빙 포트 (기본 8000)")
    sp.set_defaults(func=cmd_doxygen)

    # move-shaders
    sp = sub.add_parser("move-shaders", help="apps/*/shaders -> resources/shaders 이동")
    sp.set_defaults(func=cmd_move_shaders)

    # copy-skills
    sp = sub.add_parser("copy-skills", help="전역 skill 10종을 프로젝트 .claude/skills 로 복사")
    sp.set_defaults(func=cmd_copy_skills)

    # resume-claude
    sp = sub.add_parser("resume-claude", help="끊긴 claude 세션 이어서 진행")
    sp.add_argument("--session", default=None, help="세션 ID (생략 시 기본값)")
    sp.set_defaults(func=cmd_resume_claude)

    # schedule-resume
    sp = sub.add_parser("schedule-resume", help="지정 시각에 resume-claude 1회 예약")
    sp.add_argument("--at", default="05:41", help="실행 시각 HH:MM (기본 05:41)")
    sp.set_defaults(func=cmd_schedule_resume)

    return p


def main(argv) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args) or 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

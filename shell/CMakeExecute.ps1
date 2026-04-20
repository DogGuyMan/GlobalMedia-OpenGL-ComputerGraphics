param(
    [ValidateSet("debug", "release")]
    [string]$BuildType = "debug",

    [Parameter(Mandatory = $true)]
    [string]$Target
)

$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")

# MSVC 멀티컨피그 제너레이터: build_msvc/apps/<target>/{Debug|Release}/<target>.exe
if ($BuildType -eq "debug") {
    $ConfigDir = "Debug"
} else {
    $ConfigDir = "Release"
}

$ExecDir = Join-Path $RootDir "build_msvc\apps\$Target\$ConfigDir"
$ExeName = "$Target.exe"
$ExePath = Join-Path $ExecDir $ExeName

if (-not (Test-Path $ExePath)) {
    Write-Error "실행 파일을 찾을 수 없습니다: $ExePath"
    exit 1
}

# 리소스 상대경로 해결을 위해 실행 파일 디렉토리에서 실행
Push-Location $ExecDir
try {
    & ".\$ExeName"
} finally {
    Pop-Location
}

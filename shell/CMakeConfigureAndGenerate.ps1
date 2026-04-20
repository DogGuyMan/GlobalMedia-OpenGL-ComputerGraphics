param(
    [ValidateSet("debug", "release")]
    [string]$BuildType = "debug"
)

$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")

# MSVC 프리셋은 Debug/Release 가 동일한 configurePreset 을 공유 (build 시 --config 로 분기)
cmake --preset msvc-2022 -S $RootDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

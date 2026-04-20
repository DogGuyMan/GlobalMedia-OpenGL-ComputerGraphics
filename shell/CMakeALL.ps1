param(
    [ValidateSet("debug", "release")]
    [string]$BuildType,

    [string]$Target
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrEmpty($BuildType) -or [string]::IsNullOrEmpty($Target)) {
    Write-Host "사용법: .\shell\CMakeALL.ps1 [debug|release] <타겟명>"
    Write-Host "예시:  .\shell\CMakeALL.ps1 release exercise6"
    exit 1
}

$ScriptDir = $PSScriptRoot

& (Join-Path $ScriptDir "CMakePrepare.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $ScriptDir "CMakeConfigureAndGenerate.ps1") $BuildType
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $ScriptDir "CMakeBuild.ps1") $BuildType $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $ScriptDir "CMakeExecute.ps1") $BuildType $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

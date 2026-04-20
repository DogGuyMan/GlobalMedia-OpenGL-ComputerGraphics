param(
    [ValidateSet("debug", "release")]
    [string]$BuildType = "debug",

    [string]$Target
)

$ErrorActionPreference = "Stop"

if ($BuildType -eq "debug") {
    $BuildPreset = "msvc-2022"
} else {
    $BuildPreset = "msvc-2022-release"
}

if ([string]::IsNullOrEmpty($Target)) {
    cmake --build --preset $BuildPreset
} else {
    cmake --build --preset $BuildPreset --target $Target
}

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

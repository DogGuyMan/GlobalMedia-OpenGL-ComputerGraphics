$ErrorActionPreference = "Stop"
$RootDir = Resolve-Path (Join-Path $PSScriptRoot "..")

Get-ChildItem -Path $RootDir -Directory -Filter "build*" -ErrorAction SilentlyContinue |
    ForEach-Object {
        Write-Host "Removing $($_.FullName)"
        Remove-Item $_.FullName -Recurse -Force
    }

# Copyright (C) 2026 moasm
# SPDX-License-Identifier: Apache-2.0
param(
    [string]$Client = "$PSScriptRoot/../build-win-release/app/scrcpy.exe"
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$version = 'v5.0.1-controls.1'
$name = "scrcpy-controls-$version-windows-x64-client"
$dist = Join-Path $repo 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
# Use a new staging directory and an explicit allowlist. Never copy a local
# runtime directory: DLLs, ADB, logs and credentials do not belong in the overlay.
$stage = Join-Path $dist ("client-stage-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $Client -Destination "$stage/scrcpy.exe"
foreach ($file in @('run-controls.bat', 'LICENSE', 'NOTICE', 'MODIFICATIONS.md', 'THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $repo $file) -Destination $stage
}
Copy-Item -LiteralPath "$repo/doc/windows-overlay.md" -Destination "$stage/README.md"
New-Item -ItemType Directory -Path "$stage/licenses" | Out-Null
foreach ($file in @('LGPL-2.1.txt', 'SDL3-Zlib.txt')) {
    Copy-Item -LiteralPath "$repo/licenses/$file" -Destination "$stage/licenses/$file"
}
$archive = Join-Path $dist "$name.zip"
Compress-Archive -Path "$stage/*" -DestinationPath $archive -Force
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText("$archive.sha256", "$hash  $name.zip`n", [System.Text.UTF8Encoding]::new($false))
Write-Output $archive
Write-Output "SHA256 $hash"

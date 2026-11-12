# Copyright (C) 2026 moasm
# SPDX-License-Identifier: Apache-2.0
param(
    [ValidateSet('debug', 'release')][string]$Configuration = 'debug'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$work = Join-Path $repo 'build-tools'
$downloads = Join-Path $work 'downloads'
New-Item -ItemType Directory -Path $downloads -Force | Out-Null

function Run-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}

# All runtime binaries are from the matching upstream release. No system-wide
# toolchain installation, PATH change, or modification of an installed scrcpy.
$files = @(
    @('w64devkit-x64-2.11.0.7z.exe', 'https://github.com/skeeto/w64devkit/releases/download/v2.11.0/w64devkit-x64-2.11.0.7z.exe', '54c2bb9962e8a30c8270e34c5efc3d5fee579f53087fa7d435b95fbba2af7b2b'),
    @('SDL3-devel-3.4.18-mingw.zip', 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-devel-3.4.18-mingw.zip', 'cd98158c8d025a816f430a600b4e9526524e6219ee884ec72bd498b80fd3bf58'),
    @('ffmpeg-9.0.2.tar.xz', 'https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz', '8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e'),
    @('libusb-1.0.30.7z', 'https://github.com/libusb/libusb/releases/download/v1.0.30/libusb-1.0.30.7z', '7fb1dfec805b97983763d7d0ae244320da12add1003d4249c96cc4d586398c79'),
    @('scrcpy-win64-v5.0.1.zip', 'https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-win64-v5.0.1.zip', 'b12a2c4ee8be317422451fc7dcf8ee20a71b5ea7ef9ad73ddd825a25316227a5')
)
foreach ($file in $files) {
    $path = Join-Path $downloads $file[0]
    if (!(Test-Path -LiteralPath $path)) {
        Run-Checked 'curl.exe' @('-L', '--fail', '--retry', '3', '-o', $path, $file[1])
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file[2]) {
        throw "SHA-256 mismatch: $path"
    }
}

Push-Location $repo
$oldPath = $env:PATH
$oldPythonPath = $env:PYTHONPATH
$oldCC = $env:CC
$oldPkg = $env:PKG_CONFIG_LIBDIR
try {
    if (!(Test-Path -LiteralPath "$work/w64devkit/bin/gcc.exe")) {
        Run-Checked "$downloads/w64devkit-x64-2.11.0.7z.exe" @('-y', "-o$work")
    }
    foreach ($archive in @(
        @('SDL3-devel-3.4.18-mingw.zip', 'SDL3-3.4.18'),
        @('ffmpeg-9.0.2.tar.xz', 'ffmpeg-9.0.2'),
        @('libusb-1.0.30.7z', 'libusb'),
        @('scrcpy-win64-v5.0.1.zip', 'scrcpy-win64-v5.0.1')
    )) {
        if (!(Test-Path -LiteralPath (Join-Path $work $archive[1]))) {
            $destination = $work
            if ($archive[1] -eq 'libusb') {
                $destination = Join-Path $work 'libusb'
                New-Item -ItemType Directory -Path $destination -Force | Out-Null
            }
            Run-Checked 'tar.exe' @('-xf', (Join-Path $downloads $archive[0]), '-C', $destination)
        }
    }
    if (!(Test-Path -LiteralPath "$work/python/mesonbuild/mesonmain.py")) {
        Run-Checked 'python' @('-m', 'pip', 'install', '--target', "$work/python", 'meson==1.12.1', 'ninja==1.13.2')
    }
    $env:PATH = "$work/w64devkit/bin;$work/python/bin;$env:PATH"
    $env:PYTHONPATH = "$work/python"
    $env:CC = 'gcc'
    $env:PKG_CONFIG_LIBDIR = "$work/deps/lib/pkgconfig"
    Run-Checked 'python' @("$PSScriptRoot/prepare-windows-deps.py", $repo)
    $buildDir = Join-Path $repo "build-win-$Configuration"
    $setup = @('-m', 'mesonbuild.mesonmain', 'setup', $buildDir, "--buildtype=$Configuration", '-Dcompile_server=false', '-Dportable=true')
    if (Test-Path -LiteralPath "$buildDir/meson-private/coredata.dat") { $setup += '--reconfigure' }
    Run-Checked 'python' $setup
    Run-Checked 'ninja' @('-C', $buildDir)
    $runtime = Join-Path $work 'scrcpy-win64-v5.0.1'
    # Tests and the portable client resolve the same verified runtime DLLs.
    Copy-Item -Path "$runtime/*.dll" -Destination "$buildDir/app" -Force
    if ($Configuration -eq 'debug') {
        try {
            Run-Checked 'python' @('-m', 'mesonbuild.mesonmain', 'test', '-C', $buildDir, '--print-errorlogs')
        } finally {
            Run-Checked 'python' @("$PSScriptRoot/sanitize-test-logs.py", "$buildDir/meson-logs")
        }
    }
    $dist = Join-Path $repo 'dist/scrcpy-controls-windows-x64'
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    Copy-Item -Path "$runtime/*" -Destination $dist -Recurse -Force
    Copy-Item -LiteralPath "$buildDir/app/scrcpy.exe" -Destination "$dist/scrcpy.exe" -Force
    Copy-Item -LiteralPath "$PSScriptRoot/../run-controls.bat" -Destination $dist -Force
    Write-Output "Ready: $dist/run-controls.bat"
} finally {
    $env:PATH = $oldPath
    $env:PYTHONPATH = $oldPythonPath
    $env:CC = $oldCC
    $env:PKG_CONFIG_LIBDIR = $oldPkg
    Pop-Location
}

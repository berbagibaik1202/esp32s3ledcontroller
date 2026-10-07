$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$portableCompiler = Join-Path $projectRoot '.build-temp\w64devkit\bin\g++.exe'
if (Test-Path -LiteralPath $portableCompiler) {
    $portableExit = 1
    $previousTemp = $env:TEMP
    $previousTmp = $env:TMP
    Push-Location -LiteralPath $projectRoot
    try {
        New-Item -ItemType Directory -Force -Path '.build-temp\native-tests', '.build-temp\previews' | Out-Null
        $env:TEMP = Join-Path $projectRoot '.build-temp'
        $env:TMP = $env:TEMP
        $jsonInclude = Join-Path $projectRoot '.pio\libdeps\esp32s3_n16r8_usb\ArduinoJson\src'
        if (!(Test-Path -LiteralPath $jsonInclude)) { throw 'Run tools/build.ps1 first to install ArduinoJson.' }
        $jpegInclude = Join-Path $projectRoot '.pio\libdeps\esp32s3_n16r8_usb\JPEGDEC\src'
        if (!(Test-Path -LiteralPath $jpegInclude)) { throw 'Run tools/build.ps1 first to install JPEGDEC.' }
        # Upstream headers are system includes; warnings in project code remain errors.
        & $portableCompiler -std=c++11 -isystem $jpegInclude -c "$jpegInclude\JPEGDEC.cpp" -o .build-temp/native-tests/jpegdec.o
        if ($LASTEXITCODE -ne 0) { throw 'JPEGDEC host compilation failed.' }
        & $portableCompiler -std=c++11 -Wall -Wextra -Werror -I src -I $jsonInclude -isystem $jpegInclude `
            tests/display_tests.cpp tests/layout_tests.cpp tests/media_tests.cpp tests/jpeg_tests.cpp tests/file_image_tests.cpp src/media/file_image.cpp src/media/jpeg.cpp src/media/bmp.cpp src/media/image_renderer.cpp src/display/panel_layout.cpp src/display/panel_profile.cpp src/display/framebuffer.cpp src/display/diagnostics.cpp .build-temp/native-tests/jpegdec.o `
            -o .build-temp/native-tests/display-tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
        & '.\.build-temp\native-tests\display-tests.exe'
        $portableExit = $LASTEXITCODE
    } finally {
        $env:TEMP = $previousTemp
        $env:TMP = $previousTmp
        Pop-Location
    }
    exit $portableExit
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools for host tests.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Visual Studio C++ toolchain not found.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$oldValues = @{}
$exitCode = 1
Push-Location -LiteralPath $projectRoot
try {
    New-Item -ItemType Directory -Force -Path '.build-temp' | Out-Null
    # A relative batch path avoids Windows PowerShell's nested CMD quoting.
    $envBatch = '@echo off' + "`r`n" + 'call "' + $vcvars + '" >nul' + "`r`n" +
        'if errorlevel 1 exit /b 1' + "`r`n" + 'set' + "`r`n"
    [System.IO.File]::WriteAllText((Join-Path $projectRoot '.build-temp\load-vc-env.cmd'), $envBatch)
    $compilerEnv = & cmd.exe /d /c .build-temp\load-vc-env.cmd
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize C++ compiler environment.' }
    foreach ($line in $compilerEnv) {
        if ($line -match '^([^=]+)=(.*)$') {
            $key = $Matches[1]
            # Do not change user home variables.
            if ($key -in @('HOME', 'CODEX_HOME', 'USERPROFILE')) { continue }
            $oldValues[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
            [Environment]::SetEnvironmentVariable($key, $Matches[2], 'Process')
        }
    }
    $jsonInclude = Join-Path $projectRoot '.pio\libdeps\esp32s3_n16r8_usb\ArduinoJson\src'
    if (!(Test-Path -LiteralPath $jsonInclude)) { throw 'Run tools/build.ps1 first to install ArduinoJson.' }
    $testOutput = Join-Path $projectRoot '.build-temp\native-tests'
    New-Item -ItemType Directory -Force -Path $testOutput, '.build-temp\previews' | Out-Null
    $jpegInclude = Join-Path $projectRoot '.pio\libdeps\esp32s3_n16r8_usb\JPEGDEC\src'
    if (!(Test-Path -LiteralPath $jpegInclude)) { throw 'Run tools/build.ps1 first to install JPEGDEC.' }
    & cl.exe /nologo /EHsc /std:c++14 /w /I $jpegInclude /c "$jpegInclude\JPEGDEC.cpp" /Fo.build-temp/native-tests/jpegdec.obj
    if ($LASTEXITCODE -ne 0) { throw 'JPEGDEC host compilation failed.' }
    & cl.exe /nologo /EHsc /std:c++14 /W4 /I src /I $jsonInclude /I $jpegInclude `
        tests/display_tests.cpp tests/layout_tests.cpp tests/media_tests.cpp tests/jpeg_tests.cpp tests/file_image_tests.cpp src/media/file_image.cpp src/media/jpeg.cpp src/media/bmp.cpp src/media/image_renderer.cpp src/display/panel_layout.cpp src/display/panel_profile.cpp src/display/framebuffer.cpp src/display/diagnostics.cpp .build-temp/native-tests/jpegdec.obj `
        /Fo.build-temp/native-tests/ /Fe.build-temp/native-tests/display-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
    & "$testOutput\display-tests.exe"
    $exitCode = $LASTEXITCODE
} finally {
    foreach ($key in $oldValues.Keys) {
        [Environment]::SetEnvironmentVariable($key, $oldValues[$key], 'Process')
    }
    Pop-Location
}
exit $exitCode

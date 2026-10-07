$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$archiveName = 'w64devkit-x64-2.10.0.7z.exe'
$expectedSha256 = '18d0a4c71a166f8401ab6305781bec5882b40b5e06ba9807c61cb5f3b3c6325e'
Push-Location -LiteralPath $projectRoot
try {
    New-Item -ItemType Directory -Force -Path '.build-temp' | Out-Null
    $archivePath = Join-Path $projectRoot ".build-temp\$archiveName"
    if (!(Test-Path -LiteralPath $archivePath)) {
        & curl.exe -L --fail --silent --show-error --output $archivePath `
            "https://github.com/skeeto/w64devkit/releases/download/v2.10.0/$archiveName"
        if ($LASTEXITCODE -ne 0) { throw 'Compiler download failed.' }
    }
    if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedSha256) {
        throw 'Compiler archive hash mismatch.'
    }
    if (!(Test-Path -LiteralPath '.build-temp\w64devkit\bin\g++.exe')) {
        $extractor = Start-Process -FilePath $archivePath -ArgumentList @('-y', '-o.build-temp') `
            -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru
        if ($extractor.ExitCode -ne 0) { throw 'Compiler extraction failed.' }
    }
    Write-Output 'Portable host compiler ready in .build-temp/w64devkit.'
} finally { Pop-Location }

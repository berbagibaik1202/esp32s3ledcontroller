param(
    [ValidateSet('esp32s3_n16r8_usb', 'esp32s3_n16r8_uart')]
    [string]$Environment = 'esp32s3_n16r8_usb'
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildTemp = Join-Path $projectRoot '.build-temp'
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$buildExit = 1
Push-Location -LiteralPath $projectRoot
try {
    New-Item -ItemType Directory -Force -Path $buildTemp | Out-Null
    $env:TEMP = $buildTemp
    $env:TMP = $buildTemp
    & py -m platformio run -e $Environment
    $buildExit = $LASTEXITCODE
} finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    Pop-Location
}
exit $buildExit

# Arduino Due Programming Port (arduino:sam:arduino_due_x_dbg)
# Upload uses upload\bossac.exe the same way as upload\upload.bat:
#   bossac -i -d --port=COM5 -U false -e -w -b -v <bin> -R
#
#   .\compile.ps1                         # SD 버전 (기본)
#   .\compile.ps1 -Flavor flash           # AT25SF081 버전
#   .\compile.ps1 -Flavor flash -Upload   # 펌웨어만. 이미지는 이 스케치 GUI
#   .\compile.ps1 -Flavor flash -Upload -SkipImages
#   .\compile.ps1 -ListPorts
#   .\compile.ps1 -Upload -Monitor
#   .\compile.ps1 -Monitor
#   .\compile.ps1 -Upload -Port COM7
#
# 이미지: 이 스케치 tools\flash_upload (charger_1 과 별개)
#   python tools\flash_upload\flash_upload.py
#   COM5 Serial 115200. 모니터는 닫고 GUI만. 폴더는 han\

param(
    [ValidateSet("sd", "flash", "wire")]
    [string]$Flavor = "sd",
    [switch]$Upload,
    [switch]$Monitor,
    [switch]$ListPorts,
    [switch]$SkipImages,
    [string]$Port = "COM5",
    [string]$ImagePort = "",
    [int]$Baud = 0
)

$ErrorActionPreference = "Stop"
$SketchDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SketchDir

if ($Baud -eq 0) {
    if ($Flavor -eq "flash" -or $Flavor -eq "wire") { $Baud = 115200 } else { $Baud = 9600 }
}
if ([string]::IsNullOrWhiteSpace($ImagePort)) {
    $ImagePort = $Port
}

$ArduinoCli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if (-not (Test-Path $ArduinoCli)) {
    $ArduinoCli = (Get-Command arduino-cli -ErrorAction SilentlyContinue).Source
}
if (-not $ArduinoCli) {
    throw "arduino-cli.exe not found. Install Arduino IDE 2.x or add arduino-cli to PATH."
}

# Programming Port (COM5) — same as Arduino IDE "Arduino Due (Programming Port)"
$Fqbn = "arduino:sam:arduino_due_x_dbg"
$LibDir = Join-Path $SketchDir "libraries"
$BuildDir = Join-Path $SketchDir "build"
$UploadDir = Join-Path $SketchDir "upload"
$Bossac = Join-Path $UploadDir "bossac.exe"
$SketchName = Split-Path $SketchDir -Leaf
$BinName = "$SketchName.ino.bin"
$FlavorBinName = "${SketchName}_${Flavor}.ino.bin"
$HanDir = Join-Path $SketchDir "han"

function Show-Ports {
    Write-Host "=== serial ports ==="
    & $ArduinoCli board list
}

if ($ListPorts) {
    Show-Ports
    return
}

Write-Host "arduino-cli: $ArduinoCli"
Write-Host "FQBN:        $Fqbn  (Programming Port)"
Write-Host "flavor:      $Flavor"
Write-Host "libraries:   $LibDir"
Write-Host "build:       $BuildDir"

$compileArgs = @(
    "compile",
    "--fqbn", $Fqbn,
    "--libraries", $LibDir,
    "--build-path", $BuildDir,
    "--warnings", "none"
)
if ($Flavor -eq "flash") {
    $compileArgs += "--build-property"
    $compileArgs += "compiler.cpp.extra_flags=-DUSE_SPI_FLASH"
    $compileArgs += "--build-property"
    $compileArgs += "compiler.c.extra_flags=-DUSE_SPI_FLASH"
}
if ($Flavor -eq "wire") {
    $compileArgs += "--build-property"
    $compileArgs += "compiler.cpp.extra_flags=-DSERIAL_WIRE_TEST"
    $compileArgs += "--build-property"
    $compileArgs += "compiler.c.extra_flags=-DSERIAL_WIRE_TEST"
}

& $ArduinoCli @compileArgs $SketchDir

if ($LASTEXITCODE -ne 0) {
    throw "compile failed: exit $LASTEXITCODE"
}

$builtBin = Join-Path $BuildDir $BinName
if (-not (Test-Path $builtBin)) {
    throw "bin not found: $builtBin"
}

New-Item -ItemType Directory -Force -Path $UploadDir | Out-Null
Copy-Item $builtBin (Join-Path $UploadDir $BinName) -Force
Copy-Item $builtBin (Join-Path $UploadDir $FlavorBinName) -Force
Write-Host ""
Write-Host "OK compile: $builtBin"
Write-Host "copied to:  $UploadDir\$BinName"
Write-Host "copied to:  $UploadDir\$FlavorBinName"

if ($Upload) {
    if (-not (Test-Path $Bossac)) {
        throw "bossac.exe not found: $Bossac"
    }
    $uploadBin = Join-Path $UploadDir $BinName
    Write-Host "upload: $Bossac --port=$Port -U false (Programming Port)"
    & $Bossac -i -d "--port=$Port" -U false -e -w -b -v $uploadBin -R
    if ($LASTEXITCODE -ne 0) {
        throw "upload failed: exit $LASTEXITCODE"
    }
    Write-Host "OK upload firmware"
    Start-Sleep -Seconds 2
    if (($Flavor -eq "flash") -and (-not $SkipImages)) {
        $gui = Join-Path $SketchDir "tools\flash_upload\flash_upload.py"
        Write-Host "images: python $gui"
        Write-Host "  1) 모니터를 닫고 GUI에서 COM5(Serial) 연결"
        Write-Host "  2) 보드 리셋 (2.5초 HELLO)"
        Write-Host "  3) 폴더를 han\ 으로 지정 후 전체 전송"
        Write-Host "  이미지 전송 중에는 COM5 모니터를 열지 말 것."
    }
}

if ($Monitor) {
    Write-Host "monitor $Port @ $Baud  dtr=off rts=off (Ctrl+C to exit)"
    Write-Host "Note: monitor holds the port. Close it before the next upload."
    & $ArduinoCli monitor --port $Port --config "baudrate=$Baud,dtr=off,rts=off"
}

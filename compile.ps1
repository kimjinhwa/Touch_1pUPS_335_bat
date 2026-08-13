# Arduino Due Programming Port (arduino:sam:arduino_due_x_dbg)
# Upload uses upload\bossac.exe the same way as upload\upload.bat:
#   bossac -i -d --port=COM5 -U false -e -w -b -v <bin> -R
#
#   .\compile.ps1
#   .\compile.ps1 -ListPorts
#   .\compile.ps1 -Upload
#   .\compile.ps1 -Upload -Monitor
#   .\compile.ps1 -Monitor
#   .\compile.ps1 -Upload -Port COM7

param(
    [switch]$Upload,
    [switch]$Monitor,
    [switch]$ListPorts,
    [string]$Port = "COM5",
    [int]$Baud = 9600
)

$ErrorActionPreference = "Stop"
$SketchDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SketchDir

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
Write-Host "libraries:   $LibDir"
Write-Host "build:       $BuildDir"

& $ArduinoCli compile `
    --fqbn $Fqbn `
    --libraries $LibDir `
    --build-path $BuildDir `
    --warnings none `
    $SketchDir

if ($LASTEXITCODE -ne 0) {
    throw "compile failed: exit $LASTEXITCODE"
}

$builtBin = Join-Path $BuildDir $BinName
if (-not (Test-Path $builtBin)) {
    throw "bin not found: $builtBin"
}

New-Item -ItemType Directory -Force -Path $UploadDir | Out-Null
Copy-Item $builtBin (Join-Path $UploadDir $BinName) -Force
Write-Host ""
Write-Host "OK compile: $builtBin"
Write-Host "copied to:  $UploadDir\$BinName"

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
    Write-Host "OK upload"
    Start-Sleep -Seconds 2
}

if ($Monitor) {
    Write-Host "monitor $Port @ $Baud  dtr=off rts=off (Ctrl+C to exit)"
    Write-Host "Note: monitor holds the port. Close it before the next upload."
    & $ArduinoCli monitor --port $Port --config "baudrate=$Baud,dtr=off,rts=off"
}

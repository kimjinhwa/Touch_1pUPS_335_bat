# Arduino Due Programming Port (arduino:sam:arduino_due_x_dbg)
# Upload uses upload\bossac.exe the same way as upload\upload.bat:
#   bossac -i -d --port=COM5 -U false -e -w -b -v <bin> -R
#
#   .\compile.ps1                         # SD 버전 (기본)
#   .\compile.ps1 -Flavor flash           # AT25SF081 절전 있음+없음 두 bin
#   .\compile.ps1 -Flavor flash -Upload   # 절전 있음 bin 기록. 이미지는 GUI
#   .\compile.ps1 -Flavor flash -Upload -SkipImages
#   .\compile.ps1 -ListPorts
#   .\compile.ps1 -Upload -Monitor
#   .\compile.ps1 -Monitor
#   .\compile.ps1 -Upload -Port COM7
#
# 이미지 GUI 소스: tools\flash_upload
# 펌웨어 담당자 배포: upload\ 만 복사 (upload.bat + flash_upload.exe)
#   exe 다시 만들기: .\tools\flash_upload\build_exe.ps1
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

function Copy-RawTree([string]$From, [string]$To) {
    New-Item -ItemType Directory -Force -Path $To | Out-Null
    if (-not (Test-Path $From)) { return }
    $root = (Resolve-Path $From).Path
    Get-ChildItem $From -Recurse -File -Filter *.raw | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length).TrimStart("\", "/")
        $dest = Join-Path $To $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
        Copy-Item $_.FullName $dest -Force
    }
}

function Sync-FirmwareKit {
    $srcGui = Join-Path $SketchDir "tools\flash_upload"
    $dstGui = Join-Path $UploadDir "flash_upload"
    New-Item -ItemType Directory -Force -Path $dstGui | Out-Null
    foreach ($name in @(
        "flash_upload.py",
        "flash_upload_espprog.py",
        "app.py",
        "hw.py",
        "firmware.py",
        "pack.py",
        "protocol.py",
        "requirements.txt"
    )) {
        Copy-Item (Join-Path $srcGui $name) (Join-Path $dstGui $name) -Force
    }
    Copy-RawTree $HanDir (Join-Path $UploadDir "han")

    $bypassRoot = Join-Path (Split-Path $SketchDir -Parent) "Touch_1pUPS_335_bypass"
    $bypassUpload = Join-Path $bypassRoot "upload"
    foreach ($name in @(
        "withScreenSaver_Touch_1pUPS_335_bypass_flash.ino.bin",
        "NoScreenSaver_Touch_1pUPS_335_bypass_flash.ino.bin"
    )) {
        $src = Join-Path $bypassUpload $name
        if (Test-Path $src) {
            Copy-ToUpload $src $name
        }
    }
    $bypassHan = Join-Path $bypassRoot "han"
    if (-not (Test-Path $bypassHan)) {
        $bypassHan = Join-Path $bypassUpload "han"
    }
    if (Test-Path $bypassHan) {
        Copy-RawTree $bypassHan (Join-Path $UploadDir "han_bypass")
    }
    Write-Host "synced firmware kit: $UploadDir"
}

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

function Invoke-SketchCompile([string]$ExtraFlags, [string]$Label) {
    $compileArgs = @(
        "compile",
        "--fqbn", $Fqbn,
        "--libraries", $LibDir,
        "--build-path", $BuildDir,
        "--warnings", "none"
    )
    if (-not [string]::IsNullOrWhiteSpace($ExtraFlags)) {
        $compileArgs += "--build-property"
        $compileArgs += "compiler.cpp.extra_flags=$ExtraFlags"
        $compileArgs += "--build-property"
        $compileArgs += "compiler.c.extra_flags=$ExtraFlags"
    }
    Write-Host "compile $Label"
    if ($ExtraFlags) { Write-Host "  flags: $ExtraFlags" }
    & $ArduinoCli @compileArgs $SketchDir | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "compile failed ($Label): exit $LASTEXITCODE"
    }
    $built = Join-Path $BuildDir $BinName
    if (-not (Test-Path $built)) {
        throw "bin not found: $built"
    }
    return ,$built
}

function Copy-ToUpload([string]$SrcBin, [string]$DestName) {
    $dest = Join-Path $UploadDir $DestName
    Copy-Item $SrcBin $dest -Force
    Write-Host "copied to:  $dest"
}

New-Item -ItemType Directory -Force -Path $UploadDir | Out-Null
$SaverFlashBin = "withScreenSaver_${SketchName}_flash.ino.bin"
$NoSaverFlashBin = "NoScreenSaver_${SketchName}_flash.ino.bin"

if ($Flavor -eq "flash") {
    $builtBin = Invoke-SketchCompile "-DUSE_SPI_FLASH" "flash + screensaver"
    Copy-ToUpload $builtBin $SaverFlashBin
    Copy-ToUpload $builtBin $FlavorBinName
    Copy-ToUpload $builtBin $BinName
    $builtBin = Invoke-SketchCompile "-DUSE_SPI_FLASH -DNO_SCREEN_SAVER" "flash + no screensaver"
    Copy-ToUpload $builtBin $NoSaverFlashBin
    Write-Host ""
    Write-Host "OK compile flash (2 bins)"
    Sync-FirmwareKit
} elseif ($Flavor -eq "wire") {
    $builtBin = Invoke-SketchCompile "-DSERIAL_WIRE_TEST" "wire"
    Copy-ToUpload $builtBin $FlavorBinName
    Copy-ToUpload $builtBin $BinName
    Write-Host ""
    Write-Host "OK compile: $builtBin"
} else {
    $builtBin = Invoke-SketchCompile "" "sd"
    Copy-ToUpload $builtBin $FlavorBinName
    Copy-ToUpload $builtBin $BinName
    Write-Host ""
    Write-Host "OK compile: $builtBin"
}

if ($Upload) {
    if (-not (Test-Path $Bossac)) {
        throw "bossac.exe not found: $Bossac"
    }
    if ($Flavor -eq "flash") {
        $uploadBin = Join-Path $UploadDir $SaverFlashBin
    } else {
        $uploadBin = Join-Path $UploadDir $BinName
    }
    Write-Host "upload: $Bossac --port=$Port -U false $uploadBin"
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
        Write-Host "  펌웨어 담당자: upload\upload.bat (이 폴더만 복사)"
    }
}

if ($Monitor) {
    Write-Host "monitor $Port @ $Baud  dtr=off rts=off (Ctrl+C to exit)"
    Write-Host "Note: monitor holds the port. Close it before the next upload."
    & $ArduinoCli monitor --port $Port --config "baudrate=$Baud,dtr=off,rts=off"
}

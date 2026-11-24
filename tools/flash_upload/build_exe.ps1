# GUI → upload\flash_upload.exe 와 upload\flash_upload_ESPPROG.exe
#   .\tools\flash_upload\build_exe.ps1

$ErrorActionPreference = "Stop"
$GuiDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not [IO.Path]::IsPathRooted($GuiDir)) {
    $GuiDir = (Resolve-Path $GuiDir).Path
}
$SketchDir = Split-Path -Parent (Split-Path -Parent $GuiDir)
$UploadDir = Join-Path $SketchDir "upload"
$WorkDir = Join-Path $GuiDir "build_exe"

Set-Location $GuiDir
python -m pip install -q pyinstaller pyserial

$hidden = @(
    "--hidden-import", "serial",
    "--hidden-import", "serial.tools.list_ports",
    "--hidden-import", "app",
    "--hidden-import", "hw",
    "--hidden-import", "firmware",
    "--hidden-import", "protocol",
    "--hidden-import", "pack",
    "--hidden-import", "winsound"
)

function Build-Gui([string]$Name, [string]$Script) {
    python -m PyInstaller --noconfirm --clean --onefile --noconsole --name $Name `
        --distpath $UploadDir `
        --workpath $WorkDir `
        --specpath $GuiDir `
        @hidden `
        (Join-Path $GuiDir $Script)
    $exe = Join-Path $UploadDir "$Name.exe"
    if (-not (Test-Path $exe)) {
        throw "exe not found: $exe"
    }
    Write-Host "OK $exe"
}

Build-Gui "flash_upload" "flash_upload.py"
Build-Gui "flash_upload_ESPPROG" "flash_upload_espprog.py"

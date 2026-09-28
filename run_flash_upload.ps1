# 1pUPS 전용 이미지 업로드 GUI (charger_1 과 별개).
#
#   python tools\flash_upload\flash_upload.py
#   1) 시리얼 모니터를 닫는다
#   2) GUI에서 COM5 (Serial) 연결
#   3) 보드 리셋 (2.5초 HELLO)
#   4) 폴더는 han\  (기본값)
#   전송 중 COM5 모니터와 GUI를 같이 쓰지 말 것.

$SketchDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SketchDir
python tools\flash_upload\flash_upload.py

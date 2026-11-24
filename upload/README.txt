1pUPS 표시기 — 펌웨어 담당자용

이 폴더 전체를 복사해서 사용합니다. Python, 소스, bypass 프로젝트 경로는 필요 없습니다.
bat 그림은 han\, bypass 그림은 han_bypass\ 에 있습니다.
이 폴더의 .bin 과 flash_upload.exe 는 한 세트로 바꿉니다. 예전 펌웨어에 새 exe 를 쓰면 이미지가 안 올라갑니다.


■ 포트
  COM5 = Arduino Due Programming Port (시리얼)
  시리얼 모니터와 이미지 GUI는 동시에 쓰지 마십시오.
  업로드 전에 모니터를 닫으십시오.


■ 1. 펌웨어 + 이미지 (처음 또는 전체)

  1) 시리얼 모니터를 닫습니다.
  2) upload.bat 을 실행합니다.
  3) 펌웨어를 고릅니다.
       1  bat     screensaver ON
          withScreenSaver_Touch_1pUPS_335_bat_flash.ino.bin
       2  bat     screensaver OFF
          NoScreenSaver_Touch_1pUPS_335_bat_flash.ino.bin
       3  bypass  screensaver ON
          withScreenSaver_Touch_1pUPS_335_bypass_flash.ino.bin
       4  bypass  screensaver OFF
          NoScreenSaver_Touch_1pUPS_335_bypass_flash.ino.bin
       Enter 만 치면 1번입니다.
       네 bin 모두 이 폴더에 둡니다. 3·4번은 han_bypass 그림을 씁니다.
  4) bossac.exe 가 COM5 로 선택한 .bin 을 기록합니다.
  5) "Firmware OK. Next: image GUI." 가 나오면 아무 키나 누릅니다.
     그제야 flash_upload.exe 가 열립니다.
  6) GUI에서 COM5 연결을 누릅니다. (HELLO 대기 최대 120초, 주황으로 남은 시간 표시)
  7) 보드를 리셋합니다. 2.5초 안에 HELLO가 맞으면 업로드 모드입니다.
  8) 폴더 확인 후 [전체 전송] 합니다.
     1·2번(bat)은 han,  3·4번(bypass)은 han_bypass.
  9) 전송이 끝난 뒤에만 시리얼 모니터를 엽니다.


■ 2. 이미지만 다시 넣을 때

  flash_upload.bat 또는 flash_upload.exe 를 실행합니다.
  펌웨어는 이미 올라가 있어야 합니다.
  GUI에서 COM5 연결을 누른 다음 보드를 리셋합니다.


■ GUI 버튼

  연결 (HELLO 대기)  포트만 열고 단말 HELLO를 기다립니다. 최대 120초.
  목록 갱신          PC의 han 폴더 RAW를 다시 읽습니다. 보드는 안 읽습니다.
  단말기 스캔        보드 SPI 플래시(AT25SF081) 팩 목록을 읽습니다.
  단말기 목록        단말기 스캔과 같습니다. 아래쪽 버튼입니다.
  전체 전송          칩을 지우고 han 전체 RAW를 C1IM 팩으로 기록합니다.
  선택 팩 전송       고른 파일만 새 팩으로 올립니다. 나머지 칩 내용은 지워집니다.
  제자리 교체        칩에 같은 경로·같은 크기 파일만 바꿉니다.
  종료 (정상 부팅)   업로드 모드를 끊고 단말이 정상 화면으로 갑니다.


■ 파일 설명

  upload.bat
    펌웨어(bossac, COM5) 기록 후 이미지 GUI를 엽니다.

  flash_upload.exe
    화면 이미지(C1IM 팩)를 AT25SF081에 올리는 프로그램입니다.
    Python 없이 실행합니다. 콘솔 창은 없습니다.

  flash_upload.bat
    flash_upload.exe 만 실행합니다.

  han\
    화면 RAW 기본 폴더입니다. *.raw 만 팩에 들어갑니다.
    *.png 과 *.raw.notuse 는 넣지 않습니다.


■ 문제

  bossac 실패 / No device
    모니터를 닫고, 보드를 erase 한 뒤 리셋합니다.

  HELLO 응답 없음
    GUI에서 연결을 먼저 누른 다음 보드를 리셋합니다.
    단말이 보내는 HELLO 창은 리셋 후 약 2.5초입니다.

  LIST 타임아웃 / 목록 0개
    연결은 된 경우가 많습니다. [단말기 스캔]을 다시 누르십시오.


■ 개발자

  소스 GUI: tools\flash_upload\
  exe 다시 만들기: .\tools\flash_upload\build_exe.ps1
  결과는 upload\flash_upload.exe 입니다.
  두 펌웨어 bin: .\compile.ps1 -Flavor flash

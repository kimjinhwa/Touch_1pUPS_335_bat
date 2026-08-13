# 벤더 라이브러리 (Arduino Due)

이 폴더는 `문서\Arduino\libraries` 에서 **이 스케치가 실제로 include 하는 라이브러리만** 복사한 것이다.
git clone 후 OneDrive/스케치북 없이도 `..\compile.ps1` 로 빌드할 수 있게 하기 위함이다.

## 포함

- arduino_uip (UIPEthernet 2.0.6)
- SdFat 1.0.6
- UTFT, URTouch, UTFT_Buttons, UTFT_SdRaw 1.2.4
- DueTimer 1.4.7
- RTCDue 1.1.0
- SPIMemory 3.2.1
- DueFlashStorage 1.0.0

## 제외

- `SPI` — `arduino:sam` 코어에 포함
- 각 라이브러리의 `examples`, `extras`, `docs`, `Image-files`
- UTFT_SdRaw `Image-files` (약 36MB, 컴파일 불필요)

원본을 다시 맞출 때:

```powershell
# 문서\Arduino\libraries 에서 위 폴더를 이 위치로 복사 (examples/docs 제외)
```

Arduino IDE에서 이 폴더를 쓰려면 스케치북을 이 저장소 루트로 바꾸거나, 각 라이브러리를 `문서\Arduino\libraries` 에 복사/링크하면 된다.

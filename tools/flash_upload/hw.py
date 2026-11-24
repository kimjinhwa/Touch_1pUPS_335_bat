# -*- coding: utf-8 -*-
"""DTR/RTS 하드웨어. 이미지 프로토콜과 분리.

수동: ERASE/RESET 은 사람이 한다. 시리얼 DTR/RTS 는 예전과 같이 내린 채 연다.
ESP-Prog: RTS=ERASE(반전), DTR=RESET. idle 은 DTR=1 RTS=1.
"""
from __future__ import annotations

import time

ERASE_S = 0.28
RESET_S = 0.08
GAP_S = 0.05


class HardwareBackend:
    name = "manual"
    title = "1pUPS 표시기 플래시 업로드"
    default_port = "COM5"
    idle_dtr = False
    idle_rts = False
    _frozen = False

    def supports_firmware(self) -> bool:
        return False

    def supports_reset(self) -> bool:
        return False

    def apply_idle(self, ser) -> None:
        ser.dtr = self.idle_dtr
        ser.rts = self.idle_rts

    def freeze_run(self, ser, log) -> None:
        """이미지 세션: 리셋/ERASE 선을 건드리지 않는다."""
        self.apply_idle(ser)
        self._frozen = True

    def unfreeze(self) -> None:
        self._frozen = False

    def prepare_hello(self, ser, log, auto_reset: bool) -> None:
        self.unfreeze()
        self.apply_idle(ser)
        time.sleep(0.05)

    def pulse_reset(self, ser, log) -> None:
        log("이 업로더는 리셋 라인을 쓰지 않습니다. 보드 리셋 버튼을 누르세요.")

    def erase_then_reset(self, ser, log) -> None:
        raise NotImplementedError


class ManualHw(HardwareBackend):
    name = "manual"
    title = "1pUPS 표시기 플래시 업로드  (수동 ERASE / 115200)"
    default_port = "COM5"
    idle_dtr = False
    idle_rts = False


class EspProgHw(HardwareBackend):
    name = "espprog"
    title = "1pUPS 표시기 플래시 업로드  (ESP-Prog)"
    default_port = "COM21"
    idle_dtr = True
    idle_rts = True

    def supports_firmware(self) -> bool:
        return True

    def supports_reset(self) -> bool:
        return True

    def freeze_run(self, ser, log) -> None:
        ser.dtr = True
        ser.rts = True
        self._frozen = True
        log("이미지 구간 DTR=1 RTS=1 고정 — 리셋/ERASE 금지")

    def pulse_reset(self, ser, log) -> None:
        if self._frozen:
            log("이미지 전송 중이라 DTR 리셋을 막았습니다")
            return
        log(f"RESET  DTR=0  {int(RESET_S * 1000)}ms")
        ser.rts = True
        ser.dtr = False
        time.sleep(RESET_S)
        ser.dtr = True
        ser.rts = True
        time.sleep(0.15)

    def erase_then_reset(self, ser, log) -> None:
        if self._frozen:
            log("이미지 전송 중이라 ERASE를 막았습니다")
            return
        log(f"ERASE  RTS=0  {int(ERASE_S * 1000)}ms  (FT_nRTS High, Due ERASE)")
        self.apply_idle(ser)
        time.sleep(0.03)
        ser.dtr = True
        ser.rts = False
        time.sleep(ERASE_S)
        ser.rts = True
        time.sleep(GAP_S)
        self.pulse_reset(ser, log)
        self.apply_idle(ser)

    def prepare_hello(self, ser, log, auto_reset: bool) -> None:
        self.apply_idle(ser)
        time.sleep(0.05)
        if auto_reset:
            log("연결 시 자동 리셋")
            self.pulse_reset(ser, log)
        else:
            log("자동 리셋 꺼짐. 리셋 버튼 또는 보드 리셋을 누르세요")

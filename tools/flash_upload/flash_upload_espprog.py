# -*- coding: utf-8 -*-
"""ESP-Prog 업로더. 펌웨어 ERASE/RESET + 이미지. 프로토콜은 app/protocol 공통."""
from __future__ import annotations

from app import App
from hw import EspProgHw

if __name__ == "__main__":
    App(EspProgHw()).mainloop()

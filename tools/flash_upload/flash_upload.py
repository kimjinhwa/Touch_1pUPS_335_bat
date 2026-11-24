# -*- coding: utf-8 -*-
"""수동 ERASE/RESET 업로더. 이미지 프로토콜은 app/protocol 공통."""
from __future__ import annotations

from app import App
from hw import ManualHw

if __name__ == "__main__":
    App(ManualHw()).mainloop()

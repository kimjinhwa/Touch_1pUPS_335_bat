# -*- coding: utf-8 -*-
"""이미지 업로드 GUI. ERASE/RESET/펌웨어는 hw 백엔드만 다름."""
from __future__ import annotations

import json
import queue
import sys
import threading
import time
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, font as tkfont, messagebox, ttk

from firmware import list_firmware_bins, run_bossac
from hw import HardwareBackend, ManualHw
from pack import build_pack, build_pack_from_items, collect_raws
from protocol import (
    BAUD,
    CHUNK,
    HELLO_WAIT_S,
    close_session_log,
    init_session_log,
    list_files_retry,
    quit_session,
    send_file,
    send_pack,
    trace,
    wait_ready_and_hello,
)

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

try:
    import winsound
except ImportError:
    winsound = None


def play_done_sound(ok: bool) -> None:
    if winsound is None:
        return
    try:
        if ok:
            for freq in (880, 1175, 1568):
                winsound.Beep(freq, 220)
            winsound.MessageBeep(winsound.MB_OK)
        else:
            winsound.Beep(420, 350)
            winsound.Beep(330, 700)
            winsound.MessageBeep(winsound.MB_ICONHAND)
    except Exception:
        pass


def _app_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


HERE = _app_dir()


def _default_han() -> Path:
    if getattr(sys, "frozen", False):
        return HERE / "han"
    for candidate in (HERE.parent / "han", HERE.parents[1] / "han"):
        if candidate.is_dir():
            return candidate
    return HERE.parent / "han"


DEFAULT_HAN = _default_han()
FALLBACK_DIR = DEFAULT_HAN.parent


class PortWatch:
    def __init__(self, ser, on_activity):
        object.__setattr__(self, "_ser", ser)
        object.__setattr__(self, "_on_activity", on_activity)

    def write(self, data):
        n = self._ser.write(data)
        self._on_activity("tx")
        return n

    def read(self, size=1):
        data = self._ser.read(size)
        if data:
            self._on_activity("rx")
        return data

    def readline(self, size=-1):
        data = self._ser.readline() if size in (-1, None) else self._ser.readline(size)
        if data:
            self._on_activity("rx")
        return data

    def __getattr__(self, name):
        return getattr(self._ser, name)

    def __setattr__(self, name, value):
        if name in ("_ser", "_on_activity"):
            object.__setattr__(self, name, value)
        else:
            setattr(self._ser, name, value)


class App(tk.Tk):
    def __init__(self, backend: HardwareBackend | None = None):
        super().__init__()
        self.hw = backend or ManualHw()
        self.title(self.hw.title)
        self.geometry("980x720" if self.hw.supports_firmware() else "980x640")
        self.minsize(860, 560)
        ui_font = tkfont.nametofont("TkDefaultFont")
        ui_font.configure(family="Malgun Gothic", size=10)
        self.option_add("*Font", ui_font)

        if serial is None:
            messagebox.showerror("모듈 없음", "pyserial 이 필요합니다.\npip install pyserial")
            self.destroy()
            return

        self.log_q: queue.Queue = queue.Queue()
        self.busy = False
        self.ser = None
        self.ser_lock = threading.Lock()
        self._settings_ready = False
        saved = self._load_settings()
        start_han = DEFAULT_HAN if DEFAULT_HAN.is_dir() else FALLBACK_DIR
        saved_folder = saved.get("folder")
        if isinstance(saved_folder, str) and saved_folder and Path(saved_folder).is_dir():
            start_han = Path(saved_folder)
        if len(sys.argv) > 1:
            arg_han = Path(sys.argv[1])
            if arg_han.is_dir():
                start_han = arg_han
        self.folder_var = tk.StringVar(value=str(start_han))
        self.port_var = tk.StringVar()
        saved_port = saved.get("port")
        self._saved_port = saved_port.strip() if isinstance(saved_port, str) else ""
        self.status_var = tk.StringVar(value="대기  ·  115200 8N1")
        self.progress_var = tk.IntVar(value=0)
        self.local_sum_var = tk.StringVar(value="로컬 0개")
        self.dev_sum_var = tk.StringVar(value="단말기 — 연결 안 됨")
        self.chunk_var = tk.StringVar(value=str(CHUNK))
        self.auto_reset_var = tk.BooleanVar(value=True)
        self.bin_var = tk.StringVar()
        self.local_items: list[tuple[str, Path]] = []
        self._list_bg = "white"
        self._bin_paths: list[Path] = []
        self._line_hold = None

        self._build()
        self._refresh_ports()
        self._scan_folder()
        if self.hw.supports_firmware():
            self._refresh_bins()
        self._settings_ready = True
        self.port_var.trace_add("write", lambda *_: self._save_settings())
        self.folder_var.trace_add("write", lambda *_: self._save_settings())
        self._save_settings()
        self.after(80, self._poll_log)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _settings_path(self) -> Path:
        return HERE / f"flash_upload_{self.hw.name}.json"

    def _load_settings(self) -> dict:
        try:
            data = json.loads(self._settings_path().read_text(encoding="utf-8"))
            return data if isinstance(data, dict) else {}
        except Exception:
            return {}

    def _save_settings(self) -> None:
        if not self._settings_ready:
            return
        port = self.port_var.get().strip()
        folder = self.folder_var.get().strip()
        data = {}
        if port:
            data["port"] = port
        if folder:
            data["folder"] = folder
        try:
            self._settings_path().write_text(
                json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
            )
        except Exception:
            pass

    def _raw_of(self, ser):
        return getattr(ser, "_ser", ser)

    def _release_line_hold(self):
        hold = self._line_hold
        self._line_hold = None
        if hold is None:
            return
        try:
            self.hw.apply_idle(hold)
            hold.close()
        except Exception:
            pass

    def _keep_line_hold(self, ser, why: str):
        if not self.hw.supports_reset():
            try:
                ser.close()
            except Exception:
                pass
            return
        raw = self._raw_of(ser)
        self.hw.unfreeze()
        self.hw.apply_idle(raw)
        self._line_hold = raw
        self._log(f"포트 유지 DTR=1 RTS=1 ({why}). 창을 닫으면 PC가 DTR을 내려 리셋합니다")

    def _open_raw_serial(self, port: str):
        self._release_line_hold()
        raw = serial.Serial()
        raw.port = port
        raw.baudrate = BAUD
        raw.bytesize = serial.EIGHTBITS
        raw.parity = serial.PARITY_NONE
        raw.stopbits = serial.STOPBITS_ONE
        raw.timeout = 0.3
        raw.write_timeout = 20
        raw.xonxoff = False
        raw.rtscts = False
        raw.dsrdtr = False
        raw.dtr = self.hw.idle_dtr
        raw.rts = self.hw.idle_rts
        raw.open()
        self.hw.apply_idle(raw)
        return raw

    def _build(self):
        top = ttk.Frame(self, padding=8)
        top.pack(fill="x")

        ttk.Label(top, text="COM").pack(side="left")
        self.port_cb = ttk.Combobox(top, textvariable=self.port_var, width=18, state="readonly")
        self.port_cb.pack(side="left", padx=4)
        self.port_cb.bind("<<ComboboxSelected>>", lambda _e: self._save_settings())
        ttk.Button(top, text="새로고침", command=self._refresh_ports).pack(side="left", padx=2)
        self.conn_btn = ttk.Button(top, text="연결 (HELLO 대기)", command=self._toggle_conn)
        self.conn_btn.pack(side="left", padx=8)

        if self.hw.supports_reset():
            self.reset_btn = ttk.Button(top, text="리셋", command=self._start_reset)
            self.reset_btn.pack(side="left", padx=2)
            ttk.Checkbutton(top, text="연결 시 자동 리셋", variable=self.auto_reset_var).pack(
                side="left", padx=6
            )

        led_fr = ttk.Frame(top)
        led_fr.pack(side="left", padx=10)
        self._leds = {}
        for key, text, off, on in (
            ("conn", "연결", "#6b6b6b", "#2ecc71"),
            ("tx", "송신", "#6b3030", "#ff3b30"),
            ("rx", "수신", "#2d5a34", "#34c759"),
        ):
            cell = ttk.Frame(led_fr)
            cell.pack(side="left", padx=6)
            cv = tk.Canvas(cell, width=18, height=18, highlightthickness=0)
            oval = cv.create_oval(3, 3, 15, 15, fill=off, outline="#333333")
            cv.pack(side="left")
            ttk.Label(cell, text=text).pack(side="left", padx=(4, 0))
            self._leds[key] = {"cv": cv, "id": oval, "off": off, "on": on, "job": None}

        self.status_lbl = tk.Label(
            top,
            textvariable=self.status_var,
            fg="#333333",
            padx=8,
            pady=2,
        )
        self.status_lbl.pack(side="left", padx=8)
        ttk.Label(top, text="청크").pack(side="left", padx=(12, 0))
        chunk_cb = ttk.Combobox(
            top,
            textvariable=self.chunk_var,
            width=4,
            state="readonly",
            values=("8", "16"),
        )
        chunk_cb.pack(side="left", padx=4)

        if self.hw.supports_firmware():
            fw = ttk.Frame(self, padding=(8, 0, 8, 8))
            fw.pack(fill="x")
            ttk.Label(fw, text="펌웨어").pack(side="left")
            self.bin_cb = ttk.Combobox(fw, textvariable=self.bin_var, width=56, state="readonly")
            self.bin_cb.pack(side="left", fill="x", expand=True, padx=4)
            ttk.Button(fw, text="bin 새로고침", command=self._refresh_bins).pack(side="left")
            self.fw_btn = ttk.Button(fw, text="펌웨어 업로드", command=self._start_firmware)
            self.fw_btn.pack(side="left", padx=6)
            self.fw_img_btn = ttk.Button(fw, text="펌웨어+이미지", command=self._start_firmware_and_images)
            self.fw_img_btn.pack(side="left")

        folder_row = ttk.Frame(self, padding=(8, 0, 8, 8))
        folder_row.pack(fill="x")
        ttk.Label(folder_row, text="로컬 폴더").pack(side="left")
        ttk.Entry(folder_row, textvariable=self.folder_var).pack(side="left", fill="x", expand=True, padx=4)
        ttk.Button(folder_row, text="찾기", command=self._pick_folder).pack(side="left", padx=2)
        ttk.Button(folder_row, text="목록 갱신", command=self._scan_folder).pack(side="left")
        self.dev_scan_btn = ttk.Button(
            folder_row, text="단말기 스캔", command=self._start_list, state="disabled"
        )
        self.dev_scan_btn.pack(side="left", padx=4)

        body = ttk.Frame(self, padding=(8, 0, 8, 8))
        body.pack(fill="both", expand=True)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(1, weight=1)

        ttk.Label(body, text="로컬 RAW  (Ctrl / Shift 다중 선택)").grid(row=0, column=0, sticky="w")
        ttk.Label(body, text="단말기 파일 목록").grid(row=0, column=1, sticky="w")

        self.local_list = tk.Listbox(
            body, height=14, selectmode=tk.EXTENDED, exportselection=False
        )
        self.dev_list = tk.Listbox(body, height=14, exportselection=False)
        self._list_bg = str(self.local_list.cget("bg") or "white")
        self.local_list.grid(row=1, column=0, sticky="nsew", padx=(0, 6))
        self.dev_list.grid(row=1, column=1, sticky="nsew")
        self.local_list.bind("<Control-a>", self._select_all_local)
        self.local_list.bind("<Control-A>", self._select_all_local)

        ttk.Label(body, textvariable=self.local_sum_var).grid(row=2, column=0, sticky="w", pady=(4, 0))
        ttk.Label(body, textvariable=self.dev_sum_var).grid(row=2, column=1, sticky="w", pady=(4, 0))

        self.progress = ttk.Progressbar(body, maximum=100, variable=self.progress_var)
        self.progress.grid(row=3, column=0, columnspan=2, sticky="ew", pady=8)

        btns = ttk.Frame(body)
        btns.grid(row=4, column=0, columnspan=2, sticky="w")
        self.list_btn = ttk.Button(btns, text="단말기 목록", command=self._start_list, state="disabled")
        self.list_btn.pack(side="left")
        self.send_all_btn = ttk.Button(btns, text="전체 전송", command=self._start_send_all, state="disabled")
        self.send_all_btn.pack(side="left", padx=8)
        self.send_sel_btn = ttk.Button(btns, text="선택 팩 전송", command=self._start_send_sel, state="disabled")
        self.send_sel_btn.pack(side="left")
        self.put_btn = ttk.Button(btns, text="제자리 교체", command=self._start_put, state="disabled")
        self.put_btn.pack(side="left", padx=8)
        self.quit_btn = ttk.Button(btns, text="종료 (정상 부팅)", command=self._start_quit, state="disabled")
        self.quit_btn.pack(side="left")
        hint = "연결을 유지한 채 목록·전송을 반복할 수 있습니다. 종료를 눌러야 업로드 모드에서 빠져 정상 부팅합니다."
        if self.hw.supports_firmware():
            hint = (
                "펌웨어+이미지는 폴더를 고른 뒤 한 번에 ERASE·펌웨어·그림까지 합니다. "
                "이미지 전송 중에는 DTR 리셋을 걸지 않습니다. 끝나면 소리가 납니다. " + hint
            )
        ttk.Label(body, text=hint).grid(row=5, column=0, columnspan=2, sticky="w", pady=(8, 0))

        log_fr = ttk.LabelFrame(self, text="로그", padding=8)
        log_fr.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.log_txt = tk.Text(log_fr, height=8, wrap="word")
        self.log_txt.pack(fill="both", expand=True)

    def _connected(self) -> bool:
        return self.ser is not None and self.ser.is_open

    def _set_status(self, text: str, kind: str = "idle"):
        self.status_var.set(text)
        colors = {
            "wait": ("#9a3b00", "#ffd37a"),
            "ok": ("#146c2e", "#d8f3dc"),
            "fail": ("#9b1c1c", "#ffd6d6"),
            "idle": ("#333333", "#f0f0f0"),
        }
        fg, bg = colors.get(kind, colors["idle"])
        self.status_lbl.configure(fg=fg, bg=bg)

    def _set_session_ui(self, on: bool):
        st = "normal" if on else "disabled"
        self.list_btn.configure(state=st)
        self.dev_scan_btn.configure(state=st)
        self.send_all_btn.configure(state=st)
        self.send_sel_btn.configure(state=st)
        self.put_btn.configure(state=st)
        self.quit_btn.configure(state=st)
        self.conn_btn.configure(text="연결 끊기" if on else "연결 (HELLO 대기)")
        self._led_set("conn", on)
        if not on:
            self._led_set("tx", False)
            self._led_set("rx", False)

    def _led_set(self, key: str, on: bool):
        spec = self._leds[key]
        if spec["job"] is not None:
            self.after_cancel(spec["job"])
            spec["job"] = None
        spec["cv"].itemconfigure(spec["id"], fill=spec["on"] if on else spec["off"])

    def _led_pulse(self, key: str):
        if key == "conn":
            return
        spec = self._leds[key]
        spec["cv"].itemconfigure(spec["id"], fill=spec["on"])
        if spec["job"] is not None:
            self.after_cancel(spec["job"])
        spec["job"] = self.after(150, lambda k=key: self._led_pulse_off(k))

    def _led_pulse_off(self, key: str):
        spec = self._leds[key]
        spec["job"] = None
        spec["cv"].itemconfigure(spec["id"], fill=spec["off"])

    def _port_activity(self, kind: str):
        self.log_q.put(("led", kind))

    def _refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        prefer = self.port_var.get().strip() or self._saved_port
        if prefer and prefer not in ports:
            ports = [prefer] + ports
        self.port_cb["values"] = ports
        if self.port_var.get() in ports:
            return
        if prefer and prefer in ports:
            self.port_var.set(prefer)
            return
        if self.hw.default_port in ports:
            self.port_var.set(self.hw.default_port)
            return
        if ports:
            self.port_var.set(ports[0])

    def _refresh_bins(self):
        self._bin_paths = list_firmware_bins()
        names = [p.name for p in self._bin_paths]
        self.bin_cb["values"] = names
        if names and self.bin_var.get() not in names:
            self.bin_var.set(names[0])
        if not names:
            self.bin_var.set("")
            self._log("upload 폴더에 *.ino.bin 이 없습니다")

    def _selected_bin(self) -> Path | None:
        name = self.bin_var.get().strip()
        for path in self._bin_paths:
            if path.name == name:
                return path
        return None

    def _pick_folder(self):
        chosen = filedialog.askdirectory(initialdir=self.folder_var.get() or str(FALLBACK_DIR))
        if chosen:
            self.folder_var.set(chosen)
            self._save_settings()
            self._scan_folder()

    def _flash_list(self, box: tk.Listbox):
        box.configure(bg="#fff3c4")
        self.after(450, lambda b=box: b.configure(bg=self._list_bg))

    def _now(self) -> str:
        return datetime.now().strftime("%H:%M:%S")

    def _select_all_local(self, _event=None):
        self.local_list.select_set(0, tk.END)
        return "break"

    def _scan_folder(self):
        self.local_list.delete(0, tk.END)
        self.local_items = []
        self.local_sum_var.set("스캔 중...")
        self._flash_list(self.local_list)
        self.update_idletasks()

        folder = Path(self.folder_var.get())
        try:
            items = collect_raws(folder)
        except Exception as exc:
            self.local_sum_var.set(f"스캔 실패  ·  {self._now()}")
            self._log(f"로컬 스캔 실패: {exc}")
            return
        total = 0
        for stored, src in items:
            size = src.stat().st_size
            total += size
            self.local_list.insert(tk.END, f"{stored}  ({size} B)")
        self.local_items = items
        self.local_sum_var.set(f"로컬 {len(items)}개  /  {total} 바이트  ·  {self._now()} 갱신")
        self._log(f"로컬 스캔 완료: {len(items)}개, {total} 바이트")

    def _log(self, msg: str):
        self.log_q.put(("log", msg))

    def _toggle_conn(self):
        if self._connected():
            self._start_quit()
            return
        if self.busy:
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("포트", "COM 포트를 선택하세요.")
            return
        self.busy = True
        wait_msg = "연결 중 — HELLO 대기"
        if self.hw.supports_reset() and self.auto_reset_var.get():
            wait_msg = "연결 중 — 자동 리셋 후 HELLO"
        else:
            wait_msg = "연결 중 — 단말을 리셋하세요"
        self._set_status(f"{wait_msg}  ·  남은 {int(HELLO_WAIT_S)}초", "wait")
        self.dev_list.delete(0, tk.END)
        self.dev_sum_var.set("연결 중...")
        threading.Thread(
            target=self._connect_worker,
            args=(port, self.auto_reset_var.get()),
            daemon=True,
        ).start()

    def _connect_worker(self, port: str, auto_reset: bool):
        ser = None
        ok = False
        posted = False
        try:
            log_path = init_session_log()
            self._log(f"세션 로그 {log_path}")
            raw = self._open_raw_serial(port)
            trace(
                "META",
                f"port={port} baud={BAUD} hw={self.hw.name} "
                f"dtr={int(raw.dtr)} rts={int(raw.rts)} xonxoff=0",
            )
            ser = PortWatch(raw, self._port_activity)
            self.hw.prepare_hello(ser, self._log, auto_reset)
            if wait_ready_and_hello(
                ser,
                self._log,
                HELLO_WAIT_S,
                on_tick=lambda remain: self.log_q.put(("wait", remain)),
            ):
                ok = True
                with self.ser_lock:
                    self.ser = ser
                    ser = None
                self._log("업로드 모드")
                self.hw.freeze_run(self.ser, self._log)
                self.log_q.put(("connected", True))
                posted = True
                with self.ser_lock:
                    files = list_files_retry(self.ser, self._log)
                self.log_q.put(("devlist", files))
        except Exception as exc:
            self._log(f"연결 오류: {exc}")
        finally:
            if ser is not None:
                try:
                    ser.close()
                except Exception:
                    pass
            if posted:
                self.log_q.put(("idle", None))
            else:
                self.log_q.put(("connected", ok))

    def _start_reset(self):
        if self.busy:
            return
        if self._connected():
            if not messagebox.askyesno("리셋", "업로드 세션이 끊깁니다. 리셋할까요?"):
                return
            self.busy = True
            threading.Thread(target=self._reset_connected_worker, daemon=True).start()
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("포트", "COM 포트를 선택하세요.")
            return
        self.busy = True
        threading.Thread(target=self._reset_idle_worker, args=(port,), daemon=True).start()

    def _reset_idle_worker(self, port: str):
        ser = None
        try:
            ser = self._open_raw_serial(port)
            self.hw.pulse_reset(ser, self._log)
            self._log("리셋 완료")
            self._keep_line_hold(ser, "리셋 후")
            ser = None
        except Exception as exc:
            self._log(f"리셋 오류: {exc}")
        finally:
            if ser is not None:
                try:
                    self.hw.apply_idle(ser)
                    ser.close()
                except Exception:
                    pass
            self.log_q.put(("idle", None))

    def _reset_connected_worker(self):
        try:
            with self.ser_lock:
                if self.ser is not None:
                    try:
                        quit_session(self.ser, self._log)
                    except Exception:
                        pass
                    try:
                        self.hw.unfreeze()
                        self.hw.pulse_reset(self.ser, self._log)
                    except Exception as exc:
                        self._log(f"리셋 오류: {exc}")
                    if self.hw.supports_reset():
                        self._keep_line_hold(self.ser, "리셋 후")
                    else:
                        try:
                            self.ser.close()
                        except Exception:
                            pass
                    self.ser = None
        finally:
            self.log_q.put(("disconnected", None))

    def _start_firmware(self):
        if self.busy:
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("포트", "COM 포트를 선택하세요.")
            return
        bin_path = self._selected_bin()
        if bin_path is None:
            messagebox.showwarning("펌웨어", "bin 파일을 선택하세요.")
            return
        if self._connected():
            if not messagebox.askyesno(
                "펌웨어 업로드",
                "이미지 세션을 끊고 SAM3X를 ERASE 한 뒤 펌웨어를 씁니다. 계속할까요?",
            ):
                return
        elif not messagebox.askyesno(
            "펌웨어 업로드",
            f"ERASE 280ms → {bin_path.name} 기록 → 리셋\n계속할까요?",
        ):
            return
        self.busy = True
        self.progress_var.set(0)
        self._set_status("펌웨어 업로드 중", "wait")
        threading.Thread(
            target=self._firmware_worker, args=(port, bin_path), daemon=True
        ).start()

    def _firmware_worker(self, port: str, bin_path: Path):
        ser = None
        ok = False
        err = ""
        try:
            with self.ser_lock:
                if self.ser is not None:
                    try:
                        quit_session(self.ser, self._log)
                    except Exception:
                        pass
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                    self.ser = None
            self.log_q.put(("session_drop", None))
            self.hw.unfreeze()
            self._log(f"하드웨어 ERASE/RESET  {port}")
            ser = self._open_raw_serial(port)
            self.hw.erase_then_reset(ser, self._log)
            self.hw.apply_idle(ser)
            ser.close()
            ser = None
            ok = run_bossac(port, bin_path, self._log, self._progress)
            if ok:
                self._log("펌웨어 기록 완료. 이미지 연결을 누르면 자동 리셋 후 HELLO")
                try:
                    self._keep_line_hold(self._open_raw_serial(port), "펌웨어 후")
                except Exception as exc:
                    self._log(f"DTR 유지 실패: {exc}")
        except Exception as exc:
            self._log(f"펌웨어 오류: {exc}")
            err = str(exc)
        finally:
            if ser is not None:
                try:
                    self.hw.apply_idle(ser)
                    ser.close()
                except Exception:
                    pass
            play_done_sound(ok)
            if ok:
                detail = f"{bin_path.name}\n포트 {port}"
            elif err:
                detail = f"{bin_path.name}\n포트 {port}\n{err}"
            else:
                detail = f"{bin_path.name}\n포트 {port}\nbossac 기록이 실패했습니다."
            self.log_q.put(
                (
                    "fwdone",
                    {
                        "ok": ok,
                        "title": "펌웨어 기록 완료" if ok else "펌웨어 기록 실패",
                        "detail": detail,
                    },
                )
            )

    def _start_firmware_and_images(self):
        if self.busy:
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("포트", "COM 포트를 선택하세요.")
            return
        bin_path = self._selected_bin()
        if bin_path is None:
            messagebox.showwarning("펌웨어", "bin 파일을 선택하세요.")
            return
        folder = Path(self.folder_var.get())
        try:
            pack, listing = build_pack(folder)
        except Exception as exc:
            messagebox.showerror("이미지 폴더", str(exc))
            return
        if not messagebox.askyesno(
            "펌웨어+이미지",
            f"1) ERASE 후 펌웨어  {bin_path.name}\n"
            f"2) 이미지 전체 {len(listing)}개  ({len(pack)} 바이트)\n"
            f"3) 종료 후 정상 부팅\n\n폴더: {folder}\n계속할까요?",
        ):
            return
        self.busy = True
        self.progress_var.set(0)
        self._set_status("펌웨어+이미지 업로드 중", "wait")
        threading.Thread(
            target=self._firmware_and_images_worker,
            args=(port, bin_path, pack, len(listing)),
            daemon=True,
        ).start()

    def _firmware_and_images_worker(self, port: str, bin_path: Path, pack: bytes, nfiles: int):
        ser = None
        watch = None
        ok = False
        detail = f"{bin_path.name}\n이미지 {nfiles}개, {len(pack)} 바이트\n포트 {port}"
        stage = "시작"
        try:
            with self.ser_lock:
                if self.ser is not None:
                    try:
                        quit_session(self.ser, self._log)
                    except Exception:
                        pass
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                    self.ser = None
            self.log_q.put(("session_drop", None))
            self.hw.unfreeze()
            init_session_log()
            self._log(f"펌웨어+이미지 시작  bin={bin_path.name}  files={nfiles}")
            self.log_q.put(("status", ("펌웨어 ERASE/기록", "wait")))
            stage = "펌웨어"
            ser = self._open_raw_serial(port)
            self.hw.erase_then_reset(ser, self._log)
            self.hw.apply_idle(ser)
            ser.close()
            ser = None
            if not run_bossac(port, bin_path, self._log, lambda s, t: self._progress_span(s, t, 0, 45)):
                self._log("펌웨어 기록 실패")
                detail = f"펌웨어 기록 실패\n{bin_path.name}\n포트 {port}"
                return
            self._log("펌웨어 완료. 이미지 HELLO")
            self.log_q.put(("status", ("이미지 연결 — HELLO", "wait")))
            stage = "HELLO"
            time.sleep(0.4)
            ser = self._open_raw_serial(port)
            watch = PortWatch(ser, self._port_activity)
            ser = None
            self.hw.prepare_hello(watch, self._log, True)
            if not wait_ready_and_hello(
                watch,
                self._log,
                8.0,
                on_tick=lambda remain: self.log_q.put(("wait", remain)),
            ):
                self._log("HELLO 재시도")
                self.hw.pulse_reset(watch, self._log)
                if not wait_ready_and_hello(
                    watch,
                    self._log,
                    HELLO_WAIT_S,
                    on_tick=lambda remain: self.log_q.put(("wait", remain)),
                ):
                    self._log("이미지 HELLO 실패")
                    detail = (
                        f"펌웨어는 기록됨: {bin_path.name}\n"
                        "이미지 HELLO 실패. 보드를 리셋했는지 확인하세요."
                    )
                    return
            self.hw.freeze_run(watch, self._log)
            self._log(f"이미지 전체 전송 {nfiles}개 {len(pack)} 바이트")
            self.log_q.put(("status", ("이미지 전송 중", "wait")))
            stage = "이미지"
            if not send_pack(
                watch, pack, self._log, lambda s, t: self._progress_span(s, t, 45, 100)
            ):
                self._log("이미지 전송 실패")
                detail = (
                    f"펌웨어는 기록됨: {bin_path.name}\n"
                    f"이미지 전송 실패  ({nfiles}개, {len(pack)} 바이트)"
                )
                return
            files = list_files_retry(watch, self._log)
            self.log_q.put(("devlist", files))
            quit_session(watch, self._log)
            self._keep_line_hold(watch, "펌웨어+이미지 후")
            watch = None
            ok = True
            self._log("펌웨어+이미지 완료. 단말 정상 부팅")
            detail = (
                f"펌웨어 {bin_path.name}\n"
                f"이미지 {nfiles}개, {len(pack)} 바이트\n포트 {port}"
            )
        except Exception as exc:
            self._log(f"펌웨어+이미지 오류: {exc}")
            detail = f"{stage} 중 오류\n{exc}"
        finally:
            if ser is not None:
                try:
                    self.hw.apply_idle(ser)
                    ser.close()
                except Exception:
                    pass
            if watch is not None:
                try:
                    watch.close()
                except Exception:
                    pass
            play_done_sound(ok)
            self.log_q.put(
                (
                    "alldone",
                    {
                        "ok": ok,
                        "title": "펌웨어+이미지 완료" if ok else "펌웨어+이미지 실패",
                        "detail": detail,
                    },
                )
            )

    def _progress_span(self, sent: int, total: int, start: int, end: int):
        if total <= 0:
            pct = start
        else:
            pct = start + int(sent * (end - start) / total)
        self.log_q.put(("prog", pct))

    def _start_list(self):
        if self.busy:
            return
        if not self._connected():
            messagebox.showinfo("단말기 스캔", "먼저 연결하세요.")
            return
        self.dev_list.delete(0, tk.END)
        self.dev_sum_var.set("목록 읽는 중...")
        self._flash_list(self.dev_list)
        self.busy = True
        threading.Thread(target=self._list_worker, daemon=True).start()

    def _list_worker(self):
        try:
            with self.ser_lock:
                files = list_files_retry(self.ser, self._log)
            self.log_q.put(("devlist", files))
        except Exception as exc:
            self._log(f"목록 오류: {exc}")
        finally:
            self.log_q.put(("idle", None))

    def _chunk_size(self) -> int:
        try:
            n = int(self.chunk_var.get())
        except ValueError:
            n = CHUNK
        return 16 if n >= 16 else 8

    def _start_send_all(self):
        if not self._connected() or self.busy:
            return
        folder = Path(self.folder_var.get())
        try:
            pack, listing = build_pack(folder)
        except Exception as exc:
            messagebox.showerror("팩 오류", str(exc))
            return
        if not messagebox.askyesno(
            "전체 전송",
            f"칩을 전부 지우고 폴더 전체 {len(listing)}개 파일({len(pack)} 바이트)을 다시 씁니다.\n계속할까요?",
        ):
            return
        self._log(f"전체 전송 팩 {len(pack)} 바이트, 파일 {len(listing)}개")
        self.busy = True
        self.progress_var.set(0)
        self._set_status("전체 전송 중", "wait")
        threading.Thread(
            target=self._send_all_worker,
            args=(pack, self._chunk_size(), "전체 전송", len(listing), str(folder)),
            daemon=True,
        ).start()

    def _send_all_worker(
        self,
        pack: bytes,
        chunk_size: int = CHUNK,
        kind: str = "전체 전송",
        nfiles: int = 0,
        folder: str = "",
    ):
        ok = False
        detail = f"{kind}  {nfiles}개, {len(pack)} 바이트\n{folder}"
        try:
            with self.ser_lock:
                self.hw.freeze_run(self.ser, self._log)
                ok = send_pack(
                    self.ser, pack, self._log, self._progress, chunk_size=chunk_size
                )
            if ok:
                self._log("전체 업로드 완료")
                with self.ser_lock:
                    files = list_files_retry(self.ser, self._log)
                self.log_q.put(("devlist", files))
                detail = f"{kind} 성공\n{nfiles}개 파일, {len(pack)} 바이트\n{folder}"
            else:
                detail = (
                    f"{kind} 실패 — 단말기 CRC/전송 응답이 없습니다.\n"
                    f"{nfiles}개 파일, {len(pack)} 바이트\n{folder}"
                )
        except Exception as exc:
            self._log(f"전송 오류: {exc}")
            detail = f"{kind} 오류\n{exc}"
        finally:
            play_done_sound(ok)
            self.log_q.put(
                (
                    "sent",
                    {
                        "ok": ok,
                        "title": f"{kind} 완료" if ok else f"{kind} 실패",
                        "detail": detail,
                    },
                )
            )

    def _selected_items(self) -> list[tuple[str, Path]]:
        idxs = self.local_list.curselection()
        out: list[tuple[str, Path]] = []
        for i in idxs:
            if 0 <= int(i) < len(self.local_items):
                out.append(self.local_items[int(i)])
        return out

    def _start_send_sel(self):
        if not self._connected() or self.busy:
            return
        items = self._selected_items()
        if not items:
            messagebox.showwarning("선택 팩 전송", "로컬 목록에서 파일을 하나 이상 선택하세요.")
            return
        try:
            pack, listing = build_pack_from_items(items)
        except Exception as exc:
            messagebox.showerror("팩 오류", str(exc))
            return
        if not messagebox.askyesno(
            "선택 팩 전송",
            f"칩을 전부 지우고 선택한 {len(listing)}개만 올립니다. ({len(pack)} 바이트)\n"
            "고르지 않은 파일은 칩에서 사라집니다.\n계속할까요?",
        ):
            return
        self._log(f"선택 팩 {len(pack)} 바이트, 파일 {len(listing)}개")
        self.busy = True
        self.progress_var.set(0)
        self._set_status("선택 팩 전송 중", "wait")
        threading.Thread(
            target=self._send_all_worker,
            args=(
                pack,
                self._chunk_size(),
                "선택 팩 전송",
                len(listing),
                self.folder_var.get(),
            ),
            daemon=True,
        ).start()

    def _start_put(self):
        if not self._connected() or self.busy:
            return
        items = self._selected_items()
        if not items:
            messagebox.showwarning("제자리 교체", "로컬 목록에서 파일을 하나 이상 선택하세요.")
            return
        total = 0
        payload: list[tuple[str, bytes]] = []
        try:
            for stored, src in items:
                data = src.read_bytes()
                payload.append((stored, data))
                total += len(data)
        except Exception as exc:
            messagebox.showerror("읽기 오류", str(exc))
            return
        if not messagebox.askyesno(
            "제자리 교체",
            f"선택한 {len(payload)}개만 제자리 교체합니다. ({total} 바이트)\n"
            "칩에 같은 경로·같은 크기로 이미 있어야 합니다.\n"
            "새 파일이나 크기가 바뀐 경우는 전체/선택 팩 전송을 쓰세요.\n계속할까요?",
        ):
            return
        self._log(f"제자리 교체 {len(payload)}개, {total} 바이트")
        self.busy = True
        self.progress_var.set(0)
        self._set_status("제자리 교체 중", "wait")
        threading.Thread(target=self._put_worker, args=(payload, total), daemon=True).start()

    def _put_worker(self, payload: list[tuple[str, bytes]], total: int):
        ok = False
        names = ", ".join(path for path, _ in payload[:4])
        if len(payload) > 4:
            names += f" 외 {len(payload) - 4}개"
        detail = f"제자리 교체 {len(payload)}개, {total} 바이트\n{names}"
        try:
            already = 0
            chunk = self._chunk_size()
            with self.ser_lock:
                self.hw.freeze_run(self.ser, self._log)
                for path, data in payload:
                    self._log(f"제자리 {path}")
                    if not send_file(
                        self.ser, path, data, self._log, self._progress, already, total, chunk_size=chunk
                    ):
                        self._log(f"제자리 실패: {path}")
                        detail = f"제자리 실패: {path}\n{len(payload)}개 중 중단, {total} 바이트"
                        return
                    already += len(data)
            ok = True
            self._log("제자리 교체 완료")
            with self.ser_lock:
                files = list_files_retry(self.ser, self._log)
            self.log_q.put(("devlist", files))
            detail = f"제자리 교체 성공\n{len(payload)}개 파일, {total} 바이트\n{names}"
        except Exception as exc:
            self._log(f"전송 오류: {exc}")
            detail = f"제자리 교체 오류\n{exc}"
        finally:
            play_done_sound(ok)
            self.log_q.put(
                (
                    "sent",
                    {
                        "ok": ok,
                        "title": "제자리 교체 완료" if ok else "제자리 교체 실패",
                        "detail": detail,
                    },
                )
            )

    def _start_quit(self):
        if self.busy:
            return
        self.busy = True
        threading.Thread(target=self._quit_worker, daemon=True).start()

    def _quit_worker(self):
        try:
            with self.ser_lock:
                if self.ser is not None:
                    quit_session(self.ser, self._log)
                    if self.hw.supports_reset():
                        self._keep_line_hold(self.ser, "세션 종료 후")
                    else:
                        try:
                            self.ser.close()
                        except Exception:
                            pass
                    self.ser = None
        except Exception as exc:
            self._log(f"종료 오류: {exc}")
        finally:
            self.log_q.put(("disconnected", None))

    def _progress(self, sent: int, total: int):
        pct = 0 if total <= 0 else int(sent * 100 / total)
        self.log_q.put(("prog", pct))

    def _poll_log(self):
        while True:
            try:
                kind, payload = self.log_q.get_nowait()
            except queue.Empty:
                break
            if kind == "log":
                self.log_txt.insert("end", str(payload) + "\n")
                self.log_txt.see("end")
            elif kind == "led":
                self._led_pulse(str(payload))
            elif kind == "prog":
                self.progress_var.set(int(payload))
            elif kind == "wait":
                remain = int(payload)
                if self.hw.supports_reset() and self.auto_reset_var.get():
                    msg = f"연결 중 — 자동 리셋 후 HELLO  ·  남은 {remain}초"
                else:
                    msg = f"연결 중 — 단말을 리셋하세요  ·  남은 {remain}초"
                self._set_status(msg, "wait")
            elif kind == "devlist":
                self.dev_list.delete(0, tk.END)
                self._flash_list(self.dev_list)
                files = payload or []
                total = 0
                for path, size in files:
                    total += size
                    self.dev_list.insert(tk.END, f"{path}  ({size} B)")
                self.dev_sum_var.set(f"단말기 {len(files)}개  /  {total} 바이트  ·  {self._now()} 갱신")
                self._log(f"단말기 목록 갱신: {len(files)}개")
            elif kind == "connected":
                self.busy = False
                if payload:
                    self._set_session_ui(True)
                    self._set_status("업로드 모드", "ok")
                else:
                    self._set_session_ui(False)
                    self._set_status("연결 실패  ·  115200 8N1", "fail")
                    self.dev_sum_var.set("단말기 — 연결 안 됨")
            elif kind == "sent":
                self.busy = False
                ok, title, detail = self._result_fields(
                    payload, "이미지 업로드 완료", "이미지 업로드 실패"
                )
                if ok:
                    self.progress_var.set(100)
                    self._set_status("업로드 모드", "ok")
                else:
                    self._set_status("이미지 업로드 실패", "fail")
                self._notify_result(ok, title, detail)
            elif kind == "session_drop":
                self._set_session_ui(False)
                self.dev_sum_var.set("단말기 — 연결 안 됨")
                self.dev_list.delete(0, tk.END)
            elif kind == "status":
                text, kind_s = payload
                self._set_status(str(text), str(kind_s))
            elif kind == "fwdone":
                self.busy = False
                self._set_session_ui(False)
                ok, title, detail = self._result_fields(
                    payload, "펌웨어 기록 완료", "펌웨어 기록 실패"
                )
                if ok:
                    self.progress_var.set(100)
                    self._set_status("펌웨어 완료  ·  이미지 연결 가능", "ok")
                else:
                    self._set_status("펌웨어 실패", "fail")
                self._notify_result(ok, title, detail)
            elif kind == "alldone":
                self.busy = False
                self._set_session_ui(False)
                self.dev_sum_var.set("단말기 — 연결 안 됨")
                ok, title, detail = self._result_fields(
                    payload, "펌웨어+이미지 완료", "펌웨어+이미지 실패"
                )
                if ok:
                    self.progress_var.set(100)
                    self._set_status("펌웨어+이미지 완료  ·  단말 정상 부팅", "ok")
                else:
                    self._set_status("펌웨어+이미지 실패", "fail")
                self._notify_result(ok, title, detail)
            elif kind == "idle":
                self.busy = False
            elif kind == "disconnected":
                self.busy = False
                self._set_session_ui(False)
                self._set_status("대기  ·  단말 정상 부팅", "idle")
                self.dev_sum_var.set("단말기 — 연결 안 됨")
                self.dev_list.delete(0, tk.END)
        self.after(80, self._poll_log)

    def _result_fields(self, payload, ok_title: str, fail_title: str) -> tuple[bool, str, str]:
        if isinstance(payload, dict):
            ok = bool(payload.get("ok"))
            title = str(payload.get("title") or (ok_title if ok else fail_title))
            detail = str(payload.get("detail") or "")
            return ok, title, detail
        ok = bool(payload)
        return ok, ok_title if ok else fail_title, ""

    def _notify_result(self, ok: bool, title: str, detail: str = "") -> None:
        bg = "#1b9e4b" if ok else "#d62828"
        fg = "#ffffff"
        headline = "완료" if ok else "실패"
        win = tk.Toplevel(self)
        win.title(title)
        win.configure(bg=bg)
        win.resizable(False, False)
        win.transient(self)
        win.attributes("-topmost", True)
        width, height = 620, 380 if detail else 300
        x = max(0, (win.winfo_screenwidth() - width) // 2)
        y = max(0, (win.winfo_screenheight() - height) // 3)
        win.geometry(f"{width}x{height}+{x}+{y}")
        tk.Label(
            win, text=headline, font=("Malgun Gothic", 52, "bold"), fg=fg, bg=bg
        ).pack(pady=(28, 2))
        tk.Label(
            win, text=title, font=("Malgun Gothic", 20, "bold"), fg=fg, bg=bg
        ).pack()
        if detail:
            tk.Label(
                win,
                text=detail,
                font=("Malgun Gothic", 14),
                fg=fg,
                bg=bg,
                justify="center",
                wraplength=560,
            ).pack(pady=(12, 0))
        btn = tk.Button(
            win,
            text="확인",
            font=("Malgun Gothic", 16, "bold"),
            command=win.destroy,
            bg="#ffffff",
            fg=bg,
            activebackground="#f4f4f4",
            activeforeground=bg,
            relief="flat",
            padx=32,
            pady=8,
        )
        btn.pack(pady=24)
        win.bind("<Return>", lambda _e: win.destroy())
        win.bind("<Escape>", lambda _e: win.destroy())
        btn.focus_set()
        win.lift()
        win.grab_set()

    def _on_close(self):
        self._save_settings()
        self.hw.unfreeze()
        if self._connected():
            try:
                quit_session(self.ser, self._log)
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self._release_line_hold()
        close_session_log()
        self.destroy()

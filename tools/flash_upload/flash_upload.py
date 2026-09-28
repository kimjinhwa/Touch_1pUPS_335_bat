# -*- coding: utf-8 -*-
"""
1pUPS 표시기 AT25SF081 이미지 팩 업로드 (C1IM).

Serial COM5 115200. 시리얼 모니터는 닫고 이 프로그램만 연다.
부팅 때 HELLO에 응답하면 업로드 모드, 아니면 정상 부팅.

실행:
  pip install -r requirements.txt
  python flash_upload.py
"""
from __future__ import annotations

import queue
import threading
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, font as tkfont, messagebox, ttk

from pack import build_pack, build_pack_from_items, collect_raws
from protocol import BAUD, CHUNK, list_files, quit_session, send_file, send_pack, wait_ready_and_hello

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

SKETCH_DIR = Path(__file__).resolve().parents[2]
DEFAULT_HAN = SKETCH_DIR / "han"
DEFAULT_PORT = "COM5"


class PortWatch:
    """시리얼 write/read 때 GUI LED 를 켠다."""

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
    def __init__(self):
        super().__init__()
        self.title("1pUPS 표시기 플래시 업로드  (COM5 Serial / 115200)")
        self.geometry("980x640")
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
        self.folder_var = tk.StringVar(value=str(DEFAULT_HAN if DEFAULT_HAN.is_dir() else SKETCH_DIR))
        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="대기  ·  115200 8N1")
        self.progress_var = tk.IntVar(value=0)
        self.local_sum_var = tk.StringVar(value="로컬 0개")
        self.dev_sum_var = tk.StringVar(value="단말기 — 연결 안 됨")
        self.chunk_var = tk.StringVar(value=str(CHUNK))
        self.local_items: list[tuple[str, Path]] = []
        self._list_bg = "white"

        self._build()
        self._refresh_ports()
        self._scan_folder()
        self.after(80, self._poll_log)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build(self):
        top = ttk.Frame(self, padding=8)
        top.pack(fill="x")

        ttk.Label(top, text="COM").pack(side="left")
        self.port_cb = ttk.Combobox(top, textvariable=self.port_var, width=18, state="readonly")
        self.port_cb.pack(side="left", padx=4)
        ttk.Button(top, text="새로고침", command=self._refresh_ports).pack(side="left", padx=2)
        self.conn_btn = ttk.Button(top, text="연결 (HELLO 대기)", command=self._toggle_conn)
        self.conn_btn.pack(side="left", padx=8)

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

        ttk.Label(top, textvariable=self.status_var).pack(side="left", padx=8)
        ttk.Label(top, text="청크").pack(side="left", padx=(12, 0))
        chunk_cb = ttk.Combobox(
            top,
            textvariable=self.chunk_var,
            width=4,
            state="readonly",
            values=("8", "16"),
        )
        chunk_cb.pack(side="left", padx=4)

        folder_row = ttk.Frame(self, padding=(8, 0, 8, 8))
        folder_row.pack(fill="x")
        ttk.Label(folder_row, text="로컬 폴더").pack(side="left")
        ttk.Entry(folder_row, textvariable=self.folder_var).pack(side="left", fill="x", expand=True, padx=4)
        ttk.Button(folder_row, text="찾기", command=self._pick_folder).pack(side="left", padx=2)
        ttk.Button(folder_row, text="다시 스캔", command=self._scan_folder).pack(side="left")

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
        ttk.Label(
            body,
            text="연결을 유지한 채 목록·전송을 반복할 수 있습니다. 종료를 눌러야 업로드 모드에서 빠져 정상 부팅합니다.",
        ).grid(row=5, column=0, columnspan=2, sticky="w", pady=(8, 0))

        log_fr = ttk.LabelFrame(self, text="로그", padding=8)
        log_fr.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.log_txt = tk.Text(log_fr, height=8, wrap="word")
        self.log_txt.pack(fill="both", expand=True)

    def _connected(self) -> bool:
        return self.ser is not None and self.ser.is_open

    def _set_session_ui(self, on: bool):
        st = "normal" if on else "disabled"
        self.list_btn.configure(state=st)
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
        self.port_cb["values"] = ports
        if self.port_var.get() in ports:
            return
        if DEFAULT_PORT in ports:
            self.port_var.set(DEFAULT_PORT)
            return
        if ports:
            self.port_var.set(ports[0])

    def _pick_folder(self):
        chosen = filedialog.askdirectory(initialdir=self.folder_var.get() or str(SKETCH_DIR))
        if chosen:
            self.folder_var.set(chosen)
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
        self.status_var.set("연결 중 — 단말을 리셋하세요")
        self.dev_list.delete(0, tk.END)
        self.dev_sum_var.set("연결 중...")
        threading.Thread(target=self._connect_worker, args=(port,), daemon=True).start()

    def _connect_worker(self, port: str):
        ser = None
        ok = False
        try:
            raw = serial.Serial(
                port=port,
                baudrate=BAUD,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.3,
                write_timeout=5,
                dsrdtr=False,
                rtscts=False,
                xonxoff=False,
            )
            raw.dtr = False
            raw.rts = False
            ser = PortWatch(raw, self._port_activity)
            if wait_ready_and_hello(ser, self._log, 20.0):
                ok = True
                with self.ser_lock:
                    self.ser = ser
                    ser = None
                self._log("업로드 모드")
                files = list_files(self.ser, self._log)
                self.log_q.put(("devlist", files))
        except Exception as exc:
            self._log(f"연결 오류: {exc}")
        finally:
            if ser is not None:
                try:
                    ser.close()
                except Exception:
                    pass
            self.log_q.put(("connected", ok))

    def _start_list(self):
        if not self._connected() or self.busy:
            return
        self.dev_list.delete(0, tk.END)
        self.dev_sum_var.set("목록 읽는 중...")
        self._flash_list(self.dev_list)
        self.busy = True
        threading.Thread(target=self._list_worker, daemon=True).start()

    def _list_worker(self):
        try:
            with self.ser_lock:
                files = list_files(self.ser, self._log)
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
        self.status_var.set("전체 전송 중")
        threading.Thread(
            target=self._send_all_worker, args=(pack, self._chunk_size()), daemon=True
        ).start()

    def _send_all_worker(self, pack: bytes, chunk_size: int = CHUNK):
        ok = False
        try:
            with self.ser_lock:
                ok = send_pack(
                    self.ser, pack, self._log, self._progress, chunk_size=chunk_size
                )
            if ok:
                self._log("전체 업로드 완료")
                with self.ser_lock:
                    files = list_files(self.ser, self._log)
                self.log_q.put(("devlist", files))
        except Exception as exc:
            self._log(f"전송 오류: {exc}")
        finally:
            self.log_q.put(("sent", ok))

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
        self.status_var.set("선택 팩 전송 중")
        threading.Thread(
            target=self._send_all_worker, args=(pack, self._chunk_size()), daemon=True
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
        self.status_var.set("제자리 교체 중")
        threading.Thread(target=self._put_worker, args=(payload, total), daemon=True).start()

    def _put_worker(self, payload: list[tuple[str, bytes]], total: int):
        ok = False
        try:
            already = 0
            chunk = self._chunk_size()
            with self.ser_lock:
                for path, data in payload:
                    self._log(f"제자리 {path}")
                    if not send_file(
                        self.ser, path, data, self._log, self._progress, already, total, chunk_size=chunk
                    ):
                        self._log(f"제자리 실패: {path}")
                        return
                    already += len(data)
            ok = True
            self._log("제자리 교체 완료")
            with self.ser_lock:
                files = list_files(self.ser, self._log)
            self.log_q.put(("devlist", files))
        except Exception as exc:
            self._log(f"전송 오류: {exc}")
        finally:
            self.log_q.put(("sent", ok))

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
                    self.status_var.set("업로드 모드")
                else:
                    self._set_session_ui(False)
                    self.status_var.set("연결 실패  ·  115200 8N1")
                    self.dev_sum_var.set("단말기 — 연결 안 됨")
            elif kind == "sent":
                self.busy = False
                self.status_var.set("업로드 모드")
                if payload:
                    self.progress_var.set(100)
            elif kind == "idle":
                self.busy = False
            elif kind == "disconnected":
                self.busy = False
                self._set_session_ui(False)
                self.status_var.set("대기  ·  단말 정상 부팅")
                self.dev_sum_var.set("단말기 — 연결 안 됨")
                self.dev_list.delete(0, tk.END)
        self.after(80, self._poll_log)

    def _on_close(self):
        if self._connected():
            try:
                quit_session(self.ser, self._log)
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.destroy()


if __name__ == "__main__":
    App().mainloop()

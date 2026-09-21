#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MCU debug host: serial RX/TX, JSON presets, CLI + tkinter GUI.

GUI owns the hardware COM port. A localhost COM API (TCP 17890) lets CLI
drive the same session while the UI shows TX/RX.

Inspired by SerialRUN (CLI+GUI agent) and MegaSerial (preset shortcuts).
"""
from __future__ import print_function

import argparse
import json
import os
import socket
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.stderr.write("need pyserial: python -m pip install pyserial\n")
    sys.exit(2)

if getattr(sys, "frozen", False):
    HERE = os.path.dirname(os.path.abspath(sys.executable))
    BUNDLE = getattr(sys, "_MEIPASS", HERE)
else:
    HERE = os.path.dirname(os.path.abspath(__file__))
    BUNDLE = HERE
STATE_PATH = os.path.join(HERE, "state.json")
DEFAULT_CONFIGS = os.path.join(BUNDLE, "configs")
if not os.path.isdir(DEFAULT_CONFIGS):
    DEFAULT_CONFIGS = os.path.join(HERE, "configs")

EOL = {
    "crlf": b"\r\n",
    "lf": b"\n",
    "cr": b"\r",
    "none": b"",
}

COM_HOST = "127.0.0.1"
COM_PORT = 17890


def eprint(*a):
    sys.stderr.write(" ".join(str(x) for x in a) + "\n")


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path, obj):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, indent=2, ensure_ascii=False)
        f.write("\n")


def load_state():
    if os.path.isfile(STATE_PATH):
        st = load_json(STATE_PATH)
    else:
        st = {"imports": []}
    paths = list(st.get("imports") or [])
    if not paths and os.path.isdir(DEFAULT_CONFIGS):
        for name in sorted(os.listdir(DEFAULT_CONFIGS)):
            if name.endswith(".json"):
                paths.append(os.path.join(DEFAULT_CONFIGS, name))
        st["imports"] = paths
        save_json(STATE_PATH, st)
    return st


def save_state(st):
    save_json(STATE_PATH, st)


def validate_cfg(obj, path):
    if not isinstance(obj, dict) or not obj.get("id") or not obj.get("name"):
        raise ValueError("config needs id and name: %s" % path)
    cmds = obj.get("commands")
    if not isinstance(cmds, list):
        raise ValueError("config commands must be a list: %s" % path)
    kind = obj.get("type") or "preset"
    for c in cmds:
        if not c.get("id"):
            raise ValueError("each command needs id: %s" % path)
        if kind == "flash":
            if "tx" not in c and "flash" not in c:
                raise ValueError("flash command needs tx or flash: %s" % path)
        elif "tx" not in c:
            raise ValueError("each command needs id and tx: %s" % path)
    obj.setdefault("baud", 115200)
    obj.setdefault("eol", "crlf")
    obj.setdefault("type", kind)
    obj["_path"] = os.path.abspath(path)
    return obj


def load_configs():
    st = load_state()
    out = []
    seen = set()
    for p in st.get("imports") or []:
        p = os.path.abspath(p)
        if not os.path.isfile(p):
            eprint("missing config", p)
            continue
        try:
            cfg = validate_cfg(load_json(p), p)
        except Exception as ex:
            eprint("bad config", p, ex)
            continue
        if cfg["id"] in seen:
            eprint("duplicate config id", cfg["id"], "skip", p)
            continue
        seen.add(cfg["id"])
        out.append(cfg)
    return out


def find_cfg(cid):
    for c in load_configs():
        if c["id"] == cid:
            return c
    raise KeyError("unknown config id: %s" % cid)


def find_cmd(cfg, cmd_id):
    for c in cfg["commands"]:
        if c["id"] == cmd_id:
            return c
    raise KeyError("unknown command %s in %s" % (cmd_id, cfg["id"]))


def ports_info():
    rows = []
    for p in list_ports.comports():
        rows.append({"device": p.device, "desc": p.description or ""})
    return rows


class SerialBus(object):
    def __init__(self):
        self.lock = threading.Lock()
        self.ser = None
        self.listeners = []
        self._cap = []
        self._cap_on = False
        self._pump_stop = threading.Event()
        self._pump = None
        self.iap_busy = threading.Event()

    def add_listener(self, fn):
        self.listeners.append(fn)

    def open(self, port, baud=115200, dtr=False):
        self.close()
        ser = serial.Serial()
        ser.port = port
        ser.baudrate = int(baud)
        ser.timeout = 0.05
        ser.write_timeout = 1
        ser.dtr = bool(dtr)
        ser.rts = False
        ser.open()
        self.ser = ser
        self._pump_stop.clear()
        self._pump = threading.Thread(target=self._pump_loop, daemon=True)
        self._pump.start()
        return {"ok": True, "port": port, "baud": int(baud)}

    def close(self):
        self._pump_stop.set()
        with self.lock:
            if self.ser is not None:
                try:
                    self.ser.close()
                except Exception:
                    pass
                self.ser = None

    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def info(self):
        if not self.is_open():
            return {"ok": True, "open": False}
        return {
            "ok": True,
            "open": True,
            "port": self.ser.port,
            "baud": int(self.ser.baudrate),
        }

    def _dispatch(self, data):
        if not data:
            return
        if self._cap_on:
            self._cap.append(data)
        text = data.decode("ascii", "replace")
        for fn in list(self.listeners):
            try:
                fn(text)
            except Exception:
                pass

    def _pump_loop(self):
        while not self._pump_stop.is_set():
            try:
                if not self.is_open() or self.iap_busy.is_set():
                    time.sleep(0.02)
                    continue
                with self.lock:
                    n = self.ser.in_waiting
                    data = self.ser.read(n if n else 1)
                if data:
                    self._dispatch(data)
                else:
                    time.sleep(0.02)
            except Exception as ex:
                self._dispatch(("\n[err] %s\n" % ex).encode("ascii", "replace"))
                time.sleep(0.2)

    def write_line(self, text, eol="crlf"):
        if not self.is_open():
            raise IOError("port not open")
        payload = text.encode("ascii", "replace") + EOL.get(eol, b"\r\n")
        with self.lock:
            self.ser.write(payload)
            self.ser.flush()
        note = ">> %s\n" % text
        for fn in list(self.listeners):
            try:
                fn(note)
            except Exception:
                pass
        return {"ok": True, "tx": text, "bytes": len(payload)}

    def read_ms(self, ms):
        if not self.is_open():
            raise IOError("port not open")
        self._cap = []
        self._cap_on = True
        try:
            time.sleep(ms / 1000.0)
            raw = b"".join(self._cap)
        finally:
            self._cap_on = False
            self._cap = []
        return {
            "ok": True,
            "n": len(raw),
            "text": raw.decode("ascii", "replace"),
            "hex": raw.hex() if hasattr(raw, "hex") else "",
        }

    def send_read(self, text, eol="crlf", read_ms=400):
        self._cap = []
        self._cap_on = True
        try:
            tx = self.write_line(text, eol)
            time.sleep(read_ms / 1000.0)
            raw = b"".join(self._cap)
        finally:
            self._cap_on = False
            self._cap = []
        out = dict(tx)
        out["rx"] = raw.decode("ascii", "replace")
        out["rx_n"] = len(raw)
        out["via"] = "serial"
        return out


BUS = SerialBus()

IAP_SOH = 0xA5
IAP_ACK = 0x5A
IAP_PING = 0x00
IAP_ERASE = 0x01
IAP_WRITE = 0x02
IAP_CRC = 0x03
IAP_GO = 0x04
IAP_REBOOT = 0x06
IAP_VER = 0x07
APP_OK_MAGIC = 0x41505031
APP_MAGIC_ADDR = 0x08010000 - 4


def crc16_ccitt(data, c=0xFFFF):
    for b in bytearray(data):
        c ^= b << 8
        for _ in range(8):
            if c & 0x8000:
                c = ((c << 1) & 0xFFFF) ^ 0x1021
            else:
                c = (c << 1) & 0xFFFF
    return c


def iap_frame(cmd, payload=b""):
    n = len(payload)
    hdr = bytes([cmd, n & 0xFF, (n >> 8) & 0xFF])
    crc = crc16_ccitt(hdr + payload)
    return bytes([IAP_SOH]) + hdr + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def parse_u32(s, default=0):
    if s is None or s == "":
        return default
    if isinstance(s, int):
        return s
    return int(str(s), 0)


def repo_root():
    if os.path.basename(HERE).lower() == "dist":
        return os.path.abspath(os.path.join(HERE, "..", ".."))
    return os.path.abspath(os.path.join(HERE, ".."))


def resolve_firmware(cfg):
    image = cfg.get("image") or ""
    if not image:
        raise IOError("flash config has no image")
    cands = []
    if os.path.isabs(image):
        cands.append(image)
    else:
        cands.append(os.path.normpath(os.path.join(os.path.dirname(cfg["_path"]), image)))
    base = os.path.basename(image)
    cands.append(os.path.normpath(os.path.join(repo_root(), "out", base)))
    cands.append(os.path.normpath(os.path.join(HERE, "..", "out", base)))
    seen = set()
    for p in cands:
        p = os.path.abspath(p)
        if p in seen:
            continue
        seen.add(p)
        if os.path.isfile(p):
            return p
    raise IOError("firmware not found: %s (tried %s)" % (image, cands))


FLASH_LOCK = threading.Lock()


def load_firmware(path):
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        raise IOError("firmware not found: %s" % path)
    if path.lower().endswith(".hex"):
        buf = {}
        with open(path, "r") as f:
            base = 0
            for line in f:
                line = line.strip()
                if not line.startswith(":"):
                    continue
                n = int(line[1:3], 16)
                addr = int(line[3:7], 16)
                typ = int(line[7:9], 16)
                data = bytearray.fromhex(line[9:9 + n * 2])
                if typ == 0:
                    a = base + addr
                    for i, b in enumerate(data):
                        buf[a + i] = b
                elif typ == 4:
                    base = int(line[9:13], 16) << 16
                elif typ == 1:
                    break
        if not buf:
            raise ValueError("empty hex: %s" % path)
        lo = min(buf)
        hi = max(buf)
        out = bytearray(hi - lo + 1)
        for a, b in buf.items():
            out[a - lo] = b
        return bytes(out), lo
    data = open(path, "rb").read()
    return data, None


def iap_xfer(cmd, payload=b"", timeout=3.0):
    if not BUS.is_open():
        raise IOError("port not open")
    frame = iap_frame(cmd, payload)
    BUS.iap_busy.set()
    with BUS.lock:
        try:
            BUS.ser.reset_input_buffer()
        except Exception:
            pass
        BUS.ser.write(frame)
        BUS.ser.flush()
        BUS.ser.timeout = timeout
        ack = BUS.ser.read(4)
    if len(ack) < 4 or ack[0] != IAP_ACK or ack[1] != cmd:
        raise IOError("bad IAP ack: %r" % ack)
    if ack[2] != 0:
        raise IOError("IAP status %d cmd %d" % (ack[2], cmd))
    return ack


def iap_ping(tries=25):
    last = None
    for _ in range(tries):
        try:
            iap_xfer(IAP_PING, b"", timeout=0.15)
            return True
        except Exception as ex:
            last = ex
            time.sleep(0.05)
    raise IOError("boot not answering ping: %s" % last)


def iap_read_boot_ver():
    if not BUS.is_open():
        raise IOError("port not open")
    frame = iap_frame(IAP_VER, b"")
    BUS.iap_busy.set()
    with BUS.lock:
        try:
            BUS.ser.reset_input_buffer()
        except Exception:
            pass
        BUS.ser.write(frame)
        BUS.ser.flush()
        BUS.ser.timeout = 1.0
        hdr = BUS.ser.read(4)
        if len(hdr) < 4 or hdr[0] != IAP_ACK or hdr[1] != IAP_VER or hdr[2] != 0:
            return ""
        n = hdr[3]
        body = BUS.ser.read(n) if n else b""
        return body.decode("ascii", "replace").strip()


def text_ver(cmd="ver", read_ms=500):
    r = BUS.send_read(cmd, "crlf", read_ms)
    raw = (r.get("rx") or "")
    keep = []
    for ln in raw.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        s = ln.strip()
        if not s:
            continue
        if s.startswith("hb ") or s.startswith("FOC ") or s.startswith("uart "):
            continue
        keep.append(s)
    return "\n".join(keep)


def flash_app(cfg, logfn=None):
    if not FLASH_LOCK.acquire(False):
        raise IOError("IAP already running")
    try:
        return _flash_app_body(cfg, logfn)
    finally:
        FLASH_LOCK.release()


def _flash_app_body(cfg, logfn=None):
    def log(s):
        if logfn:
            logfn(s)
        for fn in list(BUS.listeners):
            try:
                fn(s if s.endswith("\n") else s + "\n")
            except Exception:
                pass

    image = resolve_firmware(cfg)
    data, hex_base = load_firmware(image)
    if len(data) & 1:
        data = data + b"\x00"
    app_base = parse_u32(cfg.get("app_base"), 0x08002000)
    if hex_base is not None:
        app_base = hex_base
    abort_after = int(cfg.get("abort_after") or 0)

    try:
        before_boot_uart = text_ver("ver boot")
        before_app = text_ver("ver app")
        log("[iap] before uart boot=%s" % (before_boot_uart or "(none)"))
        log("[iap] before uart app=%s" % (before_app or "(none)"))
    except Exception as ex:
        log("[iap] before uart skip: %s" % ex)
        before_boot_uart = ""
        before_app = ""

    enter = cfg.get("enter_tx")
    if enter:
        try:
            BUS.write_line(enter, cfg.get("eol") or "crlf")
            log("[iap] sent %s, waiting boot" % enter)
            time.sleep(float(cfg.get("enter_wait_s") or 0.4))
        except Exception as ex:
            log("[iap] enter skipped: %s" % ex)
    BUS.iap_busy.set()
    try:
        out = _flash_app_iap(cfg, log, data, app_base, abort_after, image,
                             before_boot_uart, before_app)
    finally:
        BUS.iap_busy.clear()
    if out.get("aborted") or not out.get("ok"):
        return out
    time.sleep(2.0)
    try:
        BUS.write_line("hb off", "crlf")
        time.sleep(0.35)
    except Exception:
        pass
    try:
        after_boot = text_ver("ver boot", 800)
        after_app = text_ver("ver app", 800)
        log("[iap] after boot=%s" % (after_boot or "(none)"))
        log("[iap] after app=%s" % (after_app or "(none)"))
        out["ver_after_boot"] = after_boot
        out["ver_after_app"] = after_app
    except Exception as ex:
        log("[iap] after ver skip: %s" % ex)
    return out


def _flash_app_iap(cfg, log, data, app_base, abort_after, image,
                   before_boot_uart, before_app):
    iap_ping()
    try:
        before_boot = iap_read_boot_ver()
        log("[iap] before boot=%s" % (before_boot or "(none)"))
    except Exception as ex:
        log("[iap] before boot skip: %s" % ex)
        before_boot = ""
    log("[iap] ping ok, erase app @ 0x%08X (%d bytes)" % (app_base, len(data)))
    iap_xfer(IAP_ERASE, b"", timeout=8.0)
    chunk = int(cfg.get("chunk") or 128)
    off = 0
    while off < len(data):
        if abort_after and off >= abort_after:
            log("[iap] UART stop at %d/%d (no tail seal)" % (off, len(data)))
            try:
                iap_xfer(IAP_REBOOT, b"", timeout=1.0)
            except Exception:
                pass
            time.sleep(0.4)
            return {
                "ok": True,
                "aborted": True,
                "bytes": off,
                "base": "0x%08X" % app_base,
                "file": image,
                "via": "iap",
            }
        part = data[off:off + chunk]
        if len(part) & 1:
            part = part + b"\x00"
        addr = app_base + off
        pl = bytes([
            addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF
        ]) + part
        iap_xfer(IAP_WRITE, pl, timeout=2.0)
        off += len(part)
        if (off // chunk) % 16 == 0:
            log("[iap] %d/%d" % (off, len(data)))
    crc = crc16_ccitt(data)
    cpl = bytes([
        app_base & 0xFF, (app_base >> 8) & 0xFF, (app_base >> 16) & 0xFF, (app_base >> 24) & 0xFF,
        len(data) & 0xFF, (len(data) >> 8) & 0xFF, (len(data) >> 16) & 0xFF, (len(data) >> 24) & 0xFF,
    ])
    # CRC reply is 6 bytes: 5A 03 00 02 crc_lo crc_hi — iap_xfer only reads 4.
    frame = iap_frame(IAP_CRC, cpl)
    with BUS.lock:
        BUS.ser.reset_input_buffer()
        BUS.ser.write(frame)
        BUS.ser.flush()
        BUS.ser.timeout = 2
        ack = BUS.ser.read(6)
    if len(ack) < 4 or ack[0] != IAP_ACK or ack[2] != 0:
        log("[iap] crc skip/fail %r" % ack)
    else:
        got = ack[4] | (ack[5] << 8) if len(ack) >= 6 else -1
        log("[iap] crc host=%04X mcu=%04X" % (crc, got))
        if got != crc and got != -1:
            raise IOError("CRC mismatch")
    mag_pl = bytes([
        APP_MAGIC_ADDR & 0xFF, (APP_MAGIC_ADDR >> 8) & 0xFF,
        (APP_MAGIC_ADDR >> 16) & 0xFF, (APP_MAGIC_ADDR >> 24) & 0xFF,
        APP_OK_MAGIC & 0xFF, (APP_OK_MAGIC >> 8) & 0xFF,
        (APP_OK_MAGIC >> 16) & 0xFF, (APP_OK_MAGIC >> 24) & 0xFF,
    ])
    iap_xfer(IAP_WRITE, mag_pl, timeout=2.0)
    log("[iap] tail seal APP1 @ 0x%08X" % APP_MAGIC_ADDR)
    iap_xfer(IAP_GO, b"", timeout=1.0)
    log("[iap] go app")
    return {
        "ok": True,
        "bytes": len(data),
        "base": "0x%08X" % app_base,
        "file": image,
        "via": "iap",
        "ver_before_uart_boot": before_boot_uart,
        "ver_before_uart_app": before_app,
        "ver_before_boot": before_boot,
    }


def import_path(path):
    path = os.path.abspath(path)
    validate_cfg(load_json(path), path)
    st = load_state()
    im = [os.path.abspath(x) for x in (st.get("imports") or [])]
    if path not in im:
        im.append(path)
    st["imports"] = im
    save_state(st)
    return {"ok": True, "imported": path, "count": len(im)}


def configs_rows():
    rows = []
    for c in load_configs():
        rows.append({
            "id": c["id"],
            "name": c["name"],
            "path": c["_path"],
            "commands": [x["id"] for x in c["commands"]],
        })
    return rows


def api_dispatch(req, ensure_port=None, baud=115200, dtr=False):
    op = (req.get("op") or "").lower()
    if op in ("ports",):
        return {"ok": True, "ports": ports_info()}
    if op == "configs":
        return {"ok": True, "configs": configs_rows()}
    if op == "import":
        return import_path(req["path"])
    if op in ("unimport", "drop", "remove"):
        return remove_import(req["path"])
    if op == "status":
        inf = BUS.info()
        inf["com"] = "%s:%d" % (COM_HOST, COM_PORT)
        return inf
    if op == "open":
        port = req.get("port") or ensure_port
        if not port:
            raise ValueError("open needs port")
        return BUS.open(port, int(req.get("baud") or baud), bool(req.get("dtr") or dtr))
    if op == "close":
        BUS.close()
        return {"ok": True, "open": False}
    if op == "flash":
        cfg = find_cfg(req["config"])
        if (cfg.get("type") or "preset") != "flash":
            raise ValueError("config is not type=flash")
        if not BUS.is_open():
            port = req.get("port") or ensure_port
            if not port:
                raise IOError("port not open")
            BUS.open(port, int(req.get("baud") or cfg.get("baud") or baud), bool(req.get("dtr") or dtr))
        if req.get("abort_after"):
            cfg = dict(cfg)
            cfg["abort_after"] = int(req["abort_after"])
        return flash_app(cfg)
    if op in ("send", "read", "preset"):
        if not BUS.is_open():
            port = req.get("port") or ensure_port
            if not port:
                raise IOError("port not open; Connect in GUI or pass COM port")
            BUS.open(port, int(req.get("baud") or baud), bool(req.get("dtr") or dtr))
        if op == "read":
            r = BUS.read_ms(int(req.get("read_ms") or 400))
            r["via"] = "com"
            return r
        if op == "preset":
            cfg = find_cfg(req["config"])
            cmd = find_cmd(cfg, req["command"])
            eol = cfg.get("eol") or "crlf"
            r = BUS.send_read(cmd["tx"], eol, int(req.get("read_ms") or 400))
            r["config"] = cfg["id"]
            r["command"] = cmd["id"]
            r["via"] = "com"
            return r
        r = BUS.send_read(
            req.get("text") or "",
            req.get("eol") or "crlf",
            int(req.get("read_ms") or 400),
        )
        r["via"] = "com"
        return r
    raise ValueError("unknown op: %s" % op)


def parse_com_line(line):
    line = line.strip()
    if not line:
        return None
    if line[0] == "{":
        return json.loads(line)
    parts = line.split(None, 2)
    head = parts[0].lower()
    if head in ("send", "tx") and len(parts) >= 2:
        return {"op": "send", "text": line.split(None, 1)[1], "read_ms": 400}
    if head == "read":
        ms = int(parts[1]) if len(parts) > 1 else 400
        return {"op": "read", "read_ms": ms}
    if head == "preset" and len(parts) >= 3:
        return {"op": "preset", "config": parts[1], "command": parts[2], "read_ms": 400}
    if head in ("status", "ports", "configs", "open", "close"):
        req = {"op": head}
        if head == "open" and len(parts) > 1:
            req["port"] = parts[1]
        return req
    return {"op": "send", "text": line, "read_ms": 400}


class ComServer(object):
    def __init__(self, host=COM_HOST, port=COM_PORT):
        self.host = host
        self.port = int(port)
        self.stop = threading.Event()
        self.sock = None
        self.thread = None

    def start(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((self.host, self.port))
        s.listen(8)
        s.settimeout(0.5)
        self.sock = s
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()
        return "%s:%d" % (self.host, self.port)

    def _loop(self):
        while not self.stop.is_set():
            try:
                conn, _addr = self.sock.accept()
            except socket.timeout:
                continue
            except Exception:
                if self.stop.is_set():
                    break
                time.sleep(0.1)
                continue
            threading.Thread(target=self._client, args=(conn,), daemon=True).start()

    def _client(self, conn):
        conn.settimeout(8)
        buf = b""
        try:
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode("utf-8", "replace").strip("\r")
                    if not line:
                        continue
                    try:
                        req = parse_com_line(line)
                        if req is None:
                            continue
                        out = api_dispatch(req)
                    except Exception as ex:
                        out = {"ok": False, "error": str(ex)}
                    conn.sendall((json.dumps(out, ensure_ascii=False) + "\n").encode("utf-8"))
        except Exception:
            pass
        try:
            conn.close()
        except Exception:
            pass

    def close(self):
        self.stop.set()
        if self.sock is not None:
            try:
                self.sock.close()
            except Exception:
                pass


def com_rpc(req, host=COM_HOST, port=COM_PORT, timeout=6):
    s = socket.create_connection((host, int(port)), timeout=0.4)
    try:
        s.settimeout(timeout)
        s.sendall((json.dumps(req, ensure_ascii=False) + "\n").encode("utf-8"))
        data = b""
        while b"\n" not in data:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
        if not data:
            raise IOError("empty COM API reply")
        return json.loads(data.split(b"\n", 1)[0].decode("utf-8"))
    finally:
        s.close()


def com_try(req, host=None, port=None, timeout=6):
    host = host or COM_HOST
    port = COM_PORT if port is None else int(port)
    try:
        return com_rpc(req, host, port, timeout)
    except (socket.error, OSError, ValueError):
        return None


def cmd_ports(args):
    r = None if args.direct else com_try({"op": "ports"})
    print(json.dumps((r or {}).get("ports") or ports_info(), indent=2, ensure_ascii=False))
    return 0


def cmd_import(args):
    r = None if args.direct else com_try({"op": "import", "path": args.path})
    if r is None:
        r = import_path(args.path)
    print(json.dumps(r, ensure_ascii=False))
    return 0 if r.get("ok") else 1


def cmd_unimport(args):
    r = None if args.direct else com_try({"op": "unimport", "path": args.path})
    if r is None:
        r = remove_import(args.path)
    print(json.dumps(r, ensure_ascii=False))
    return 0 if r.get("ok") else 1


def remove_import(path):
    path = os.path.abspath(path)
    st = load_state()
    im = [os.path.abspath(x) for x in (st.get("imports") or [])]
    st["imports"] = [x for x in im if x != path]
    save_state(st)
    return {"ok": True, "removed": path, "count": len(st["imports"])}


def cmd_configs(args):
    r = None if args.direct else com_try({"op": "configs"})
    rows = (r or {}).get("configs") if r else None
    print(json.dumps(rows or configs_rows(), indent=2, ensure_ascii=False))
    return 0


def cmd_status(args):
    r = None if args.direct else com_try({"op": "status"})
    if r is None:
        r = dict(BUS.info())
        r["com"] = "offline"
    print(json.dumps(r, indent=2, ensure_ascii=False))
    return 0


def via_or_local(args, req):
    timeout = 120 if (req.get("op") or "").lower() == "flash" else 6
    if not args.direct:
        try:
            r = com_rpc(req, args.com_host, args.com_port, timeout=timeout)
            print(json.dumps(r, ensure_ascii=False, indent=2))
            return 0 if r.get("ok") else 1
        except (socket.error, OSError, ValueError):
            st = com_try({"op": "status"}, args.com_host, args.com_port, timeout=1)
            if st is not None:
                err = {
                    "ok": False,
                    "error": "GUI COM API is up; IAP still running or timed out. Do not open COM3 again.",
                    "via": "com",
                }
                print(json.dumps(err, ensure_ascii=False, indent=2))
                return 1
    if not BUS.is_open():
        BUS.open(args.port, args.baud, args.dtr)
        close_after = True
    else:
        close_after = False
    try:
        out = api_dispatch(req, ensure_port=args.port, baud=args.baud, dtr=args.dtr)
        out["via"] = out.get("via") or "serial"
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 0 if out.get("ok") else 1
    finally:
        if close_after:
            BUS.close()


def cmd_send(args):
    return via_or_local(args, {
        "op": "send",
        "port": args.port,
        "text": args.text,
        "eol": args.eol,
        "read_ms": args.read_ms,
        "baud": args.baud,
        "dtr": args.dtr,
    })


def cmd_read(args):
    return via_or_local(args, {
        "op": "read",
        "port": args.port,
        "read_ms": args.read_ms,
        "baud": args.baud,
        "dtr": args.dtr,
    })


def cmd_flash(args):
    cfg = find_cfg(args.config)
    return via_or_local(args, {
        "op": "flash",
        "port": args.port,
        "config": args.config,
        "baud": int(getattr(args, "baud", None) or cfg.get("baud") or 115200),
        "dtr": args.dtr,
        "abort_after": int(getattr(args, "abort_after", 0) or 0),
    })


def cmd_preset(args):
    cfg = find_cfg(args.config)
    return via_or_local(args, {
        "op": "preset",
        "port": args.port,
        "config": args.config,
        "command": args.command,
        "read_ms": args.read_ms,
        "baud": int(getattr(args, "baud", None) or cfg.get("baud") or 115200),
        "dtr": args.dtr,
    })


def run_gui(args):
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    root = tk.Tk()
    root.title("MCU Host")
    root.geometry("1000x640")
    root.minsize(720, 420)

    top = ttk.Frame(root)
    top.grid(row=0, column=0, sticky="ew", padx=6, pady=4)
    mid = ttk.Panedwindow(root, orient=tk.HORIZONTAL)
    mid.grid(row=1, column=0, sticky="nsew")
    root.grid_rowconfigure(1, weight=1)
    root.grid_columnconfigure(0, weight=1)

    left = ttk.Frame(mid)
    right = ttk.Frame(mid)
    mid.add(left, weight=4)
    mid.add(right, weight=1)

    log = tk.Text(left, wrap="none")
    ys = ttk.Scrollbar(left, orient="vertical", command=log.yview)
    xs = ttk.Scrollbar(left, orient="horizontal", command=log.xview)
    log.configure(yscrollcommand=ys.set, xscrollcommand=xs.set)
    log.grid(row=0, column=0, sticky="nsew")
    ys.grid(row=0, column=1, sticky="ns")
    xs.grid(row=1, column=0, sticky="ew")
    left.grid_rowconfigure(0, weight=1)
    left.grid_columnconfigure(0, weight=1)

    canvas = tk.Canvas(right, highlightthickness=0, width=240)
    rsb = ttk.Scrollbar(right, orient="vertical", command=canvas.yview)
    inner = ttk.Frame(canvas)
    inner.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
    win = canvas.create_window((0, 0), window=inner, anchor="nw")
    canvas.configure(yscrollcommand=rsb.set)

    def _sync_inner(e):
        canvas.itemconfigure(win, width=max(e.width, 200))

    canvas.bind("<Configure>", _sync_inner)
    canvas.grid(row=0, column=0, sticky="nsew")
    rsb.grid(row=0, column=1, sticky="ns")
    right.grid_rowconfigure(0, weight=1)
    right.grid_columnconfigure(0, weight=1)

    def _wheel(e):
        canvas.yview_scroll(int(-1 * (e.delta / 120)), "units")

    canvas.bind("<Enter>", lambda e: canvas.bind_all("<MouseWheel>", _wheel))
    canvas.bind("<Leave>", lambda e: canvas.unbind_all("<MouseWheel>"))
    inner.bind("<Enter>", lambda e: canvas.bind_all("<MouseWheel>", _wheel))

    port_var = tk.StringVar()
    baud_var = tk.StringVar(value="115200")
    ports = [p["device"] for p in ports_info()] or ["COM3"]
    ttk.Label(top, text="Port").pack(side="left")
    port_box = ttk.Combobox(top, textvariable=port_var, values=ports, width=12)
    port_box.pack(side="left", padx=4)
    if "COM3" in ports:
        port_var.set("COM3")
    elif ports:
        port_var.set(ports[0])
    ttk.Label(top, text="Baud").pack(side="left")
    ttk.Entry(top, textvariable=baud_var, width=8).pack(side="left", padx=4)
    st_var = tk.StringVar(value="closed")
    ttk.Label(top, textvariable=st_var).pack(side="left", padx=8)

    bot = ttk.Frame(root)
    bot.grid(row=2, column=0, sticky="ew", padx=6, pady=4)
    send_var = tk.StringVar()
    ttk.Entry(bot, textvariable=send_var).pack(side="left", fill="x", expand=True)

    rx_q = []
    rx_lock = threading.Lock()

    def append(s):
        log.insert("end", s)
        log.see("end")

    def on_rx(text):
        with rx_lock:
            rx_q.append(text)

    BUS.add_listener(on_rx)

    def drain():
        with rx_lock:
            chunk = "".join(rx_q)
            del rx_q[:]
        if chunk:
            append(chunk)
        root.after(80, drain)

    def refresh_st():
        inf = BUS.info()
        if inf.get("open"):
            st_var.set("open %s | com %s:%d" % (inf.get("port"), COM_HOST, COM_PORT))
        else:
            st_var.set("closed | com %s:%d" % (COM_HOST, COM_PORT))
        root.after(500, refresh_st)

    def connect():
        try:
            BUS.open(port_var.get(), int(baud_var.get()), False)
            refresh_st()
            append("\n[open %s]\n" % port_var.get())
        except Exception as ex:
            messagebox.showerror("open", str(ex))

    def disconnect():
        BUS.close()
        refresh_st()
        append("\n[close]\n")

    def do_send(text):
        if not text:
            return
        try:
            if not BUS.is_open():
                connect()
            BUS.write_line(text, "crlf")
        except Exception as ex:
            messagebox.showerror("send", str(ex))

    def load_presets():
        try:
            _load_presets_inner()
        except Exception as ex:
            append("\n[config] %s\n" % ex)

    def _load_presets_inner():
        for w in inner.winfo_children():
            w.destroy()
        ttk.Button(inner, text="Import JSON", command=do_import).pack(fill="x", pady=2)
        for cfg in load_configs():
            box = ttk.LabelFrame(inner, text=cfg["name"] + " (" + cfg["id"] + ")")
            box.pack(fill="x", pady=4)
            ttk.Button(
                box, text="关闭此配置",
                command=lambda p=cfg["_path"]: do_unimport(p),
            ).pack(fill="x", padx=2, pady=1)
            for c in cfg["commands"]:
                lab = c.get("label") or c["id"]
                if c.get("flash"):
                    ttk.Button(
                        box, text=lab,
                        command=lambda cf=cfg: do_flash(cf),
                    ).pack(fill="x", padx=2, pady=1)
                else:
                    ttk.Button(
                        box, text=lab,
                        command=lambda t=c["tx"]: do_send(t),
                    ).pack(fill="x", padx=2, pady=1)
        canvas.configure(scrollregion=canvas.bbox("all"))

    def do_unimport(path):
        try:
            remove_import(path)
            load_presets()
            append("\n[drop %s]\n" % path)
        except Exception as ex:
            messagebox.showerror("close json", str(ex))

    def do_flash(cfg):
        if not BUS.is_open():
            connect()
        def go():
            try:
                flash_app(cfg)
            except Exception as ex:
                root.after(0, lambda: messagebox.showerror("flash", str(ex)))
        threading.Thread(target=go, daemon=True).start()

    def do_import():
        path = filedialog.askopenfilename(filetypes=[("JSON", "*.json")])
        if not path:
            return
        try:
            import_path(path)
            load_presets()
        except Exception as ex:
            messagebox.showerror("import", str(ex))

    ttk.Button(top, text="Connect", command=connect).pack(side="left", padx=4)
    ttk.Button(top, text="Disconnect", command=disconnect).pack(side="left")
    ttk.Button(bot, text="Send", command=lambda: do_send(send_var.get())).pack(side="left", padx=4)
    ttk.Button(bot, text="Clear", command=lambda: log.delete("1.0", "end")).pack(side="left")
    root.bind("<Return>", lambda e: do_send(send_var.get()))
    load_presets()
    root.after(80, drain)

    srv = ComServer(args.com_host, args.com_port)
    try:
        addr = srv.start()
        append("[com api %s  CLI uses this while GUI shows RX]\n" % addr)
        refresh_st()
    except Exception as ex:
        append("[com api failed: %s]\n" % ex)

    def on_close():
        srv.close()
        BUS.close()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()
    srv.close()
    BUS.close()
    return 0


def add_com_flags(p):
    p.add_argument("--direct", action="store_true", help="open hardware COM, skip GUI COM API")
    p.add_argument("--com-host", default=COM_HOST)
    p.add_argument("--com-port", type=int, default=COM_PORT)


def build_parser():
    p = argparse.ArgumentParser(
        prog="mcu_host",
        description="Portable MCU serial host (CLI + GUI + COM API). See host/API.md.",
    )
    com = argparse.ArgumentParser(add_help=False)
    add_com_flags(com)
    ser = argparse.ArgumentParser(add_help=False)
    add_com_flags(ser)
    ser.add_argument("port")
    ser.add_argument("--baud", type=int, default=115200)
    ser.add_argument("--dtr", action="store_true")
    ser.add_argument("--eol", default="crlf", choices=list(EOL.keys()))
    ser.add_argument("--read-ms", dest="read_ms", type=int, default=400)

    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports", parents=[com], help="list COM ports (JSON)")
    sub.add_parser("configs", parents=[com], help="list imported preset files")
    sub.add_parser("status", parents=[com], help="GUI COM API / serial status")
    im = sub.add_parser("import", parents=[com], help="import a JSON preset pack")
    im.add_argument("path")
    um = sub.add_parser("unimport", parents=[com], help="remove an imported JSON pack")
    um.add_argument("path")

    s = sub.add_parser("send", parents=[ser], help="send one line and read reply")
    s.add_argument("text")
    sub.add_parser("read", parents=[ser], help="read port for N ms")
    pr = sub.add_parser("preset", parents=[ser], help="send a named command from a config")
    pr.add_argument("config")
    pr.add_argument("command")
    fl = sub.add_parser("flash", parents=[ser], help="serial IAP using a type=flash JSON")
    fl.add_argument("config")
    fl.add_argument("--abort-after", dest="abort_after", type=int, default=0,
                    help="stop UART IAP after N bytes (no tail seal; for abort test)")
    sub.add_parser("gui", parents=[com], help="open the desktop UI + COM API")
    return p


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    if not argv:
        argv = ["gui"]
    p = build_parser()
    args = p.parse_args(argv)
    if not hasattr(args, "direct"):
        args.direct = False
    if not hasattr(args, "com_host"):
        args.com_host = COM_HOST
        args.com_port = COM_PORT
    fn = {
        "ports": cmd_ports,
        "configs": cmd_configs,
        "import": cmd_import,
        "unimport": cmd_unimport,
        "send": cmd_send,
        "read": cmd_read,
        "preset": cmd_preset,
        "flash": cmd_flash,
        "gui": run_gui,
        "status": cmd_status,
    }[args.cmd]
    return fn(args)


if __name__ == "__main__":
    sys.exit(main())

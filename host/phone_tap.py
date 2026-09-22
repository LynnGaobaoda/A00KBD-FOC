#!/usr/bin/env python3
"""LAN one-tap for iPhone Shortcuts -> mcu_host CLI.

PC (same Wi-Fi):
  1. python mcu_host.py gui   then Connect COM3
  2. set FOC_TAP_TOKEN=your-secret
  3. python phone_tap.py --port COM3

iPhone 快捷指令:
  导入 host/ios/FOC-motor.shortcut（或打开 http://<PC>:17891/shortcut）
  先改指令里的「电脑IP」和「Token」，每次运行弹出全部功能。

Only allowlisted actions run. No flash / iap.
"""
from __future__ import print_function

import argparse
import json
import os
import subprocess
import sys

try:
    from urllib.parse import parse_qs, urlparse
except ImportError:
    from urlparse import parse_qs, urlparse

if sys.version_info[0] >= 3:
    from http.server import BaseHTTPRequestHandler, HTTPServer
else:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
HOST_PY = os.path.join(HERE, "mcu_host.py")
MENU = [
    ("顺时针 60°", "cw"),
    ("逆时针 60°", "ccw"),
    ("停止保持", "stop"),
    ("棘轮手感", "r"),
    ("恒速 +3", "spd3"),
    ("恒速 -3", "spd-3"),
    ("恒力 +1100", "force1100"),
    ("恒力 -1100", "force-1100"),
    ("位置保持", "pos"),
    ("位置 90°", "pos90"),
    ("方向 CW", "dircw"),
    ("方向 CCW", "dirccw"),
    ("读状态", "stat"),
    ("帮助", "help"),
]

PHONE_HTML = os.path.join(HERE, "ios", "foc_phone.html")
TOKEN_FILE = os.path.join(HERE, ".tap_token")
SHORTCUT_LOCAL = os.path.join(HERE, "ios", "FOC-motor.local.shortcut")
SHORTCUT_TMPL = os.path.join(HERE, "ios", "FOC-motor.shortcut")


def shortcut_file():
    if os.path.isfile(SHORTCUT_LOCAL):
        return SHORTCUT_LOCAL
    return SHORTCUT_TMPL

# action -> (cli args after python mcu_host.py)
ACTIONS = {
    "cw": ["preset", "foc-motor", "cw"],
    "ccw": ["preset", "foc-motor", "ccw"],
    "stop": ["preset", "foc-motor", "stop"],
    "r": ["preset", "foc-motor", "r"],
    "spd3": ["preset", "foc-motor", "spd3"],
    "spd-3": ["preset", "foc-motor", "spd-3"],
    "force1100": ["preset", "foc-motor", "force1100"],
    "force-1100": ["preset", "foc-motor", "force-1100"],
    "pos": ["preset", "foc-motor", "pos0"],
    "pos90": ["preset", "foc-motor", "pos90"],
    "dircw": ["preset", "foc-motor", "dircw"],
    "dirccw": ["preset", "foc-motor", "dirccw"],
    "stat": ["send", "stat"],
    "help": ["send", "help"],
}

CFG = {"com": "COM3", "token": "", "bind": "0.0.0.0", "http_port": 17891}


def run_cli(action, direct=False):
    spec = ACTIONS[action]
    if spec[0] == "preset":
        cmd = [sys.executable, HOST_PY, "preset", CFG["com"], spec[1], spec[2]]
    else:
        cmd = [sys.executable, HOST_PY, spec[0], CFG["com"], spec[1]]
    cmd += ["--read-ms", "800"]
    if direct:
        cmd.append("--direct")
    p = subprocess.Popen(
        cmd, cwd=HERE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    out, err = p.communicate()
    if sys.version_info[0] >= 3:
        out = out.decode("utf-8", "replace")
        err = err.decode("utf-8", "replace")
    return p.returncode, out, err


def run_cli_best(action):
    code, out, err = run_cli(action, False)
    if code == 0:
        return code, out, err
    return run_cli(action, True)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.client_address[0], fmt % args))

    def _send_bytes(self, code, raw, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _send_html_app(self):
        with open(PHONE_HTML, "rb") as f:
            raw = f.read()
        html = raw.decode("utf-8")
        self._send_bytes(200, html.encode("utf-8"), "text/html; charset=utf-8")

    def _send(self, code, obj):
        raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self._send_bytes(code, raw, "application/json; charset=utf-8")

    def _send_file(self, path, ctype, download_name):
        with open(path, "rb") as f:
            raw = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header(
            "Content-Disposition", "attachment; filename=\"%s\"" % download_name
        )
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        u = urlparse(self.path)
        parts = [p for p in u.path.split("/") if p]
        if not parts or parts[0] in ("app", "index.html"):
            self._send_html_app()
            return
        if parts[0] in ("shortcut", "FOC-motor.shortcut"):
            path = shortcut_file()
            if os.path.isfile(path):
                self._send_file(
                    path,
                    "application/x-shortcut",
                    "FOC-motor.shortcut",
                )
            else:
                self._send(404, {"ok": False, "err": "missing shortcut file"})
            return
        q = parse_qs(u.query)
        token = (q.get("token") or [""])[0]
        if not CFG["token"] or token != CFG["token"]:
            self._send(403, {"ok": False, "err": "bad token"})
            return
        if len(parts) == 1 and parts[0] == "actions":
            self._send(200, {"ok": True, "actions": sorted(ACTIONS.keys())})
            return
        if len(parts) != 2 or parts[0] != "tap":
            self._send(404, {"ok": False, "err": "use /tap/<action>?token="})
            return
        action = parts[1]
        if action not in ACTIONS:
            self._send(400, {"ok": False, "err": "unknown action", "actions": sorted(ACTIONS.keys())})
            return
        code, out, err = run_cli_best(action)
        self._send(200 if code == 0 else 500, {
            "ok": code == 0,
            "action": action,
            "code": code,
            "cli": out.strip(),
            "err": err.strip(),
        })


def main(argv=None):
    p = argparse.ArgumentParser(description="iPhone Shortcut tap -> FOC CLI")
    p.add_argument("--port", default="COM3", help="MCU COM port name")
    p.add_argument("--bind", default="127.0.0.1")
    p.add_argument("--http-port", type=int, default=17891)
    p.add_argument("--token", default=os.environ.get("FOC_TAP_TOKEN", ""))
    args = p.parse_args(argv)
    if not args.token and os.path.isfile(TOKEN_FILE):
        with open(TOKEN_FILE, "r") as f:
            args.token = f.read().strip()
    if not args.token:
        sys.stderr.write("set FOC_TAP_TOKEN or pass --token\n")
        return 2
    CFG["com"] = args.port
    CFG["token"] = args.token
    CFG["bind"] = args.bind
    CFG["http_port"] = args.http_port
    httpd = HTTPServer((args.bind, args.http_port), Handler)
    sys.stderr.write("FOC phone on 127.0.0.1:%d COM=%s\n" % (args.http_port, args.port))
    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())

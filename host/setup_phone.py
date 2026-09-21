#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Prepare LAN phone tap: token, PC IP, iOS shortcut, firewall, optional serve."""
from __future__ import print_function

import argparse
import os
import re
import secrets
import socket
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOKEN_PATH = os.path.join(HERE, ".tap_token")
LOCAL_SHORTCUT = os.path.join(HERE, "ios", "FOC-motor.local.shortcut")
BUILDER = os.path.join(HERE, "ios", "build_foc_shortcut.py")
WAN_URL_PATH = os.path.join(HERE, ".wan_url")
HTTP_PORT = 17891
CF_CANDIDATES = [
    os.path.join("C:\\Program Files (x86)", "cloudflared", "cloudflared.exe"),
    os.path.join("C:\\Program Files", "cloudflared", "cloudflared.exe"),
    os.path.join("D:\\mysorftware", "cloudflared", "cloudflared.exe"),
]


def lan_ip():
    ip = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.3)
        s.connect(("223.5.5.5", 80))
        ip = s.getsockname()[0]
        s.close()
    except Exception:
        ip = None
    if ip and not ip.startswith("127."):
        return ip
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            cand = info[4][0]
            if cand.startswith("192.168.") or cand.startswith("10.") or cand.startswith("172."):
                return cand
    except Exception:
        pass
    return ip or "127.0.0.1"


def load_or_make_token():
    if os.path.isfile(TOKEN_PATH):
        with open(TOKEN_PATH, "r") as f:
            t = f.read().strip()
        if t:
            return t
    t = secrets.token_urlsafe(16)
    with open(TOKEN_PATH, "w") as f:
        f.write(t + "\n")
    return t


def first_com():
    try:
        out = subprocess.check_output(
            [sys.executable, os.path.join(HERE, "mcu_host.py"), "ports", "--direct"],
            cwd=HERE,
            stderr=subprocess.STDOUT,
        )
        text = out.decode("utf-8", "replace")
        if "COM3" in text:
            return "COM3"
        import re
        m = re.search(r"COM\d+", text)
        if m:
            return m.group(0)
    except Exception:
        pass
    return "COM3"


def firewall(port):
    name = "FOC phone_tap %d" % port
    subprocess.call(
        [
            "netsh", "advfirewall", "firewall", "delete", "rule",
            "name=" + name,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    r = subprocess.call(
        [
            "netsh", "advfirewall", "firewall", "add", "rule",
            "name=" + name,
            "dir=in",
            "action=allow",
            "protocol=TCP",
            "localport=%d" % port,
            "profile=private,domain",
        ]
    )
    return r == 0


def build_shortcut(ip, token):
    return subprocess.call(
        [
            sys.executable, BUILDER,
            "--ip", ip,
            "--token", token,
            "--out", LOCAL_SHORTCUT,
        ]
    )


def write_start_bat(com, token):
    path = os.path.join(HERE, "start_phone_tap.bat")
    lines = [
        "@echo off",
        "cd /d \"%~dp0\"",
        "python setup_phone.py --serve --wan",
        "",
    ]
    with open(path, "w") as f:
        f.write("\r\n".join(lines))
    return path


def find_cloudflared():
    for p in CF_CANDIDATES:
        if os.path.isfile(p):
            return p
    return None


def start_wan_tunnel(http_port):
    exe = find_cloudflared()
    if not exe:
        print("wan=missing cloudflared")
        return None, None
    log_path = os.path.join(HERE, ".wan_cloudflared.log")
    with open(log_path, "wb") as f:
        f.write(b"")
    logf = open(log_path, "ab")
    proc = subprocess.Popen(
        [exe, "tunnel", "--url", "http://127.0.0.1:%d" % http_port, "--no-autoupdate"],
        stdout=logf,
        stderr=subprocess.STDOUT,
        cwd=HERE,
    )
    found = []

    def wait_url():
        pat = re.compile(br"https://[a-z0-9-]+\.trycloudflare\.com")
        deadline = time.time() + 30
        while time.time() < deadline:
            try:
                with open(log_path, "rb") as f:
                    blob = f.read()
            except Exception:
                blob = b""
            hits = pat.findall(blob)
            if hits:
                url = hits[-1].decode("ascii")
                found.append(url)
                with open(WAN_URL_PATH, "w") as wf:
                    wf.write(url + "\n")
                print("wan=%s" % url)
                return
            time.sleep(0.4)
        print("wan=timeout (see host/.wan_cloudflared.log)")

    t = threading.Thread(target=wait_url)
    t.daemon = True
    t.start()
    t.join(32)
    return proc, (found[-1] if found else None)


def mail_wan(url, token):
    if not url:
        print("mail=skip no url")
        return
    page = url.rstrip("/") + "/?token=" + token
    try:
        from smtp_wan import send_wan_link
        send_wan_link(page)
    except Exception as ex:
        print("mail=fail %s" % type(ex).__name__)


def main(argv=None):
    p = argparse.ArgumentParser()
    p.add_argument("--serve", action="store_true")
    p.add_argument("--wan", action="store_true", help="Cloudflare tunnel for internet access")
    p.add_argument("--http-port", type=int, default=HTTP_PORT)
    args = p.parse_args(argv)

    token = load_or_make_token()
    com = first_com()
    os.chdir(HERE)
    write_start_bat(com, token)
    print("com=%s" % com)
    print("start_bat=%s" % os.path.join(HERE, "start_phone_tap.bat"))
    if args.serve and args.wan:
        tap = subprocess.Popen(
            [
                sys.executable, os.path.join(HERE, "phone_tap.py"),
                "--port", com, "--token", token,
                "--bind", "127.0.0.1",
                "--http-port", str(args.http_port),
            ],
            cwd=HERE,
        )
        time.sleep(1)
        _proc, url = start_wan_tunnel(args.http_port)
        mail_wan(url, token)
        return tap.wait()
    if args.wan:
        _proc, url = start_wan_tunnel(args.http_port)
        mail_wan(url, token)
    if args.serve:
        os.environ["FOC_TAP_TOKEN"] = token
        from phone_tap import main as tap_main
        return tap_main([
            "--port", com,
            "--token", token,
            "--bind", "127.0.0.1",
            "--http-port", str(args.http_port),
        ])
    return 0


if __name__ == "__main__":
    sys.exit(main())

# -*- coding: utf-8 -*-
"""Send WAN control URL by QQ SMTP. Credentials in gitignored .smtp.json."""
from __future__ import print_function

import json
import os
import smtplib
from email.header import Header
from email.mime.text import MIMEText
from email.utils import formataddr

HERE = os.path.dirname(os.path.abspath(__file__))
CFG_PATH = os.path.join(HERE, ".smtp.json")


def load_smtp():
    with open(CFG_PATH, "r") as f:
        return json.load(f)


def send_wan_link(page_url):
    cfg = load_smtp()
    user = cfg["user"]
    to = cfg.get("to") or user
    html = (
        "<p>FOC 手机控制（点击即可，口令已含在链接里）</p>"
        '<p><a href="%s">%s</a></p>'
        "<p>此地址在每次重新开启 Cloudflare 临时隧道后会变化。</p>"
    ) % (page_url, page_url)
    msg = MIMEText(html, "html", "utf-8")
    msg["Subject"] = Header("FOC 外网控制地址已更新", "utf-8")
    msg["From"] = formataddr(("FOC", user))
    msg["To"] = to
    s = smtplib.SMTP_SSL(cfg.get("host") or "smtp.qq.com", int(cfg.get("port") or 465), timeout=20)
    try:
        s.login(user, cfg["password"])
        s.sendmail(user, [to], msg.as_string())
    finally:
        s.quit()
    print("mail=ok")

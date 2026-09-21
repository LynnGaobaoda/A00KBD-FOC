#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Build an unsigned iOS Shortcut: popup list of all FOC tap actions."""
from __future__ import print_function

import os
import plistlib
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "FOC-motor.shortcut")

OBJ = "\ufffc"  # Shortcuts magic attachment character

# Shown in the iPhone popup, in this order.
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


def uid():
    return str(uuid.uuid4()).upper()


def tok_str(text, attachments=None):
    d = {"string": text}
    if attachments:
        d["attachmentsByRange"] = attachments
    return {"Value": d, "WFSerializationType": "WFTextTokenString"}


def tok_out(uuid_, name):
    return {
        "Value": {
            "OutputName": name,
            "OutputUUID": uuid_,
            "Type": "ActionOutput",
        },
        "WFSerializationType": "WFTextTokenAttachment",
    }


def dict_item(key, value):
    return {
        "WFItemType": 0,
        "WFKey": tok_str(key),
        "WFValue": {
            "SerializableObjectGraph": tok_str(value)["Value"]
            if False
            else tok_str(value),
            "WFSerializationType": "WFDictionaryFieldValue",
        },
    }


def dict_field_item(key, value):
    return {
        "WFItemType": 0,
        "WFKey": tok_str(key),
        "WFValue": tok_str(value),
    }


def list_item(text):
    return {
        "WFItemType": 0,
        "WFValue": tok_str(text),
    }


def action(ident, params):
    return {
        "WFWorkflowActionIdentifier": ident,
        "WFWorkflowActionParameters": params,
    }


def build(ip="192.168.1.20", token="改成你的FOC_TAP_TOKEN"):
    ip_id = uid()
    tok_id = uid()
    dict_id = uid()
    list_id = uid()
    choose_id = uid()
    key_id = uid()
    url_id = uid()
    get_id = uid()

    # http://{IP}:17891/tap/{action}?token={TOKEN}
    url_s = "http://" + OBJ + ":17891/tap/" + OBJ + "?token=" + OBJ
    ip_range = "{7, 1}"
    act_range = "{%d, 1}" % (7 + 1 + len(":17891/tap/"))
    tok_range = "{%d, 1}" % (7 + 1 + len(":17891/tap/") + 1 + len("?token="))

    actions = [
        action(
            "is.workflow.actions.comment",
            {
                "WFCommentActionText": (
                    "先改下面两段文本：电脑局域网 IP，以及 phone_tap.py 的 token。"
                    "每次运行会弹出全部电机功能。"
                )
            },
        ),
        action(
            "is.workflow.actions.gettext",
            {
                "UUID": ip_id,
                "CustomOutputName": "电脑IP",
                "WFTextActionText": ip,
            },
        ),
        action(
            "is.workflow.actions.gettext",
            {
                "UUID": tok_id,
                "CustomOutputName": "Token",
                "WFTextActionText": token,
            },
        ),
        action(
            "is.workflow.actions.dictionary",
            {
                "UUID": dict_id,
                "CustomOutputName": "功能表",
                "WFItems": {
                    "Value": {
                        "WFDictionaryFieldValueItems": [
                            dict_field_item(label, aid) for label, aid in MENU
                        ]
                    },
                    "WFSerializationType": "WFDictionaryFieldValue",
                },
            },
        ),
        action(
            "is.workflow.actions.list",
            {
                "UUID": list_id,
                "CustomOutputName": "功能列表",
                "WFItems": [list_item(label) for label, _ in MENU],
            },
        ),
        action(
            "is.workflow.actions.choosefromlist",
            {
                "UUID": choose_id,
                "CustomOutputName": "所选功能",
                "WFInput": tok_out(list_id, "功能列表"),
                "WFChooseFromListActionPrompt": "选择电机功能",
                "WFChooseFromListActionSelectAll": False,
            },
        ),
        action(
            "is.workflow.actions.getvalueforkey",
            {
                "UUID": key_id,
                "CustomOutputName": "动作ID",
                "WFInput": tok_out(dict_id, "功能表"),
                "WFDictionaryKey": tok_out(choose_id, "所选功能"),
            },
        ),
        action(
            "is.workflow.actions.gettext",
            {
                "UUID": url_id,
                "CustomOutputName": "请求地址",
                "WFTextActionText": tok_str(
                    url_s,
                    {
                        ip_range: {
                            "OutputName": "电脑IP",
                            "OutputUUID": ip_id,
                            "Type": "ActionOutput",
                        },
                        act_range: {
                            "OutputName": "动作ID",
                            "OutputUUID": key_id,
                            "Type": "ActionOutput",
                        },
                        tok_range: {
                            "OutputName": "Token",
                            "OutputUUID": tok_id,
                            "Type": "ActionOutput",
                        },
                    },
                ),
            },
        ),
        action(
            "is.workflow.actions.downloadurl",
            {
                "UUID": get_id,
                "CustomOutputName": "MCU回复",
                "WFHTTPMethod": "GET",
                "ShowHeaders": False,
                "WFURL": tok_out(url_id, "请求地址"),
            },
        ),
        action(
            "is.workflow.actions.showresult",
            {"Text": tok_out(get_id, "MCU回复")},
        ),
    ]

    # Fix gettext WFTextActionText: must be either plain string or token wrapper.
    # The URL action used tok_str nested inside WFTextActionText — unwrap:
    for a in actions:
        p = a["WFWorkflowActionParameters"]
        t = p.get("WFTextActionText")
        if isinstance(t, dict) and "WFSerializationType" in t:
            p["WFTextActionText"] = t

    wf = {
        "WFWorkflowClientRelease": "26.0",
        "WFWorkflowClientVersion": "3306.0.4",
        "WFWorkflowHasOutputFallback": False,
        "WFWorkflowHasShortcutInputVariables": False,
        "WFWorkflowIcon": {
            "WFWorkflowIconGlyphNumber": 59770,
            "WFWorkflowIconStartColor": 2072149247,
        },
        "WFWorkflowImportQuestions": [],
        "WFWorkflowInputContentItemClasses": [
            "WFStringContentItem",
        ],
        "WFWorkflowMinimumClientVersion": 1113,
        "WFWorkflowMinimumClientVersionString": "1113",
        "WFWorkflowMinimumiOSVersion": "15.0",
        "WFWorkflowOutputContentItemClasses": [],
        "WFWorkflowTypes": ["WatchKit", "NCWidget"],
        "WFQuickActionSurfaces": [],
        "WFWorkflowActions": actions,
        "WFWorkflowName": "FOC电机",
    }
    return wf


def main(argv=None):
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--ip", default="192.168.1.20")
    p.add_argument("--token", default="改成你的FOC_TAP_TOKEN")
    p.add_argument("--out", default=OUT)
    args = p.parse_args(argv)
    wf = build(args.ip, args.token)
    raw = plistlib.dumps(wf, fmt=plistlib.FMT_BINARY)
    out = os.path.abspath(args.out)
    with open(out, "wb") as f:
        f.write(raw)
    print("wrote", out, "bytes", len(raw))
    return 0


if __name__ == "__main__":
    main()

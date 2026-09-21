# MCU Host API

Portable serial host for STM32 FOC debug. No installer. CLI, JSON stdout, a tkinter GUI, and a localhost **COM API** share one implementation.

Surveyed SerialRUN (CLI+GUI agent), MegaSerial (preset shortcuts), CNTerminal (single exe). Those stacks are Modbus/PLC-heavy or GUI-only. This repo uses the same ideas: JSON presets, `ports`/`send`/`read` CLI, plus a small GUI.

## Run

```text
cd D:\A00KBD\FOC\host
python -m pip install -r requirements.txt
python mcu_host.py              # GUI + COM API
python mcu_host.py gui
```

Pack: `pyinstaller --onefile mcu_host.py` (keep console so CLI works).

## Two ports

| Port | Who opens it | Role |
|------|----------------|------|
| Hardware `COM3` (MCU UART) | **GUI only** after Connect | RX live view + TX |
| COM API `127.0.0.1:17890` | GUI listens | CLI / scripts control the same session |

CLI `send` / `preset` / `read` **prefer the COM API** if the GUI is running, so the window shows `>>` and MCU replies. Use `--direct` to open the hardware port from CLI (GUI must Disconnect first).

```text
python mcu_host.py gui
# in another terminal, after GUI Connect:
python mcu_host.py status
python mcu_host.py send COM3 p
python mcu_host.py preset COM3 foc-motor cw
```

Raw COM-API lines (one JSON or text line, reply is JSON):

```text
{"op":"send","text":"cw","read_ms":800}
SEND set spd 1
preset foc-motor ccw
status
```

`op`: `ports` `configs` `import` `status` `open` `close` `send` `read` `preset`.

## Config JSON

```json
{
  "id": "foc-motor",
  "name": "电机控制",
  "baud": 115200,
  "eol": "crlf",
  "commands": [
    { "id": "cw", "label": "顺时针60°", "tx": "cw" }
  ]
}
```

`eol`: `crlf` | `lf` | `cr` | `none`. Import many files; ids must be unique. Default packs live in `configs/`.

## CLI

| Command | Meaning |
|---------|---------|
| `mcu_host.py ports` | List hardware COM ports |
| `mcu_host.py status` | GUI COM API / open serial |
| `mcu_host.py configs` | List imported packs |
| `mcu_host.py import PATH.json` | Add a pack |
| `mcu_host.py unimport PATH.json` | Remove a pack from the host |
| `mcu_host.py send COM3 TEXT [--read-ms 400]` | TX + wait RX (via COM API if GUI up) |
| `mcu_host.py read COM3 [--read-ms 800]` | RX only |
| `mcu_host.py preset COM3 CONFIG_ID CMD_ID` | Named preset |
| `mcu_host.py flash COM3 foc-flash` | Serial IAP using a `type=flash` JSON |
| `mcu_host.py gui` | UI + listen `127.0.0.1:17890` |

`--direct` skips COM API. `--dtr` is off by default.

## Flash JSON (`type: flash`)

Surveyed STM32 YMODEM IAP (justloong ~12 KB, shatang ~6 KB). This tree uses a smaller custom frame on USART1 (same 115200 COM as debug):

- Boot `0x08000000` size 8 KB (`boot/boot.uvprojx`)
- APP `0x08002000` size 56 KB (`FOC_Code` + `VECT_TAB_OFFSET=0x2000`)
- Frame `A5 | cmd | lenle | payload | crc16-ccitt`

Import `configs/foc_flash.json`. GUI button **刷FOC APP**, or CLI `flash COM3 foc-flash`. APP UART `iap` writes BKP magic and resets into boot.

```json
{
  "id": "foc-flash",
  "type": "flash",
  "app_base": "0x08002000",
  "image": "../../out/motor.bin",
  "enter_tx": "iap",
  "commands": [
    {"id": "enter", "label": "进Boot", "tx": "iap"},
    {"id": "app", "label": "刷FOC APP", "flash": "image"}
  ]
}
```

## JSON replies

```json
{ "ok": true, "tx": "p", "bytes": 3, "rx": "cal 3205B ...", "rx_n": 80, "via": "com" }
```

`via` is `com` when the GUI COM API handled the call, else `serial`.

## GUI

Connect hardware COM, live RX pane (also shows CLI TX), send box, Import JSON, buttons from every imported pack. Status line shows `open COMx | com 127.0.0.1:17890`.

## iPhone WAN (Cloudflare + SMTP)

`start_phone_tap.bat` runs `setup_phone.py --serve --wan`: localhost HTTP on `127.0.0.1:17891` (`phone_tap.py` + `ios/foc_phone.html`), a `cloudflared` quick tunnel, then `smtp_wan.py` emails `https://<trycloudflare>/?token=...`. Copy `smtp.json.example` to `.smtp.json`. Taps call `mcu_host.py` CLI (`preset` / `send`); GUI is optional. If the GUI already owns COM, CLI uses `127.0.0.1:17890`; otherwise `--direct` opens the serial port.

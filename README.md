# STM32 PMSM FOC

STM32F103 上的 PMSM 磁场定向控制（FOC）工程：IAP **boot**、电机 **APP**、Python **上位机**。Keil MDK 编译，UART 调试与串口刷写共用 USART1（115200）。

## 目录

| 路径 | 说明 |
|------|------|
| `boot/` | 8 KB UART IAP 引导（`boot.uvprojx`） |
| `FOC_Code/` | FOC 应用固件（`motor.uvprojx`） |
| `host/` | 上位机：GUI、CLI、localhost COM API、串口 IAP |
| `host/API.md` | 上位机命令与 JSON 协议 |
| `host/configs/` | 电机控制 / 调参 / 刷写预设 |
| `out/` | 发布产物：`boot.bin`、`motor.bin` 及 axf/hex |
| `tools/publish_fw.bat` | Keil 编译后把固件拷到 `out/` |

本仓库不包含 Keil `Objects/` 中间文件、课程资料包和本机 `.codegraph` 索引。

## Flash 划分（STM32F103C8 64 KB）

| 区 | 地址 | 大小 |
|----|------|------|
| Boot | `0x08000000` | 8 KB |
| APP | `0x08002000` | 56 KB |

APP 需设置 `VECT_TAB_OFFSET=0x2000`。IAP 帧：`A5 | cmd | lenle | payload | crc16-ccitt`。版本字符串见 `FOC_Code/user/iap_map.h`（boot 与 APP 共用）。

## 编译与烧录

工具链：Keil MDK（UV4）+ ST-Link。目标名一般为 `STM32`。

1. 先下载 **boot**（`boot/boot.uvprojx`）。
2. 再下载 **APP**（`FOC_Code/motor.uvprojx`），或只下 APP 后用上位机串口 IAP。
3. 编译后可用 `tools/publish_fw.bat` 把 `motor`/`boot` 发布到 `out/`。

下载后若 MCU 停在调试 halt，需让内核运行后再拔调试器，否则电机、LED、UART 都会像死机。

## 上位机

依赖：Python 3 + `pyserial`（见 `host/requirements.txt`）。

```text
cd host
python -m pip install -r requirements.txt
python mcu_host.py              # GUI + COM API 127.0.0.1:17890
python mcu_host.py ports
python mcu_host.py send COM3 p
python mcu_host.py flash COM3 foc-flash
```

导入 `host/configs/foc_flash.json` 后，GUI 可 **刷FOC APP**（镜像默认 `out/motor.bin`）。APP 侧发 `iap` 会写入 BKP 魔术字并复位进 boot。完整协议见 [host/API.md](host/API.md)。

## 硬件约定

- MCU：STM32F103（72 MHz）
- 调试/IAP UART：USART1，115200
- 调试器：ST-Link

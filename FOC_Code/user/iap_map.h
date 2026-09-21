#ifndef IAP_MAP_H
#define IAP_MAP_H

/* STM32F103C8 64 KB: 8 KB boot + 56 KB app. Page = 1 KB. */
#define IAP_FLASH_BASE   0x08000000u
#define IAP_BOOT_SIZE    0x00002000u
#define IAP_APP_BASE     0x08002000u
#define IAP_APP_SIZE     0x0000E000u
#define IAP_APP_END      (IAP_APP_BASE + IAP_APP_SIZE)
#define IAP_APP_MAGIC_ADDR (IAP_APP_END - 4u)
#define IAP_APP_OK_MAGIC 0x41505031u  /* 'APP1' LE; only set after full IAP */
#define IAP_PAGE_SIZE    1024u
#define IAP_BKP_MAGIC    0x4941u   /* 'IA' in BKP_DR1 -> stay in boot */

#define IAP_SOH          0xA5u
#define IAP_ACK          0x5Au
#define IAP_CMD_PING     0x00u
#define IAP_CMD_ERASE    0x01u
#define IAP_CMD_WRITE    0x02u
#define IAP_CMD_CRC      0x03u
#define IAP_CMD_GO       0x04u
#define IAP_CMD_INFO     0x05u
#define IAP_CMD_REBOOT   0x06u
#define IAP_CMD_VER      0x07u

#define IAP_BOOT_VER_ADDR 0x08001FC0u
#define IAP_BOOT_VER_STR  "Boot_01.000_20260921"
#define IAP_APP_VER_STR   "App_01.000_20260921"

#endif

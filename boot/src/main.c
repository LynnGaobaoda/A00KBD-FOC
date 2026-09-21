#include "stm32f10x.h"
#include "iap_map.h"

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

/* Fixed in boot flash so APP can read Boot_xx.xxx_YYYYMMDD. */
const char IapBootVer[32] __attribute__((at(IAP_BOOT_VER_ADDR))) = IAP_BOOT_VER_STR;

/* Tiny UART IAP. Ideas from STM32 YMODEM IAP (wait-window + flash app),
 * without YMODEM/1 KB stack. Frames: A5 | cmd | lenle | payload | crc16le */

static uint16 Crc16Update(uint16 c, const uint8 *p, uint16 n)
{
    uint16 i;
    uint8 b;

    while (n--)
    {
        c ^= (uint16)(*p++) << 8;
        for (i = 0; i < 8u; i++)
        {
            b = (c & 0x8000u) ? 1u : 0u;
            c = (uint16)(c << 1);
            if (b)
            {
                c ^= 0x1021u;
            }
        }
    }
    return c;
}

static uint16 Crc16(const uint8 *p, uint16 n)
{
    return Crc16Update(0xFFFFu, p, n);
}

static void DelayMs(uint32 ms)
{
    SysTick->LOAD = 72000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = 5u;
    while (ms--)
    {
        while ((SysTick->CTRL & (1u << 16)) == 0u)
        {
        }
    }
    SysTick->CTRL = 0;
}

static void UartInit(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN;
    GPIOA->CRH &= ~0x00000FF0u;
    GPIOA->CRH |= 0x000004B0u; /* PA9 AF PP 50M, PA10 floating in */
    USART1->BRR = 0x0271u;     /* 72 MHz / 115200 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UartPut(uint8 b)
{
    while ((USART1->SR & USART_SR_TXE) == 0u)
    {
    }
    USART1->DR = b;
}

static int UartGet(uint8 *b, uint32 ms)
{
    uint32 t;

    SysTick->LOAD = 72000u - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = 5u;
    t = ms;
    while (t)
    {
        if ((USART1->SR & USART_SR_RXNE) != 0u)
        {
            *b = (uint8)USART1->DR;
            SysTick->CTRL = 0;
            return 0;
        }
        if ((SysTick->CTRL & (1u << 16)) != 0u)
        {
            t--;
        }
    }
    SysTick->CTRL = 0;
    return -1;
}

static void LedInit(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= (0x1u << 20);
    GPIOC->BSRR = (1u << 13);
}

static void FlashUnlock(void)
{
    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
}

static void FlashLock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int FlashWait(void)
{
    uint32 n = 800000u;

    while (((FLASH->SR & FLASH_SR_BSY) != 0u) && n)
    {
        n--;
    }
    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u)
    {
        FLASH->SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
        return -1;
    }
    return (n == 0u) ? -1 : 0;
}

static int FlashErasePage(uint32 a)
{
    if (FlashWait() != 0)
    {
        return -1;
    }
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = a;
    FLASH->CR |= FLASH_CR_STRT;
    if (FlashWait() != 0)
    {
        FLASH->CR &= ~FLASH_CR_PER;
        return -1;
    }
    FLASH->CR &= ~FLASH_CR_PER;
    return 0;
}

static int FlashEraseApp(void)
{
    uint32 a;
    uint32 last = IAP_APP_END - IAP_PAGE_SIZE;

    FlashUnlock();
    /* Kill tail magic first so erase-interrupt cannot leave (valid vectors + old APP1). */
    if (FlashErasePage(last) != 0)
    {
        FlashLock();
        return -1;
    }
    for (a = IAP_APP_BASE; a < last; a += IAP_PAGE_SIZE)
    {
        if (FlashErasePage(a) != 0)
        {
            FlashLock();
            return -1;
        }
    }
    FlashLock();
    return 0;
}

static int FlashWrite(uint32 addr, const uint8 *p, uint16 n)
{
    uint16 i;
    uint16 hw;

    if ((n & 1u) != 0u)
    {
        return -1;
    }
    if ((addr == IAP_APP_MAGIC_ADDR) && (n == 4u))
    {
        /* seal write only */
    }
    else if ((addr < IAP_APP_BASE) || ((addr + n) > IAP_APP_MAGIC_ADDR))
    {
        return -1;
    }
    FlashUnlock();
    for (i = 0; i < n; i += 2u)
    {
        hw = (uint16)p[i] | ((uint16)p[i + 1u] << 8);
        if (FlashWait() != 0)
        {
            FlashLock();
            return -1;
        }
        FLASH->CR |= FLASH_CR_PG;
        *(volatile uint16 *)(addr + i) = hw;
        if (FlashWait() != 0)
        {
            FLASH->CR &= ~FLASH_CR_PG;
            FlashLock();
            return -1;
        }
        FLASH->CR &= ~FLASH_CR_PG;
        if (*(volatile uint16 *)(addr + i) != hw)
        {
            FlashLock();
            return -1;
        }
    }
    FlashLock();
    return 0;
}

static uint8 AppValid(void)
{
    uint32 sp = *(volatile uint32 *)IAP_APP_BASE;
    uint32 pc = *(volatile uint32 *)(IAP_APP_BASE + 4u);
    uint32 tail = *(volatile uint32 *)IAP_APP_MAGIC_ADDR;

    /* Front: vector table. Tail: APP1 written only after full image. Both required. */
    if ((sp & 0x2FFE0000u) != 0x20000000u)
    {
        return 0u;
    }
    if ((pc < IAP_APP_BASE) || (pc >= IAP_APP_MAGIC_ADDR))
    {
        return 0u;
    }
    if (tail != IAP_APP_OK_MAGIC)
    {
        return 0u;
    }
    return 1u;
}

static uint16 BkpRead(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    return BKP->DR1;
}

static void BkpClear(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP->DR1 = 0;
}

static __asm void SetMsp(uint32 v)
{
    MSR MSP, r0
    BX lr
}

static void JumpApp(void)
{
    uint32 sp = *(volatile uint32 *)IAP_APP_BASE;
    uint32 pc = *(volatile uint32 *)(IAP_APP_BASE + 4u);
    void (*go)(void);

    if (!AppValid())
    {
        return;
    }
    BkpClear();
    USART1->CR1 = 0;
    SysTick->CTRL = 0;
    SCB->VTOR = IAP_APP_BASE;
    SetMsp(sp);
    go = (void (*)(void))pc;
    go();
}

static void UartPuts(const char *s)
{
    while (*s)
    {
        UartPut((uint8)*s++);
    }
}

static void ReplyVer(void)
{
    const char *s = IapBootVer;
    uint8 n = 0;
    uint8 i;

    while (s[n] != 0)
    {
        n++;
    }
    UartPut(IAP_ACK);
    UartPut(IAP_CMD_VER);
    UartPut(0);
    UartPut(n);
    for (i = 0; i < n; i++)
    {
        UartPut((uint8)s[i]);
    }
}

static void Reply(uint8 cmd, uint8 st)
{
    uint8 r[4];

    r[0] = IAP_ACK;
    r[1] = cmd;
    r[2] = st;
    r[3] = 0;
    UartPut(r[0]);
    UartPut(r[1]);
    UartPut(r[2]);
    UartPut(r[3]);
}

static void ReplyInfo(void)
{
    uint8 p[12];
    uint16 c;
    uint8 i;

    p[0] = IAP_ACK;
    p[1] = IAP_CMD_INFO;
    p[2] = 0;
    p[3] = 8;
    p[4] = (uint8)(IAP_APP_BASE);
    p[5] = (uint8)(IAP_APP_BASE >> 8);
    p[6] = (uint8)(IAP_APP_BASE >> 16);
    p[7] = (uint8)(IAP_APP_BASE >> 24);
    p[8] = (uint8)(IAP_APP_SIZE);
    p[9] = (uint8)(IAP_APP_SIZE >> 8);
    p[10] = (uint8)(IAP_APP_SIZE >> 16);
    p[11] = (uint8)(IAP_APP_SIZE >> 24);
    c = Crc16(&p[1], 11);
    for (i = 0; i < 12u; i++)
    {
        UartPut(p[i]);
    }
    UartPut((uint8)c);
    UartPut((uint8)(c >> 8));
}

static uint8 g_stay;

static void DoCmd(uint8 cmd, uint8 *pl, uint16 n)
{
    uint32 addr;
    uint32 len;
    uint16 crc;
    uint8 st = 0;

    switch (cmd)
    {
        case IAP_CMD_PING:
            g_stay = 1u;
            Reply(cmd, 0);
            break;
        case IAP_CMD_ERASE:
            g_stay = 1u;
            st = (FlashEraseApp() == 0) ? 0u : 3u;
            Reply(cmd, st);
            break;
        case IAP_CMD_WRITE:
            if (n < 6u)
            {
                Reply(cmd, 2);
                break;
            }
            addr = (uint32)pl[0] | ((uint32)pl[1] << 8) | ((uint32)pl[2] << 16) | ((uint32)pl[3] << 24);
            st = (FlashWrite(addr, pl + 4, (uint16)(n - 4u)) == 0) ? 0u : 3u;
            Reply(cmd, st);
            break;
        case IAP_CMD_CRC:
            if (n < 8u)
            {
                Reply(cmd, 2);
                break;
            }
            addr = (uint32)pl[0] | ((uint32)pl[1] << 8) | ((uint32)pl[2] << 16) | ((uint32)pl[3] << 24);
            len = (uint32)pl[4] | ((uint32)pl[5] << 8) | ((uint32)pl[6] << 16) | ((uint32)pl[7] << 24);
            if ((addr < IAP_APP_BASE) || ((addr + len) > IAP_APP_END))
            {
                Reply(cmd, 2);
                break;
            }
            crc = Crc16((const uint8 *)addr, (uint16)len);
            {
                uint8 r[6];
                r[0] = IAP_ACK;
                r[1] = cmd;
                r[2] = 0;
                r[3] = 2;
                r[4] = (uint8)crc;
                r[5] = (uint8)(crc >> 8);
                UartPut(r[0]);
                UartPut(r[1]);
                UartPut(r[2]);
                UartPut(r[3]);
                UartPut(r[4]);
                UartPut(r[5]);
            }
            break;
        case IAP_CMD_GO:
            if (!AppValid())
            {
                Reply(cmd, 4);
                break;
            }
            Reply(cmd, 0);
            DelayMs(20);
            JumpApp();
            Reply(cmd, 4);
            break;
        case IAP_CMD_REBOOT:
            BkpClear();
            Reply(cmd, 0);
            DelayMs(20);
            NVIC_SystemReset();
            break;
        case IAP_CMD_INFO:
            g_stay = 1u;
            ReplyInfo();
            break;
        case IAP_CMD_VER:
            g_stay = 1u;
            ReplyVer();
            break;
        default:
            Reply(cmd, 1);
            break;
    }
}

static void Pump(void)
{
    uint8 b;
    uint8 cmd;
    uint8 lo;
    uint8 hi;
    uint16 n;
    uint16 i;
    uint16 crc;
    uint16 got;
    static uint8 pl[260];

    if (UartGet(&b, 20) != 0)
    {
        return;
    }
    if (b != IAP_SOH)
    {
        static char line[24];
        static uint8 lp;
        char c = (char)b;

        if ((c == '\r') || (c == '\n'))
        {
            line[lp] = 0;
            if ((lp > 0) && ((line[0] == 'v') || (line[0] == 'V'))
                && (line[1] == 'e') && (line[2] == 'r'))
            {
                if ((line[3] == 0) || (line[4] == 'b') || (line[4] == 'B'))
                {
                    UartPuts(IapBootVer);
                    UartPuts("\r\n");
                }
                else
                {
                    UartPuts("boot only\r\n");
                }
            }
            lp = 0;
        }
        else if (lp < 23u)
        {
            line[lp++] = c;
        }
        else
        {
            lp = 0;
        }
        return;
    }
    if (UartGet(&cmd, 50) != 0)
    {
        return;
    }
    if (UartGet(&lo, 50) != 0)
    {
        return;
    }
    if (UartGet(&hi, 50) != 0)
    {
        return;
    }
    n = (uint16)lo | ((uint16)hi << 8);
    if (n > 256u)
    {
        return;
    }
    for (i = 0; i < n; i++)
    {
        if (UartGet(&pl[i], 50) != 0)
        {
            return;
        }
    }
    if (UartGet(&lo, 50) != 0)
    {
        return;
    }
    if (UartGet(&hi, 50) != 0)
    {
        return;
    }
    got = (uint16)lo | ((uint16)hi << 8);
    {
        uint8 hdr[3];
        hdr[0] = cmd;
        hdr[1] = (uint8)n;
        hdr[2] = (uint8)(n >> 8);
        crc = Crc16Update(Crc16(hdr, 3), pl, n);
        if (crc != got)
        {
            Reply(cmd, 1);
            return;
        }
    }
    DoCmd(cmd, pl, n);
}

int main(void)
{
    uint32 wait_ms;
    uint8 stay_magic;

    LedInit();
    UartInit();
    stay_magic = (BkpRead() == IAP_BKP_MAGIC) ? 1u : 0u;
    g_stay = stay_magic || (!AppValid());
    wait_ms = g_stay ? 0xFFFFFFFFu : 1200u;

    while (wait_ms)
    {
        GPIOC->ODR ^= (1u << 13);
        Pump();
        if (g_stay)
        {
            wait_ms = 0xFFFFFFFFu;
        }
        else if (wait_ms != 0xFFFFFFFFu)
        {
            if (wait_ms > 20u)
            {
                wait_ms -= 20u;
            }
            else
            {
                wait_ms = 0;
            }
        }
    }
    JumpApp();
    g_stay = 1u;
    for (;;)
    {
        GPIOC->ODR ^= (1u << 13);
        Pump();
    }
}

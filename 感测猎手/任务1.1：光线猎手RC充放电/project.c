#include "AI8051U.h"
#include "ai_usb.h"            /* STC USB 头文件，usb_init() / printf_usb() */

/* ====================================================================
   感测猎手 · 任务 1.1  光线猎手（光敏电阻 + 定时器 + RC 充放电测时间）
   ====================================================================
   任务要求：
     1.1  光敏电阻 + 分压电阻 + 电容充放电，接 P1.0，用"定时器 + 电容充放电
          测时间法"做到约 8 位有效分辨率（计数值动态范围 >= 256 档可区分，
          或最终量化到 0~255）。
     1.2  串口实时打印原始计数值（充电计数值）。
     1.3  三级光照判断（黑暗 / 室内 / 日光）。
     禁止直接调用片内 ADC。

   硬件（本版）：
     光敏电阻 LDR + 1k 串联 + 0.1uF，接 P1.0。

                 VCC (5V)
                   |
             [光敏电阻 LDR]      亮 <100Ω .. 暗 >70MΩ
                   |
             [1kΩ 串联]          ← 串联限流，不要改成下拉到地
                   |
                   +------ P1.0   放电时推挽输出 0 / 充电时高阻输入
                   |
             [0.1μF 电容]        ← 必须直接跨在 P1.0 与 GND 之间！
                   |
                  GND

   时钟与计时：
       AiCube-ISP "输入用户程序运行时的 IRC 频率" = 10.000 MHz
       Timer0 用 1T 模式：1 tick = 1/10MHz = 0.1μs

   2026-11-12 实测记录（1k 串联 / 1T / 10MHz / C=0.1μF）：
       手电筒   ticks=6        ← 不可能，见下
       室内光   ticks=5377     sd=19，level=81
       黑暗     ticks=350034   超时（35ms）

       室内光 5377 tick 反推：R_total = 537.7us / (0.1uF * 0.7765) = 6.9kΩ
       → R_LDR ≈ 5.9kΩ，与万用表量级一致，说明电容在位、模型正确。
       → 但 6 tick 意味着节点在定时器启动前就已经是高电平：
         即使 R_LDR=0，也应有 (0+1k)*0.1uF*0.7765 = 78us ≈ 780 tick。
         所以手电筒那一次的 P1.0 被直接接到了 VCC（LDR/1k 支路被旁路），
         属于接线问题，不是光太强。

   2026-11-12 删除了上一版的 dis= 自检，原因（重要，别再用）：
       自检时 P1.0 处于推挽输出低电平，读 P1 永远读到 0，
       因此 dis 恒为 6，根本测不出电容放电时间。这个方法本身不成立。
       真正判断电容在位的依据是"室内光 5377 tick 与模型吻合"。
   ==================================================================== */


#define USE_USB         1         /* 1 = USB-CDC 虚拟串口打印，0 = 关闭 */

/* ---- 时钟 / 计时 ---- */
#define FOSC_HZ         10000000UL   /* 以 AiCube-ISP 里设定值为准 */
                                 /* 1T 模式：1 tick = 1/FOSC = 0.1μs   */

/* ---- 量程 / 标定常数 ---- */
#define TICKS_MIN       750UL     /* 最亮端：R_LDR→0 时约 780 tick */
#define TICKS_MAX       350000UL  /* 最暗端：黑暗超时处 */
#define TIMEOUT_TICKS   350000UL  /* 35ms：暗处超时判为最暗，保 1.5 的 <50ms */

/* ---- 1.3 三级判断门限（单位 tick，可按实测微调）----
   日光   R_LDR 约 100Ω~1k  → 约 100~1600 tick
   室内   R_LDR 约 5~10k    → 约 4800~8600 tick
   黑暗   R_LDR > 10M       → 超时 350034
   门限取在两者中间（对数中点附近）： */
#define TH_DARK         100000UL  /* >= 判黑暗（R_total > 13MΩ） */
#define TH_SUN          2000UL    /* <= 判日光（R_total < 2.6kΩ） */

/* 平均次数：1 = 打印真实原始值（1.2 要求）。 */
#define AVG_N           1

/* ---- 延时（单位：tick，1 tick = 0.1μs）---- */
#define DISCHARGE_TICKS 50000UL   /* 5ms，远大于放电 τ，保证放到 0V */
#define GAP_TICKS       30000UL   /* 3ms，两次打印之间的间隔 */


/* ===== 系统初始化（AI8051U 32 位模式，先解锁扩展 SFR）===== */
void Sys_Init(void)
{
    EAXFR = 1;                  /* 使能访问扩展寄存器 XFR */
    WTST  = 0;                  /* 取消取指等待 */
    CKCON = 0;                  /* 片内 xdata 访问不等待 */
}


#if USE_USB

void Usb_PutChar(char c)
{
    printf_usb("%c", (int)c);
}

void Usb_SendString(char *str)
{
    while (*str)
        Usb_PutChar(*str++);
}

/* 打印十进制（unsigned long，0~4294967295） */
void Usb_SendDec32(unsigned long val)
{
    char buf[12];
    unsigned char i = 11;
    unsigned char k;

    if (val == 0)
    {
        Usb_PutChar('0');
        return;
    }
    while (val)
    {
        buf[--i] = (char)((val % 10) + '0');
        val /= 10;
    }
    for (k = i; k < 11; k++)
        Usb_PutChar(buf[k]);
}

#endif


/* ===== Timer0 初始化：16 位模式，1T 时钟 ===== */
void Timer0_Init(void)
{
    TMOD &= 0xF0;               /* 清 T0 字段 */
    TMOD |= 0x01;               /* M1M0 = 01，16 位定时器 */
    AUXR |= 0x80;               /* T0x12 = 1，1T：1 tick = 1/10MHz = 0.1μs */
    /* 不使能中断，主循环直接查询 TF0 */
}


/* ===== 用 Timer0 做精确延时，单位 tick（0.1μs）===== */
void DelayTicks(unsigned long n)
{
    unsigned long cnt = 0;
    unsigned int lo;

    TH0 = 0;
    TL0 = 0;
    TF0 = 0;
    TR0 = 1;

    while (1)
    {
        if (TF0)                /* 16 位溢出一次 = 65536 tick = 6.55ms */
        {
            TF0 = 0;
            cnt += 65536UL;
        }
        lo = (unsigned int)((TH0 << 8) | TL0);
        if (cnt + (unsigned long)lo >= n)
            break;
    }
    TR0 = 0;
}


/* ===== RC 充放电一次，返回计数值（1 tick = 0.1μs）=====
   串联拓扑：放电时 P1.0 输出 0 把节点拉低，充电时 P1.0 高阻输入，
   电容经 (LDR + 1k) 朝 VCC 充电；到不了 VIH 时超时返回。 */
unsigned long MeasureTicks(void)
{
    unsigned long ticks = 0;
    unsigned int lo;

    /* ① 放电：P1.0 推挽输出 0，把电容放到约 0V */
    P1M1 &= ~0x01;              /* P1.0 推挽输出 */
    P1M0 |=  0x01;
    P1  &= ~0x01;               /* 输出低 */
    DelayTicks(DISCHARGE_TICKS);/* 5ms，远大于 (1k||引脚内阻)*0.1μF */

    /* ② 切高阻输入：电容开始经 (LDR+1k) 向 VCC 充电 */
    P1M1 |=  0x01;              /* P1.0 高阻输入 */
    P1M0 &= ~0x01;

    /* ③ 启动定时器 */
    TH0 = 0;
    TL0 = 0;
    TF0 = 0;
    TR0 = 1;

    /* ④ 轮询 IO，0→1 跳变即停 */
    while ((P1 & 0x01) == 0)
    {
        if (TF0)                /* 16 位溢出一次 = 65536 tick */
        {
            TF0 = 0;
            ticks += 65536UL;
        }
        lo = (unsigned int)((TH0 << 8) | TL0);
        if (ticks + (unsigned long)lo >= TIMEOUT_TICKS)
            break;
    }
    TR0 = 0;

    /* 收尾：把最后一次可能漏掉的溢出补上（TR0 已停，读数无竞争） */
    lo = (unsigned int)((TH0 << 8) | TL0);
    if (TF0)
    {
        TF0 = 0;
        ticks += 65536UL;
    }

    return ticks + lo;          /* 超时则返回 ≈ TIMEOUT_TICKS */
}


/* ===== 测 AVG_N 次取平均（AVG_N=1 时即原始值）===== */
unsigned long MeasureAvg(void)
{
    unsigned long sum = 0;
    unsigned char n;

    for (n = 0; n < AVG_N; n++)
        sum += MeasureTicks();

    return sum / AVG_N;
}


/* ===== 计算 log2(v)*256（v>0），用于对数映射 ===== */
unsigned long Log2x256(unsigned long v)
{
    unsigned char shifts = 0;
    unsigned long frac;

    if (v == 0) return 0;

    while (!(v & 0x80000000UL))
    {
        v <<= 1;
        shifts++;
    }
    frac = (v >> 23) & 0xFFUL;

    return (((unsigned long)(31 - shifts)) << 8) + frac;
}


/* ===== 把原始计数值按 log2 映射到 0~255（0=亮 .. 255=暗）===== */
unsigned char Quantize(unsigned long ticks)
{
    unsigned long Lmin = Log2x256(TICKS_MIN);
    unsigned long Lmax = Log2x256(TICKS_MAX);
    unsigned long L;
    unsigned long v;

    if (ticks == 0)
        return 0;

    L = Log2x256(ticks);

    if (L <= Lmin) return 0;
    if (L >= Lmax) return 255;

    v = (L - Lmin) * 255UL / (Lmax - Lmin);
    if (v > 255UL)
        v = 255UL;
    return (unsigned char)v;
}


/* ===== 1.3 三级光照判断 ===== */
char *LightClass(unsigned long ticks)
{
    if (ticks >= TH_DARK) return "DARK  (黑暗)";
    if (ticks <= TH_SUN)  return "SUN   (日光)";
    return "ROOM  (室内)";
}


void main(void)
{
    unsigned long ticks;
    unsigned char level;

    Sys_Init();
    Timer0_Init();

#if USE_USB
    usb_init();
    EA = 1;

    Usb_SendString("\r\n[1.1] 光线猎手  LDR+1k 串联 RC 充放电测时间\r\n");
    Usb_SendString("10MHz / Timer0 1T / 1 tick = 0.1us\r\n");
    Usb_SendString("ticks=原始计数值  level=0~255(0亮..255暗)  class=三级判断\r\n");
#endif

    while (1)
    {
        ticks = MeasureAvg();
        level = Quantize(ticks);

#if USE_USB
        Usb_SendString("ticks=");
        Usb_SendDec32(ticks);
        Usb_SendString("  level=");
        Usb_SendDec32((unsigned long)level);
        Usb_SendString("  class=");
        Usb_SendString(LightClass(ticks));
        Usb_SendString("\r\n");
#endif

        DelayTicks(GAP_TICKS);  /* 3ms；暗处一轮约 35ms+3ms，仍满足 1.5 的 <50ms */
    }
}

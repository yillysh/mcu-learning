#include "AI8051U.h"

/* ==================== 任务 6.4  低边开关 PWM 调光 ====================
   ------------------------------------------------------------------
   接线（承接 6.1 串联LED + 6.2 低边三极管 + 6.3 地线，本任务不改线）：

        VLED(5V) --[限流电阻 R]-- LED1 -->|-- LED2 -->|--+
                                                          |
                                                    三极管 集电极 C
        P1.0 --[基极电阻 1k]--+                            |
                             +-- 三极管 基极 B       发射极 E -- 功率地
                                                          |
                          MCU 地 -----------------------+-- 单点(星形)连接

   极性结论（必须写进报告）：
       低边 NPN 做开关：基极高电平 -> 管子导通 -> LED 通电点亮
       所以   D_on = D_high      （LED导通占空比 = GPIO高电平占空比）
       LED 平均电流  I_led_avg ~= I_on x D_on
       （基极电流只有约 (3.3-0.7)/1k = 2.6mA，LED 大电流走三极管，不灌进 MCU 脚）

   参数算例（Fclk = 40MHz，本文件两种 PWM 都做成 1kHz）：
       硬件 PWM： f = Fclk / (PSC+1) / (ARR+1) = 40M / 1 / 40000 = 1kHz
                  D_high = CCR1 / (ARR+1)，ARR+1 = 40000
                  所以 CCR1 = 占空比(%) x 400
       软件 PWM： 定时器 10us 中断一次，100 次 = 1ms = 1kHz
                  D_high = duty / 100

   MODE = 0   标定模式：0% -> 25% -> 50% -> 75% -> 100%，每档停 3s 循环
              （用示波器量 T_high 与 T，算 D_high = T_high/T，填下面表）
   MODE = 1   呼吸模式：D_on 在 0%~100% 之间缓慢往返，看亮度是否平滑
   USE_HW_PWM = 1   硬件 PWMA 通道1（PWM1P -> P1.0）
   USE_HW_PWM = 0   软件 PWM（定时器中断 + IO，还是 P1.0，接线不变）

   ---- 占空比标定表（自己量完填）-----------------------------------
    设定D_on  CCR1值   T_high(实测)   T(实测)   D_high=T_high/T   亮度观感
      0%        0       静态低电平      -        不适用            灭
     25%     10000
     50%     20000
     75%     30000
    100%     40000     静态高电平      -        不适用           最亮
   ------------------------------------------------------------------
   ==================================================================== */


#define MODE        0           /* 0 = 标定模式(5档)   1 = 呼吸模式 */
#define USE_HW_PWM  1           /* 1 = 硬件PWMA        0 = 软件PWM  */
#define LEVEL_MS    3000        /* 标定模式每档停留时间(ms) */

#define ARR_H       0x9C        /* ARR = 39999 -> 周期 40000 个计数 */
#define ARR_L       0x3F
#define CCR_PER_PCT 400         /* CCR1 = 占空比% x 400  (40000/100) */


/* 5 档占空比，硬件/软件 PWM 共用同一张表（单位 %） */
const unsigned char code duty_pct[5] = { 0, 25, 50, 75, 100 };

unsigned char duty = 0;         /* 当前 D_on，单位 %，范围 0~100 */

#if (MODE == 0)
unsigned char level_idx = 0;    /* 标定模式：当前档位索引 0~4 */
#else
unsigned char breath_duty = 0;  /* 呼吸模式：当前 D_on 0~100 */
bit breath_dir = 0;             /* 0 = 变亮，1 = 变暗 */
#endif

#if !USE_HW_PWM
unsigned char pwm_cnt = 0;      /* 软件 PWM 计数器，0~99 */
#endif


/* ===== 毫秒延时（约 1ms @40MHz，不够准请自己用示波器校准） ===== */
void delay_ms(unsigned int ms)
{
    unsigned int i;
    while (ms--)
    {
        i = 2500;
        while (--i);
    }
}


/* ===== 设置 D_on（0~100 %），硬件与软件 PWM 共用同一个入口 ===== */
void SetDuty(unsigned char pct)
{
    duty = pct;                 /* 软件 PWM 直接在中断里用这个值 */

#if USE_HW_PWM
    {
        unsigned int ccr;
        ccr = (unsigned int)pct * CCR_PER_PCT;  /* 100% -> 40000 >= ARR+1，得静态高 */
        PWMA_CCR1H = (unsigned char)(ccr >> 8);
        PWMA_CCR1L = (unsigned char)(ccr & 0xFF);
    }
#endif
}


/* ======================= 硬件 PWM 配置 ======================= */
#if USE_HW_PWM

void PWMA_Init(void)
{
    EAXFR = 1;                  /* 必须先置1，否则 PWMA 扩展寄存器读写无效 */

    P1M0 |= 0x01;               /* P1.0 推挽输出（PWM1P 从此脚出去驱动基极） */
    P1M1 &= ~0x01;

    PWMA_PS = 0x00;             /* C1PS[1:0]=00 -> PWM1P 映射到 P1.0 */
                                /* （=01 是 P0.0，=10 是 P2.0，别配错脚） */

    PWMA_PSCRH = 0x00;          /* 预分频 = 0，PWM 时钟 = 40MHz */
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = ARR_H;          /* ARR = 39999 -> 周期 40000 -> 1kHz */
    PWMA_ARRL = ARR_L;

    PWMA_CCMR1 = 0x60;          /* OC1M[2:0]=110 -> PWM 模式1 */

    PWMA_CCR1H = 0x00;          /* 初始 D_on = 0%，LED 灭 */
    PWMA_CCR1L = 0x00;          /* 运行时改这两个寄存器就是调光 */

    PWMA_CCER1 = 0x01;          /* CC1E(bit0)=1 开通道1输出；CC1P(bit1)=0 高电平有效 */

    PWMA_BKR = 0x80;            /* MOE=1 使能 PWM 主输出（少这句脚上没波形） */

    PWMA_ENO |= 0x01;           /* ENO1P=1 把 PWM1P 接到引脚 */

    PWMA_CR1 = 0x01;            /* CEN=1 启动计数器 */

    /* 注意：启动后 P1.0 归 PWM 外设驱动，程序里不要再写 P10 = x */
}

/* ======================= 软件 PWM 配置 ======================= */
#else

void Timer0_Init(void)
{
    TM0PS = 0;                  /* 定时器0 时钟 = 40MHz/12 ~ 3.333MHz，0.3us 一个计数 */
                                /* 10us / 0.3us ~ 33.3 -> 取 33
                                   65536 - 33 = 65503 = 0xFFDF */
    TH0 = 0xFF;
    TL0 = 0xDF;
    ET0 = 1;                    /* 开定时器0中断 */
    TR0 = 1;                    /* 启动定时器0 */
}

/* ===== 软件 PWM 中断：100 次 x 10us = 1ms = 1kHz ===== */
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xFF;                 /* 重装初值 */
    TL0 = 0xDF;

    pwm_cnt++;
    if (pwm_cnt >= 100)         /* 100 档 = 1 个周期 */
        pwm_cnt = 0;

    if (pwm_cnt < duty)         /* duty=0 恒低，duty=100 恒高，正好对上静态两档 */
        P10 = 1;                /* 基极高 -> NPN 导通 -> LED 亮（D_on = D_high） */
    else
        P10 = 0;                /* 基极低 -> 截止   -> LED 灭 */
}

#endif


void main(void)
{
#if USE_HW_PWM
    PWMA_Init();
#else
    P1M0 |= 0x01;               /* P1.0 推挽输出 */
    P1M1 &= ~0x01;
    P10 = 0;                    /* 上电先关断三极管，LED 灭 */
    Timer0_Init();
    EA = 1;                     /* 开总中断（软件 PWM 全靠它） */
#endif

#if (MODE == 0)

    /* ---- 标定模式：5 档循环，每档 3s，配合示波器读数 ---- */
    while (1)
    {
        SetDuty(duty_pct[level_idx]);

        if (++level_idx >= 5)
            level_idx = 0;

        delay_ms(LEVEL_MS);
    }

#else

    /* ---- 呼吸模式：D_on 每 20ms 动 1%，0% <-> 100% 往返 ---- */
    while (1)
    {
        delay_ms(20);

        if (breath_dir == 0)            /* 变亮 */
        {
            if (breath_duty >= 100)
                breath_dir = 1;
            else
                breath_duty++;
        }
        else                            /* 变暗 */
        {
            if (breath_duty == 0)
                breath_dir = 0;
            else
                breath_duty--;
        }

        SetDuty(breath_duty);
    }

#endif
}

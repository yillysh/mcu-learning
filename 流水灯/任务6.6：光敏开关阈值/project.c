#include "AI8051U.h"

/* ==================== 任务 6.6  光敏参与开关 / 阈值 ====================
   ------------------------------------------------------------------
   目标：用光敏电阻感知环境光，参与低边开关的开/关决策，并把光强数值
         记录下来，和环境（全黑 / 室内 / 台灯直照）做对照。

   >>> 用回差(滞回)做阈值，不用单点比较 <<<
       单点比较在阈值附近会疯狂抖动：亮一下 -> 光敏读到更亮 -> 灭 ->
       又变暗 -> 再亮…… 眨眼式的闪烁。
       本代码用两个阈值：
           比 LIGHT_ON_ADC  还暗  -> 开灯
           比 LIGHT_OFF_ADC 还亮  -> 关灯
       两者之间是"保持原状态"的死区，这就是回差(迟滞)，和施密特触发器
       一个道理。回差大小 = LIGHT_OFF_ADC - LIGHT_ON_ADC，报告里要写。

   ------------------------------------------------------------------
   接线（在 6.5 基础上加一条光敏分压，灯串那一路不变）：

   光敏分压（LDR_TOP = 1 的接法，推荐）：
        VCC(3.3V) -- [光敏电阻 LDR] --+-- P1.1 (ADC1)
                                      |
                                  [10k 电阻]
                                      |
                                     GND
       越亮 -> LDR 阻值越小 -> P1.1 电压越高 -> ADC 值越大

   灯串（同 6.5）：
        VOUT --[限流电阻]-- LED1..LEDn -->|--+-- 三极管/MOS 集电极(漏极)
                                             |  发射极(源极) -- 功率地
        P1.0 --[基极/栅极电阻]-- B(G)        |
                            MCU 地 -- 单点(星形)连接

   若你的光敏是反过来接的（LDR 在下臂，越亮 ADC 越小），把 LDR_TOP 改成 0，
   代码会自动翻转，后面的阈值判断逻辑一行都不用动。

   ------------------------------------------------------------------
   关键寄存器（已查 AI8051U 规格书 / 官方 AiCube 头核对）：

     ADC 通道：ADC0 = P1.0（已被开关占用）
               ADC1 = P1.1  <- 光敏接这里
     ADC_CONTR：bit7 ADC_POWER，bit6 ADC_START，bit5 ADC_FLAG，bit[3:0] 通道
     ADCCFG   ：bit5 RESFMT。0=左对齐，1=右对齐
                右对齐时 ADC_RES 存高 4 位(在低半字节)，ADC_RESL 存低 8 位
                所以 12 位结果 = ((ADC_RES & 0x0F) << 8) | ADC_RESL
     ADCTIM   ：扩展 SFR(0x7EFEA8)，必须先 EAXFR = 1 才能写
                官方推荐固定值 0x3F
     SPEED    ：FADC = SYSclk / 2 / (SPEED+1)
                取 SPEED=15 -> FADC = 40MHz/2/16 = 1.25MHz

   参考电压默认就是 VCC(3.3V)，所以：
        节点电压 Vnode = ADC值 / 4095 x 3.3V
   报告里可以把"ADC 值"和"万用表量的节点电压"并排列成一张表，正好对照。

   ------------------------------------------------------------------
   MODE = 0   光控开关 + 回差（默认，任务本体）
   MODE = 1   光控比例调光：越暗越亮，占空比随光强连续变化
   MODE = 2   光强标定：灯串保持关断，只周期采样并串口打印，
              用来给 LIGHT_ON_ADC / LIGHT_OFF_ADC 定值

   ---- 光强标定记录表（用 MODE = 2 测，量完填）----------------------
    环境          原始ADC(0~4095)   节点电压(mV)   归一化light(越大越亮)
    全黑(遮住)
    室内正常光
    台灯直照
    手电/强光
   ------------------------------------------------------------------
   特别注意：光敏别对着自己驱动的那串灯！
       否则形成光反馈：灯亮 -> 光敏变亮 -> 关灯 -> 变暗 -> 开灯 -> 自激振荡。
       回差只能压制小幅抖动，压不住这种正反馈。装位置时让光敏朝环境光、
       背对灯串，或者加遮光隔板。

   另外：P1.1 同时也是 PWM1N 的复用脚，本代码只开 PWM1P(ENO1P)，
         没开 PWM1N(CC1NE)，所以不冲突，不用额外处理。
   ==================================================================== */


#define MODE            0       /* 0 = 光控开关(带回差)
                                   1 = 光控比例调光
                                   2 = 光强标定(只采样打印)      */

#define LDR_TOP         1       /* 1 = 光敏接VCC上臂(越亮ADC越大)
                                   0 = 光敏接GND下臂(越亮ADC越小) */

#define LIGHT_ON_ADC    1000    /* 归一化光强低于此值 -> 开灯（偏暗） */
#define LIGHT_OFF_ADC   1400    /* 归一化光强高于此值 -> 关灯（变亮） */

#define SAMPLE_MS       100     /* 采样周期 */
#define SOFT_ON_MS      300     /* 开灯软启动时间，避免高压灯串上电浪涌 */
#define USE_UART        1       /* 1 = 串口打印(115200 @40MHz)，0 = 关闭 */

#define ARR_H           0x9C    /* ARR = 39999 -> 周期 40000 -> 1kHz @40MHz */
#define ARR_L           0x3F
#define CCR_PER_PCT     400     /* CCR1 = 占空比% x 400 */


#if (LIGHT_OFF_ADC <= LIGHT_ON_ADC)
#error "LIGHT_OFF_ADC 必须大于 LIGHT_ON_ADC，否则没有回差，会来回闪"
#endif

unsigned char duty = 0;         /* 当前 D_on，单位 %，0~100 */
bit light_on = 0;               /* 0 = 灯串灭，1 = 灯串亮 */


/* ===== 毫秒延时（约 1ms @40MHz）===== */
void delay_ms(unsigned int ms)
{
    unsigned int i;
    while (ms--)
    {
        i = 2500;
        while (--i);
    }
}


/* ===== 串口 1（沿用任务2 的 115200 @40MHz 配置）===== */
#if USE_UART

void Uart1_SendByte(unsigned char dat)
{
    SBUF = dat;
    while (!TI);                /* 等发送完成 */
    TI = 0;                     /* 软件清标志 */
}

void Uart1_SendString(unsigned char *str)
{
    while (*str)
        Uart1_SendByte(*str++);
}

/* 打印 0~65535 的十进制 */
void Uart1_SendDec16(unsigned int val)
{
    unsigned char buf[5];
    unsigned char i = 0;

    if (val == 0)
    {
        Uart1_SendByte('0');
        return;
    }
    while (val)
    {
        buf[i++] = (unsigned char)((val % 10) + '0');
        val /= 10;
    }
    while (i)
        Uart1_SendByte(buf[--i]);
}

void Uart1_Init(void)
{
    SCON = 0x50;                /* 8位可变波特率，允许接收 */
    AUXR |= 0x01;               /* 串口1 用定时器2 做波特率 */
    AUXR |= 0x04;               /* 定时器2 时钟 = Fosc (1T) */
    T2L = 0xA9;                 /* 115200 @ 40MHz */
    T2H = 0xFF;
    AUXR |= 0x10;               /* 启动定时器2 */

    P3M0 |= 0x02;               /* TXD = P3.1 推挽 */
    P3M1 &= ~0x02;
}

/* 一行状态：ADC=原始值 V=节点电压mV light=归一化光强 D=占空比 */
void Report(unsigned int adc, unsigned int light, unsigned char d)
{
    unsigned int mv;

    mv = (unsigned int)(((unsigned long)adc * 3300UL) / 4095UL);

    Uart1_SendString("ADC=");
    Uart1_SendDec16(adc);
    Uart1_SendString("  V=");
    Uart1_SendDec16(mv);
    Uart1_SendString("mV  light=");
    Uart1_SendDec16(light);
    Uart1_SendString("  D=");
    Uart1_SendDec16(d);
    Uart1_SendString("%  ");
    if (light_on)
        Uart1_SendString("ON\r\n");
    else
        Uart1_SendString("OFF\r\n");
}

#endif


/* ===== 设置 D_on（0~100 %）===== */
void SetDuty(unsigned char pct)
{
    unsigned int ccr;

    if (pct > 100)
        pct = 100;

    duty = pct;

    ccr = (unsigned int)pct * CCR_PER_PCT;  /* 100% -> 40000 >= ARR+1 -> 恒高 */
    PWMA_CCR1H = (unsigned char)(ccr >> 8);
    PWMA_CCR1L = (unsigned char)(ccr & 0xFF);
}


/* ===== 硬件 PWM：PWMA 通道1，PWM1P -> P1.0 ===== */
void PWMA_Init(void)
{
    EAXFR = 1;                  /* 访问 PWMA 扩展寄存器必须先置1 */

    P1M0 |= 0x01;               /* P1.0 推挽输出 */
    P1M1 &= ~0x01;

    PWMA_PS = 0x00;             /* C1PS[1:0]=00 -> PWM1P 映射到 P1.0 */

    PWMA_PSCRH = 0x00;          /* 预分频 0，PWM 时钟 40MHz */
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = ARR_H;          /* ARR = 39999 -> 1kHz */
    PWMA_ARRL = ARR_L;

    PWMA_CCMR1 = 0x60;          /* PWM 模式1 */

    PWMA_CCR1H = 0x00;          /* 初始关断 */
    PWMA_CCR1L = 0x00;

    PWMA_CCER1 = 0x01;          /* CC1E=1 开通道1输出，高电平有效 */

    PWMA_BKR = 0x80;            /* MOE=1 使能主输出 */

    PWMA_ENO |= 0x01;           /* ENO1P=1 接到引脚 */

    PWMA_CR1 = 0x01;            /* 启动计数器 */
}


/* ===== ADC 初始化：P1.1 = ADC1 ===== */
void ADC_Init(void)
{
    EAXFR = 1;                  /* ADCTIM 是扩展 SFR，必须先置1 */

    P1M1 |= 0x02;               /* P1.1 高阻输入（模拟输入脚） */
    P1M0 &= ~0x02;

    ADCCFG = 0x2F;              /* RESFMT=1 右对齐 + SPEED=15 -> 1.25MHz */
    ADCTIM = 0x3F;              /* 官方推荐值 */

    ADC_CONTR = 0x80;           /* ADC_POWER = 1，通着但先不启动 */
    delay_ms(1);                /* 等 ADC 电源稳定 */
}


/* ===== 读一次 ADC，返回 0~4095 ===== */
unsigned int ADC_Read(unsigned char ch)
{
    unsigned int timeout = 2000;

    ADC_RES  = 0;
    ADC_RESL = 0;

    ADC_CONTR = (unsigned char)(0x80 | 0x40 | (ch & 0x0F));  /* 电源+启动+通道 */
    /* 官方例程在这里用 NOP(5) 给启动留几个机器周期，这里用空循环代替，避免依赖额外头文件 */
    {
        unsigned char i = 20;
        while (i--);
    }

    while (!(ADC_CONTR & 0x20))     /* 等 ADC_FLAG */
    {
        if (--timeout == 0)         /* 加超时，避免卡死 */
            break;
    }

    ADC_CONTR &= ~0x20;             /* 清 ADC_FLAG */

    /* RESFMT=1 右对齐：ADC_RES 低 4 位是高 4 位，ADC_RESL 是低 8 位 */
    return (unsigned int)(((unsigned int)(ADC_RES & 0x0F) << 8) | ADC_RESL);
}


/* ===== 8 次平均，压掉 ADC 抖动 ===== */
unsigned int ADC_ReadAvg(unsigned char ch)
{
    unsigned char i;
    unsigned int sum = 0;

    for (i = 0; i < 8; i++)
        sum += ADC_Read(ch);

    return sum >> 3;
}


/* ===== 读光强，统一成"越大越亮"的 light ===== */
unsigned int Light_Read(unsigned int *adc_out)
{
    unsigned int adc;

    adc = ADC_ReadAvg(1);       /* P1.1 = ADC1 */
    *adc_out = adc;

#if LDR_TOP
    return adc;                 /* 上臂接法：ADC 越大越亮 */
#else
    return (unsigned int)(4095 - adc);   /* 下臂接法：翻转过来 */
#endif
}


/* ===== 开灯软启动：0% -> 100%，避免高压灯串瞬间浪涌 ===== */
void SoftOn(void)
{
    unsigned char d;
    unsigned int step_ms;

    step_ms = SOFT_ON_MS / 100;
    if (step_ms == 0)
        step_ms = 1;

    for (d = 0; d < 100; d++)
    {
        SetDuty(d);
        delay_ms(step_ms);
    }

    SetDuty(100);
}


void main(void)
{
    unsigned int adc;
    unsigned int light;
#if (MODE == 1)
    unsigned char target;       /* 只有比例调光模式用得到 */
#endif

    PWMA_Init();
    SetDuty(0);
    ADC_Init();

#if USE_UART
    Uart1_Init();
    Uart1_SendString("\r\n[6.6] LDR switch / threshold\r\n");
    Uart1_SendString("ON_ADC=");
    Uart1_SendDec16(LIGHT_ON_ADC);
    Uart1_SendString("  OFF_ADC=");
    Uart1_SendDec16(LIGHT_OFF_ADC);
    Uart1_SendString("\r\n");
#endif

    delay_ms(200);              /* 等光敏分压和 ADC 稳定 */

    while (1)
    {
        light = Light_Read(&adc);

#if (MODE == 2)

        /* ---- 光强标定：灯串保持关断，只报数 ----
           拿这张表去定 LIGHT_ON_ADC / LIGHT_OFF_ADC             */
        light_on = 0;
        SetDuty(0);
        #if USE_UART
        Report(adc, light, duty);
        #endif
        delay_ms(300);

#elif (MODE == 1)

        /* ---- 比例调光：越暗越亮 ----
           light 在 [LIGHT_ON_ADC, LIGHT_OFF_ADC] 之间线性映射到 100%..0%
           超出区间就取边界值                                       */
        if (light <= LIGHT_ON_ADC)
            target = 100;
        else if (light >= LIGHT_OFF_ADC)
            target = 0;
        else
            target = (unsigned char)
                     (((unsigned int)(LIGHT_OFF_ADC - light) * 100)
                      / (LIGHT_OFF_ADC - LIGHT_ON_ADC));

        /* 限速：每次最多变 10%，让亮度平滑过渡而不是跳变 */
        if (target > duty + 10)
            target = (unsigned char)(duty + 10);
        else if ((unsigned int)target + 10 < duty)
            target = (unsigned char)(duty - 10);

        SetDuty(target);

        light_on = (duty > 0);

        #if USE_UART
        Report(adc, light, duty);
        #endif
        delay_ms(SAMPLE_MS);

#else

        /* ---- 光控开关 + 回差（默认）---- */
        if (!light_on && light < LIGHT_ON_ADC)
        {
            /* 暗到阈值以下 -> 开灯 */
            light_on = 1;
            SoftOn();

            #if USE_UART
            Report(adc, light, duty);
            #endif
        }
        else if (light_on && light > LIGHT_OFF_ADC)
        {
            /* 亮到阈值以上 -> 关灯（关断是立刻的，不会浪涌） */
            light_on = 0;
            SetDuty(0);

            #if USE_UART
            Report(adc, light, duty);
            #endif
        }
        else
        {
            /* 在回差死区内 -> 保持原状态，不动作 */
            #if USE_UART
            Report(adc, light, duty);
            #endif
        }

        delay_ms(SAMPLE_MS);

#endif
    }
}

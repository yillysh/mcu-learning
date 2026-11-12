#include "AI8051U.h"

/* ==================== 任务 6.5  升压驱动串联 LED 串 ====================
   ------------------------------------------------------------------
   目标：升压后驱动 6~8 只串联 LED，能安全点亮，并给出【稳定可读】的
         直流工作点，方便逐点记录 升压前/后节点电压 与 电流。

   >>> 默认 MODE = 0 是【静态 100% 直流】<<<
       开关管静态导通，整个回路是纯 DC，万用表读到的是真直流值，
       不跳、可复现、可对比。做 6.5 的记录就用这个模式。

       为什么不用 PWM 状态量？因为 PWM 时升压模块输出被脉冲负载抽动，
       Vout 会跟着脉动；万用表按平均值或真有效值积分，不同表、不同档
       可能给出不一样的数 —— 那种读数写进报告没法复现。
       调光是 6.4 的活，→ 要 PWM 调光对比请改 MODE = 1。

   接线（在 6.4 基础上把灯串换成高压长串，P1.0 那一路不变）：

     VIN(5V) ---> [升压模块 MT3608 / XL6009] ---> VOUT (升压后十几~三十V)
                    (常开，不由单片机控制)              |
                                                        |
              VOUT --[限流电阻 R]-- LED1 ->|-- LED2 ->|-- ... -- LED8 ->|--+
                                                                            |
                                                       三极管 集电极 C (MOS 漏极 D)
     P1.0 --[基极/栅极电阻]-- B (G)                          发射极 E (源极 S) -- 功率地
                                                                            |
                                     MCU 地 -- 单点(星形)连接 --------------+

   ------------------------------------------------------------------
   动手前必须想清楚的四件事（报告里要写）：

   1) 开关管耐压：关断时 VOUT 几乎全部加在它的 C-E (D-S) 两端。
      所以 三极管 Vceo / MOS 的 Vds 必须 > 升压后电压并留裕量。
      低压 S8050 (Vceo=25V) 在 30V 下会击穿，高压串建议直接换 MOS。

   2) 升压模块是【恒压】不是【恒流】：MT3608 / XL6009 这类成品板不能把
      标称电流当 LED 限流用。限流必须另外做：串联限流电阻 / 恒流驱动 /
      受控电源限流，三者至少有一个。

   3) 串联回路电流处处相等：I 只需测一次，但报告要写清测点位置。

   4) 串联总压实测对照：VOUT ≈ ΣVf + I x R_限流 + Vce(sat)/Vds(on)
      R_限流 上的压降随 I 变，所以 R 取值不同 VOUT 也会跟着变。

   ------------------------------------------------------------------
   测量技巧（直接影响报告数据质量，强烈建议照做）：

   1) 别直接把电流表串进回路：电表有内阻(burden voltage)，串进去本身
      就会压低电流、改变工作点。更准的做法是量【限流电阻两端电压 V_R】，
      再算  I = V_R / R。想对比就两种都测一次，写清差异。
   2) 求灯串总压降：ΣVf = VOUT - V_R - Vce(sat)（MOS 用 Vds(on)，通常更小）
   3) 等读数稳定再记：软启动约 2s 结束，建议再等 10~20s 让结温和模块
      输出都稳住，第一次数读数和几分钟后读数一并记下来对比温漂。
   4) 极性别搞错：低边开关 → 栅极/基极高电平导通 → 灯串通电。

   ------------------------------------------------------------------
   PWM 参数（MODE = 1 时才真正用到；硬件 PWMA 通道1 -> PWM1P -> P1.0）：
       f = Fclk / (PSC+1) / (ARR+1) = 40MHz / 1 / 40000 = 1 kHz
       D_high = CCR1 / (ARR+1)，ARR+1 = 40000，故 CCR1 = 占空比% x 400
       写 CCR1 = 40000 (>= ARR+1) 时输出恒为高 -> 真静态直流，不是 99.99%
       （就算芯片把 CCR 钳到 ARR，也只剩 0.0025% 误差，万用表读不出）

   MODE = 0   静态 DC 测量（默认）: 软启动到上限后保持不动，纯直流
   MODE = 1   1kHz PWM 5 档扫描   : 调光/对比用，读数是平均值
   MODE = 2   常关断              : 接线检查 / 上探针时的安全状态

   ---- 静态 DC 工作点记录表（MODE = 0，量完填）-----------------------
    项目          符号        实测值      说明
   升压前输入     Vin                   模块输入，通常就是 5V 轨
   升压后输出     Vout                  模块输出，空载先量一次做对照
   限流电阻压降   V_R                   算电流用这个比串表准
   灯串电流       I = V_R / R
   灯串总压降     ΣVf = Vout - V_R - Vce(sat)
   单灯平均 Vf    ΣVf / n                n = 串联灯数，和单灯规格比对
   ------------------------------------------------------------------
   安全：手部干燥，勿碰裸露功率点；通电前先做短路检查；
         第一轮先把 MAX_DUTY_PCT 调小（如 25）验证极性与限流算得对，
         确认无误再改回 100；静态满档时 LED 是持续满载，注意温升。
   ==================================================================== */


#define MODE            0       /* 0 = 静态DC测量(推荐做6.5记录)
                                   1 = 1kHz PWM 调光扫描(对比用)
                                   2 = 常关断(安全检查)          */

#define SETTLE_MS       500     /* 上电先关断，等升压模块输出稳定再动灯串 */
#define SOFT_START_MS   2000    /* 软启动总时间：0% 缓慢爬到 MAX_DUTY_PCT */
#define LEVEL_MS        8000    /* MODE=1 每档停留 8s，够量几个读数        */

#define MAX_DUTY_PCT    100     /* 占空比上限(安全阀)，首轮试建议先改 25  */

#define ARR_H           0x9C    /* ARR = 39999 -> 周期 40000 -> 1kHz @40MHz */
#define ARR_L           0x3F
#define CCR_PER_PCT     400     /* CCR1 = 占空比% x 400  (40000/100) */


#if (MODE == 1)
/* 5 档占空比，PWM 扫描对比用（单位 %） */
const unsigned char code duty_pct[5] = { 0, 25, 50, 75, 100 };
unsigned char level_idx = 0;    /* 当前档位索引 0~4 */
#endif

unsigned char duty = 0;         /* 当前 D_on，单位 %，0~100 */


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


/* ===== 设置 D_on（0~100 %），统一在这里卡上限 ===== */
void SetDuty(unsigned char pct)
{
    unsigned int ccr;

    if (pct > MAX_DUTY_PCT)         /* 安全阀：任何调用路径都超不过上限 */
        pct = (unsigned char)MAX_DUTY_PCT;

    duty = pct;

    ccr = (unsigned int)pct * CCR_PER_PCT;  /* 100% -> 40000 >= ARR+1 -> 恒高 */
    PWMA_CCR1H = (unsigned char)(ccr >> 8);
    PWMA_CCR1L = (unsigned char)(ccr & 0xFF);
}


/* ===== 硬件 PWM：PWMA 通道1，PWM1P -> P1.0，1kHz ===== */
void PWMA_Init(void)
{
    EAXFR = 1;                  /* 必须先置1，否则 PWMA 扩展寄存器读写无效 */

    P1M0 |= 0x01;               /* P1.0 推挽输出（PWM1P 从此脚出去驱动开关管） */
    P1M1 &= ~0x01;

    PWMA_PS = 0x00;             /* C1PS[1:0]=00 -> PWM1P 映射到 P1.0 */

    PWMA_PSCRH = 0x00;          /* 预分频 = 0，PWM 时钟 = 40MHz */
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = ARR_H;          /* ARR = 39999 -> 周期 40000 -> 1kHz */
    PWMA_ARRL = ARR_L;

    PWMA_CCMR1 = 0x60;          /* OC1M[2:0]=110 -> PWM 模式1 */

    PWMA_CCR1H = 0x00;          /* 初始 D_on = 0%，开关关断 */
    PWMA_CCR1L = 0x00;

    PWMA_CCER1 = 0x01;          /* CC1E(bit0)=1 开通道1输出；CC1P(bit1)=0 高电平有效 */

    PWMA_BKR = 0x80;            /* MOE=1 使能 PWM 主输出（少这句脚上没波形） */

    PWMA_ENO |= 0x01;           /* ENO1P=1 把 PWM1P 接到引脚 */

    PWMA_CR1 = 0x01;            /* CEN=1 启动计数器 */

    /* 注意：启动后 P1.0 归 PWM 外设驱动，程序里不要再写 P10 = x */
}


/* ===== 软启动：占空比从 0% 一小步一小步爬到 MAX_DUTY_PCT =====
   每步加 1%，步间隔 = SOFT_START_MS / MAX_DUTY_PCT（默认 2000/100 = 20ms）
   作用：让灯串电流平缓建立，避免上电瞬间、升压输出未稳时的大电流冲击，
         也能顺便看出有没有接反、有没有过流（亮度该是渐亮而不是一跳到底）*/
void SoftStart(void)
{
    unsigned char d;
    unsigned int  step_ms;

    step_ms = SOFT_START_MS / MAX_DUTY_PCT;
    if (step_ms == 0)               /* 上限设得很小时兜个底 */
        step_ms = 1;

    for (d = 0; d < MAX_DUTY_PCT; d++)
    {
        SetDuty(d);
        delay_ms(step_ms);
    }

    SetDuty((unsigned char)MAX_DUTY_PCT);   /* 补上终点，顺带避开边界回绕 */
}


void main(void)
{
    PWMA_Init();                /* PWM 初始化，但 CCR1 = 0，开关是关断的 */

    SetDuty(0);                 /* 上电瞬间开关必须不导通 */

    delay_ms(SETTLE_MS);        /* 等升压模块输出建立稳定，再去碰灯串 */

#if (MODE == 2)

    /* ---- 常关断：接线检查 / 探针安全调试 ---- */
    while (1)
    {
        ;
    }

#elif (MODE == 1)

    /* ---- 1kHz PWM 5 档扫描：调光对比、亮度和波形观察 ----
       本模式开关管在 1kHz 斩波，万用表读到的 Vout / I 是【平均值】，
       仅用于趋势对比，不要拿来当 6.5 的直流工作点记录           */
    SoftStart();

    while (1)
    {
        SetDuty(duty_pct[level_idx]);

        if (++level_idx >= 5)
            level_idx = 0;

        delay_ms(LEVEL_MS);
    }

#else

    /* ---- 静态 DC 测量（默认）----
       软启动爬到上限后就不再动了：CCR1 停在 40000，输出恒为高电平，
       开关管静态导通，回路是纯直流 —— 这时候量 Vin / Vout / V_R 最准 */
    SoftStart();

    while (1)
    {
        ;                       /* 输出恒定，读数随时量，想量多久量多久 */
    }

#endif
}

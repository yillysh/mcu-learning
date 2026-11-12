#include "AI8051U.h"

// ===== ??PWM?? =====
unsigned char pwm_cnt = 0;      // PWM???,0~99

// ===== ??PWM??????:10µs???? =====
void Timer0_Init(void)
{
    TM0PS = 0;
    TH0 = 0xFF;                 // 10µs @ 40MHz/12
    TL0 = 0xDF;
    ET0 = 1;
    TR0 = 1;
}

// ===== ??PWM???:P1.2, 1kHz, ??50% =====
void PWMA_Init(void)
{
    EAXFR = 1;                  // ??:??XFR??
    
    P1M0 |= 0x04;               // P1.2 ????
    P1M1 &= ~0x04;
    
    PWMA_PS = 0x00;             // PWM2P???P1.2
    PWMA_PSCRH = 0x00;
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = 0x9C;           // ARR = 39999 (1kHz)
    PWMA_ARRL = 0x3F;
    
    PWMA_CCMR2 = 0x60;          // PWM??1,?????
    PWMA_CCR2H = 0x4E;          // CCR2 = 20000 (50%)
    PWMA_CCR2L = 0x20;
    
    PWMA_CCER1 = 0x10;          // ????2??(CC2E?bit4)
    PWMA_BKR = 0x80;            // ?????
    PWMA_ENO |= 0x04;           // ??PWM2P??
    PWMA_CR1 = 0x01;            // ?????
}

void main(void)
{
    // 1. ??P1.0?????(??PWM)
    P1M0 |= 0x01;
    P1M1 &= ~0x01;
    P10 = 0;

    // 2. ??????0(??PWM)
    Timer0_Init();

    // 3. ?????PWMA(??PWM)
    PWMA_Init();

    // 4. ????
    EA = 1;

    while(1)
    {
        // ??50%???,?????
    }
}

// ===== ??PWM??:?10µs????,100?=1ms=1kHz =====
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xFF;
    TL0 = 0xDF;

    pwm_cnt++;
    if (pwm_cnt >= 100)
    {
        pwm_cnt = 0;
    }

    // ??50%???:?50????,?50????
    if (pwm_cnt < 50)
        P10 = 1;
    else
        P10 = 0;
}
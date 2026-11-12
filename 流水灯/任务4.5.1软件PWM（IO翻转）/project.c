#include "AI8051U.h"

unsigned char pwm_cnt = 0;
unsigned int  div_cnt = 0;      // ?????????

void Timer0_Init(void)
{
    TM0PS = 0;
    TH0 = 0xFF;
    TL0 = 0xDF;
    ET0 = 1;
    TR0 = 1;
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xFF;
    TL0 = 0xDF;

    pwm_cnt++;
    if (pwm_cnt >= 100)
        pwm_cnt = 0;

    if (pwm_cnt < 50)
        P10 = 1;
    else
        P10 = 0;
}

void main(void)
{
    // P1.0 ????(PWM)
    P1M0 |= 0x01;  P1M1 &= ~0x01;  P10 = 0;

    // P1.1 ????(IO???CPU)
    P1M0 |= 0x02;  P1M1 &= ~0x02;  P11 = 0;

    Timer0_Init();
    EA = 1;

    while(1)
    {
        if (++div_cnt >= 10000)     // ?10000???????
        {
            div_cnt = 0;
            P11 = !P11;
        }
    }
}
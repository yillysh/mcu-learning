#include "AI8051U.h"

#define MODE        0       /* 0 = ??(????)  1 = ??(????) */
#define BLINK_MS    500

sbit SW = P1^0;              /* ??????? */

bit flag_10ms = 0;
unsigned char cnt_ms = 0;

void Timer0_Init(void)
{
    TM0PS = 0;
    TH0 = 0x7D;  TL0 = 0xCB;     /* 10 ms @ 40MHz/12T */
    ET0 = 1;
    TR0 = 1;
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0x7D;  TL0 = 0xCB;
    flag_10ms = 1;
}

void main(void)
{
    P1M0 = 0x01;  P1M1 = 0x00;   /* P1.0 ???? */
    SW = 0;                      /* ????? */

#if (MODE == 0)
    SW = 1;                      /* ???,???? */
    while (1) { ; }
#else
    Timer0_Init();
    EA = 1;
    while (1)
    {
        if (flag_10ms)
        {
            flag_10ms = 0;
            if (++cnt_ms >= (BLINK_MS / 10))
            {
                cnt_ms = 0;
                SW = !SW;        /* ???? */
            }
        }
    }
#endif
}

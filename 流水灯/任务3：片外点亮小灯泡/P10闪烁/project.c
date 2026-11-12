#include "AI8051U.h"

bit flag_200ms = 0;          // 200ms ??
unsigned char counter = 0;   // ????

// ???0???:10ms ???? @ 40MHz
void Timer0_Init(void)
{
    TM0PS = 0;               // 1??
    // 10ms @ 40MHz/12 ? ?? 33333 ?
    // ?? = 65536 - 33333 = 32203 = 0x7DCB
    TH0 = 0x7D;
    TL0 = 0xCB;
    ET0 = 1;                 // ?????0??
    TR0 = 1;                 // ?????0
}

void main()
{
    // ?? P1.0 ?????
    P1M0 = 0x01;
    P1M1 = 0x00;
    
    P10 = 1;                 // ????(???????,????????0)
    
    Timer0_Init();
    EA = 1;                  // ????

    while (1)
    {
        if (flag_200ms)      // ?200ms????
        {
            flag_200ms = 0;
            P10 = !P10;      // ??P1.0??
        }
    }
}

// ???0??????
void Timer0_ISR(void) interrupt 1
{
    TH0 = 0x7D;              // ????,??10ms??
    TL0 = 0xCB;
    
    counter++;
    if (counter >= 20)       // 20 × 10ms = 200ms
    {
        counter = 0;
        flag_200ms = 1;      // ?200ms??
    }
}
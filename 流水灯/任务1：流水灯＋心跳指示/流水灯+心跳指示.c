/* =====================================================================
 * ??1.5 ??:P2.0 ???? + P2.1~P2.7 ???
 * ??:STC AI8051U-34K64(32???,??40MHz),Keil C251
 *
 * ????:???????"??? + ????"??????,
 *           ????? P2,????????????????
 * ===================================================================== */
#include "AI8051U.h"

#define LED_ON   0      // ??LED?????:0 = ?
#define LED_OFF  1      // 1 = ?

unsigned char tick_cnt = 0;        // ????(1~60)
unsigned char step = 0;            // ????????(0~6,?? P2.1~P2.7)
unsigned char heart_bit = LED_OFF; // P2.0 ????:1=?,0=?
bit flag = 0;                      // ????:??????1,?????

void Timer0_Init(void)
{
    TM0PS = 0;          // 1 ??,40MHz ??
    TH0 = 0x4C;
    TL0 = 0x00;         // ?? 0x4C00,?? ˜ 13.8ms
    ET0 = 1;
    TR0 = 1;
}

void main()
{
    P2M0 = 0xFF;
    P2M1 = 0x00;        // P2 ?????
    Timer0_Init();
    EA = 1;
    while (1)
    {
        if (flag)
        {
            flag = 0;
            tick_cnt++;

            /* ---- 1) ????:??? P2.0 ---- */
            if      (tick_cnt <= 2) heart_bit = LED_ON;   // ?
            else if (tick_cnt <= 3) heart_bit = LED_OFF;  // ??
            else if (tick_cnt <= 5) heart_bit = LED_ON;   // ?
            else                    heart_bit = LED_OFF;  // ???
            if (tick_cnt >= 60) tick_cnt = 0;

            /* ---- 2) ?????:P2.1~P2.7,????? ---- */
            step++;
            if (step >= 7) step = 0;

            /* ---- 3) ???????(????)----
             * ~(0x01<<step):? step ?? 0(????),??? 1
             * <<1:????? bit1~bit7(?? P2.1~P2.7)
             * | heart_bit:? bit0(P2.0 ????)??? */
            P2 = ((~(0x01 << step)) << 1) | heart_bit;
        }
    }
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0x4C;
    TL0 = 0x00;
    flag = 1;
}

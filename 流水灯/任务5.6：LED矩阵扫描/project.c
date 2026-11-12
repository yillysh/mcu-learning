#include "AI8051U.h"

/* ================= 4×4 ???? =================
   ?? P1.0~P1.3:???,????
   ?? P1.4~P1.7:???,??????
   ????:??? = 1,??? = 0
   =============================================== */

#define ROW_MS   5        /* ???? 5ms,4 ??? 20ms ? ????? 50Hz */
#define ROW_NUM  4

/* ????:???? 4 ? bit0~bit3 ?? ?0~?3,1 = ??
   ???????:  1111
                     1001
                     1001
                     1111                            */
const unsigned char code disp[ROW_NUM] = {
    0x0F, 0x09, 0x09, 0x0F
};

/* ???? disp[] ??,??:
   ??      { 0x0F, 0x0F, 0x0F, 0x0F }
   ??      { 0x01, 0x02, 0x04, 0x08 }
   ??      { 0x06, 0x06, 0x06, 0x06 }
   ??      { 0x05, 0x0A, 0x05, 0x0A }   */

bit flag_1ms = 0;
unsigned char cnt_ms  = 0;
unsigned char row_now = 0;

void Timer0_Init(void)
{
    TM0PS = 0;
    TH0 = 0xF2;  TL0 = 0xFB;      /* 1ms @ 40MHz/12 */
    ET0 = 1;  TR0 = 1;
}

void main(void)
{
    unsigned char out;

    P1M0 = 0xFF;                  /* P1 ?????? */
    P1M1 = 0x00;
    P1   = 0x00;                  /* ???? ? ?? */

    Timer0_Init();
    EA = 1;

    while (1)
    {
        if (flag_1ms)
        {
            flag_1ms = 0;

            if (++cnt_ms >= ROW_MS)
            {
                cnt_ms = 0;

                /* ? ??:disp ? 1 = ??,????????,??????? 4 ? */
                out  = (unsigned char)((~disp[row_now]) & 0x0F) << 4;

                /* ? ??:????????? */
                out |= (1 << row_now);

                /* ? ?? P1 ????? —— ???????? 1µs,????? */
                P1 = out;

                /* ? ????? */
                row_now++;
                if (row_now >= ROW_NUM) row_now = 0;
            }
        }
    }
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xF2;  TL0 = 0xFB;
    flag_1ms = 1;
}

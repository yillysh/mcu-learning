#include "AI8051U.h"

/* ===================== ?? 5.4  74HC595 ???? 8 ? =====================
   ????:
     DS   (14?) ? P2.0    ??????
     SHCP (11?) ? P2.1    ????
     STCP (12?) ? P2.2    ????
     OE   (13?) ? GND     ????,???,????
     MR   (10?) ? VCC     ???,???,????
     VCC  (16?) ? 5V ,GND (8?) ? GND

   LED ??:Q0~Q7 ? LED ?? ? LED ?? ? 1kO ? GND
             (595 ???????,??? 1 ??? 0 ?)

   ??:?1? Q7'(9?) ? ?2? DS(14?),SHCP/STCP ??????
   ========================================================================== */

sbit DS   = P2^0;
sbit SHCP = P2^1;
sbit STCP = P2^2;

#define CHIP_NUM   1        /* ????:1 ? = 8 ?,2 ? = 16 ?,3 ? = 24 ? */
#define SPEED_MS   120      /* ????????? */

unsigned char led_buf[CHIP_NUM];   /* led_buf[0] ??????????? 8 ? */

volatile unsigned int ms_cnt = 0;

/* ---------------------------- 1ms ?? ---------------------------- */
void Timer0_Init(void)
{
    TM0PS = 0;
    TH0 = 0xF2;  TL0 = 0xFB;       /* 1ms @ 40MHz/12 */
    ET0 = 1;  TR0 = 1;
}

void Timer0_ISR(void) interrupt 1
{
    TH0 = 0xF2;  TL0 = 0xFB;
    ms_cnt++;
}

void Delay_ms(unsigned int n)
{
    ms_cnt = 0;
    while (ms_cnt < n) { ; }
}

/* ------------------ ? led_buf[] ???? 595 ------------------ */
void HC595_Send(void)
{
    unsigned char i, j, dat;

    /* ??????,??????? —— ?????? */
    for (i = CHIP_NUM; i > 0; i--)
    {
        dat = led_buf[i - 1];

        for (j = 0; j < 8; j++)
        {
            if (dat & 0x80)  DS = 1;    /* ?????,???? Q7 */
            else             DS = 0;

            SHCP = 0;
            SHCP = 1;                   /* ??? ? ???? */

            dat <<= 1;
        }
    }

    STCP = 0;
    STCP = 1;                           /* ??? ? 8 ???????? */
}

/* ------------------------------ ??? ------------------------------ */
void main(void)
{
    unsigned char i;
    unsigned int  total = CHIP_NUM * 8;
    unsigned int  pos   = 0;

    P2M0 = 0x07;                        /* P2.0~P2.2 ???? */
    P2M1 = 0x00;
    DS = 0;  SHCP = 0;  STCP = 0;

    for (i = 0; i < CHIP_NUM; i++)  led_buf[i] = 0x00;
    HC595_Send();                       /* ??????? */

    Timer0_Init();
    EA = 1;

    while (1)
    {
        for (i = 0; i < CHIP_NUM; i++)  led_buf[i] = 0x00;   /* ??? */
        led_buf[pos / 8] = (1 << (pos % 8));                 /* ???? pos ? */

        HC595_Send();

        Delay_ms(SPEED_MS);

        pos++;
        if (pos >= total)  pos = 0;
    }
}

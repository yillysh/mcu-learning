#include "AI8051U.h"

// ===== ???? =====
unsigned char display_value = 0;    // ????? (0~255)
bit flag_200ms = 0;                 // 200ms ?? (??LED)
bit flag_10ms  = 0;                 // 10ms ?? (????)

// ????
unsigned char key1_press_count = 0; // ??1(P3.2) ???? (??:10ms)
unsigned char key2_press_count = 0; // ??2(P3.3) ???? (??:10ms)
bit key1_was_pressed = 0;           // ??1 ??????????
bit key2_was_pressed = 0;           // ??2 ??????????

// ===== ??1 ???? =====
void Uart1_SendByte(unsigned char dat)
{
    SBUF = dat;
    while (!TI);        // ??????
    TI = 0;             // ????????
}

void Uart1_SendString(unsigned char *str)
{
    while (*str)
        Uart1_SendByte(*str++);
}

// ???? 0~255 ?????
void Uart1_SendDec(unsigned char val)
{
    unsigned char buf[4];
    unsigned char i = 0;

    if (val == 0)
    {
        Uart1_SendByte('0');
        return;
    }
    while (val)
    {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i)
        Uart1_SendByte(buf[--i]);
}

// 115200bps @ 40MHz (???2???????, 1T??)
void Uart1_Init(void)
{
    SCON = 0x50;        // 8???, ?????, ????
    AUXR |= 0x01;       // ??1?????2???????
    AUXR |= 0x04;       // ???2?? = Fosc (1T)
    T2L = 0xA9;         // ?????? (115200 @ 40MHz)
    T2H = 0xFF;         // ??????
    AUXR |= 0x10;       // ?????2

    P3M0 |= 0x02;       // TXD = P3.1 ????
    P3M1 &= ~0x02;
}

// ===== ???0??? (10ms ??) =====
void Timer0_Init(void)
{
    TM0PS = 0;          // ??? = 0 (1??)
    // 10ms @ 40MHz/12: ??? = 65536 - 33333 = 32203 = 0x7DCB
    TH0 = 0x7D;
    TL0 = 0xCB;
    ET0 = 1;            // ?????0??
    TR0 = 1;            // ?????0
}

// ===== ???? (?10ms????) =====
void Key_Scan(void)
{
    bit key1_pressed, key2_pressed;

    key1_pressed = !P32;    // ??1: P3.2 (???=??)
    key2_pressed = !P33;    // ??2: P3.3

    // ===== ??1 (+) =====
    if (key1_pressed)
    {
        key1_press_count++;

        // ??: ??20ms(2?10ms)????
        if (key1_press_count == 2 && !key1_was_pressed)
        {
            display_value++;
            key1_was_pressed = 1;

            Uart1_SendString("KEY+ : ");
            Uart1_SendDec(display_value);
            Uart1_SendString("\r\n");
        }

        // ??: 500ms(50?10ms)??100ms????
        if (key1_press_count >= 50)
        {
            if ((key1_press_count - 50) % 10 == 0)
            {
                display_value++;

                Uart1_SendString("KEY+ : ");
                Uart1_SendDec(display_value);
                Uart1_SendString("\r\n");
            }
        }
    }
    else
    {
        key1_press_count = 0;
        key1_was_pressed = 0;
    }

    // ===== ??2 (-) =====
    if (key2_pressed)
    {
        key2_press_count++;

        if (key2_press_count == 2 && !key2_was_pressed)
        {
            display_value--;
            key2_was_pressed = 1;

            Uart1_SendString("KEY- : ");
            Uart1_SendDec(display_value);
            Uart1_SendString("\r\n");
        }

        if (key2_press_count >= 50)
        {
            if ((key2_press_count - 50) % 10 == 0)
            {
                display_value--;

                Uart1_SendString("KEY- : ");
                Uart1_SendDec(display_value);
                Uart1_SendString("\r\n");
            }
        }
    }
    else
    {
        key2_press_count = 0;
        key2_was_pressed = 0;
    }
}

// ===== ??? =====
void main()
{
    // P2 ???????? (8?LED)
    P2M0 = 0xFF;
    P2M1 = 0x00;

    // P3.2 / P3.3 ????? (????)
    P3M0 = 0x00;
    P3M1 = 0x00;

    Timer0_Init();
    Uart1_Init();
    EA = 1;             // ????

    while (1)
    {
        // ?10ms??????
        if (flag_10ms)
        {
            flag_10ms = 0;
            Key_Scan();
        }

        // ?200ms????LED??
        if (flag_200ms)
        {
            flag_200ms = 0;
            P2 = ~display_value;    // ?????
        }
    }
}

// ===== ???0?????? (10ms) =====
void Timer0_ISR(void) interrupt 1
{
    static unsigned char counter_200ms = 0;

    // ????
    TH0 = 0x7D;
    TL0 = 0xCB;

    // ?10ms??
    flag_10ms = 1;

    // 200ms: 20 × 10ms = 200ms
    counter_200ms++;
    if (counter_200ms >= 20)
    {
        counter_200ms = 0;
        flag_200ms = 1;
    }
}
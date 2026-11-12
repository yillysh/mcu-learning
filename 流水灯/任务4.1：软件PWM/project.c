#include "AI8051U.h"

// ===== ???? =====
unsigned char pwm_cnt = 0;      // PWM???,0~99
unsigned char duty = 0;         // ???,0~100(??0%~100%)
bit breath_flag = 0;            // ??????,?1ms?1
bit direction = 0;              // 0=??,1=??
unsigned char breath_cnt = 0;   // ?????,?20ms????duty

// ===== ???0???:10µs???? =====
void Timer0_Init(void)
{
    TM0PS = 0;                  // ???0(?????12??????)
    // 40MHz/12 ˜ 3.333MHz,????0.3µs
    // 10µs / 0.3µs = 33.33,?33?
    // ?? = 65536 - 33 = 65503 = 0xFFDF
    TH0 = 0xFF;
    TL0 = 0xDF;
    ET0 = 1;                    // ?????0??
    TR0 = 1;                    // ?????0
}

// ===== ??? =====
void main()
{
    // ??P1.0?????
    P1M0 = 0x01;
    P1M1 = 0x00;
    
    P10 = 0;                    // ????
    
    Timer0_Init();
    EA = 1;                     // ????

    while(1)
    {
        if (breath_flag)        // ?1ms????
        {
            breath_flag = 0;
            breath_cnt++;
            
            if (breath_cnt >= 20)   // 20ms???????
            {
                breath_cnt = 0;
                
                if (direction == 0) // ????
                {
                    duty++;
                    if (duty >= 100)
                    {
                        duty = 100;
                        direction = 1;  // ?????
                    }
                }
                else                // ????
                {
                    duty--;
                    if (duty == 0)
                    {
                        duty = 0;
                        direction = 0;  // ?????
                    }
                }
            }
        }
    }
}

// ===== ???0??????:?10µs???? =====
void Timer0_ISR(void) interrupt 1
{
    // ????
    TH0 = 0xFF;
    TL0 = 0xDF;
    
    pwm_cnt++;
    if (pwm_cnt >= 100)         // 100??? = 1ms
    {
        pwm_cnt = 0;
        breath_flag = 1;        // ?????,1ms???
    }
    
    // PWM??:?????
    if (pwm_cnt < duty)
        P10 = 1;                // ???,LED?
    else
        P10 = 0;                // ???,LED?
}
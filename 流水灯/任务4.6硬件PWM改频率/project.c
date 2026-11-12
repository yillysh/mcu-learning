#include "AI8051U.h"

// ????:1=100Hz, 2=1kHz, 3=10kHz
#define FREQ_SEL 1

void PWMA_Init(void)
{
    EAXFR = 1;                  // ?????????

    // P1.2 ????
    P1M0 |= 0x04;
    P1M1 &= ~0x04;

    PWMA_PS = 0x00;             // PWM2P ??? P1.2

    // ===== ??????? =====
#if FREQ_SEL == 1
    // 100Hz:???=9(10??),ARR=39999
    PWMA_PSCRH = 0x00;
    PWMA_PSCRL = 0x09;
    PWMA_ARRH = 0x9C;           // 39999 ?8?
    PWMA_ARRL = 0x3F;           // 39999 ?8?
    PWMA_CCR2H = 0x4E;          // 50% -> 20000
    PWMA_CCR2L = 0x20;

#elif FREQ_SEL == 2
    // 1kHz:???=0,ARR=39999
    PWMA_PSCRH = 0x00;
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = 0x9C;
    PWMA_ARRL = 0x3F;
    PWMA_CCR2H = 0x4E;
    PWMA_CCR2L = 0x20;

#elif FREQ_SEL == 3
    // 10kHz:???=0,ARR=3999
    PWMA_PSCRH = 0x00;
    PWMA_PSCRL = 0x00;
    PWMA_ARRH = 0x0F;           // 3999 ?8?
    PWMA_ARRL = 0x9F;           // 3999 ?8?
    PWMA_CCR2H = 0x07;          // 50% -> 2000
    PWMA_CCR2L = 0xD0;
#endif

    PWMA_CCMR2 = 0x60;          // PWM??1,?????
    PWMA_CCER1 = 0x10;          // ????2??
    PWMA_BKR = 0x80;            // ?????
    PWMA_ENO |= 0x04;           // ?? PWM2P ??
    PWMA_CR1 = 0x01;            // ?????
}

void main(void)
{
    PWMA_Init();
    while(1);
}
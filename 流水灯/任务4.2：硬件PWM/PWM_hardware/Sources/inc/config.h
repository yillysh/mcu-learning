//<<AICUBE_USER_HEADER_REMARK_BEGIN>>
////////////////////////////////////////
// 在此添加用户文件头说明信息  
// 文件名称: config.h
////////////////////////////////////////
//<<AICUBE_USER_HEADER_REMARK_END>>


#ifndef __CONFIG_H__
#define __CONFIG_H__


//<<AICUBE_USER_DEFINE_BEGIN>>
// 在此添加用户宏定义  
//<<AICUBE_USER_DEFINE_END>>


#define __ENCODING              "GB2312" //DO NOT DELETE or MODIFY


#define HIRC                    40000000UL
#define FOSC                    40000000UL
#define SYSCLK                  FOSC
#define MAIN_Fosc               FOSC

#include <ai8051u.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <intrins.h>

#include "def.h"
#include "ai8051u_def.h"


//<<AICUBE_USER_INCLUDE_BEGIN>>
// 在此添加用户头文件包含  
//<<AICUBE_USER_INCLUDE_END>>



void PORT1_Init(void);
#define PWMA_CH1                0x11
#define PWMA_CH2                0x12
#define PWMA_CH3                0x13
#define PWMA_CH4                0x14
#define PWMB_CH5                0x25
#define PWMB_CH6                0x26
#define PWMB_CH7                0x27
#define PWMB_CH8                0x28

#define IS_PWMA(ch)             (((ch) >= PWMA_CH1) && ((ch) <= PWMA_CH4))
#define IS_PWMB(ch)             (((ch) >= PWMB_CH5) && ((ch) <= PWMB_CH8))

#define PWM_MODE1               0x06
#define PWM_MODE2               0x07

void PWMA_Init(void);

void PWM_UpdateDuty(uint8_t PWMx, uint16_t nCompare);
void HSPWM_UpdateDuty(uint8_t PWMx, uint16_t nCompare);
uint16_t PWM_ReadCapture(uint8_t PWMx);
uint16_t HSPWM_ReadCapture(uint8_t PWMx);

uint8_t HSPWMA_ReadReg(uint8_t addr);
uint8_t HSPWMB_ReadReg(uint8_t addr);
void HSPWMA_WriteReg(uint8_t addr, uint8_t dat);
void HSPWMB_WriteReg(uint8_t addr, uint8_t dat);


void SYS_Init(void);
void delay_us(uint16_t us);
void delay_ms(uint16_t ms);



//<<AICUBE_USER_EXTERNAL_DECLARE_BEGIN>>
// 在此添加用户外部函数和外部变量声明  
//<<AICUBE_USER_EXTERNAL_DECLARE_END>>


#endif

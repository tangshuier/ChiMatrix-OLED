#ifndef _LED_H_
#define _LED_H_

#include "../Start/stm32f10x.h"

/************************** LED灯 连接引脚定义********************************/
#define LED_PIN 						GPIO_Pin_0
#define LED_GPIO						GPIOA
#define LED_GPIO_Clock 			RCC_APB2Periph_GPIOA
/************************** LED灯 连接引脚定义********************************/

 void LED_Config(void);
 void LED_ON(void);
void LED_OFF(void);
void LED_Turn(void);
#endif


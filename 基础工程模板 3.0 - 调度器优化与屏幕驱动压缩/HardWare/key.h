#ifndef __KEY_H
#define __KEY_H

#include "../Start/stm32f10x.h"

/************************** 按键 连接引脚定义********************************/
#define KEY1_GPIO_PORT     GPIOA
#define KEY1_GPIO_CLK      RCC_APB2Periph_GPIOA
#define KEY1_GPIO_PIN      GPIO_Pin_15

#define KEY2_GPIO_PORT     GPIOB
#define KEY2_GPIO_CLK      RCC_APB2Periph_GPIOB
#define KEY2_GPIO_PIN      GPIO_Pin_3

#define KEY3_GPIO_PORT     GPIOB
#define KEY3_GPIO_CLK      RCC_APB2Periph_GPIOB
#define KEY3_GPIO_PIN      GPIO_Pin_4

#define KEY4_GPIO_PORT     GPIOB
#define KEY4_GPIO_CLK      RCC_APB2Periph_GPIOB
#define KEY4_GPIO_PIN      GPIO_Pin_5
/************************** 按键 连接引脚定义********************************/

void Key_Init(void);
uint8_t Get_Key(void);

#endif

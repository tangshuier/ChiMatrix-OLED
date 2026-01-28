#include "./key.h"
#include "../System/Delay.h"

void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(KEY1_GPIO_CLK | KEY2_GPIO_CLK | KEY3_GPIO_CLK | KEY4_GPIO_CLK, ENABLE);
    
		GPIO_InitTypeDef GPIO_InitStructure;
	
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_InitStructure.GPIO_Pin = KEY1_GPIO_PIN;
    GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = KEY2_GPIO_PIN;
    GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure);
	
		GPIO_InitStructure.GPIO_Pin = KEY3_GPIO_PIN;
    GPIO_Init(KEY3_GPIO_PORT, &GPIO_InitStructure);
		
		GPIO_InitStructure.GPIO_Pin = KEY4_GPIO_PIN;
    GPIO_Init(KEY4_GPIO_PORT, &GPIO_InitStructure);
}

uint8_t Get_Key(void)
{
    uint8_t KeyNum = 0;
    if (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == 0);
        Delay_ms(20);
        KeyNum = 1;
    }
    else if (GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == 0);
        Delay_ms(20);
        KeyNum = 2;
    }
    else if (GPIO_ReadInputDataBit(KEY3_GPIO_PORT, KEY3_GPIO_PIN) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(KEY3_GPIO_PORT, KEY3_GPIO_PIN) == 0);
        Delay_ms(20);
        KeyNum = 3;
    }
    else if (GPIO_ReadInputDataBit(KEY4_GPIO_PORT, KEY4_GPIO_PIN) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(KEY4_GPIO_PORT, KEY4_GPIO_PIN) == 0);
        Delay_ms(20);
        KeyNum = 4;
    }
    return KeyNum;
}

#include "./LED.h"

 void LED_Config(void)
 {
	 RCC_APB2PeriphClockCmd(LED_GPIO_Clock,ENABLE);
	 
	GPIO_InitTypeDef GPIO_InitStructure;
	 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	 GPIO_InitStructure.GPIO_Pin = LED_PIN;
	 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	 
	 GPIO_Init(LED_GPIO,&GPIO_InitStructure);
	 LED_OFF();
 }
 
 void LED_ON(void)
{
	GPIO_ResetBits(LED_GPIO,LED_PIN);
}

void LED_OFF(void)
{
	GPIO_SetBits(LED_GPIO,LED_PIN);
}
void LED_Turn(void)
{
	if(GPIO_ReadInputDataBit(LED_GPIO,LED_PIN)==0)
	{
		GPIO_SetBits(LED_GPIO,LED_PIN);
	}
	else
	{
		GPIO_ResetBits(LED_GPIO,LED_PIN);
	}
}

#ifndef _MAIN_H_
#define _MAIN_H_

#include "../Start/stm32f10x.h"
#include "math.h"
#include "../HardWare/OLED_Data.h"
#include "../Library/stm32f10x_iwdg.h"
/**********�����ṹ��************/
typedef struct 
{
	float Temp;
	float Humi;
} var;
/*******************************/

/**********��ֵ�ṹ��************/
typedef struct 
{
	float Temp;
	float Humi;
} thresh;
/*******************************/

/**********��־λ�ṹ��**********/
typedef struct 
{
	u8 i;
	u8 mode;
	u8 key;
	int x;
	int y;
	int z;
} flg;
/*******************************/

typedef struct 
{
	var Variable;       // Variable ���͵ĳ�Ա������Ϊ var
	thresh Threshold;   // Threshold ���͵ĳ�Ա������Ϊ thresh
	flg flag;           // flag ���͵ĳ�Ա������Ϊ flg
} dt;

extern dt data;

void Task_Initialization(void);
/**********ģ�����ݽṹ��**********/
// �����ﶨ���ģ����Ҫʹ�õ����ݽṹ
// ���磺
// typedef struct {
//     float temperature;
//     float humidity;
// } Module_DHT11_Data;
/**********************************/

// ����ģ�����ݣ������Ҫ��
// ���磺extern Module_DHT11_Data dht11_data;
#endif

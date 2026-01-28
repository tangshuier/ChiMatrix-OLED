#include "stm32f10x.h"                  // Device header
#include "OLED.h"
#include "CPUUtil.h"
#include "main.h"
#include "task.h"
#include "Delay.h"				//延时函数头文件
#include "oled.h"					//OLED屏头文件
#include "key.h"					//按键头文件
#include "LED.h"					//LED灯头文件
#include "MemPool.h"
dt data;
//	模块结构体声明
//	
//	模块结构体声明

// 
// 任务ID存储（全局变量）
TaskID keyTaskID, dataTaskID, alarmTaskID, showTaskID, IWDG_ID, I_ID;

float CPU = 0;

void GetData(void){
	LED_Turn();
}
/**
  * @brief 数据显示逻辑 - 简洁版
  * 使用协程宏实现非阻塞延时
  */
void ShowData(void)
{
	// 这里是延时前的代码，会正常执行一次
	OLED_UpdateAsync();
	OLED_Clear();
	
	// 显示系统信息
	OLED_ShowImage(0,0,8,8,wifi_int);
	
}

// 按键扫描逻辑
void Menu_key_set(void)									//按键扫描
{
	data.flag.key = Get_Key();
	
    switch(data.flag.key){
        case 1:
            
            break;
        case 2:
			
            break;
        case 3:
           
            break;
        default:
            break;
    }
}
// 报警处理逻辑
void Alarm(void)
{
//	if(data.flag.z == 4){
//		LED_Turn();
//	}
}

void IWDGADD(void){
	data.flag.y = data.flag.x;
	data.flag.x=0;
	//IWDG_ReloadCounter();
}

/**
 * @brief 任务初始化
 * 
 * 添加系统需要的所有任务并保存任务ID
 */

void Task_Initialization(void){
	keyTaskID = Task_Add(Menu_key_set, 10, "KeyScan");
    //dataTaskID = Task_Add(GetData, 100, "GetData"); // 数据获取任务
    //alarmTaskID = Task_Add(Alarm, 100, "Alarm");
    showTaskID = Task_Add(ShowData, 50, "ShowData"); // 显示任务
	IWDG_ID = Task_Add(IWDGADD, 1000, "IWDG_ID");
}

void Data_Init(void){
	data.flag.mode = 1;
	data.flag.z = 50;
//	变量初始化
}

/**
 * @brief 系统初始化
 * 
 * 1. 硬件初始化
 * 2. 任务系统初始化
 * 3. 添加任务
 * @brief 模块初始化总函数
 * 在这里调用所有已添加模块的初始化函数
 */
void Initialization(void){
	SystemInit();  
	Delay_Init();					//滴答定时器初始化
	OLED_Init();					//OLED屏初始化
	LED_Config();
	Key_Init();						//按键初始化	
	
//	模块初始化调用
//
//	模块初始化调用结束
	
	Data_Init();
	MemPool_Init();
	Task_Init();
	Task_Initialization();
	
	/*IWDG初始化*/
	// IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);	//独立看门狗写使能
	// IWDG_SetPrescaler(IWDG_Prescaler_32);			//设置预分频为16
	// IWDG_SetReload(2499);							//设置重装值为2499，独立看门狗的超时时间为1000ms
	// IWDG_ReloadCounter();							//重装计数器，喂狗
	// IWDG_Enable();									//独立看门狗使能
}

int main(void)
{	
	Initialization();
	
	uint32_t start_time = Task_GetSystemTime();
	
	while(1)
	{		
        //ShowData();
		data.flag.x++;
		Task_RunScheduler();
	}
}

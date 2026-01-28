#include "OLED.h"
#include "stdint.h"
#include "stdbool.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
// 双缓冲区实现
#define OLED_DOUBLE_BUFFER 1

// 添加清晰的缓冲区状态枚举
typedef enum {
    BUFFER_A = 0,
    BUFFER_B = 1
} BufferSelect;

// 两个显存缓冲区
static uint8_t OLED_GRAM1[OLED_PAGE_COUNT][OLED_COLUMN_COUNT] = {0};
static uint8_t OLED_GRAM2[OLED_PAGE_COUNT][OLED_COLUMN_COUNT] = {0};

// 当前活动缓冲区指针
static uint8_t (*OLED_GRAM)[OLED_COLUMN_COUNT] = OLED_GRAM1;

// 当前显示缓冲区指针 - 初始与活动缓冲区指向同一区域
// 确保第一次传输时显示缓冲区有有效的数据
static uint8_t (*OLED_DISPLAY_GRAM)[OLED_COLUMN_COUNT] = OLED_GRAM1;

// 传输状态标志
volatile uint8_t OLED_DMA_TransferBusy = 0;
volatile uint8_t OLED_DMA_TransferComplete = 0;

// 全局缓冲区状态
static BufferSelect active_buffer = BUFFER_A;    // 当前绘制缓冲区
static BufferSelect display_buffer = BUFFER_A;   // 当前显示缓冲区

// 根据选择的I2C类型包含不同的实现
#if OLED_I2C_TYPE == 1
// OLED地址和超时定义
#define OLED_ADDRESS 0x78
#define I2C_TIMEOUT 10000

// 函数声明
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_WaitEvent(uint32_t event);
void I2C_SendAddress(uint8_t address, uint8_t direction);
void I2C_SendByte(uint8_t data);
void I2C_SendBytes(const uint8_t* data, uint16_t length);
static uint8_t OLED_StartDMATransferPage(uint8_t page, uint8_t buffer_select);

// I2C起始信号
void I2C_Start(void)
{
    I2C_GenerateSTART(OLED_IIC, ENABLE);
    if (!I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
        // 错误处理
    }
}

// I2C停止信号
void I2C_Stop(void)
{
    I2C_GenerateSTOP(OLED_IIC, ENABLE);
}

// 等待I2C事件发生,带超时功能
uint8_t I2C_WaitEvent(uint32_t event)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!I2C_CheckEvent(OLED_IIC, event)) {
        if (--timeout == 0) {
            return 0; // 超时返回0
        }
    }
    return 1; // 成功返回1
}

// 发送I2C地址
void I2C_SendAddress(uint8_t address, uint8_t direction)
{
    I2C_Send7bitAddress(OLED_IIC, address, direction);
    if (direction == I2C_Direction_Transmitter) {
        if (!I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
            // 错误处理
            I2C_InitTypeDef I2C_InitStructure;
            RCC_APB1PeriphClockCmd(OLED_IIC_Clock, ENABLE);
            I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
            I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
            I2C_InitStructure.I2C_OwnAddress1 = 0x00;
            I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
            I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
            I2C_InitStructure.I2C_ClockSpeed = 800000;
            I2C_Init(OLED_IIC, &I2C_InitStructure);
            I2C_Cmd(OLED_IIC, ENABLE);
        }
    } else {
        // 接收模式处理
    }
}

// 发送单个字节
void I2C_SendByte(uint8_t data)
{
    I2C_SendData(OLED_IIC, data);
    if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        // 错误处理
    }
}

// 批量发送字节
void I2C_SendBytes(const uint8_t* data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        I2C_SendData(OLED_IIC, data[i]);
        if (i == length - 1) {
            if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
                break;
            }
        } else {
            if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
                break;
            }
        }
    }
}

// 写入I2C命令
void Write_IIC_Command(unsigned char I2C_Command)
{
    uint8_t command[] = {0x00, I2C_Command};
    uint32_t timeout = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(OLED_IIC, I2C_FLAG_BUSY)) {
        if (--timeout == 0) return;
    }
    I2C_Start();
    I2C_SendAddress(OLED_ADDRESS, I2C_Direction_Transmitter);
    I2C_SendBytes(command, 2);
    I2C_Stop();
}

// 写入I2C数据
void Write_IIC_Data(const uint8_t *Data, uint8_t Len)
{
    uint8_t buffer[256];
    buffer[0] = 0x40;
    for (int i = 0; i < Len; i++) {
        buffer[i + 1] = Data[i];
    }
    uint32_t timeout = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(OLED_IIC, I2C_FLAG_BUSY)) {
        if (--timeout == 0) return;
    }
    I2C_Start();
    I2C_SendAddress(OLED_ADDRESS, I2C_Direction_Transmitter);
    I2C_SendBytes(buffer, Len + 1);
    I2C_Stop();
}

// 设置OLED显示位置
void OLED_Set_Pos(unsigned char x, unsigned char y)
{
    Write_IIC_Command(0xb0 | y);
    Write_IIC_Command(((x & 0xf0) >> 4) | 0x10);
    Write_IIC_Command(0x00 | (x & 0x0f));
}

#else
/* 软件I2C实现 */

// 软件I2C函数
void OLED_W_SCL(uint8_t BitValue);
void OLED_W_SDA(uint8_t BitValue);
void OLED_I2C_Start(void);
void OLED_I2C_Stop(void);
void OLED_I2C_SendByte(uint8_t Byte);

// OLED写SCL高低电平
void OLED_W_SCL(uint8_t BitValue)
{
    GPIO_WriteBit(OLED_GPIO, OLED_SCL_PIN, (BitAction)BitValue);
}

// OLED写SDA高低电平
void OLED_W_SDA(uint8_t BitValue)
{
    GPIO_WriteBit(OLED_GPIO, OLED_SDA_PIN, (BitAction)BitValue);
}

// I2C起始
void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_W_SDA(0);
    OLED_W_SCL(0);
}

// I2C终止
void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

// I2C发送一个字节
void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_W_SCL(1);
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1);
    OLED_W_SCL(0);
}

// 写入I2C命令
void Write_IIC_Command(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);    // OLED地址
    OLED_I2C_SendByte(0x00);    // 命令标识
    OLED_I2C_SendByte(Command); // 命令值
    OLED_I2C_Stop();
}

// 写入I2C数据
void Write_IIC_Data(const uint8_t *Data, uint8_t Len)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);    // OLED地址
    OLED_I2C_SendByte(0x40);    // 数据标识
    
    for (uint8_t i = 0; i < Len; i++) {
        OLED_I2C_SendByte(Data[i]); // 发送数据
    }
    
    OLED_I2C_Stop();
}

// 设置OLED显示位置
void OLED_Set_Pos(uint8_t x, uint8_t y)
{
    Write_IIC_Command(0xB0 | y);          // 设置页地址
    Write_IIC_Command(0x10 | (x >> 4));   // 设置列地址高4位
    Write_IIC_Command(0x00 | (x & 0x0F)); // 设置列地址低4位
}

#endif

// 写入数据或命令到OLED
void OLED_WR_Byte(uint8_t dat, uint8_t cmd)
{
    if (cmd == OLED_CMD) {
        Write_IIC_Command(dat);
    } else {
        uint8_t data[1] = {dat};
        Write_IIC_Data(data, 1);
    }
}

#if OLED_USE_DMA

// 定义DMA相关资源(根据实际硬件修改)
#define OLED_DMA                 DMA1
#define OLED_DMA_CHANNEL         DMA1_Channel6
#define OLED_DMA_CLOCK           RCC_AHBPeriph_DMA1
#define OLED_DMA_TC_FLAG         DMA1_FLAG_TC6
#define OLED_DMA_IRQn            DMA1_Channel6_IRQn
#define OLED_DMA_IRQHandler      DMA1_Channel6_IRQHandler

// 当前传输的页面和缓冲区
volatile uint8_t current_dma_page = 0;
volatile uint8_t current_dma_buffer = 0;

// DMA传输时间记录
volatile uint32_t dma_transfer_start_time = 0;  // DMA传输开始时间
volatile uint32_t dma_transfer_end_time = 0;    // DMA传输结束时间
volatile float dma_last_transfer_time = 0.0f;   // 上次DMA传输用时(ms)

// DMA中断处理函数 - 优化版本
void OLED_DMA_IRQHandler(void)
{
    if (DMA_GetITStatus(OLED_DMA_TC_FLAG))
    {
        DMA_ClearITPendingBit(OLED_DMA_TC_FLAG);
        
        // 停止I2C传输
        I2C_GenerateSTOP(OLED_IIC, ENABLE);
        
        // 传输完成标志
        OLED_DMA_TransferComplete = 1;
        
        // 禁用DMA通道
        DMA_Cmd(OLED_DMA_CHANNEL, DISABLE);
        I2C_DMACmd(OLED_IIC, DISABLE);
        
        // 处理下一页
        current_dma_page++;
        if (current_dma_page < OLED_PAGE_COUNT)
        {
            // 自动传输下一页
            OLED_StartDMATransferPage(current_dma_page, current_dma_buffer);
        }
        else
        {
            // 所有页面传输完成
            extern volatile uint32_t system_time;  // 从Task.c导入系统时间变量
            dma_transfer_end_time = system_time;  // 记录结束时间
            // 计算差值并转换为两位小数的浮点值
            dma_last_transfer_time = (dma_transfer_end_time - dma_transfer_start_time) / 100.0f;
            dma_last_transfer_time = ((int)(dma_last_transfer_time * 100.0f + 0.5f)) / 100.0f; // 四舍五入
            
            OLED_DMA_TransferBusy = 0;
            
            #if OLED_DOUBLE_BUFFER
            OLED_DISPLAY_GRAM = (current_dma_buffer == 0) ? OLED_GRAM1 : OLED_GRAM2;
            #endif
        }
    }
}

// 初始化DMA - 增强版
void OLED_DMA_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    
    // 启用DMA时钟
    RCC_AHBPeriphClockCmd(OLED_DMA_CLOCK, ENABLE);
    
    // 配置DMA - 双缓冲区优化配置
    DMA_DeInit(OLED_DMA_CHANNEL);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(OLED_IIC->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr = 0; // 将在传输时设置
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 128;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; // 正常模式，传输完成自动停止
    DMA_InitStructure.DMA_Priority = DMA_Priority_High; // 高优先级
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(OLED_DMA_CHANNEL, &DMA_InitStructure);
    
    // 配置DMA中断 - 优先级优化
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = OLED_DMA_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 启用传输完成中断
    DMA_ITConfig(OLED_DMA_CHANNEL, DMA_IT_TC, ENABLE);
}

// 启动DMA传输指定页面
static uint8_t OLED_StartDMATransferPage(uint8_t page, uint8_t buffer_select)
{
    // 仅在第一页时记录开始时间
    if (page == 0) {
        extern volatile uint32_t system_time;  // 从Task.c导入系统时间变量
        dma_transfer_start_time = system_time;  // 记录开始时间
    }
    
    // 设置显示位置
    OLED_Set_Pos(0, page);
    
    // 等待总线空闲
    uint32_t timeout = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(OLED_IIC, I2C_FLAG_BUSY)) 
    {
        if (--timeout == 0) 
            return 0; // 超时
    }
    
    // 启动I2C传输
    I2C_GenerateSTART(OLED_IIC, ENABLE);
    if (!I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) 
        return 0;
    
    // 发送地址
    I2C_Send7bitAddress(OLED_IIC, OLED_ADDRESS, I2C_Direction_Transmitter);
    if (!I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) 
        return 0;
    
    // 发送控制字节(0x40表示数据)
    I2C_SendData(OLED_IIC, 0x40);
    if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING)) 
        return 0;
    
    // 配置DMA传输
    DMA_Cmd(OLED_DMA_CHANNEL, DISABLE);
    
    // 设置传输长度和源地址
    OLED_DMA_CHANNEL->CNDTR = OLED_COLUMN_COUNT;  // 传输128字节
    
    #if OLED_DOUBLE_BUFFER
    // 根据缓冲区选择设置源地址
    if (buffer_select == 0)
        OLED_DMA_CHANNEL->CMAR = (uint32_t)OLED_GRAM1[page];
    else
        OLED_DMA_CHANNEL->CMAR = (uint32_t)OLED_GRAM2[page];
    #else
    // 单缓冲区模式
    OLED_DMA_CHANNEL->CMAR = (uint32_t)OLED_GRAM[page];
    #endif
    
    // 清除传输完成标志
    OLED_DMA_TransferComplete = 0;
    
    // 启动传输
    DMA_Cmd(OLED_DMA_CHANNEL, ENABLE);
    I2C_DMACmd(OLED_IIC, ENABLE);
    
    return 1; // 成功启动
}
#endif

// 初始化SSD1306控制器(实际为I2C初始化)
void OLED_Init(void)
{
    #if OLED_I2C_TYPE == 1
    /* 硬件I2C初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    RCC_APB1PeriphClockCmd(OLED_IIC_Clock, ENABLE);
    RCC_APB2PeriphClockCmd(OLED_GPIO_Clock, ENABLE);

    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_GPIO, &GPIO_InitStructure);

    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_16_9;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 1300000;
    I2C_Init(OLED_IIC, &I2C_InitStructure);
    I2C_Cmd(OLED_IIC, ENABLE);
#else
    /* 软件I2C初始化 */
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 初始化延时
    for (uint32_t i = 0; i < 1000; i++) {
        for (uint32_t j = 0; j < 1000; j++);
    }
    
    RCC_APB2PeriphClockCmd(OLED_GPIO_Clock, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_Init(OLED_GPIO, &GPIO_InitStructure);
    
    OLED_W_SCL(1);
    OLED_W_SDA(1);
#endif

    OLED_WR_Byte(0xAE, OLED_CMD); // 关闭显示屏
    OLED_WR_Byte(0x40, OLED_CMD); // 设置起始行地址
    OLED_WR_Byte(0xB0, OLED_CMD); // 设置页面起始地址为页面寻址模式,0-7
    OLED_WR_Byte(0xC8, OLED_CMD); // 上下反置关(行重映射),C8,从COM[N-1]扫描到COM0;C0,设置 从COM0扫描到COM[N-1],N为复用率
    OLED_WR_Byte(0x81, OLED_CMD); // 设置对比度
    OLED_WR_Byte(0xff, OLED_CMD); // 选择0xff对比度,选择范围0x00-0xff
    OLED_WR_Byte(0xa1, OLED_CMD); // 左右反置关(段重映射),A0H 设置GDDRAM的COL0映射到驱动器输出SEG0,A1H 设置COL127映射到SEG0
    OLED_WR_Byte(0xa6, OLED_CMD); // 正常显示(1亮0灭)
    OLED_WR_Byte(0xa8, OLED_CMD); // 设置多路传输比率,显示行数
    OLED_WR_Byte(0x3f, OLED_CMD); //  MUX=31	 (显示31行)
    OLED_WR_Byte(0xd3, OLED_CMD); // 设置垂直显示偏移(向上)
    OLED_WR_Byte(0x00, OLED_CMD); // 偏移0行
    OLED_WR_Byte(0xd5, OLED_CMD); // 设置DCLK分频和OSC频率
    OLED_WR_Byte(0xf0, OLED_CMD); // 频率最高
    OLED_WR_Byte(0xd9, OLED_CMD); // 设置预充电的持续时间
    OLED_WR_Byte(0x22, OLED_CMD);
    OLED_WR_Byte(0xda, OLED_CMD); // 设置COM引脚配置
    OLED_WR_Byte(0x12, OLED_CMD); // 交替COM,左右不反置
    OLED_WR_Byte(0xdb, OLED_CMD); // 调整Vcomh调节器的输出
    OLED_WR_Byte(0x49, OLED_CMD);
    OLED_WR_Byte(0x8d, OLED_CMD); // 启用电荷泵
    OLED_WR_Byte(0x14, OLED_CMD); // 启用电荷泵
    OLED_WR_Byte(0xaf, OLED_CMD); // 开OLED显示
	
	#if OLED_I2C_TYPE == 1 && OLED_USE_DMA
		OLED_DMA_Init();  // 仅在启用DMA时初始化
	#endif
}

// 清空OLED显示
// 优化说明：使用memset函数一次性清零整个显存区域，提高清空效率
void OLED_Clear(void)
{
    // 使用memset直接清空整个活动缓冲区(8页×128列=1024字节)
    // 比双重循环逐字节清零更高效，尤其在编译器优化开启时
    memset(OLED_GRAM, 0x00, OLED_PAGE_COUNT * OLED_COLUMN_COUNT);
}

// 将OLED显存数组更新到OLED屏幕
// 优化说明：
// 1. 整合重复的超时检查逻辑
// 2. 优化I2C传输流程，减少条件判断
// 3. 使用页指针直接访问显存，提高内存访问效率
// 4. 增加批量传输优化
// 阻塞式更新 - 将活动缓冲区内容更新到OLED屏幕
void OLED_Update(void)
{
#if OLED_USE_DMA && OLED_I2C_TYPE == 1
    // 等待当前传输完成
    while (OLED_DMA_TransferBusy);
    
    // 启动非阻塞更新并等待完成
    OLED_UpdateAsync();
    while (OLED_DMA_TransferBusy);
#elif OLED_I2C_TYPE == 1
    // 使用普通更新(优化版)
    uint8_t j;
    for (j = 0; j < OLED_PAGE_COUNT; j++)
    {
        OLED_Set_Pos(0, j);
        
        // 等待总线空闲
        uint32_t timeout = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(OLED_IIC, I2C_FLAG_BUSY) && --timeout);
        if (timeout == 0) return;
        
        I2C_Start();
        I2C_SendAddress(OLED_ADDRESS, I2C_Direction_Transmitter);
        
        I2C_SendData(OLED_IIC, 0x40);
        if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
            I2C_Stop();
            return;
        }
        
        // 优化：使用指针访问显存，减少数组索引计算
        const uint8_t* gramPage = OLED_GRAM[j];
        for (int i = 0; i < OLED_COLUMN_COUNT; i++) {
            I2C_SendData(OLED_IIC, gramPage[i]);
            
            // 优化：使用位运算替代条件分支
            if (i == OLED_COLUMN_COUNT - 1) {
                if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
                    break;
                }
            } else {
                if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
                    break;
                }
            }
        }
        
        I2C_Stop();
    }
#else
	// 软件I2C模式 - 优化：使用页指针
    uint8_t page;
    for(page = 0; page < OLED_PAGE_COUNT; page++) {
        OLED_Set_Pos(0, page);
        // 使用软件I2C的Write_IIC_Data函数，直接传递显存页地址
        Write_IIC_Data(OLED_GRAM[page], OLED_COLUMN_COUNT);
    }
#endif
}

// 非阻塞式更新 - 启动DMA传输但不等待完成
// 返回值：1-成功启动传输，0-传输已在进行中
uint8_t OLED_UpdateAsync(void)
{
#if OLED_USE_DMA && OLED_I2C_TYPE == 1
    // 检查传输是否正在进行
    if (OLED_DMA_TransferBusy)
        return 0;
    
    OLED_DMA_TransferBusy = 1;
    
#if OLED_DOUBLE_BUFFER
    // 设置要传输的缓冲区（当前活动缓冲区）
    current_dma_buffer = active_buffer;
    
    // 更新显示缓冲区为当前活动缓冲区
    display_buffer = active_buffer;
    
    // 切换到另一个缓冲区用于下一帧绘制
    active_buffer = (active_buffer == BUFFER_A) ? BUFFER_B : BUFFER_A;
    
    // 更新指针
    OLED_GRAM = (active_buffer == BUFFER_A) ? OLED_GRAM1 : OLED_GRAM2;
    OLED_DISPLAY_GRAM = (display_buffer == BUFFER_A) ? OLED_GRAM1 : OLED_GRAM2;
#else
    current_dma_buffer = 0;
    OLED_DISPLAY_GRAM = OLED_GRAM;
#endif
    
    current_dma_page = 0;
    
    if (!OLED_StartDMATransferPage(0, current_dma_buffer))
    {
        OLED_DMA_TransferBusy = 0;
        return 0;
    }
    
    return 1;
#else
    // 非DMA模式下的实现
    OLED_Update();
    return 0;
#endif
}

// 检查OLED是否正在更新
// 返回值：1-正在更新，0-空闲
uint8_t OLED_IsUpdating(void)
{
#if OLED_USE_DMA && OLED_I2C_TYPE == 1
    return OLED_DMA_TransferBusy;
#else
    // 非DMA模式下，OLED_Update是阻塞的，所以这里始终返回0
    return 0;
#endif
}

/**
  * 函    数:更新指定区域显存到屏幕
  * 参    数:x1 起始列坐标
  * 参    数:y1 起始行坐标
  * 参    数:x2 结束列坐标
  * 参    数:y2 结束行坐标
  * 返 回 值:无
  * 说    明:调用此函数后,指定区域的内容将被更新到屏幕上
  */
// 优化版本：根据I2C类型选择最优实现
void OLED_UpdateArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    // 参数检查优化：提前返回无效参数
    if (x1 >= OLED_COLUMN_COUNT || y1 >= OLED_HEIGHT || x2 < 0 || y2 < 0 || x1 > x2 || y1 > y2)
        return;

    uint8_t start_page = y1 / 8;
    uint8_t end_page = y2 / 8;
    uint16_t data_len = x2 - x1 + 1;

#if OLED_USE_DMA && OLED_I2C_TYPE == 1
    // 等待当前传输完成
    while (OLED_DMA_TransferBusy);
    
    // 设置传输忙标志
    OLED_DMA_TransferBusy = 1;
    
    uint8_t page;
    for (page = start_page; page <= end_page; page++)
    {
        OLED_Set_Pos(x1, page);
        
        // 等待总线空闲
        uint32_t timeout = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(OLED_IIC, I2C_FLAG_BUSY) && --timeout);
        if (timeout == 0) {
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 启动I2C传输
        I2C_GenerateSTART(OLED_IIC, ENABLE);
        if (!I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 发送地址
        I2C_Send7bitAddress(OLED_IIC, OLED_ADDRESS, I2C_Direction_Transmitter);
        if (!I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 发送控制字节(0x40表示数据)
        I2C_SendData(OLED_IIC, 0x40);
        if (!I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 配置并启动DMA传输
        OLED_DMA_TransferComplete = 0;
        DMA_Cmd(OLED_DMA_CHANNEL, DISABLE);
        
        // 选择正确的缓冲区
        #if OLED_DOUBLE_BUFFER
        uint32_t buffer_address = (uint32_t)&OLED_DISPLAY_GRAM[page][x1];
        #else
        uint32_t buffer_address = (uint32_t)&OLED_GRAM[page][x1];
        #endif
        
        OLED_DMA_CHANNEL->CNDTR = data_len;  // 只传输需要的字节数
        OLED_DMA_CHANNEL->CMAR = buffer_address;  // 源地址
        
        // 启动传输
        DMA_Cmd(OLED_DMA_CHANNEL, ENABLE);
        I2C_DMACmd(OLED_IIC, ENABLE);
        
        // 等待DMA传输完成
        timeout = I2C_TIMEOUT * 5;
        while (!OLED_DMA_TransferComplete && --timeout);
        
        if (timeout == 0) {
            DMA_Cmd(OLED_DMA_CHANNEL, DISABLE);
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 等待I2C传输完成
        timeout = I2C_TIMEOUT;
        while (!I2C_CheckEvent(OLED_IIC, I2C_EVENT_MASTER_BYTE_TRANSMITTED) && --timeout);
        if (timeout == 0) {
            OLED_DMA_TransferBusy = 0;
            return;
        }
        
        // 停止I2C传输
        I2C_GenerateSTOP(OLED_IIC, ENABLE);
    }
    
    // 所有页面传输完成
    OLED_DMA_TransferBusy = 0;
#elif OLED_I2C_TYPE == 1
    // 硬件I2C模式
    uint8_t page;
    for (page = start_page; page <= end_page; page++)
    {
        OLED_Set_Pos(x1, page);
        #if OLED_DOUBLE_BUFFER
        Write_IIC_Data(&OLED_DISPLAY_GRAM[page][x1], data_len);
        #else
        Write_IIC_Data(&OLED_GRAM[page][x1], data_len);
        #endif
    }
#else
    // 软件I2C模式
    uint8_t page;
    for(page = start_page; page <= end_page; page++)
    {
        OLED_Set_Pos(x1, page);
        #if OLED_DOUBLE_BUFFER
        Write_IIC_Data(&OLED_DISPLAY_GRAM[page][x1], data_len);
        #else
        Write_IIC_Data(&OLED_GRAM[page][x1], data_len);
        #endif
    }
#endif
}

/**
  * 函    数:次方函数
  * 参    数:X 底数
  * 参    数:Y 指数
  * 返 回 值:等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//结果默认为1
	while (Y --)			//累乘Y次
	{
		Result *= X;		//每次把X累乘到结果上
	}
	return Result;
}

/**
  * 函    数:OLED绘制折线图
  * 参    数:x0 指定折线图左上角的横坐标,范围:0~127
  * 参    数:y0 指定折线图左上角的纵坐标,范围:0~63
  * 参    数:width 指定折线图的宽度,范围:0~128
  * 参    数:height 指定折线图的高度,范围:0~64
  * 参    数:xData 指向X轴数据数组的指针
  * 参    数:yData 指向Y轴数据数组的指针
  * 参    数:pointCount 数据点的数量
  * 参    数:color 折线颜色,OLED_COLOR_BLACK或OLED_COLOR_WHITE
  * 参    数:drawAxis 是否绘制坐标轴,1:绘制,0:不绘制
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// 优化说明：
// 1. 使用指针访问数组，减少数组索引计算
// 2. 减少重复计算，提高循环效率
// 3. 优化边界检查逻辑
// 4. 使用整数运算优化归一化计算
void OLED_DrawLineChart(int16_t x0, int16_t y0, int16_t width, int16_t height, 
                        const int16_t* xData, const int16_t* yData, uint8_t pointCount, 
                        uint8_t color, uint8_t drawAxis)
{
    if (pointCount < 2) return;  // 至少需要2个点才能绘制折线

    // 1. 绘制坐标轴
    if (drawAxis)
    {
        // X轴和Y轴
        OLED_DrawLine(x0, y0 + height, x0 + width, y0 + height, color);
        OLED_DrawLine(x0, y0, x0, y0 + height, color);

        // 绘制箭头
        OLED_DrawLine(x0 + width, y0 + height, x0 + width - 5, y0 + height - 3, color);
        OLED_DrawLine(x0 + width, y0 + height, x0 + width - 5, y0 + height + 3, color);
        OLED_DrawLine(x0, y0, x0 - 3, y0 + 5, color);
        OLED_DrawLine(x0, y0, x0 + 3, y0 + 5, color);
    }

    // 2. 找到X和Y数据的最小值和最大值，用于归一化
    // 优化：使用指针访问数据，减少数组索引计算
    const int16_t* xDataPtr = xData;
    const int16_t* yDataPtr = yData;
    
    int16_t minX = *xDataPtr, maxX = *xDataPtr;
    int16_t minY = *yDataPtr, maxY = *yDataPtr;
    
    for (uint8_t i = 1; i < pointCount; i++)
    {
        xDataPtr++;
        yDataPtr++;
        
        if (*xDataPtr < minX) minX = *xDataPtr;
        if (*xDataPtr > maxX) maxX = *xDataPtr;
        if (*yDataPtr < minY) minY = *yDataPtr;
        if (*yDataPtr > maxY) maxY = *yDataPtr;
    }

    // 避免除零错误
    int16_t xRange = maxX - minX;
    int16_t yRange = maxY - minY;
    
    if (xRange == 0) xRange = 1;
    if (yRange == 0) yRange = 1;
    
    // 预计算归一化的除数倒数，用于乘法代替除法
    // 注意：这里使用16.16定点数表示
    uint32_t xScaleFactor = ((uint32_t)width << 16) / xRange;
    uint32_t yScaleFactor = ((uint32_t)height << 16) / yRange;

    // 3. 计算并绘制均值线
    if (drawAxis)
    {
        // 计算最近数据的均值
        // 重置指针
        xDataPtr = xData;
        yDataPtr = yData;
        
        int32_t sum = 0;
        for (uint8_t i = 0; i < pointCount; i++)
        {
            sum += *yDataPtr++;
        }
        int16_t meanValue = sum / pointCount;
        
        // 计算均值对应的Y坐标 - 使用定点数优化
        int16_t meanY = y0 + height - (((int32_t)(meanValue - minY) * yScaleFactor) >> 16);
        // 绘制均值线
        OLED_DrawLine(x0, meanY, x0 + width, meanY, color);
        
        // 绘制均值标签 - 优化：预先分配固定大小缓冲区
        char meanLabel[20];
        sprintf(meanLabel, "均值: %d", meanValue);
        // 在图表中间位置显示均值
        OLED_Printf(x0 + (width >> 1) - 30, y0 - 10, 8, "%s", meanLabel);
    }

    // 4. 绘制折线
    // 重置指针
    xDataPtr = xData;
    yDataPtr = yData;
    
    int16_t prevX, prevY;
    
    // 计算第一个点的坐标
    int16_t x = x0 + (((int32_t)(*xDataPtr - minX) * xScaleFactor) >> 16);
    int16_t y = y0 + height - (((int32_t)(*yDataPtr - minY) * yScaleFactor) >> 16);
    
    // 边界检查 - 使用更高效的逻辑
    if (x < x0) x = x0;
    else if (x > x0 + width) x = x0 + width;
    if (x > 127) x = 127;  // 额外的屏幕边界检查
    
    if (y < y0) y = y0;
    else if (y > y0 + height) y = y0 + height;
    
    // 绘制第一个点
    OLED_DrawPoint(x, y, color);
    
    prevX = x;
    prevY = y;
    
    // 绘制剩余的点和连接线
    for (uint8_t i = 1; i < pointCount; i++)
    {
        xDataPtr++;
        yDataPtr++;
        
        // 归一化X和Y坐标 - 使用定点数优化计算
        x = x0 + (((int32_t)(*xDataPtr - minX) * xScaleFactor) >> 16);
        y = y0 + height - (((int32_t)(*yDataPtr - minY) * yScaleFactor) >> 16);

        // 边界检查
        if (x < x0) x = x0;
        else if (x > x0 + width) x = x0 + width;
        if (x > 127) x = 127;  // 额外的屏幕边界检查
        
        if (y < y0) y = y0;
        else if (y > y0 + height) y = y0 + height;

        // 绘制点
        OLED_DrawPoint(x, y, color);

        // 绘制连接线
        OLED_DrawLine(prevX, prevY, x, y, color);

        prevX = x;
        prevY = y;
    }
}

/**
  * 函    数:将OLED显存数组全部取反
  * 参    数:无
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// 优化说明：
// 1. 使用指针直接访问显存，减少数组索引计算
// 2. 优化循环结构，减少循环控制开销
// 3. 考虑使用编译器优化的循环展开
void OLED_Reverse(void)
{
	// 使用指针访问显存，减少数组索引计算
	for (uint8_t j = 0; j < OLED_PAGE_COUNT; j++) {
		uint8_t* gramPage = OLED_GRAM[j];
		
		// 使用内层循环对每一列数据进行取反操作
		for (uint8_t i = 0; i < OLED_COLUMN_COUNT; i++) {
			gramPage[i] ^= 0xFF;  // 对当前字节取反
		}
	}
}

/**
  * 函    数:OLED绘制时间横轴折线图
  * 参    数:x0 指定折线图左上角的横坐标,范围:0~127
  * 参    数:y0 指定折线图左上角的纵坐标,范围:0~63
  * 参    数:width 指定折线图的宽度,范围:0~128
  * 参    数:height 指定折线图的高度,范围:0~64
  * 参    数:yData 指向Y轴数据数组的指针(支持int16_t或float类型)
  * 参    数:dataType 数据类型(DATA_TYPE_INT16或DATA_TYPE_FLOAT)
  * 参    数:pointCount 数据点的数量
  * 参    数:timeInterval 数据点之间的时间间隔(单位:任意)
  * 参    数:color 折线颜色,OLED_COLOR_BLACK或OLED_COLOR_WHITE
  * 参    数:drawAxis 是否绘制坐标轴,1:绘制,0:不绘制
  * 参    数:showLatest 是否只显示最近20个数据点,1:是,0:否
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawTimeLineChart(int16_t x0, int16_t y0, int16_t width, int16_t height, 
                            const void* yData, DataType dataType, uint8_t pointCount, uint16_t timeInterval, 
                            uint8_t color, uint8_t drawAxis, uint8_t showLatest)
{
    // 确定要显示的数据点数量和起始索引
    uint8_t displayCount = pointCount;
    uint8_t startIndex = 0;
    
    if (showLatest && pointCount > 20)
    {
        displayCount = 20;
        startIndex = pointCount - 20;
    }
    
    if (displayCount < 2) return;  // 至少需要2个点才能绘制折线

    // 1. 找到Y数据的最小值和最大值，用于归一化
    float minYf, maxYf;
    int16_t minYi, maxYi;

    if (dataType == DATA_TYPE_INT16)
    {
        const int16_t* yDataInt = (const int16_t*)yData;
        minYi = yDataInt[startIndex];
        maxYi = yDataInt[startIndex];
        for (uint8_t i = startIndex + 1; i < startIndex + displayCount; i++)
        {
            if (yDataInt[i] < minYi) minYi = yDataInt[i];
            if (yDataInt[i] > maxYi) maxYi = yDataInt[i];
        }
        minYf = (float)minYi;
        maxYf = (float)maxYi;
    }
    else // DATA_TYPE_FLOAT
    {
        const float* yDataFloat = (const float*)yData;
        minYf = yDataFloat[startIndex];
        maxYf = yDataFloat[startIndex];
        for (uint8_t i = startIndex + 1; i < startIndex + displayCount; i++)
        {
            if (yDataFloat[i] < minYf) minYf = yDataFloat[i];
            if (yDataFloat[i] > maxYf) maxYf = yDataFloat[i];
        }
        minYi = (int16_t)minYf;
        maxYi = (int16_t)maxYf;
    }

    // 避免除零错误
    if (maxYf == minYf) maxYf = minYf + 1.0f;

    // 调整Y轴范围，使其比实际数据范围稍大
    float rangeFloat = maxYf - minYf;
    minYf -= rangeFloat * 0.1f;  // 减去10%的范围
    maxYf += rangeFloat * 0.1f;  // 加上10%的范围

    // 2. 绘制坐标轴
    if (drawAxis)
    {
        // X轴
        OLED_DrawLine(x0, y0 + height, x0 + width, y0 + height, color);
        // Y轴
        OLED_DrawLine(x0, y0, x0, y0 + height, color);

        // 绘制箭头
        OLED_DrawLine(x0 + width, y0 + height, x0 + width - 5, y0 + height - 3, color);
        OLED_DrawLine(x0 + width, y0 + height, x0 + width - 5, y0 + height + 3, color);
        OLED_DrawLine(x0, y0, x0 - 3, y0 + 5, color);
        OLED_DrawLine(x0, y0, x0 + 3, y0 + 5, color);

        // 绘制X轴刻度和时间标签
        uint8_t labelIntervalX = width / 5;  // X轴刻度间隔
        for (uint8_t i = 1; i <= 5; i++)
        {
            int16_t xPos = x0 + i * labelIntervalX;
            // 绘制刻度线
            OLED_DrawLine(xPos, y0 + height, xPos, y0 + height + 3, color);
            // 绘制时间标签
            uint16_t timeValue;
            if (showLatest && pointCount > 20)
            {
                // 如果只显示最近20个点，时间从(startIndex)开始
                timeValue = (startIndex + i * labelIntervalX * displayCount / width) * timeInterval;
            }
            else
            {
                // 否则时间从0开始
                timeValue = i * labelIntervalX * pointCount / width * timeInterval;
            }
            OLED_Printf(xPos - 15, y0 + height + 5, 8, "%d", timeValue);
        }

        // 绘制Y轴刻度和数值标签
        uint8_t labelIntervalY = height / 5;  // Y轴刻度间隔
        int16_t range = maxYi - minYi;
        // 防止除零错误
        if (range == 0) range = 1;
        for (uint8_t i = 1; i <= 5; i++)
        {
            int16_t yPos = y0 + height - i * labelIntervalY;
            // 绘制刻度线
            OLED_DrawLine(x0 - 3, yPos, x0, yPos, color);
            // 计算对应的Y值
            int16_t yValue = minYi + (i * labelIntervalY * range) / height;
            // 绘制数值标签
            char yLabel[10];
            sprintf(yLabel, "%d", yValue);
            OLED_Printf(x0 - 30, yPos - 4, 8, "%s", yLabel);
        }
    }
    
    // 计算最近数据的均值
    float sum = 0.0f;
    uint8_t meanCount = displayCount;
    for (uint8_t i = 0; i < meanCount; i++)
    {
        uint8_t dataIndex = startIndex + i;
        if (dataType == DATA_TYPE_INT16)
        {
            const int16_t* yDataInt = (const int16_t*)yData;
            sum += (float)yDataInt[dataIndex];
        }
        else // DATA_TYPE_FLOAT
        {
            const float* yDataFloat = (const float*)yData;
            sum += yDataFloat[dataIndex];
        }
    }
    float meanValue = sum / meanCount;

    // 绘制均值线
    int16_t meanY = y0 + height - (int16_t)((meanValue - minYf) * height / (maxYf - minYf));
    OLED_DrawLine(x0, meanY, x0 + width, meanY, color);

    // 3. 绘制折线
    if (displayCount > 0)
    {
        // 处理第一个点
        uint8_t firstDataIndex = startIndex;
        float firstYValue;
        if (dataType == DATA_TYPE_INT16)
        {
            const int16_t* yDataInt = (const int16_t*)yData;
            firstYValue = (float)yDataInt[firstDataIndex];
        }
        else // DATA_TYPE_FLOAT
        {
            const float* yDataFloat = (const float*)yData;
            firstYValue = yDataFloat[firstDataIndex];
        }
        int16_t prevX = x0;
        int16_t prevY = y0 + height - ((firstYValue - minYf) * height) / (maxYf - minYf);

        // 处理剩余点并绘制折线
        for (uint8_t i = 1; i < displayCount; i++)
        {
            // 计算X坐标(基于时间序列)，越靠右数据越新
            int16_t x = x0 + (i * width) / (displayCount - 1);
            // 确保X坐标不超出屏幕边界
            if (x > 127) x = 127;
            // 当前数据点在原数组中的索引
            uint8_t dataIndex = startIndex + i;
            // 归一化Y坐标
            float yValue;
            if (dataType == DATA_TYPE_INT16)
            {
                const int16_t* yDataInt = (const int16_t*)yData;
                yValue = (float)yDataInt[dataIndex];
            }
            else // DATA_TYPE_FLOAT
            {
                const float* yDataFloat = (const float*)yData;
                yValue = yDataFloat[dataIndex];
            }
            
            int16_t y = y0 + height - ((yValue - minYf) * height) / (maxYf - minYf);

            // 确保坐标在屏幕范围内
            if (x < x0) x = x0;
            if (x > x0 + width) x = x0 + width;
            if (y < y0) y = y0;
            if (y > y0 + height) y = y0 + height;

            // 绘制线段
            OLED_DrawLine(prevX, prevY, x, y, color);

            // 更新前一个点的坐标
            prevX = x;
            prevY = y;
        }
    }
}



/**
  * 函    数:将OLED显存数组部分取反
  * 参    数:X 指定区域左上角的横坐标,范围:0~127
  * 参    数:Y 指定区域左上角的纵坐标,范围:0~63
  * 参    数:Width 指定区域的宽度,范围:0~128
  * 参    数:Height 指定区域的高度,范围:0~64
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// 优化说明：
// 1. 使用早期返回模式减少嵌套
// 2. 预计算起始页和结束页，减少循环内计算
// 3. 使用指针直接访问显存，减少数组索引计算
// 4. 批量处理整页数据，提高效率
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	/*参数检查,保证指定区域不会超出屏幕范围 - 早期返回优化*/
	OLED_CHECK_RECTANGLE(X, Y, Width, Height);
	if (X + Width > 128) {Width = 128 - X;}
	if (Y + Height > 64) {Height = 64 - Y;}
	
	// 计算起始页和结束页
	uint8_t startPage = Y / 8;
	uint8_t endPage = (Y + Height - 1) / 8;
	uint8_t startBit = Y % 8;
	uint8_t endBit = (Y + Height - 1) % 8;
	
	// 处理跨页情况
	for (uint8_t page = startPage; page <= endPage; page++) {
		uint8_t* gramPage = OLED_GRAM[page]; // 获取当前页的指针
		uint8_t startY = (page == startPage) ? startBit : 0;
		uint8_t endY = (page == endPage) ? endBit : 7;
		
		// 对当前页的指定区域进行处理
		for (uint8_t yBit = startY; yBit <= endY; yBit++) {
			uint8_t mask = 0x01 << yBit;
			
			// 对当前列的所有指定位进行取反操作
			for (int16_t x = X; x < X + Width; x++) {
				gramPage[x] ^= mask;
			}
		}
	}
}

/**
  * 函    数:将OLED显存数组部分清零
  * 参    数:X 指定区域左上角的横坐标,范围:0~127
  * 参    数:Y 指定区域左上角的纵坐标,范围:0~63
  * 参    数:Width 指定区域的宽度,范围:0~128
  * 参    数:Height 指定区域的高度,范围:0~64
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// 优化说明：
// 1. 使用早期返回模式减少嵌套
// 2. 预计算起始页和结束页，减少循环内计算
// 3. 使用指针直接访问显存，减少数组索引计算
// 4. 对整页情况进行特殊优化，使用memset提高效率
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	/*参数检查,保证指定区域不会超出屏幕范围 - 早期返回优化*/
	OLED_CHECK_RECTANGLE(X, Y, Width, Height);
	if (X + Width > 128) {Width = 128 - X;}
	if (Y + Height > 64) {Height = 64 - Y;}
	
	// 计算起始页和结束页
	uint8_t startPage = Y / 8;
	uint8_t endPage = (Y + Height - 1) / 8;
	uint8_t startBit = Y % 8;
	uint8_t endBit = (Y + Height - 1) % 8;
	
	// 处理整页清除的特殊情况
	if (startPage == endPage && startBit == 0 && endBit == 7) {
		// 如果是整页清除，使用memset进行优化
		memset(OLED_GRAM[startPage] + X, 0x00, Width);
		return;
	}
	
	// 处理跨页情况
	for (uint8_t page = startPage; page <= endPage; page++) {
		uint8_t* gramPage = OLED_GRAM[page]; // 获取当前页的指针
		uint8_t startY = (page == startPage) ? startBit : 0;
		uint8_t endY = (page == endPage) ? endBit : 7;
		
		// 对当前页的指定区域进行处理
		for (uint8_t yBit = startY; yBit <= endY; yBit++) {
			uint8_t mask = ~(0x01 << yBit); // 生成清零掩码
			
			// 对当前列的所有指定位进行清零操作
			for (int16_t x = X; x < X + Width; x++) {
				gramPage[x] &= mask;
			}
		}
	}
}

/**
  * 函    数:OLED显示图像
  * 参    数:X 指定图像左上角的横坐标,范围:无限制
  * 参    数:Y 指定图像左上角的纵坐标,范围:无限制
  * 参    数:Width 指定图像的宽度,范围:0~128
  * 参    数:Height 指定图像的高度,范围:0~64
  * 参    数:Image 指定要显示的图像
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// 优化说明：
// 1. 预计算所有边界值，减少循环内计算
// 2. 使用指针直接访问显存，减少数组索引计算
// 3. 减少条件分支，优化循环内逻辑
// 4. 批量处理数据，提高内存访问效率
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
    /*参数检查,保证指定区域不会超出屏幕范围*/
    OLED_CHECK_RECTANGLE(X, Y, Width, Height);
    
    /*参数检查,确保图像指针有效*/
    if (Image == NULL) return;
    
    // 计算实际显示区域(处理负坐标和超出边界的情况)
    int16_t displayX = (X < 0) ? 0 : X;
    int16_t displayY = (Y < 0) ? 0 : Y;
    
    // 计算可显示的宽度和高度(防止负数)
    int16_t displayWidth = (X < 0) ? (Width + X) : Width;
    int16_t displayHeight = (Y < 0) ? (Height + Y) : Height;
    
    // 快速检查是否完全在屏幕外 - 早期返回优化
    if (displayWidth <= 0 || displayHeight <= 0 || displayX >= 128 || displayY >= 64) {
        return;
    }
    
    // 调整负值情况
    if (displayWidth < 0) displayWidth = 0;
    if (displayHeight < 0) displayHeight = 0;
    
    // 限制在屏幕范围内
    if (displayX + displayWidth > 128) displayWidth = 128 - displayX;
    if (displayY + displayHeight > 64) displayHeight = 64 - displayY;
    
    // 再次检查，确保有效
    if (displayWidth <= 0 || displayHeight <= 0) {
        return;
    }
    
    // 计算源图像起始偏移
    int16_t srcXOffset = (X < 0) ? -X : 0;
    int16_t srcYOffset = (Y < 0) ? -Y : 0;
    
    // 计算目标起始页和位偏移
    uint8_t destStartPage = displayY / 8;
    uint8_t destStartBit = displayY % 8;
    
    // 计算需要处理的页数
    uint8_t pageCount = (displayHeight + destStartBit + 7) / 8;
    
    // 预计算图像总大小，用于边界检查
    uint16_t totalImageSize = (uint16_t)Width * Height;
    
    // 逐页处理
    for (uint8_t page = 0; page < pageCount; page++) {
        uint8_t destPage = destStartPage + page;
        if (destPage >= 8) break;  // 最多8页(64像素)
        
        // 获取当前页的显存指针
        uint8_t* destGramPage = OLED_GRAM[destPage];
        uint8_t* destGramNextPage = (destPage < 7) ? OLED_GRAM[destPage + 1] : NULL;
        
        // 计算当前页在源图像中的起始行
        int16_t srcStartRow = srcYOffset + (page * 8) - destStartBit;
        srcStartRow = (srcStartRow < 0) ? 0 : srcStartRow;
        
        // 计算源图像起始页和位偏移
        uint8_t srcPage = (uint8_t)(srcStartRow / 8);
        uint8_t srcBitOffset = (uint8_t)(srcStartRow % 8);
        
        // 当前目标页的位偏移(只有第一页有初始偏移)
        uint8_t currentDestBitOffset = (page == 0) ? destStartBit : 0;
        
        // 预计算一些常用值
        uint16_t srcPageOffset = (uint16_t)srcPage * Width;
        
        // 遍历每列
        for (int16_t col = 0; col < displayWidth; col++) {
            // 源图像索引 - 使用预计算的页偏移
            uint16_t srcIndex = srcPageOffset + srcXOffset + col;
            
            // 检查索引是否有效 - 优化：使用预计算的总大小
            if (srcIndex >= totalImageSize) continue;
            
            uint8_t srcData = Image[srcIndex];
            uint8_t nextData = 0;
            
            // 获取下一行数据(如果需要) - 优化：减少条件判断的复杂度
            if (srcBitOffset != 0) {
                if ((srcPage + 1) * 8 < (int16_t)Height) {
                    uint16_t nextIndex = srcIndex + Width;
                    if (nextIndex < totalImageSize) {
                        nextData = Image[nextIndex];
                    }
                }
            }
            
            // 组合源数据 - 优化：减少条件分支
            uint8_t combinedData;
            if (srcBitOffset == 0) {
                combinedData = srcData;
            } else {
                combinedData = (srcData >> srcBitOffset) | (nextData << (8 - srcBitOffset));
            }
            
            // 目标位置索引
            int16_t destPos = displayX + col;
            
            // 应用目标位偏移 - 优化：使用指针直接操作
            if (currentDestBitOffset == 0) {
                // 无偏移,直接写入
                destGramPage[destPos] |= combinedData;
            } else {
                // 写入当前页
                destGramPage[destPos] |= combinedData << currentDestBitOffset;
                
                // 写入下一页(如果需要)
                if (destGramNextPage != NULL) {
                    destGramNextPage[destPos] |= combinedData >> (8 - currentDestBitOffset);
                }
            }
        }
    }
}
    

/**
  * 函    数:OLED使用printf函数打印格式化字符串
  * 参    数:X 指定格式化字符串左上角的横坐标,范围:0~127
  * 参    数:Y 指定格式化字符串左上角的纵坐标,范围:0~63
  * 参    数:FontSize 指定字体大小
  *           范围:OLED_8X16		宽8像素,高16像素
  *                 OLED_6X8		宽6像素,高8像素
  * 参    数:format 指定要显示的格式化字符串,范围:ASCII码可见字符组成的字符串
  * 参    数:... 格式化字符串参数列表
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
// ASCII字符快速渲染函数 - 直接操作OLED_GRAM (优化版) - 内联优化
static inline void OLED_DrawASCIIFast(int16_t x, int16_t y, char c, uint8_t fontHeight) {
    // 参数边界检查 - 使用早期返回模式减少嵌套
    if (x < 0 || x >= 128 || y < 0 || y >= 64 || c < ' ' || c > '~') {
        return;
    }
    
    // 预计算常用值
    uint8_t pageStart = y / 8;
    uint8_t bitOffset = y % 8;
    
    if (fontHeight == 16) {
        // 快速渲染8x16字体
        const uint8_t* fontData = OLED_F8x16[c - ' '];
        uint8_t pageCount = 2; // 16像素高，需要2页
        uint8_t page, col;
        uint8_t* gramPtr; // 显存指针加速访问
        
        for (page = 0; page < pageCount; page++) {
            uint8_t destPage = pageStart + page;
            if (destPage >= 8) break;
            
            gramPtr = OLED_GRAM[destPage] + x; // 指向当前页的起始位置
            
            for (col = 0; col < 8; col++) {
                if (x + col >= 128) break;
                
                uint8_t data = fontData[page * 8 + col];
                // 考虑垂直偏移
                if (bitOffset > 0 && page == 0) {
                    *gramPtr++ |= data << bitOffset;
                    // 如果有数据溢出到下一页
                    if (destPage + 1 < 8 && (data >> (8 - bitOffset)) != 0) {
                        OLED_GRAM[destPage + 1][x + col] |= data >> (8 - bitOffset);
                    }
                } else {
                    *gramPtr++ |= data;
                }
            }
        }
    } else if (fontHeight == 8) {
        // 快速渲染6x8字体 - 内联常用计算
        const uint8_t* fontData = OLED_F6x8[c - ' '];
        uint8_t destPage = pageStart;
        uint8_t col;
        uint8_t* gramPtr = OLED_GRAM[destPage] + x; // 指向当前页的起始位置
        
        for (col = 0; col < 6; col++) {
            if (x + col >= 128) break;
            
            uint8_t data = fontData[col];
            // 考虑垂直偏移
            if (bitOffset > 0) {
                *gramPtr++ |= data << bitOffset;
                // 如果有数据溢出到下一页
                if (destPage + 1 < 8 && (data >> (8 - bitOffset)) != 0) {
                    OLED_GRAM[destPage + 1][x + col] |= data >> (8 - bitOffset);
                }
            } else {
                *gramPtr++ |= data;
            }
        }
    }
}

// 快速检查字符串是否只包含ASCII字符 - 内联优化
static inline bool OLED_IsPureASCII(const char* str) {
    while (*str) {
        if ((*str & 0x80) != 0) {
            return false;
        }
        str++;
    }
    return true;
}

// 汉字哈希表结构 - 优化版
typedef struct {
    const char* name;           // 汉字的名称
    const uint8_t* data;        // 汉字的点阵数据
    uint8_t isUsed;             // 该哈希表项是否被使用
} ChineseCharHashTable;

// 定义哈希表大小，选择质数以减少冲突
#define HASH_TABLE_SIZE 131

// 声明静态哈希表和初始化标志
static ChineseCharHashTable chineseHashTable[HASH_TABLE_SIZE] = {0};
static uint8_t isHashTableInitialized = 0;

// 字符串哈希函数 - 针对UTF-8编码汉字优化 - 内联优化
static inline uint16_t OLED_HashString(const char* str) {
    uint16_t hash = 0;
    // 处理UTF-8编码的汉字（通常占3个字节）
    for (uint8_t i = 0; i < OLED_CHN_CHAR_WIDTH && str[i] != '\0'; i++) {
        hash = (hash * 31) + (unsigned char)str[i];
    }
    return hash % HASH_TABLE_SIZE;
}

// 初始化汉字哈希表
static void OLED_InitChineseHashTable(void) {
    if (isHashTableInitialized) return;
    
    // 清空哈希表
    memset(chineseHashTable, 0, sizeof(chineseHashTable));
    
    // 遍历汉字字模库，将汉字添加到哈希表
    // 注意：使用NULL检查而不是strcmp来避免处理NULL指针
    for (uint8_t i = 0; OLED_CF16x16[i].Name != NULL && OLED_CF16x16[i].Name[0] != '\0'; i++) {
        uint16_t hash = OLED_HashString(OLED_CF16x16[i].Name);
        
        // 线性探测解决冲突
        uint8_t attempts = 0;
        while (chineseHashTable[hash].isUsed && attempts < HASH_TABLE_SIZE) {
            hash = (hash + 1) % HASH_TABLE_SIZE;
            attempts++;
        }
        
        // 如果找到空位，添加到哈希表
        if (!chineseHashTable[hash].isUsed) {
            chineseHashTable[hash].name = OLED_CF16x16[i].Name;
            chineseHashTable[hash].data = OLED_CF16x16[i].Data;
            chineseHashTable[hash].isUsed = 1;
        }
    }
    
    isHashTableInitialized = 1;
}

// 通过哈希表快速查找汉字点阵数据
static const uint8_t* OLED_FindChineseChar(const char* chineseChar) {
    if (!isHashTableInitialized) {
        OLED_InitChineseHashTable();
    }
    
    // 确保输入不是NULL
    if (chineseChar == NULL || chineseChar[0] == '\0') {
        return NULL;
    }
    
    uint16_t hash = OLED_HashString(chineseChar);
    
    // 线性探测查找
    uint8_t attempts = 0;
    while (chineseHashTable[hash].isUsed && attempts < HASH_TABLE_SIZE) {
        // 安全地比较哈希表中存储的汉字名称
        if (chineseHashTable[hash].name != NULL) {
            // 对于汉字，我们只比较前2-3个字符，因为字模库中的汉字通常是2字节表示
            // 注意：这取决于实际的编码方式和字模库的设计
            if (strncmp(chineseHashTable[hash].name, chineseChar, 3) == 0) {
                return chineseHashTable[hash].data;
            }
        }
        hash = (hash + 1) % HASH_TABLE_SIZE;
        attempts++;
    }
    
    return NULL;  // 未找到
}

// 优化的OLED_Printf函数 - 更高效的字符串处理
void OLED_Printf(int16_t X, int16_t Y, uint8_t FontSize, const char *format, ...) {
    /*参数检查,保证指定位置不会超出屏幕范围*/
    OLED_CHECK_COORDINATES(X, Y);
    
    /*参数检查,确保字体大小有效*/
    if (FontSize != 8 && FontSize != 16) return;
    
    /*参数检查,确保格式化字符串指针有效*/
    if (format == NULL) return;
    
    // 预计算常用值 - 避免重复计算
    uint8_t lineHeight = (FontSize == 16) ? 16 : 8;
    uint8_t charWidth = (lineHeight == 16) ? 8 : 6;
    int16_t currentX = X;
    int16_t currentY = Y;
    
    // 使用较小的缓冲区以减少内存占用
    char String[128];  
    va_list arg;
    va_start(arg, format);
    vsnprintf(String, sizeof(String), format, arg);  // 使用vsnprintf避免缓冲区溢出
    va_end(arg);

    // 快速路径：纯ASCII字符串 - 内联常用操作
    if (OLED_IsPureASCII(String)) {
        char* strPtr = String; // 使用指针直接访问字符串
        while (*strPtr != '\0') {
            if (*strPtr == '\n') {
                // 处理换行符
                currentY += lineHeight;
                currentX = X;
                strPtr++;
                continue;
            } else if (*strPtr == '\r') {
                // 处理回车符
                currentX = X;
                strPtr++;
                continue;
            }
            
            // 使用快速ASCII渲染函数
            OLED_DrawASCIIFast(currentX, currentY, *strPtr, lineHeight);
            
            // 更新X坐标 - 使用预计算的字符宽度
            currentX += charWidth;
            strPtr++;
        }
        return; // 快速路径结束，避免不必要的处理
    }

    // 标准路径：包含非ASCII字符的情况 - 优化字符类型判断
    char* strPtr = String;
    while (*strPtr != '\0') {
        if ((*strPtr & 0x80) == 0) {  // ASCII字符
            if (*strPtr == '\n') {
                // 处理换行符
                currentY += lineHeight;
                currentX = X;
                strPtr++;
                continue;
            } else if (*strPtr == '\r') {
                // 处理回车符
                currentX = X;
                strPtr++;
                continue;
            }
            
            // 使用快速ASCII渲染函数
            OLED_DrawASCIIFast(currentX, currentY, *strPtr, lineHeight);
            
            // 更新X坐标
            currentX += charWidth;
            strPtr++;
        } else {  // 非ASCII字符处理
            // 尝试使用哈希表查找汉字
            const uint8_t* chineseData = OLED_FindChineseChar(strPtr);
            
            if (currentX >= 0 && currentY >= 0 && chineseData != NULL) {
                // 显示找到的汉字
                OLED_ShowImage(currentX, currentY, 16, 16, chineseData);
                currentX += 16;
                // 跳过当前汉字（假设汉字在字模库中占2个字节）
                // 注意：这里需要根据实际字模库的编码方式调整
                if(strlen(strPtr) >= 3 && (strPtr[0] & 0x80) && (strPtr[1] & 0x80)) {
                    strPtr += 3;
                } else {
                    // 如果不是标准的两字节汉字，至少跳过一个字节
                    strPtr++;
                }
            } else {
                // 如果没有找到汉字，尝试按字节处理
                // 显示一个占位符或直接跳过
                currentX += charWidth;
                strPtr++;
            }
        }
    }
}


/**
  * 函    数:OLED在指定位置画一个点
  * 参    数:X 指定点的横坐标,范围:0~127
  * 参    数:Y 指定点的纵坐标,范围:0~63
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
static inline void OLED_DrawPoint(int16_t X, int16_t Y, uint8_t Color)
{
	/*参数检查,保证指定位置不会超出屏幕范围*/
	OLED_CHECK_COORDINATES(X, Y);
	
	/*根据颜色参数设置或清除显存数组指定位置的一个Bit数据*/
	if (Color == OLED_COLOR_WHITE) {
		OLED_GRAM[Y / 8][X] |= 0x01 << (Y % 8);  // 设置位为1
	} else {
		OLED_GRAM[Y / 8][X] &= ~(0x01 << (Y % 8)); // 清除位为0
	}
}

/**
  * 函    数:判断指定点是否在指定多边形内部
  * 参    数:nvert 多边形的顶点数
  * 参    数:vertx verty 包含多边形顶点的x和y坐标的数组
  * 参    数:testx testy 测试点的X和y坐标
  * 返 回 值:指定点是否在指定多边形内部,1:在内部,0:不在内部
  */
static inline uint8_t OLED_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
	int16_t i, j, c = 0;
	
	/*此算法由W. Randolph Franklin提出*/
	/*参考链接:https://wrfranklin.org/Research/Short_Notes/pnpoly.html*/
	for (i = 0, j = nvert - 1; i < nvert; j = i++)
	{
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
		{
			c = !c;
		}
	}
	return c;
}

/**
  * 函    数:判断指定点是否在指定角度内部
  * 参    数:X Y 指定点的坐标
  * 参    数:StartAngle EndAngle 起始角度和终止角度,范围:-180~180
  *           水平向右为0度,水平向左为180度或-180度,下方为正数,上方为负数,顺时针旋转
  * 返 回 值:指定点是否在指定角度内部,1:在内部,0:不在内部
  */
static inline uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14 * 180;	//计算指定点的弧度,并转换为角度表示
	if (StartAngle < EndAngle)	//起始角度小于终止角度的情况
	{
		/*如果指定角度在起始终止角度之间,则判定指定点在指定角度*/
		if (PointAngle >= StartAngle && PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	else			//起始角度大于于终止角度的情况
	{
		/*如果指定角度大于起始角度或者小于终止角度,则判定指定点在指定角度*/
		if (PointAngle >= StartAngle || PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	return 0;		//不满足以上条件,则判断判定指定点不在指定角度
}

/**
  * 函    数:OLED画线
  * 参    数:X0 指定一个端点的横坐标,范围:0~127
  * 参    数:Y0 指定一个端点的纵坐标,范围:0~63
  * 参    数:X1 指定另一个端点的横坐标,范围:0~127
  * 参    数:Y1 指定另一个端点的纵坐标,范围:0~63
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, uint8_t Color)
{
	/*参数检查,保证指定位置不会超出屏幕范围*/
	OLED_CHECK_COORDINATES(X0, Y0);
	OLED_CHECK_COORDINATES(X1, Y1);
	
	int16_t x, y, dx, dy, d, incrE, incrNE, temp;
	int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
	uint8_t yflag = 0, xyflag = 0;
	
	if (y0 == y1)		//横线单独处理
	{
		/*0号点X坐标大于1号点X坐标,则交换两点X坐标*/
		if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
		
		/*遍历X坐标*/
		for (x = x0; x <= x1; x ++)
		{
            OLED_DrawPoint(x, y0, 1);	//依次画点
		}
	}
	else if (x0 == x1)	//竖线单独处理
	{
		/*0号点Y坐标大于1号点Y坐标,则交换两点Y坐标*/
		if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
		
		/*遍历Y坐标*/
		for (y = y0; y <= y1; y ++)
		{
			OLED_DrawPoint(x0, y, 1);	//依次画点
		}
	}
	else				//斜线
	{
		/*使用Bresenham算法画直线,可以避免耗时的浮点运算,效率更高*/
		/*参考文档:https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
		/*参考教程:https://www.bilibili.com/video/BV1364y1d7Lo*/
		
		if (x0 > x1)	//0号点X坐标大于1号点X坐标
		{
			/*交换两点坐标*/
			/*交换后不影响画线,但是画线方向由第一、二、三、四象限变为第一、四象限*/
			temp = x0; x0 = x1; x1 = temp;
			temp = y0; y0 = y1; y1 = temp;
		}
		
		if (y0 > y1)	//0号点Y坐标大于1号点Y坐标
		{
			/*将Y坐标取负*/
			/*取负后影响画线,但是画线方向由第一、四象限变为第一象限*/
			y0 = -y0;
			y1 = -y1;
			
			/*置标志位yflag,记住当前变换,在后续实际画线时,再将坐标换回来*/
			yflag = 1;
		}
		
		if (y1 - y0 > x1 - x0)	//画线斜率大于1
		{
			/*将X坐标与Y坐标互换*/
			/*互换后影响画线,但是画线方向由第一象限0~90度范围变为第一象限0~45度范围*/
			temp = x0; x0 = y0; y0 = temp;
			temp = x1; x1 = y1; y1 = temp;
			
			/*置标志位xyflag,记住当前变换,在后续实际画线时,再将坐标换回来*/
			xyflag = 1;
		}
		
		/*以下为Bresenham算法画直线*/
		/*算法要求,画线方向必须为第一象限0~45度范围*/
		dx = x1 - x0;
		dy = y1 - y0;
		incrE = 2 * dy;
		incrNE = 2 * (dy - dx);
		d = 2 * dy - dx;
		x = x0;
		y = y0;
		
		/*画起始点,同时判断标志位,将坐标换回来*/
		if (yflag && xyflag){OLED_DrawPoint(y, -x, 1);}
		else if (yflag)		{OLED_DrawPoint(x, -y, 1);}
		else if (xyflag)	{OLED_DrawPoint(y, x, 1);}
		else				{OLED_DrawPoint(x, y, 1);}
		
		while (x < x1)		//遍历X轴的每个点
		{
			x ++;
			if (d < 0)		//下一个点在当前点东方
			{
				d += incrE;
			}
			else			//下一个点在当前点东北方
			{
				y ++;
				d += incrNE;
			}
			
			/*画每一个点,同时判断标志位,将坐标换回来*/
			if (yflag && xyflag){OLED_DrawPoint(y, -x, 1);}
			else if (yflag)		{OLED_DrawPoint(x, -y, 1);}
			else if (xyflag)	{OLED_DrawPoint(y, x, 1);}
			else				{OLED_DrawPoint(x, y, 1);}
		}	
	}
}

//// 获取DMA传输时间（单位：毫秒，精确到两位小数）
//float OLED_GetDMATransferTime(void)
//{
//    return dma_last_transfer_time;
//}

/**
  * 函    数:OLED矩形(带坐标循环)
  * 参    数:X 指定矩形左上角的横坐标,范围:自动循环处理
  * 参    数:Y 指定矩形左上角的纵坐标,范围:自动循环处理
  * 参    数:Width 指定矩形的宽度,范围:0~128
  * 参    数:Height 指定矩形的高度,范围:0~64
  * 参    数:IsFilled 指定矩形是否填充
  *           范围:OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值:无
  * 说    明:1. 当X或Y超出屏幕范围时,自动循环到另一侧
  *           2. 调用此函数后,需调用OLED_Update更新屏幕显示
  */
void OLED_DrawRectangle(int16_t X, int16_t Y, int16_t Width, int16_t Height, uint8_t IsFilled)
{
    /*参数检查,保证指定区域不会超出屏幕范围*/
    OLED_CHECK_RECTANGLE(X, Y, Width, Height);
    
    // 处理X坐标循环(超出右边界时回到左侧)
    if (X >= 128) X %= 128;
    if (X < 0) X = 128 + (X % 128);
    
    // 处理Y坐标循环(超出下边界时回到顶部)
    if (Y >= 64) Y %= 64;
    if (Y < 0) Y = 64 + (Y % 64);
    
    uint8_t i, j;
    if (!IsFilled)        // 指定矩形不填充
    {
        /*遍历上下X坐标,画矩形上下两条线*/
        for (i = X; i < X + Width; i++)
        {
            OLED_DrawPoint(i % 128, Y, 1);
            OLED_DrawPoint(i % 128, (Y + Height - 1) % 64, 1);
        }
        /*遍历左右Y坐标,画矩形左右两条线*/
        for (i = Y; i < Y + Height; i++)
        {
            OLED_DrawPoint(X, i % 64, 1);
            OLED_DrawPoint((X + Width - 1) % 128, i % 64, 1);
        }
    }
    else                  // 指定矩形填充
    {
        /*遍历X坐标*/
        for (i = X; i < X + Width; i++)
        {
            /*遍历Y坐标*/
            for (j = Y; j < Y + Height; j++)
            {
                /*在指定区域画点,填充满矩形*/
                OLED_DrawPoint(i % 128, j % 64, 1);

            }
        }
    }
}

/**
  * 函    数:OLED反色矩形(带坐标循环)
  * 参    数:X 指定矩形左上角的横坐标,范围:自动循环处理
  * 参    数:Y 指定矩形左上角的纵坐标,范围:自动循环处理
  * 参    数:Width 指定矩形的宽度,范围:0~128
  * 参    数:Height 指定矩形的高度,范围:0~64
  * 参    数:IsFilled 指定反色范围(空心/实心)
  *           范围:OLED_UNFILLED		仅反色矩形边框
  *                 OLED_FILLED			反色整个矩形区域
  * 返 回 值:无
  * 说    明:1. 当X或Y超出屏幕范围时,自动循环到另一侧
  *           2. 调用后需调用OLED_Update更新屏幕显示
  *           3. 连续调用两次可恢复原显示(反色两次等价于无操作)
  */
void OLED_ReverseRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
    /*参数检查,保证指定区域不会超出屏幕范围*/
    OLED_CHECK_RECTANGLE(X, Y, Width, Height);
    
    // 处理X/Y坐标循环(超出边界时自动循环)
    if (X >= 128) X %= 128;
    if (X < 0) X = 128 + (X % 128);
    if (Y >= 64) Y %= 64;
    if (Y < 0) Y = 64 + (Y % 64);
    
    uint8_t i, j;
    // 计算右下角坐标(用于边界判断)
    uint8_t x2 = (X + Width - 1) % 128;  // 右边框X坐标
    uint8_t y2 = (Y + Height - 1) % 64;   // 下边框Y坐标
    
    if (!IsFilled)  // 反色空心矩形(仅边框)
    {
        /* 1. 反色上下两条横线(完整范围,包含四个角点) */
        // 上横线(Y坐标=Y)
        for (i = X; i < X + Width; i++)
        {
            uint8_t curr_x = i % 128;
            OLED_GRAM[Y / 8][curr_x] ^= 0x01 << (Y % 8);
        }
        // 下横线(Y坐标=Y+Height-1)
        for (i = X; i < X + Width; i++)
        {
            uint8_t curr_x = i % 128;
            OLED_GRAM[y2 / 8][curr_x] ^= 0x01 << (y2 % 8);
        }
        
        /* 2. 反色左右两条竖线(缩小Y范围,避开四个角点) */
        // 竖线Y范围:从Y+1到Y+Height-2(不包含上下端点)
        uint8_t start_y = (Y + 1) % 64;
        uint8_t end_y = (Y + Height - 2) % 64;
        
        // 左边框(X坐标=X)
        // 处理Y循环:如果start_y <= end_y,直接循环；否则分两段(跨边界时)
        if (start_y <= end_y)
        {
            for (j = start_y; j <= end_y; j++)
            {
                OLED_GRAM[j / 8][X] ^= 0x01 << (j % 8);
            }
        }
        else
        {
            // 跨边界时:先从start_y到63,再从0到end_y
            for (j = start_y; j < 64; j++)
            {
                OLED_GRAM[j / 8][X] ^= 0x01 << (j % 8);
            }
            for (j = 0; j <= end_y; j++)
            {
                OLED_GRAM[j / 8][X] ^= 0x01 << (j % 8);
            }
        }
        
        // 右边框(X坐标=x2)
        if (start_y <= end_y)
        {
            for (j = start_y; j <= end_y; j++)
            {
                OLED_GRAM[j / 8][x2] ^= 0x01 << (j % 8);
            }
        }
        else
        {
            for (j = start_y; j < 64; j++)
            {
                OLED_GRAM[j / 8][x2] ^= 0x01 << (j % 8);
            }
            for (j = 0; j <= end_y; j++)
            {
                OLED_GRAM[j / 8][x2] ^= 0x01 << (j % 8);
            }
        }
    }
    else  // 反色实心矩形(整个区域,无需处理角点)
    {
        for (j = Y; j < Y + Height; j++)
        {
            for (i = X; i < X + Width; i++)
            {
                OLED_GRAM[(j % 64) / 8][i % 128] ^= 0x01 << (j % 8);
            }
        }
    }
}

/**
  * 函    数:OLED三角形
  * 参    数:X0 指定第一个端点的横坐标,范围:0~127
  * 参    数:Y0 指定第一个端点的纵坐标,范围:0~63
  * 参    数:X1 指定第二个端点的横坐标,范围:0~127
  * 参    数:Y1 指定第二个端点的纵坐标,范围:0~63
  * 参    数:X2 指定第三个端点的横坐标,范围:0~127
  * 参    数:Y2 指定第三个端点的纵坐标,范围:0~63
  * 参    数:IsFilled 指定三角形是否填充
  *           范围:OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
	uint8_t minx = X0, miny = Y0, maxx = X0, maxy = Y0;
	uint8_t i, j;
	int16_t vx[] = {X0, X1, X2};
	int16_t vy[] = {Y0, Y1, Y2};
	
	if (!IsFilled)			//指定三角形不填充
	{
		/*调用画线函数,将三个点用直线连接*/
		OLED_DrawLine(X0, Y0, X1, Y1, 1);
		OLED_DrawLine(X0, Y0, X2, Y2, 1);
		OLED_DrawLine(X1, Y1, X2, Y2, 1);
	}
	else					//指定三角形填充
	{
		/*找到三个点最小的X、Y坐标*/
		if (X1 < minx) {minx = X1;}
		if (X2 < minx) {minx = X2;}
		if (Y1 < miny) {miny = Y1;}
		if (Y2 < miny) {miny = Y2;}
		
		/*找到三个点最大的X、Y坐标*/
		if (X1 > maxx) {maxx = X1;}
		if (X2 > maxx) {maxx = X2;}
		if (Y1 > maxy) {maxy = Y1;}
		if (Y2 > maxy) {maxy = Y2;}
		
		/*最小最大坐标之间的矩形为可能需要填充的区域*/
		/*遍历此区域中所有的点*/
		/*遍历X坐标*/		
		for (i = minx; i <= maxx; i ++)
		{
			/*遍历Y坐标*/	
			for (j = miny; j <= maxy; j ++)
			{
				/*调用OLED_pnpoly,判断指定点是否在指定三角形之中*/
				/*如果在,则画点,如果不在,则不做处理*/
				if (OLED_pnpoly(3, vx, vy, i, j)) {OLED_DrawPoint(i, j, 1);}

			}
		}
	}
}

/**
  * 函    数:OLED画圆
  * 参    数:X 指定圆的圆心横坐标,范围:0~127
  * 参    数:Y 指定圆的圆心纵坐标,范围:0~63
  * 参    数:Radius 指定圆的半径,范围:0~255
  * 参    数:IsFilled 指定圆是否填充
  *           范围:OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawCircle(int16_t X, int16_t Y, int16_t Radius, uint8_t IsFilled)

{
	int16_t x, y, d, j;
	
	/*使用Bresenham算法画圆,可以避免耗时的浮点运算,效率更高*/
	/*参考文档:https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
	/*参考教程:https://www.bilibili.com/video/BV1VM4y1u7wJ*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*画每个八分之一圆弧的起始点*/
	OLED_DrawPoint(X + x, Y + y, 1);
	OLED_DrawPoint(X - x, Y - y, 1);
	OLED_DrawPoint(X + y, Y + x, 1);
	OLED_DrawPoint(X - y, Y - x, 1);
	
	if (IsFilled)		//指定圆填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在指定区域画点,填充部分圆*/
			OLED_DrawPoint(X, Y + j, 1);

		}
	}
	
	while (x < y)		//遍历X轴的每个点
	{
		x ++;
		if (d < 0)		//下一个点在当前点东方
		{
			d += 2 * x + 1;
		}
		else			//下一个点在当前点东南方
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*画每个八分之一圆弧的点*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X + y, Y + x, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - y, Y - x, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
		OLED_DrawPoint(X + y, Y - x, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X - y, Y + x, 1);
		
		if (IsFilled)	//指定圆填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点,填充部分圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
			
			/*遍历两侧部分*/
			for (j = -x; j < x; j ++)
			{
				/*在指定区域画点,填充部分圆*/
				OLED_DrawPoint(X - y, Y + j, 1);
				OLED_DrawPoint(X + y, Y + j, 1);
			}
		}
	}
}

/**
  * 函    数:OLED画椭圆
  * 参    数:X 指定椭圆的圆心横坐标,范围:0~127
  * 参    数:Y 指定椭圆的圆心纵坐标,范围:0~63
  * 参    数:A 指定椭圆的横向半轴长度,范围:0~255
  * 参    数:B 指定椭圆的纵向半轴长度,范围:0~255
  * 参    数:IsFilled 指定椭圆是否填充
  *           范围:OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x, y, j;
	int16_t a = A, b = B;
	float d1, d2;
	
	/*使用Bresenham算法画椭圆,可以避免部分耗时的浮点运算,效率更高*/
	/*参考链接:https://blog.csdn.net/myf_666/article/details/128167392*/
	
	x = 0;
	y = b;
	d1 = b * b + a * a * (-b + 0.5);
	
	if (IsFilled)	//指定椭圆填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在指定区域画点,填充部分椭圆*/
			OLED_DrawPoint(X, Y + j, 1);
		}
	}
	
	/*画椭圆弧的起始点*/
	OLED_DrawPoint(X + x, Y + y, 1);
	OLED_DrawPoint(X - x, Y - y, 1);
	OLED_DrawPoint(X - x, Y + y, 1);
	OLED_DrawPoint(X + x, Y - y, 1);
	
	/*画椭圆中间部分*/
	while (b * b * (x + 1) < a * a * (y - 0.5))
	{
		if (d1 <= 0)		//下一个点在当前点东方
		{
			d1 += b * b * (2 * x + 3);
		}
		else				//下一个点在当前点东南方
		{
			d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
			y --;
		}
		x ++;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点,填充部分椭圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
		}
		
		/*画椭圆中间部分圆弧*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
	}
	
	/*画椭圆两侧部分*/
	d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;
	
	while (y > 0)
	{
		if (d2 <= 0)		//下一个点在当前点东方
		{
			d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
			x ++;
			
		}
		else				//下一个点在当前点东南方
		{
			d2 += a * a * (-2 * y + 3);
		}
		y --;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历两侧部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点,填充部分椭圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
		}
		
		/*画椭圆两侧部分圆弧*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
	}
}

/**
  * 函    数:OLED画圆弧
  * 参    数:X 指定圆弧的圆心横坐标,范围:0~127
  * 参    数:Y 指定圆弧的圆心纵坐标,范围:0~63
  * 参    数:Radius 指定圆弧的半径,范围:0~255
  * 参    数:StartAngle 指定圆弧的起始角度,范围:-180~180
  *           水平向右为0度,水平向左为180度或-180度,下方为正数,上方为负数,顺时针旋转
  * 参    数:EndAngle 指定圆弧的终止角度,范围:-180~180
  *           水平向右为0度,水平向左为180度或-180度,下方为正数,上方为负数,顺时针旋转
  * 参    数:IsFilled 指定圆弧是否填充,填充后为扇形
  *           范围:OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值:无
  * 说    明:调用此函数后,要想真正地呈现在屏幕上,还需调用更新函数
  */
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/*此函数借用Bresenham算法画圆的方法*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*在画圆的每个点时,判断指定点是否在指定角度内,在,则画点,不在,则不做处理*/
	if (OLED_IsInAngle(x, y, StartAngle, EndAngle))	{OLED_DrawPoint(X + x, Y + y, 1);}
	if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y, 1);}
	if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x, 1);}
	if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x, 1);}

	
	if (IsFilled)	//指定圆弧填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在填充圆的每个点时,判断指定点是否在指定角度内,在,则画点,不在,则不做处理*/
			if (OLED_IsInAngle(0, j, StartAngle, EndAngle)) {OLED_DrawPoint(X, Y + j, 1);}

		}
	}
	
	while (x < y)		//遍历X轴的每个点
	{
		x ++;
		if (d < 0)		//下一个点在当前点东方
		{
			d += 2 * x + 1;
		}
		else			//下一个点在当前点东南方
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*在画圆的每个点时,判断指定点是否在指定角度内,在,则画点,不在,则不做处理*/
		if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + y, 1);}
		if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x, 1);}
		if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y, 1);}
		if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x, 1);}
		if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y  + y, 1);}
		if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y - x, 1);}
		if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + y, 1);}
		if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + x, 1);}
		
		if (IsFilled)	//指定圆弧填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在填充圆的每个点时,判断指定点是否在指定角度内,在,则画点,不在,则不做处理*/
				if (OLED_IsInAngle(x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + j, 1);}
				if (OLED_IsInAngle(-x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + j, 1);}
			}
			
			/*遍历两侧部分*/
			for (j = -x; j < x; j ++)
			{
				/*在填充圆的每个点时,判断指定点是否在指定角度内,在,则画点,不在,则不做处理*/
				if (OLED_IsInAngle(-y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + j, 1);}
				if (OLED_IsInAngle(y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + j, 1);}
			}
		}
	}
}

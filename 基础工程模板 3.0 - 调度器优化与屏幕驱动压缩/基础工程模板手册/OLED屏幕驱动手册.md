# OLED屏幕驱动手册

## 目录

1. [概述](#1-概述)
   1.1 [主要特性](#11-主要特性)
2. [硬件连接](#2-硬件连接)
   2.1 [引脚定义](#21-引脚定义)
   2.2 [通信配置](#22-通信配置)
3. [API函数说明](#3-api函数说明)
   3.1 [初始化函数](#31-初始化函数)
   3.2 [显示更新函数](#32-显示更新函数)
   3.3 [图形绘制函数](#33-图形绘制函数)
   3.4 [文本显示函数](#34-文本显示函数)
   3.5 [特殊功能函数](#35-特殊功能函数)
4. [快速入门示例](#4-快速入门示例)
   4.1 [基本显示示例](#41-基本显示示例)
   4.2 [动态数据显示示例](#42-动态数据显示示例)
   4.3 [时间序列数据显示示例](#43-时间序列数据显示示例)
5. [高级特性说明](#5-高级特性说明)
   5.1 [双缓冲区机制](#51-双缓冲区机制)
   5.2 [DMA传输优化](#52-dma传输优化)
   5.3 [字体和汉字显示](#53-字体和汉字显示)
   5.4 [性能优化技巧](#54-性能优化技巧)
6. [注意事项](#6-注意事项)
7. [常见问题解答](#7-常见问题解答)
   7.1 [屏幕显示异常或不显示](#71-屏幕显示异常或不显示)
   7.2 [显示内容闪烁](#72-显示内容闪烁)
   7.3 [汉字显示乱码](#73-汉字显示乱码)
   7.4 [DMA传输失败](#74-dma传输失败)

## 1. 概述

本手册介绍了128x64分辨率OLED屏幕的驱动实现，该驱动支持I2C通信方式（硬件I2C或软件I2C可选），提供了丰富的图形绘制、文本显示和屏幕控制功能。

### 1.1 主要特性

- 128x64像素分辨率OLED屏幕驱动
- 支持硬件I2C和软件I2C两种通信方式
- 支持DMA传输（仅硬件I2C模式下）
- 双缓冲区实现，避免显示闪烁
- 丰富的图形绘制功能：点、线、矩形、圆形、椭圆、三角形、圆弧等
- 文本显示功能：ASCII字符显示、字符串显示、汉字显示
- 高级功能：折线图绘制、时间横轴折线图等
- 屏幕控制：清屏、反色、局部更新等

## 2. 硬件连接

### 2.1 引脚定义

| 引脚 | 功能 | 对应GPIO |
|------|------|----------|
| SCL  | 时钟线 | GPIOB_PIN_6 |
| SDA  | 数据线 | GPIOB_PIN_7 |

### 2.2 通信配置

- I2C地址：0x78
- 通信速率：最高1.3MHz（硬件I2C模式）

## 3. API函数说明

### 3.1 初始化函数

#### OLED_Init
```c
void OLED_Init(void);
```
初始化OLED屏幕，包括I2C初始化、控制器初始化和显示设置。

**参数**：无
**返回值**：无
**使用说明**：必须在使用其他OLED函数前调用此函数

#### OLED_Clear
```c
void OLED_Clear(void);
```
清空OLED显示缓冲区。

**参数**：无
**返回值**：无
**使用说明**：调用后需要使用OLED_UpdateAsync函数才能在屏幕上看到效果

### 3.2 显示更新函数

#### OLED_Update
```c
void OLED_Update(void);
```
将显示缓冲区内容更新到OLED屏幕（阻塞式）。

**参数**：无
**返回值**：无
**使用说明**：在完成图形绘制或文本显示后调用此函数，将内容显示到屏幕上

#### OLED_UpdateAsync
```
uint8_t OLED_UpdateAsync(void);
```
启动非阻塞式更新（硬件IIC模式下使用DMA传输，软件IIC模式下会进行阻塞式传输）。

**参数**：无
**返回值**：1-成功启动传输（硬件IIC+DMA模式）或完成传输（软件IIC模式），0-传输已在进行中（仅硬件IIC+DMA模式有效）
**使用说明**：适合需要快速响应的应用场景，会根据当前IIC模式自动选择传输方式

#### OLED_UpdateArea
```c
void OLED_UpdateArea(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
```
更新指定区域的显示内容。

**参数**：
- x1, y1：区域左上角坐标
- x2, y2：区域右下角坐标
**返回值**：无
**使用说明**：用于局部更新，提高显示效率

### 3.3 图形绘制函数

#### OLED_DrawPoint
```c
static inline void OLED_DrawPoint(int16_t x, int16_t y, uint8_t Color);
```
在指定位置绘制一个点。

**参数**：
- x, y：点的坐标
- Color：颜色（OLED_COLOR_BLACK或OLED_COLOR_WHITE）
**返回值**：无

#### OLED_DrawLine
```c
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t Color);
```
绘制一条直线。

**参数**：
- x1, y1：起点坐标
- x2, y2：终点坐标
- Color：颜色
**返回值**：无
**使用说明**：使用Bresenham算法，避免浮点运算，效率较高

#### OLED_DrawRectangle
```c
void OLED_DrawRectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t Color);
```
绘制矩形。

**参数**：
- x1, y1：左上角坐标
- x2, y2：右下角坐标
- Color：颜色
**返回值**：无

#### OLED_DrawFillRectangle
```c
void OLED_DrawFillRectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t Color);
```
绘制填充矩形。

**参数**：与OLED_DrawRectangle相同
**返回值**：无

#### OLED_DrawCircle
```c
void OLED_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t Color);
```
绘制圆形。

**参数**：
- x0, y0：圆心坐标
- r：半径
- Color：颜色
**返回值**：无
**使用说明**：使用Bresenham算法

#### OLED_DrawFillCircle
```c
void OLED_DrawFillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t Color);
```
绘制填充圆形。

**参数**：与OLED_DrawCircle相同
**返回值**：无

### 3.4 文本显示函数

#### OLED_Printf
```c
void OLED_Printf(int16_t x, int16_t y, uint8_t FontSize, const char *format, ...);
```
使用格式化字符串显示文本。

**参数**：
- x, y：显示位置坐标
- FontSize：字体大小（OLED_6X8或OLED_8X16）
- format：格式化字符串
- ...：可变参数
**返回值**：无
**使用说明**：支持%d、%s、%f等格式化输出，类似于标准C库的printf函数

#### OLED_ShowImage
```c
void OLED_ShowImage(int16_t x, int16_t y, uint8_t width, uint8_t height, const uint8_t *Image);
```
显示图像。

**参数**：
- x, y：显示位置坐标
- width, height：图像宽高
- Image：图像数据
**返回值**：无
**使用说明**：图像数据格式为按行排列的点阵数据

### 3.5 特殊功能函数

#### OLED_Reverse
```c
void OLED_Reverse(void);
```
将整个屏幕显示内容反色。

**参数**：无
**返回值**：无
**使用说明**：调用后需要使用OLED_Update函数才能看到效果

#### OLED_ReverseArea
```c
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
```
将指定区域显示内容反色。

**参数**：
- X, Y：区域左上角坐标
- Width, Height：区域宽高
**返回值**：无

#### OLED_DrawLineChart
```c
void OLED_DrawLineChart(int16_t x0, int16_t y0, int16_t width, int16_t height, 
                        const int16_t* xData, const int16_t* yData, uint8_t pointCount, 
                        uint8_t color, uint8_t drawAxis);
```
绘制折线图。

**参数**：
- x0, y0：图表左上角坐标
- width, height：图表宽高
- xData, yData：X轴和Y轴数据数组
- pointCount：数据点数量
- color：折线颜色
- drawAxis：是否绘制坐标轴（1-绘制，0-不绘制）
**返回值**：无

#### OLED_DrawTimeLineChart
```c
void OLED_DrawTimeLineChart(int16_t x0, int16_t y0, int16_t width, int16_t height, 
                            const void* yData, DataType dataType, uint8_t pointCount, uint16_t timeInterval, 
                            uint8_t color, uint8_t drawAxis, uint8_t showLatest);
```
绘制时间横轴折线图。

**参数**：
- x0, y0：图表左上角坐标
- width, height：图表宽高
- yData：Y轴数据数组（支持int16_t或float类型）
- dataType：数据类型（DATA_TYPE_INT16或DATA_TYPE_FLOAT）
- pointCount：数据点数量
- timeInterval：数据点之间的时间间隔
- color：折线颜色
- drawAxis：是否绘制坐标轴
- showLatest：是否只显示最近20个数据点
**返回值**：无

## 4. 快速入门示例

### 4.1 基本显示示例

```c
#include "OLED.h"

void OLED_Demo(void) {
    // 初始化OLED
    OLED_Init();
    OLED_Clear();
    
    // 显示文本
    OLED_Printf(0, 0, OLED_8X16, "Hello World!");
    OLED_Printf(0, 16, OLED_8X16, "OLED Display");
    OLED_Printf(0, 32, OLED_6X8, "128x64 Resolution");
    
    // 绘制图形
    OLED_DrawLine(0, 48, 127, 48, OLED_COLOR_WHITE);
    OLED_DrawRectangle(10, 50, 50, 60, OLED_COLOR_WHITE);
    OLED_DrawCircle(90, 55, 8, OLED_COLOR_WHITE);
    
    // 更新显示
    OLED_Update();
}
```

### 4.2 动态数据显示示例

```c
#include "OLED.h"

void OLED_DynamicDemo(void) {
    int16_t data[20];
    uint8_t i = 0;
    
    // 初始化OLED
    OLED_Init();
    
    // 循环显示动态数据
    while (1) {
        // 生成模拟数据
        data[i] = rand() % 100;
        i = (i + 1) % 20;
        
        // 清空屏幕
        OLED_Clear();
        
        // 显示标题
        OLED_Printf(0, 0, OLED_8X16, "Dynamic Data");
        
        // 绘制折线图
        OLED_DrawLineChart(10, 20, 108, 40, NULL, data, 20, OLED_COLOR_WHITE, 1);
        
        // 更新显示
        OLED_Update();
        
        // 延时
        Delay_ms(500);
    }
}
```

### 4.3 时间序列数据显示示例

```c
#include "OLED.h"

void OLED_TimeSeriesDemo(void) {
    float sensorData[100];
    uint8_t count = 0;
    uint16_t timeInterval = 10; // 10秒间隔
    
    // 初始化OLED
    OLED_Init();
    
    // 模拟数据采集和显示
    while (count < 100) {
        // 模拟传感器数据
        sensorData[count] = 25.0 + 10.0 * sin((float)count * 0.2);
        count++;
        
        // 清空屏幕
        OLED_Clear();
        
        // 显示标题
        OLED_Printf(0, 0, OLED_8X16, "Sensor Data");
        
        // 绘制时间横轴折线图，只显示最近20个数据点
        OLED_DrawTimeLineChart(0, 20, 128, 44, sensorData, DATA_TYPE_FLOAT, count, timeInterval, 
                              OLED_COLOR_WHITE, 1, 1);
        
        // 更新显示
        OLED_Update();
        
        // 延时
        Delay_ms(1000);
    }
}
```

## 5. 高级特性说明

### 5.1 双缓冲区机制

OLED驱动采用双缓冲区机制，通过`OLED_DOUBLE_BUFFER`宏控制是否启用。启用后，系统会维护两个显存缓冲区：
- 活动缓冲区：用于当前绘制操作
- 显示缓冲区：当前正在显示的内容

在非阻塞更新模式下，调用`OLED_UpdateAsync`会启动DMA传输，并自动切换缓冲区，实现无闪烁显示。

### 5.2 DMA传输优化

在硬件I2C模式下，可以通过`OLED_USE_DMA`宏启用DMA传输功能，大幅提高显示更新速度，减少CPU占用。DMA配置如下：
- DMA通道：DMA1_Channel6
- 优先级：高
- 支持传输完成中断

### 5.3 字体和汉字显示

驱动支持两种ASCII字体：
- 6x8字体：宽6像素，高8像素
- 8x16字体：宽8像素，高16像素

汉字显示通过哈希表进行优化，支持快速查找汉字字模数据。

### 5.4 性能优化技巧

1. 使用局部更新函数`OLED_UpdateArea`替代全屏更新，提高显示效率
2. 对于静态内容，避免重复绘制
3. 启用DMA传输功能，减少CPU占用
4. 合理使用双缓冲区机制，避免显示闪烁
5. 对于大量文本显示，优先使用`OLED_Printf`函数

## 6. 注意事项

1. 在使用任何OLED绘图函数后，必须调用`OLED_Update`或`OLED_UpdateAsync`才能在屏幕上看到效果
2. 坐标范围：X坐标0-127，Y坐标0-63，超出范围的坐标将被自动截断
3. 在硬件I2C模式下，确保正确配置GPIO引脚为复用开漏模式
4. 当使用DMA传输时，确保正确配置NVIC中断优先级
5. 绘制复杂图形时，注意优化代码，避免过度占用CPU资源
6. 长时间不使用OLED时，可以考虑关闭显示以节省电量

## 7. 常见问题解答

### 7.1 屏幕显示异常或不显示

- 检查I2C通信是否正常
- 确认OLED_Init函数是否被正确调用
- 检查绘制内容后是否调用了OLED_Update函数
- 验证硬件连接是否正确

### 7.2 显示内容闪烁

- 启用双缓冲区机制
- 使用OLED_UpdateAsync进行非阻塞更新
- 避免频繁的全屏刷新，尽量使用局部更新

### 7.3 汉字显示乱码

- 确保使用的汉字在字模库中存在
- 检查字符串编码是否正确
- 验证字模数据是否正确加载

### 7.4 DMA传输失败

- 检查DMA配置是否正确
- 确认中断处理函数是否正确实现
- 验证I2C和DMA的时钟是否正确启用

---

本手册详细介绍了OLED屏幕驱动的各项功能和使用方法，用户可以根据实际需求选择合适的函数进行开发。如有任何问题，请参考相关源代码或联系技术支持。
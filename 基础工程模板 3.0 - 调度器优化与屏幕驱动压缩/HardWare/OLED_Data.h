#ifndef __OLED_DATA_H
#define __OLED_DATA_H

#include "stdint.h"

// 汉字字模结构体定义
typedef struct {
    const char* Name;          // 汉字名称
    const uint8_t Data[32];    // 字模数据(16x16)
    uint8_t Width;             // 宽度
    uint8_t Height;            // 高度
} ChineseCell_t;

// ASCII字模数据
extern const uint8_t OLED_F8x16[][16];
extern const uint8_t OLED_F6x8[][6];

// 汉字字模数据
extern const ChineseCell_t OLED_CF16x16[];
extern const uint16_t OLED_CF16x16_COUNT; // 汉字数量

// 图像数据
extern const uint8_t Diode[];
extern const uint8_t yi[];
extern const uint8_t er[];
extern const uint8_t wifi_int[];
extern const uint8_t wifi_out[];
extern const uint8_t server_out[];
extern const uint8_t server_int[];
extern const uint8_t home[];













// 函数声明
const uint8_t *OLED_FindChinese(const char *ch);

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/

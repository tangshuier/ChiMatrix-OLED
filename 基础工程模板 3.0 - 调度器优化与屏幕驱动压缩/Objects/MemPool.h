#ifndef __MEMPOOL_H
#define __MEMPOOL_H

#include <stdint.h>
#include <stdbool.h>

// 内存池配置
#define MEMPOOL_BLOCK_SIZE    32    // 每个块的大小（字节）
#define MEMPOOL_BLOCK_COUNT   32    // 块的数量

// 内存池初始化
void MemPool_Init(void);

// 从内存池分配内存
void* MemPool_Alloc(void);

// 释放内存回内存池
void MemPool_Free(void* ptr);

// 获取内存池使用情况（百分比）
uint8_t MemPool_GetUsage(void);

#endif
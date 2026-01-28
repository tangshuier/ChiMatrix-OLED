#include "MemPool.h"
#include "stm32f10x.h"

// 内存池结构
typedef struct {
    uint8_t pool[MEMPOOL_BLOCK_COUNT][MEMPOOL_BLOCK_SIZE];  // 内存池数据
    bool block_used[MEMPOOL_BLOCK_COUNT];                  // 块使用状态
    uint8_t free_blocks;                                   // 空闲块数量
} MemPool_t;

// 全局内存池实例
static MemPool_t mem_pool;

/**
 * @brief 初始化内存池
 * 
 * 功能：
 * 1. 将所有内存块标记为未使用
 * 2. 初始化空闲块计数
 */
void MemPool_Init(void) {
    // 初始化所有块为未使用状态
    for (uint8_t i = 0; i < MEMPOOL_BLOCK_COUNT; i++) {
        mem_pool.block_used[i] = false;
    }
    mem_pool.free_blocks = MEMPOOL_BLOCK_COUNT;
}

/**
 * @brief 从内存池分配一个内存块
 * 
 * @return void* 分配的内存块指针，失败返回NULL
 */
void* MemPool_Alloc(void) {
    // 查找第一个空闲块
    for (uint8_t i = 0; i < MEMPOOL_BLOCK_COUNT; i++) {
        if (!mem_pool.block_used[i]) {
            // 标记块为已使用
            mem_pool.block_used[i] = true;
            mem_pool.free_blocks--;
            
            return (void*)mem_pool.pool[i];
        }
    }
    
    return 0;  // 没有空闲块
}

/**
 * @brief 释放内存块回内存池
 * 
 * @param ptr 要释放的内存块指针
 */
void MemPool_Free(void* ptr) {
    // 检查指针是否有效
    if (!ptr) return;
    
    // 计算指针所在的块索引
    uint32_t block_index = ((uint8_t*)ptr - (uint8_t*)mem_pool.pool) / MEMPOOL_BLOCK_SIZE;
    
    // 验证索引有效性
    if (block_index < MEMPOOL_BLOCK_COUNT) {
        if (mem_pool.block_used[block_index]) {
            mem_pool.block_used[block_index] = false;
            mem_pool.free_blocks++;
        }
    }
}

/**
 * @brief 获取内存池使用率
 * 
 * @return float 内存池使用率（百分比）
 */
float MemPool_GetUsage(void) {
    return ((float)(MEMPOOL_BLOCK_COUNT - mem_pool.free_blocks) * 100.0f) / MEMPOOL_BLOCK_COUNT;
}

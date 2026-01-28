#include "CPUUtil.h"
#include <string.h>

// 全局CPU利用率数据
CPUUtil_t cpu_util;

// 用于移动平均的历史数据缓冲区大小
#define CPU_UTIL_HISTORY_SIZE 3  // 减少历史缓冲区大小，让响应更快

// 历史利用率数据，用于移动平均计算
static float cpu_usage_history[CPU_UTIL_HISTORY_SIZE] = {0.0f};
static uint8_t history_index = 0;

/**
 * @brief 初始化CPU利用率计算模块
 */
void CPUUtil_Init(void) {
    memset(&cpu_util, 0, sizeof(CPUUtil_t));
    // 初始化历史缓冲区为一个小的非零值，避免初始阶段一直显示为0
    for (uint8_t i = 0; i < CPU_UTIL_HISTORY_SIZE; i++) {
        cpu_usage_history[i] = 0.1f;  // 给一个小的初始值
    }
    history_index = 0;
    cpu_util.last_sample_time = system_time;
    cpu_util.cpu_usage = 0.1f;  // 初始化CPU利用率为一个小值
}

/**
 * @brief 更新任务运行时间
 * @param task_id 任务ID
 * @param run_time 本次运行的时间
 */
void CPUUtil_UpdateTaskRunTime(TaskID task_id, uint32_t run_time) {
    if (task_id < MAX_TASKS) {
        // 使用64位整数可以有效防止溢出，不再需要复杂的溢出检查
        cpu_util.task_run_time[task_id] += run_time;
        cpu_util.total_run_time += run_time;
    }
}

/**
 * @brief 计算CPU利用率
 * @return 当前CPU总体利用率(%)
 */
float CPUUtil_Calculate(void) {
    // 计算从上次采样到现在的时间差
    uint32_t current_time = system_time;
    uint32_t elapsed_time;
    
    if (current_time >= cpu_util.last_sample_time) {
        elapsed_time = current_time - cpu_util.last_sample_time;
    } else {
        // 处理系统时间溢出
        elapsed_time = 0xFFFFFFFF - cpu_util.last_sample_time + current_time + 1;
    }
    
    // 确保有足够的时间差进行有效计算，但降低阈值以提高响应速度
    if (elapsed_time < 10) {  // 只要有10ms的时间差就可以计算
        return cpu_util.cpu_usage;
    }
    
    // 计算总体CPU利用率
    float calculated_usage = 0.0f;
    
    if (elapsed_time > 0) {
        // 使用64位整数和float计算，保持4位小数精度
        calculated_usage = (float)cpu_util.total_run_time * 100.0f / elapsed_time;
        
        // 确保利用率不超过100%
        if (calculated_usage > 100.0f) {
            calculated_usage = 100.0;
        }
        
        // 应用移动平均滤波以平滑波动，但使用加权平均给当前值更高权重
        cpu_usage_history[history_index] = calculated_usage;
        history_index = (history_index + 1) % CPU_UTIL_HISTORY_SIZE;
        
        // 计算加权历史平均值，当前值权重更高
        float avg_usage = 0.0f;
        for (uint8_t i = 0; i < CPU_UTIL_HISTORY_SIZE; i++) {
            // 给最近的值更高权重
            uint8_t weight = (CPU_UTIL_HISTORY_SIZE - i);
            avg_usage += cpu_usage_history[(history_index + i) % CPU_UTIL_HISTORY_SIZE] * weight;
        }
        calculated_usage = avg_usage / ((CPU_UTIL_HISTORY_SIZE * (CPU_UTIL_HISTORY_SIZE + 1)) / 2);  // 权重总和
        
        // 更新全局CPU利用率
        cpu_util.cpu_usage = calculated_usage;
        
        // 计算每个任务的CPU利用率
        for (TaskID i = 0; i < MAX_TASKS; i++) {
            if (cpu_util.task_run_time[i] > 0 && elapsed_time > 0) {
                // 使用64位整数和float计算任务利用率
                cpu_util.task_usage[i] = (float)cpu_util.task_run_time[i] * 100.0f / elapsed_time;
            } else {
                cpu_util.task_usage[i] = 0.0f;
            }
        }
    }
    
    // 重置计数器，准备下一次采样
    cpu_util.total_run_time = 0;
    memset(cpu_util.task_run_time, 0, sizeof(cpu_util.task_run_time));
    cpu_util.last_sample_time = current_time;
    
    return calculated_usage;
}

/**
 * @brief 获取指定任务的CPU利用率
 * @param task_id 任务ID
 * @return 任务的CPU利用率(%)
 */
float CPUUtil_GetTaskUsage(TaskID task_id) {
    if (task_id < MAX_TASKS) {
        return cpu_util.task_usage[task_id];
    }
    return 0.0f;
}

/**
 * @brief 获取总体CPU利用率
 * @return 总体CPU利用率(%)
 */
float CPUUtil_GetTotalUsage(void) {
    return cpu_util.cpu_usage;
}

/**
 * @brief CPU利用率计算任务函数
 * 定期计算并更新CPU利用率
 */
void CPUUtil_CalculateTask(void) {
    // 定期计算CPU利用率
    CPUUtil_Calculate();
}

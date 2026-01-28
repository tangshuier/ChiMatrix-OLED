#ifndef __CPU_UTIL_H
#define __CPU_UTIL_H

#include "task.h"
#define MAX_TASKS 15
// CPU利用率计算的采样周期(单位: 系统滴答数)
#define CPU_UTIL_SAMPLE_INTERVAL 1000

// CPU利用率数据结构
typedef struct {
    uint64_t total_run_time;        // 总运行时间(使用64位防止溢出)
    uint64_t task_run_time[MAX_TASKS]; // 每个任务的运行时间(使用64位防止溢出)
    float cpu_usage;                // CPU总体利用率(%) - 4位小数精度
    float task_usage[MAX_TASKS];    // 每个任务的CPU利用率(%) - 4位小数精度
    uint32_t last_sample_time;      // 上次采样时间
} CPUUtil_t;

// 全局CPU利用率数据
extern CPUUtil_t cpu_util;

/**
 * @brief 初始化CPU利用率计算模块
 */
void CPUUtil_Init(void);

/**
 * @brief 更新任务运行时间
 * @param task_id 任务ID
 * @param run_time 本次运行的时间
 */
void CPUUtil_UpdateTaskRunTime(TaskID task_id, uint32_t run_time);

/**
 * @brief 计算CPU利用率
 * @return 当前CPU总体利用率(%)
 */
float CPUUtil_Calculate(void);

/**
 * @brief 获取指定任务的CPU利用率
 * @param task_id 任务ID
 * @return 任务的CPU利用率(%)
 */
float CPUUtil_GetTaskUsage(TaskID task_id);

/**
 * @brief 获取总体CPU利用率
 * @return 总体CPU利用率(%)
 */
float CPUUtil_GetTotalUsage(void);

/**
 * @brief CPU利用率计算任务函数
 * 定期计算并更新CPU利用率
 */
void CPUUtil_CalculateTask(void);

#endif /* __CPU_UTIL_H */

#ifndef __TASK_H
#define __TASK_H

#include "../Start/stm32f10x.h"

// 声明全局变量（在task.c中定义）
extern volatile uint32_t system_time; // 系统时间计数器（单位：毫秒）

// 任务ID类型定义（0-9为有效ID，0xFF表示无效ID）
typedef uint8_t TaskID;
#define INVALID_TASK_ID 0xFF

// 任务状态定义
typedef enum
{
    TASK_READY,    // 任务就绪（等待执行）
    TASK_RUNNING,  // 任务正在执行中
    TASK_SUSPENDED // 任务挂起（不参与调度）
} TaskState;

// 优先级功能已移除，任务按执行时间排序

// 任务链表节点结构
typedef struct TaskNode
{
    TaskID id;              // 任务ID
    uint32_t next_run_time; // 下次执行时间
    struct TaskNode *next;  // 指向下一个节点
} TaskNode;

// 最大任务数量
#define MAX_TASKS 15

// 任务节点映射表，用于快速查找任务对应的链表节点
extern TaskNode *task_nodes[MAX_TASKS];

// 任务控制块 (TCB) - 存储任务的所有信息
typedef struct
{
    void (*task_func)(void);   // 任务函数指针（无参数无返回值）
    uint32_t interval;         // 执行间隔（毫秒），0表示每次循环都执行
    uint32_t last_run;         // 上次执行时间（系统时间戳）
    uint32_t max_exec_time;    // 最大执行时间（用于性能监控）
    uint32_t actual_exec_time; // 实际执行时间（毫秒）
    TaskState state;           // 当前任务状态
    const char *name;          // 任务名称（调试用）
    uint8_t is_active;         // 任务是否激活
} Task_t;

/********************* 任务管理接口 *********************/

/**
 * @brief 添加一次性任务
 * @param task_func 任务函数指针（void func(void)形式）
 * @param delay 延迟执行时间（毫秒）
 * @param priority 任务优先级
 * @param name 任务名称（用于调试）
 * @return TaskID 分配的任务ID（INVALID_TASK_ID表示添加失败）
 *
 * @note 一次性任务会在执行一次后自动删除
 */
TaskID Task_AddOneShot(void (*task_func)(void),
                       uint32_t delay,
                       const char *name);

/**
 * @brief 初始化任务系统
 * @note 必须在添加任何任务前调用
 */
void Task_Init(void);

/**
 * @brief 添加新任务
 * @param task_func 任务函数指针（void func(void)形式）
 * @param interval 执行间隔（毫秒），0表示每次调度循环都执行
 * @param priority 任务优先级
 * @param name 任务名称（用于调试）
 * @return TaskID 分配的任务ID（INVALID_TASK_ID表示添加失败）
 *
 * @example
 * TaskID myTask = Task_Add(MyTaskFunc, 100, PRIORITY_NORMAL, "MyTask");
 */
TaskID Task_Add(void (*task_func)(void),
                uint32_t interval,
                const char *name);

/**
 * @brief 删除任务
 * @param id 要删除的任务ID
 *
 * @note 删除后任务不再执行，ID可被新任务重用
 */
void Task_Remove(TaskID id);

/**
 * @brief 挂起任务
 * @param id 要挂起的任务ID
 *
 * @note 挂起的任务保留状态，可通过Task_Resume恢复
 */
void Task_Suspend(TaskID id);

/**
 * @brief 恢复挂起的任务
 * @param id 要恢复的任务ID
 *
 * @note 恢复后任务会立即参与调度（重置last_run时间）
 */
void Task_Resume(TaskID id);

/**
 * @brief 修改任务执行间隔
 * @param id 任务ID
 * @param new_interval 新的执行间隔(毫秒)
 */
void Task_ChangeInterval(TaskID id, int new_interval);

/**
 * @brief 任务延时函数
 * @param id 任务ID
 * @param delay_ms 延时时长(毫秒)
 *
 * @note 该函数会将任务挂起指定时间，期间不会阻塞其他任务
 *       延时结束后任务会自动恢复执行
 */
void Task_Delay(TaskID id, uint32_t delay_ms);

// 简单协程实现 - 允许任务在延时后从当前位置继续执行

// 定义协程上下文结构
typedef struct
{
    uint8_t state;       // 协程状态
    uint32_t delay_time; // 延时时间点
} CoroutineContext;

// 全局协程上下文数组
extern CoroutineContext coroutine_contexts[MAX_TASKS];

// 协程延时宏 - 延时后从下一行继续执行
#define COROUTINE_DELAY(id, ms)                                   \
    do                                                            \
    {                                                             \
        if (coroutine_contexts[id].state == __LINE__)             \
        {                                                         \
            if (system_time >= coroutine_contexts[id].delay_time) \
            {                                                     \
                coroutine_contexts[id].state = __LINE__ + 1;      \
            }                                                     \
            else                                                  \
            {                                                     \
                return;                                           \
            }                                                     \
        }                                                         \
        else if (coroutine_contexts[id].state < __LINE__)         \
        {                                                         \
            coroutine_contexts[id].state = __LINE__;              \
            coroutine_contexts[id].delay_time = system_time + ms; \
            return;                                               \
        }                                                         \
    } while (0)

/**
 * @brief 任务中断延时宏
 * @param id 任务ID
 * @param ms 延时时长(毫秒)
 *
 * 功能：中断当前任务执行，延时期间任务不会被调用，延时结束后从下一行代码继续执行
 * 用法：在任务函数中任意位置添加此宏，即可实现简单的非阻塞延时
 */
#define TASK_DELAY(id, ms) do { \
    static uint8_t resume_flag = 0; \
    if (!resume_flag) { \
        /* 第一次执行时，设置协程状态并挂起任务 */ \
        coroutine_contexts[id].state = 1; \
        resume_flag = 1; \
        Task_Delay(id, ms); \
        /* 必须立即return来中断函数执行，这是关键 */ \
        return; \
    } else { \
        /* 延时结束后恢复执行，清除标志 */ \
        resume_flag = 0; \
        coroutine_contexts[id].state = 0; \
    } \
} while (0)

// 协程结束宏
#define COROUTINE_END(id) coroutine_contexts[id].state = 0;

    /**
     * @brief 运行任务调度器
     * @note 在主循环中不断调用此函数
     */
    void Task_RunScheduler(void);

/**
 * @brief 获取当前系统时间
 * @return uint32_t 系统时间（毫秒）
 */
static inline uint32_t Task_GetSystemTime(void)
{
    return system_time;
}

/**
 * @brief 获取指定任务的实际执行时间
 * @param id 任务ID
 * @return uint32_t 任务实际执行时间差（系统时间单位）
 * @note 如果任务不存在或未激活，返回0xFFFFFFFF
 */
uint32_t Task_GetActualRunInterval(TaskID id);

// 任务就绪和标志清除功能已在链表实现中内置

/**
 * @brief 系统心跳更新（由SysTick中断调用）
 * @note 每毫秒调用一次，更新系统时间和任务状态
 */
void Task_UpdateTick(void);

#endif

#include "Task.h"
#include <string.h>
#include "CPUUtil.h"

// 任务数组(存储所有任务的信息)
Task_t tasks[MAX_TASKS];

// 任务节点映射表定义
TaskNode* task_nodes[MAX_TASKS];

// 系统时间计数器
volatile uint32_t system_time = 0;

// 用于存储延时任务需要恢复的原任务ID
static TaskID delayed_task_ids[MAX_TASKS] = {INVALID_TASK_ID};

// 静态分配的任务节点数组
static TaskNode task_node_pool[MAX_TASKS];
static uint8_t node_pool_used[MAX_TASKS] = {0};

// 协程上下文数组定义
CoroutineContext coroutine_contexts[MAX_TASKS];

// 前向声明
static TaskNode* Task_NodeAlloc(void);
static void Task_NodeFree(TaskNode* node);

// 任务链表头指针（按下次执行时间排序）
static TaskNode* task_list_head = NULL;

/**
 * @brief 将任务节点插入到按时间排序的链表中
 * 
 * @param node 要插入的节点
 */
static void InsertTaskNode(TaskNode* node) {
    // 确保节点的next指针为NULL，避免环形引用
    node->next = NULL;
    
    if (task_list_head == NULL || node->next_run_time < task_list_head->next_run_time) {
        // 插入到链表头部
        node->next = task_list_head;
        task_list_head = node;
    } else {
        // 插入到链表中间或尾部
        TaskNode* current = task_list_head;
        while (current->next != NULL && current->next->next_run_time <= node->next_run_time) {
            current = current->next;
        }
        node->next = current->next;
        current->next = node;
    }
}

/**
 * @brief 从链表中移除任务节点但不释放
 * 
 * @param id 任务ID
 * @return TaskNode* 被移除的节点指针，如果未找到返回NULL
 */
static TaskNode* RemoveTaskNodeNoFree(TaskID id) {
    if (task_list_head == NULL) return NULL;
    
    // 如果是头节点
    if (task_list_head->id == id) {
        TaskNode* temp = task_list_head;
        task_list_head = task_list_head->next;
        return temp;
    }
    
    // 查找节点
    TaskNode* current = task_list_head;
    while (current->next != NULL && current->next->id != id) {
        current = current->next;
    }
    
    // 找到节点并移除
    if (current->next != NULL) {
        TaskNode* temp = current->next;
        current->next = temp->next;
        return temp;
    }
    
    return NULL;
}

/**
 * @brief 从链表中移除任务节点并释放
 * 
 * @param id 任务ID
 */
static void RemoveTaskNode(TaskID id) {
    TaskNode* node = RemoveTaskNodeNoFree(id);
    if (node != NULL) {
        Task_NodeFree(node);
        task_nodes[id] = NULL;
    }
}

/**
 * @brief 初始化任务系统
 * 
 * 功能：
 * 1. 初始化任务数组和节点映射表
 * 2. 初始化任务链表头
 * 3. 初始化节点池使用状态
 */
void Task_Init(void) {
    // 初始化任务数组
    memset(tasks, 0, sizeof(tasks));
    
    // 初始化任务节点映射表
    for (TaskID i = 0; i < MAX_TASKS; i++) {
        task_nodes[i] = NULL;
    }
    
    // 初始化协程上下文数组
    memset(coroutine_contexts, 0, sizeof(coroutine_contexts));
    
    // 初始化链表头
    task_list_head = NULL;
    
    // 初始化节点池使用状态数组（已在全局定义时初始化）
}

/**
 * @brief 添加任务
 * 
 * @param task_func 任务函数指针
 * @param interval 执行间隔(毫秒)
 * @param name 任务名称
 * @return TaskID 返回的任务ID(0-9)，INVALID_TASK_ID表示失败
 * 
 * 工作流程：
 * 1. 寻找一个空闲位置
 * 2. 从内存池分配任务节点
 * 3. 初始化任务结构体和节点
 * 4. 将节点插入到有序链表中
 */
TaskID Task_Add(void (*task_func)(void), 
                uint32_t interval, 
                const char *name) {
    // 参数检查
    if (task_func == NULL) {
        return INVALID_TASK_ID;
    }
    
    // 遍历任务槽寻找空闲位置
    for (TaskID i = 0; i < MAX_TASKS; i++) {
        if (!tasks[i].is_active) {
            // 从内存池分配任务节点
            TaskNode* node = Task_NodeAlloc();
            if (node == NULL) {
                return INVALID_TASK_ID;  // 内存池已满
            }
            
            // 初始化任务结构体
            tasks[i].task_func = task_func;
            tasks[i].interval = interval;
            tasks[i].last_run = system_time;
            tasks[i].max_exec_time = 0;
            tasks[i].actual_exec_time = 0;
            tasks[i].state = TASK_READY;
            tasks[i].name = name;
            tasks[i].is_active = 1;
            
            // 初始化任务节点
            node->id = i;
            node->next_run_time = system_time + interval;
            node->next = NULL;
            
            // 将节点插入到有序链表中
            InsertTaskNode(node);
            
            // 更新任务节点映射表
            task_nodes[i] = node;
            
            return i;
        }
    }
    return INVALID_TASK_ID; // 无可用任务槽
}

/**
 * @brief 删除任务
 * @param id 要删除的任务ID
 * 
 * 功能：
 * 1. 从链表中移除任务节点
 * 2. 释放内存池资源
 * 3. 重置任务状态
 */
void Task_Remove(TaskID id) {
    // 参数有效性检查
    if (id >= MAX_TASKS || !tasks[id].is_active) {
        return;
    }
    
    // 从链表中移除并释放任务节点
    RemoveTaskNode(id);
    
    // 重置任务状态，确保任务完全无效
    tasks[id].is_active = 0;
    tasks[id].task_func = NULL;
    tasks[id].state = TASK_SUSPENDED;
    tasks[id].interval = 0;
    tasks[id].name = NULL;
}

/**
 * @brief 挂起任务
 * @param id 要挂起的任务ID
 * 
 * 功能：
 * 1. 设置任务状态为挂起
 * 2. 从链表中移除任务节点（避免挂起任务占用链表资源）
 * 3. 不修改协程上下文状态，确保任务恢复后能从TASK_INTERRUPT_DELAY的下一行继续执行
 */
void Task_Suspend(TaskID id) {
    // 参数有效性检查
    if (id < MAX_TASKS && tasks[id].is_active) {
        // 设置任务状态为挂起
        tasks[id].state = TASK_SUSPENDED;
        
        // 从链表中移除节点但不释放，保留在内存池中
        TaskNode* node = RemoveTaskNodeNoFree(id);
        // 注意：挂起时不释放节点，保留节点指针供Resume使用
        if (node != NULL) {
            // 节点已从链表移除，更新映射表
            task_nodes[id] = node;
        }
        
        // 关键点：严格保持协程上下文状态不变
        // 协程状态(CoroutineContext.state)必须保持原值不变
        // 这是TASK_INTERRUPT_DELAY宏能够正确从断点继续执行的关键
    }
}

/**
 * @brief 恢复被挂起的任务
 * @param id 要恢复的任务ID
 * 
 * 功能：
 * 1. 设置任务状态为就绪
 * 2. 重新计算下次执行时间并排序
 * 3. 不修改协程上下文状态，确保任务能从TASK_INTERRUPT_DELAY的下一行继续执行
 */
void Task_Resume(TaskID id) {
    // 参数有效性检查
    if (id >= MAX_TASKS || !tasks[id].is_active || task_nodes[id] == NULL) {
        return;
    }
    
    // 设置任务状态为就绪
    tasks[id].state = TASK_READY;
    
    // 获取挂起时保存的节点
    TaskNode* node = task_nodes[id];
    
    // 先从链表中移除节点（如果已存在）
    TaskNode* temp = RemoveTaskNodeNoFree(id);
    if (temp != NULL) {
        // 如果节点在链表中，使用这个节点
        node = temp;
        task_nodes[id] = node;
    }
    
    // 关键点：对于使用TASK_INTERRUPT_DELAY的任务，立即执行，不等待interval
    // 检查协程状态是否非零（非零表示任务被TASK_INTERRUPT_DELAY中断）
    // 注意：现在支持使用静态变量跟踪执行状态的TASK_INTERRUPT_DELAY宏
    if (coroutine_contexts[id].state != 0) {
        // 协程状态非零，说明任务被TASK_INTERRUPT_DELAY中断，需要立即恢复执行
        node->next_run_time = system_time;  // 立即执行
    } else {
        // 协程状态为零，正常计算下次执行时间
        node->next_run_time = system_time + tasks[id].interval;
    }
    
    // 插入到有序链表
    InsertTaskNode(node);
    
    // 关键点：严格保持协程上下文状态不变
    // 协程状态(CoroutineContext.state)必须保持原值不变
    // 这是TASK_INTERRUPT_DELAY宏能够正确从断点继续执行的关键
}

/**
 * @brief 修改任务执行间隔
 * @param id 任务ID
 * @param new_interval 新的执行间隔(毫秒)
 */
void Task_ChangeInterval(TaskID id, int new_interval) {
    // 参数有效性检查
    if (id >= MAX_TASKS || !tasks[id].is_active) {
        return;
    }
    
    // 不允许将间隔设置为0，最小间隔为1毫秒
    // 这样可以避免与一次性任务的处理逻辑冲突
    if (new_interval <= 0) {
        new_interval = 1;  // 防止设置为0导致的调度问题
    }
    
    // 更新执行间隔
    tasks[id].interval = new_interval;
    
    // 重新排序任务
    if (task_nodes[id] != NULL) {
        // 直接调用修复后的Resume函数进行重新排序
        Task_Resume(id);
    }
}



/**
 * @brief 任务调度器(在主循环中调用)
 * 
 * 工作流程：
 * 1. 循环检查链表头部任务是否到达执行时间
 * 2. 执行所有到期的任务
 * 3. 计算执行时间并更新统计信息
 * 4. 对于周期性任务，重新计算下次执行时间并插入链表
 */
void Task_RunScheduler(void) {
    // 循环处理所有到期的任务，而不是只处理一个
    while (task_list_head != NULL && system_time >= task_list_head->next_run_time) {
        TaskNode* node = task_list_head;
        TaskID id = node->id;
        
        // 从链表中移除当前节点（先移除，避免任务执行过程中链表被修改导致问题）
        task_list_head = node->next;
        
        // 检查任务ID有效性
        if (id < MAX_TASKS) {
            Task_t* task = &tasks[id];
            
            // 检查任务状态和有效性
            if (task->is_active && task->state == TASK_READY) {
                // 记录开始时间
                uint32_t start_time = system_time;
                
                // 设置任务状态为运行中
                task->state = TASK_RUNNING;
                
                // 执行任务函数
                if (task->task_func != NULL) {
                    // 关键点：严格保持协程上下文状态不变
                    // 协程状态(CoroutineContext.state)由TASK_INTERRUPT_DELAY宏控制，
                    // 当任务从延时恢复时，宏会检查state值并确保从下一行代码继续执行
                    // 绝不能在任何地方重置或修改这个状态值
                    task->task_func();
                }
                
                // 计算并更新执行时间
                uint32_t exec_time = system_time - start_time;
                task->actual_exec_time = exec_time;
                
                // 更新最大执行时间
                if (exec_time > task->max_exec_time) {
                    task->max_exec_time = exec_time;
                }
                
                // 更新CPU利用率统计
                CPUUtil_UpdateTaskRunTime(id, exec_time);
                
                // 更新任务最后运行时间
                task->last_run = system_time;
                
                // 关键点：检查任务状态和协程状态
                // 1. 如果任务被挂起(TASK_SUSPENDED)，说明它可能使用了TASK_INTERRUPT_DELAY宏
                // 2. 此时不重新插入链表，节点已在Task_Suspend中被保留
                if (task->state == TASK_SUSPENDED) {
                    // 任务已被挂起，不重新插入链表
                    // 节点已在Task_Suspend中被保留，不需要释放
                    // 协程状态(coroutine_contexts[id].state)保持不变，等待Task_Resume恢复
                } else {
                    // 恢复任务状态为就绪
                    task->state = TASK_READY;
                    
                    // 检查任务是否为一次性任务
                    // 一次性任务是通过Task_AddOneShot添加的，interval为0
                    if (task->interval == 0) {
                        // 一次性任务，释放资源
                        Task_NodeFree(node);
                        task_nodes[id] = NULL;
                        task->is_active = 0;
                        
                        // 对于一次性任务，重置协程状态
                        coroutine_contexts[id].state = 0;
                    } else {
                        // 周期性任务，重新计算下次执行时间并插入链表
                        node->next_run_time = system_time + task->interval;
                        InsertTaskNode(node);
                    }
                }
            } else if (task->is_active) {
                // 任务存在但未就绪（如被挂起），重新插入链表
                InsertTaskNode(node);
            } else {
                // 任务已无效，释放节点
                Task_NodeFree(node);
                task_nodes[id] = NULL;
            }
        } else {
            // ID无效，释放节点
            Task_NodeFree(node);
        }
    }
}

/**
 * @brief 系统节拍处理(在SysTick中断中调用)
 * 
 * 工作流程：
 * 1. 更新系统时间(递增32位计数器)
 */
void Task_UpdateTick(void) {
    // 直接递增系统时间，利用无符号整数的自然溢出特性
    system_time++;
}

/**
 * @brief 清除任务就绪标志
 * @param id 任务ID
 * 
 * 注意：此函数在新的链表实现中已不再需要，但为了保持API兼容性而保留
 */
// Task_ClearReadyFlag功能已在链表实现中内置

/**
 * @brief 任务节点分配函数
 * @return TaskNode* 分配的节点指针，NULL表示失败
 */
static TaskNode* Task_NodeAlloc(void) {
    for (uint8_t i = 0; i < MAX_TASKS; i++) {
        if (node_pool_used[i] == 0) {
            node_pool_used[i] = 1;
            // 初始化节点，确保所有字段都是0
            memset(&task_node_pool[i], 0, sizeof(TaskNode));
            return &task_node_pool[i];
        }
    }
    return NULL;  // 节点池已满
}

/**
 * @brief 任务节点释放函数
 * @param node 要释放的节点指针
 */
static void Task_NodeFree(TaskNode* node) {
    if (node != NULL) {
        // 计算节点在数组中的索引并验证有效性
        uint8_t index = node - task_node_pool;
        if (index < MAX_TASKS) {
            // 标记为未使用并清零
            node_pool_used[index] = 0;
            memset(node, 0, sizeof(TaskNode));
        }
    }
}

/**
 * @brief 获取指定任务的实际执行时间
 * @param id 任务ID
 * @return uint32_t 任务实际执行时间差（系统时间单位）
 * @note 如果任务不存在或未激活，返回0xFFFFFFFF
 */
uint32_t Task_GetActualRunInterval(TaskID id) {
    // 检查任务ID是否有效且任务是否激活
    if (id >= MAX_TASKS || !tasks[id].is_active) {
        return 0xFFFFFFFF;
    }
    
    // 直接返回系统时间差
    return tasks[id].actual_exec_time;
}

// 定义一个恢复任务的函数
void ResumeTaskFunc(void) {
    // 查找当前正在执行的延迟任务，并获取需要恢复的原任务ID
    for (TaskID i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].is_active && tasks[i].state == TASK_RUNNING && delayed_task_ids[i] != INVALID_TASK_ID) {
            // 恢复被挂起的原任务
            TaskID original_id = delayed_task_ids[i];
            
            // 关键点：严格保持协程上下文状态不变
            // CoroutineContext结构体中的state和delay_time字段必须保持原值
            // 这是TASK_INTERRUPT_DELAY宏能够正确从断点继续执行的关键
            Task_Resume(original_id);
            
            // 清除存储的任务ID
            delayed_task_ids[i] = INVALID_TASK_ID;
            break;
        }
    }
}
	
/**
 * @brief 任务延时函数
 * @param id 任务ID
 * @param delay_ms 延时时长(毫秒)
 * 
 * @note 该函数会将任务挂起指定时间，期间不会阻塞其他任务
 *       延时结束后任务会自动恢复执行
 */
void Task_Delay(TaskID id, uint32_t delay_ms) {
    // 参数有效性检查
    if (id >= MAX_TASKS || !tasks[id].is_active) {
        return;
    }
    
    // 关键点：直接挂起任务，不要修改协程上下文状态
    // 协程状态在TASK_INTERRUPT_DELAY宏中已经设置，必须保持不变
    Task_Suspend(id);
    
    // 创建一次性任务，在指定延时后恢复原任务
    char delay_task_name[32];
    snprintf(delay_task_name, sizeof(delay_task_name), "DelayResume_%d", id);
    
    // 添加一次性任务
    TaskID delay_task_id = Task_AddOneShot(ResumeTaskFunc, delay_ms, delay_task_name);
    
    // 存储需要恢复的原任务ID
    if (delay_task_id != INVALID_TASK_ID) {
        delayed_task_ids[delay_task_id] = id;
    }
}

/**
 * @brief 添加一次性任务
 * @param task_func 任务函数指针
 * @param delay 延迟执行时间(毫秒)
 * @param name 任务名称
 * @return TaskID 返回的任务ID(0-9)，INVALID_TASK_ID表示失败
 */
TaskID Task_AddOneShot(void (*task_func)(void), 
                       uint32_t delay, 
                       const char *name) {
    // 添加一个间隔为0的任务，执行后会自动释放
    TaskID task_id = Task_Add(task_func, 0, name);
    
    // 如果任务添加成功，需要正确设置延迟执行时间并重新排序
    if (task_id != INVALID_TASK_ID && task_nodes[task_id] != NULL) {
        // 从链表中移除节点
        TaskNode* node = RemoveTaskNodeNoFree(task_id);
        if (node != NULL) {
            // 更新延迟执行时间
            node->next_run_time = system_time + delay;
            // 重新按时间顺序插入到链表中
            InsertTaskNode(node);
            // 更新任务节点映射表
            task_nodes[task_id] = node;
        }
    }
    
    return task_id;
}

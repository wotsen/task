/**
 * @file task_impl.h
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#pragma once

#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "task.h"

namespace wotsen
{

/**
 * @brief 当前任务状态
 * 
 */
enum task_state
{
    e_task_alive,				///< 活跃
    e_task_wait,				///< 阻塞
    e_task_stop,				///< 停止

	e_task_timeout,				///< 超时
    e_task_dead,				///< 死亡
    e_task_overload				///< 等待重载
};

/**
 * @brief 任务调用
 * 
 */
struct TaskCall
{
	std::function<void()> task;			///< 任务接口
	std::function<void()> e_action;		///< 异常接口
	std::function<void()> clean;		///< 清理接口
};

/**
 * @brief 任务状态
 * 
 */
struct TaskState
{
	uint32_t create_time;        ///< 创建时间
    uint32_t last_update_time;   ///< 上次更新时间
    uint8_t timeout_times;       ///< 超时次数
	enum task_state state;		 ///< 线程状态
};

/**
 * @brief 任务描述
 * 
 */
struct TaskDesc
{
	uint64_t tid;						///< 任务id
	TaskAttribute task_attr;			///< 任务属性
	TaskState task_state;				///< 任务状态
	TaskCall calls;						///< 任务调用
	std::mutex mtx;						///< 任务锁
    std::condition_variable condition;	///< 任务同步
};

class TaskImpl
{
public:
	TaskImpl(const uint32_t &max_tasks, abnormal_task_do except_fun);
	~TaskImpl();

public:
	void wait(void);

public:
	// 添加任务
	bool add_task(uint64_t &tid, const TaskAttribute &task_attr, const std::function<void()> &task);
	// 添加任务异常处理
	bool add_e_action(const uint64_t &tid, const std::function<void()> &e_action);
	// 添加任务退出处理
	bool add_clean(const uint64_t &tid, const std::function<void()> &clean);

public:
	// 查找任务
	std::shared_ptr<TaskDesc> search_task(const uint64_t &tid) noexcept;
	
	// 任务心跳
	bool task_alive(const uint64_t &tid);
	// 任务检测
	bool is_task_alive(const uint64_t &tid);
	// 任务停止
	void task_stop(const uint64_t &tid);

private:
	uint32_t max_tasks_;							///< 任务数量
	abnormal_task_do except_fun_;					///< 任务队列
	std::mutex mtx_;								///< 操作锁
	std::vector<std::shared_ptr<TaskDesc>> tasks_;	///< 任务队列
};

} // namespace wotsen

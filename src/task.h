/**
 * @file task.h
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#pragma once

#include <cstdio>
#include <cinttypes>
#include <string>
#include <functional>
#include <type_traits>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <exception>
#include "task_utils.h"

namespace wotsen
{

// using task_time_t = std::chrono::system_clock::time_point;

// #define TASK_NS(n) std::chrono::nanoseconds(n)
// #define TASK_US(n) std::chrono::microseconds(n)
// #define TASK_MS(n) std::chrono::milliseconds(n)
// #define TASK_SEC(n) std::chrono::seconds(n)
// #define TASK_MIN(n) std::chrono::minutes(n)
// #define TASK_HOUR(n) std::chrono::hours(n)

/**
 * @brief 任务崩溃处理
 * 
 */
enum task_except_action
{
	// [NOTE]:如果是任务超时，连续3此则会执行超时接口后，直接强制结束，下个检测周期进行异常处理
    e_task_default,             ///< 默认(执行注册的异常接口)
	e_task_ignore,				///< 忽略
	e_task_restart,				///< 任务重启
	// e_task_kill,				///< 清除任务
	e_task_reboot_system,		///< 系统重启
};

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
};

/**
 * @brief 任务注冊信息
 * 
 */
struct TaskRegisterInfo
{
	// [NOTE]:不要使用memcpy拷贝
	TaskAttribute task_attr;				///< 任务属性
    time_t alive_time;						///< 存活时间
    enum task_except_action e_action;		///< 异常动作
};

/**
 * @brief 任务异常信息
 * 
 */
struct TaskExceptInfo {
    uint64_t tid;				///< 任务id
    std::string task_name;		///< 任务名称
    std::string reason;			///< 原因
};

/**
 * @brief 任务调用
 * 
 */
struct TaskCall
{
	std::function<void()> task;				///< 任务接口
	std::function<void()> e_action;			///< 异常接口
	std::function<void()> timout_action;	///< 超时接口
	std::function<void()> clean;			///< 清理接口
};

/**
 * @brief 任务状态
 * 
 */
struct TaskState
{
	time_t create_time;        	///< 创建时间
    time_t last_update_time;   	///< 上次更新时间
    uint8_t timeout_times;      ///< 超时次数
	enum task_state state;		///< 线程状态
};

/**
 * @brief 任务描述
 * 
 */
struct TaskDesc
{
	uint64_t tid;						///< 任务id
	TaskRegisterInfo reg_info;			///< 任务属性
	TaskState task_state;				///< 任务状态
	TaskCall calls;						///< 任务调用
	std::mutex mtx;						///< 任务锁
    std::condition_variable condition;	///< 任务同步
};

// 异常任务外部处理回调接口
using abnormal_task_do = void (*)(const struct TaskExceptInfo &);

class Task
{
public:
	Task(const uint32_t &max_tasks=128, abnormal_task_do except_fun = nullptr);
	~Task();

	// 创建任务
	template <typename F, typename... Args>
	TaskKey<callable_ret_type<F, Args ...>>
	register_task(const TaskRegisterInfo &reg_info, F&& f, Args&& ... args)
	{
		TaskKey<callable_ret_type<F, Args ...>> ret;

		// 可调用对象封装为void(void)
		auto task = std::make_shared< std::packaged_task<callable_ret_type<F, Args ...>()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);

		// 获取未来值对象
		ret.fut = task->get_future();

		// 添加任务
		if (!add_task(ret.tid, reg_info, [task](){ (*task)(); }))
		{
			throw std::invalid_argument("add task create failed");
		}

		return ret;
	}

	// 添加任务异常行为
	template <typename F, typename... Args>
	future_callback_type<F, Args ...>
	add_task_except_action(const uint64_t &tid, F&& f, Args&& ... args)
	{
		auto task = std::make_shared< std::packaged_task<callable_ret_type<F, Args ...>()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
		
		future_callback_type<F, Args ...> ret = task->get_future();

		if (!add_e_action(tid, [task](){ (*task)(); }))
		{
			throw std::invalid_argument("add task except action failed");
		}

		return ret;
	}

	// 添加任务超时行为
	template <typename F, typename... Args>
	future_callback_type<F, Args ...>
	add_task_timeout_action(const uint64_t &tid, F&& f, Args&& ... args)
	{
		auto task = std::make_shared< std::packaged_task<callable_ret_type<F, Args ...>()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
		
		future_callback_type<F, Args ...> ret = task->get_future();

		if (!add_timeout_action(tid, [task](){ (*task)(); }))
		{
			throw std::invalid_argument("add task timeout action failed");
		}

		return ret;
	}

	// 添加任务结束行为
	template <typename F, typename... Args>
	future_callback_type<F, Args ...>
	add_task_exit_action(const uint64_t &tid, F&& f, Args&& ... args)
	{
		auto task = std::make_shared< std::packaged_task<callable_ret_type<F, Args ...>()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
		
		future_callback_type<F, Args ...> ret = task->get_future();

		if (!add_clean(tid, [task](){ (*task)(); }))
		{
			throw std::invalid_argument("add task exit action failed");
		}

		return ret;
	}

	// 启动任务
	void task_run(const uint64_t &tid);
	// 任务结束
	void task_exit(const uint64_t &tid);

	// 任务心跳
	bool task_alive(const uint64_t &tid);
	// 任务是否存活
	bool is_task_alive(const uint64_t &tid);
	// 获取任务状态
	enum task_state task_state(const uint64_t &tid);

	// 任务暂停
	void task_wait(const uint64_t &tid);
	// 任务继续
	void task_continue(const uint64_t &tid);

public:
	void wait(void);
	// 查找任务
	std::shared_ptr<TaskDesc> search_task(const uint64_t &tid) noexcept;

	friend TaskKey<int> task_manage(Task *task);
	friend class TaskManage;

private:
	// 添加任务
	bool add_task(uint64_t &tid, const TaskRegisterInfo &reg_info, const std::function<void()> &task);
	// 添加任务异常处理
	bool add_e_action(const uint64_t &tid, const std::function<void()> &e_action);
	// 超时处理
	bool add_timeout_action(const uint64_t &tid, const std::function<void()> &timeout);
	// 添加任务退出处理
	bool add_clean(const uint64_t &tid, const std::function<void()> &clean);

public:
	static bool stop;								///< 停止标记

private:
	uint32_t max_tasks_;							///< 任务数量
	abnormal_task_do except_fun_;					///< 任务队列
	std::mutex mtx_;								///< 操作锁
	std::vector<std::shared_ptr<TaskDesc>> tasks_;	///< 任务队列
	std::future<int> manage_exit_fut_;
};

} // namespace wotsen

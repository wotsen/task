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
#include <type_traits>
#include <future>
#include <memory>

namespace wotsen
{

/**
 * @brief 任务崩溃处理
 * 
 */
enum task_except_action
{
    e_task_default,             ///< 默认(先执行清理接口，下次任务存在且仍旧超时，则尝试kill任务并移除管理队列)
	e_task_restart,				///< 任务重启
    e_task_reboot_system,		///< 系统重启
    e_task_ignore				///< 忽略
};

/**
 * @brief 任务优先级，值越大，优先级越高
 * 
 */
enum task_priority : int
{
	e_max_task_pri_lv = 100,
	e_sys_task_pri_lv = 90,
	e_run_task_pri_lv = 80,
	e_fun_task_pri_lv = 70,
	e_thr_task_pri_lv = 60,
	e_min_task_pri_lv = 50,
};

/**
 * @brief 任务属性
 * 
 */
struct TaskAttribute
{
	// [NOTE]:不要使用memcpy拷贝
    std::string task_name;					///< 任务名
    size_t stacksize;						///< 线程栈
    enum task_priority priority;			///< 优先级
    uint32_t alive_time;					///< 存活时间
    enum task_except_action e_action;		///< 异常动作
};

/**
 * @brief 任务描述符
 * 
 * @tparam T : 任务返回值类型
 */
template <class T>
class TaskKey
{
public:
    std::future<T> fut;	///< 返回值
    uint64_t tid;		///< 任务id
};

/**
 * @brief 任务异常信息
 * 
 */
struct except_task_info {
    uint64_t tid;				///< 任务id
    std::string task_name;		///< 任务名称
    std::string reason;			///< 原因
};

// 异常任务外部处理回调接口
using abnormal_task_do = void (*)(const struct except_task_info &);

// 可调用对象返回类型
template <typename F, typename... Args>
using callable_ret_type = typename std::result_of<F(Args ...)>::type;

// 可调用对象返回值
template <typename F, typename... Args>
using future_callback_type = std::future<callable_ret_type<F, Args ...>>;

class TaskImpl;

class Task
{
public:
	Task(const uint32_t &max_tasks=128, abnormal_task_do except_fun = nullptr);
	~Task();

	// 创建任务
	template <typename F, typename... Args>
	TaskKey<callable_ret_type<F, Args ...>> create_task(const TaskAttribute &attr, F&& f, Args&& ... args);
	// 添加任务异常行为
	template <typename F, typename... Args>
	future_callback_type<F, Args ...> add_task_except_action(const uint64_t &tid, F&& f, Args&& ... args);
	// 添加任务结束行为
	template <typename F, typename... Args>
	future_callback_type<F, Args ...> add_task_exit_action(const uint64_t &tid, F&& f, Args&& ... args);

	// 启动任务
	void task_run(const uint64_t &tid);
	// 任务结束
	void task_exit(const uint64_t &tid);

	// 任务心跳
	bool task_alive(const uint64_t &tid);
	// 任务是否存活
	bool is_task_alive(const uint64_t &tid);

	// 任务暂停
	void task_wait(const uint64_t &tid);
	// 任务继续
	void task_continue(const uint64_t &tid);

private:
	std::shared_ptr<TaskImpl> impl;			///< 任务实现
};

} // namespace wotsen

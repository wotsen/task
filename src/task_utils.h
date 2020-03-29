/**
 * @file task_utils.h
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-15
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#pragma once

#include <future>
#include <string>
#include <cinttypes>

namespace wotsen
{

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

// 可调用对象返回类型
template <typename F, typename... Args>
using callable_ret_type = typename std::result_of<F(Args...)>::type;

// 可调用对象返回值
template <typename F, typename... Args>
using future_callback_type = std::future<callable_ret_type<F, Args...>>;

/**
 * @brief 任务描述符
 * 
 * @tparam T : 任务返回值类型
 */
template <class T>
class TaskKey
{
public:
	std::future<T> fut;   ///< 返回值
#define INVALID_TASK_ID 0 ///< 无效任务id
	uint64_t tid;		  ///< 任务id
};

/**
 * @brief 任务属性
 * 
 */
struct TaskAttribute
{
	// [NOTE]:不要使用memcpy拷贝
	std::string task_name;			 ///< 任务名
#define TASK_STACKSIZE(k) ((k)*1024) ///< 栈内存计算k
	size_t stacksize;				 ///< 栈内存
	enum task_priority priority;	 ///< 优先级
};

/**
 * @brief 任务属性扩展
 * 
 */
struct TaskAttrEx
{
	TaskAttribute attr;			///< 基本属性
	std::function<void()> task; ///< 可调对象
};

/*******************************************************/
// 内部调用
using task_util_call = void *(*)(TaskAttrEx *);

// 内部任务创建
bool _create_util_task(uint64_t *tid,
					   const size_t &stacksize,
					   const int &priority,
					   task_util_call fn,
					   void *arg = nullptr);
/*******************************************************/

// 获取任务id
uint64_t task_id(void);

// 任务检测
bool is_task_alive(const uint64_t &tid);

// 设置任务名
void set_task_name(const std::string &name, const uint64_t &tid=INVALID_TASK_ID);
void set_task_name(const char *name, const uint64_t &tid=INVALID_TASK_ID);

// 获取任务名
std::string &&get_task_name(const uint64_t &tid=INVALID_TASK_ID);

// 结束任务
void kill_task(const uint64_t &tid);

// 创建任务
template <typename F, typename... Args>
TaskKey<callable_ret_type<F, Args...>>
new_task(const TaskAttribute &attr, F &&f, Args &&... args)
{
	TaskKey<callable_ret_type<F, Args...>> ret;

	// 可调用对象封装为void(void)
	auto task = std::make_shared<std::packaged_task<callable_ret_type<F, Args...>()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	// 获取未来值对象
	ret.fut = task->get_future();
	ret.tid = INVALID_TASK_ID;

	// 资源申请
	TaskAttrEx *attr_ex = new TaskAttrEx;

	attr_ex->attr = attr;
	attr_ex->task = [task]() { (*task)(); };

	auto _task_run = [](TaskAttrEx *attr_ex) -> void * {
		set_task_name(attr_ex->attr.task_name);

		auto task = attr_ex->task;

		delete attr_ex;

		task();

		return (void *)0;
	};

	// 创建线程
	if (!_create_util_task(&ret.tid,
							attr.stacksize,
							attr.priority,
							(task_util_call)_task_run,
							attr_ex))
	{
		delete attr_ex;
	}

	return ret;
}

} // namespace wotsen

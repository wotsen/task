/**
 * @file task.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <iostream>
#include <functional>
#include <exception>
#include "task_impl.h"
#include "task.h"

namespace wotsen
{

Task::Task(const uint32_t &max_tasks, abnormal_task_do except_fun) :
	impl(std::make_shared<TaskImpl>(max_tasks, except_fun))
{
}

Task::~Task()
{
}

template <typename F, typename... Args>
TaskKey<callable_ret_type<F, Args ...>> Task::create_task(const TaskAttribute &attr, F&& f, Args&& ... args)
{
	TaskKey<callable_ret_type<F, Args ...>> ret;

	// 可调用对象封装为void(void)
	auto task = std::make_shared< std::packaged_task<callable_ret_type<F, Args ...>()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

	// 获取未来值对象
	ret.fut = task->get_future();

	// 添加任务
	if (!impl->add_task(ret.tid, attr, [task](){ (*task)(); }))
	{
		throw std::invalid_argument("add task create failed");
	}

	return ret;
}

// 添加任务异常行为
template <typename F, typename... Args>
future_callback_type<F, Args ...> Task::add_task_except_action(const uint64_t &tid, F&& f, Args&& ... args)
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

// 添加任务结束行为
template <typename F, typename... Args>
future_callback_type<F, Args ...> Task::add_task_exit_action(const uint64_t &tid, F&& f, Args&& ... args)
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
void Task::task_run(const uint64_t &tid)
{
	task_continue(tid);
}

// 任务结束
void Task::task_exit(const uint64_t &tid)
{
	impl->task_stop(tid);
}

// 任务心跳
bool Task::task_alive(const uint64_t &tid)
{
	return impl->task_alive(tid);
}

// 任务暂停
void Task::task_wait(const uint64_t &tid)
{
	auto _task = impl->search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 修改状态
	if (e_task_alive == _task->task_state.state) _task->task_state.state = e_task_wait;
}

// 任务继续
void Task::task_continue(const uint64_t &tid)
{
	auto _task = impl->search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	if (e_task_wait != _task->task_state.state) return;

	_task->task_state.state = e_task_alive;

	_task->condition.notify_one();
}

// 任务是否存活
bool Task::is_task_alive(const uint64_t &tid)
{
	return impl->is_task_alive(tid);
}

} // namespace wotsen

/************************************************测试代码*********************************************************/

#include <cstring>
#include <thread>
#include <chrono>
#include "posix_thread.h"

int main(void)
{
	using namespace wotsen;

	std::shared_ptr<Task> task(new Task);

	TaskAttribute attr;

	attr.task_name = "test task";
	attr.stacksize = 50 * 1024;
	attr.priority = e_sys_task_pri_lv;
	attr.alive_time = 3 * 60;
	attr.e_action = e_task_default;

	auto ret = task->create_task(attr, [&]()->int{

		for (int i = 3; task->is_task_alive(thread_id()) && i; i--)
		{
			std::cout << "alive......." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	
		return 100;
	});

	task->task_run(ret.tid);

	std::cout << ret.fut.get() << std::endl;

	return 0;
}
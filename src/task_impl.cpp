/**
 * @file task_impl.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <ctime>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include "posix_thread.h"
#include "task_impl.h"

namespace wotsen
{

static void *task_run(TaskImpl *taskImpl);

TaskImpl::TaskImpl(const uint32_t &max_tasks, abnormal_task_do except_fun) :
	max_tasks_(max_tasks), except_fun_(except_fun)
{
	// 优先级校验
	static_assert((int)e_max_task_pri_lv == (int)e_max_thread_pri_lv, "e_max_task_pri_lv != e_max_thread_pri_lv");
	static_assert((int)e_sys_task_pri_lv == (int)e_sys_thread_pri_lv, "e_sys_task_pri_lv != e_sys_thread_pri_lv");
	static_assert((int)e_run_task_pri_lv == (int)e_run_thread_pri_lv, "e_run_task_pri_lv != e_run_thread_pri_lv");
	static_assert((int)e_fun_task_pri_lv == (int)e_fun_thread_pri_lv, "e_fun_task_pri_lv != e_fun_thread_pri_lv");
	static_assert((int)e_thr_task_pri_lv == (int)e_thr_thread_pri_lv, "e_thr_task_pri_lv != e_thr_thread_pri_lv");
	static_assert((int)e_min_task_pri_lv == (int)e_min_thread_pri_lv, "e_min_task_pri_lv != e_min_thread_pri_lv");
}

TaskImpl::~TaskImpl()
{
	for (auto item : tasks_)
	{
		task_stop(item->tid);
	}

	tasks_.clear();
}

bool TaskImpl::add_task(uint64_t &tid, const TaskAttribute &task_attr, const std::function<void()> &task)
{
	if (tasks_.size() >= max_tasks_)
	{
		std::cout << "max full." << std::endl;
		return false;
	}

	uint64_t _tid = INVALID_PTHREAD_TID;
	// 资源申请
	std::shared_ptr<TaskDesc> task_desc(new TaskDesc);
	std::unique_lock<std::mutex> lck(mtx_);

	// 创建线程
	if (!create_thread(&_tid, task_attr.stacksize, task_attr.priority, (thread_func)task_run, this))
    {
		std::cout << "create thread failed." << std::endl;
        return false;
    }

	tid = _tid;

	// 任务描述记录
	task_desc->tid = _tid;
	task_desc->task_attr = task_attr;
	task_desc->calls.task = task;
	task_desc->task_state.create_time = time(nullptr);
	task_desc->task_state.last_update_time = task_desc->task_state.create_time;
	task_desc->task_state.timeout_times = 0;
	task_desc->task_state.state = e_task_wait;

	// 入栈
	tasks_.push_back(task_desc);

	return true;
}

bool TaskImpl::add_e_action(const uint64_t &tid, const std::function<void()> &e_action)
{
	// TODO
	return true;
}

bool TaskImpl::add_clean(const uint64_t &tid, const std::function<void()> &clean)
{
	// TODO
	return true;
}

void TaskImpl::wait(void)
{
	std::unique_lock<std::mutex> lock(mtx_);
}

bool TaskImpl::task_alive(const uint64_t &tid)
{
	auto _task = search_task(tid);

	if (nullptr == _task)
    {
		return false;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 如果是等待则一直休眠
	while (e_task_wait == _task->task_state.state) _task->condition.wait(lock);

	// 如果是非存活状态则直接返回
	if (e_task_alive != _task->task_state.state) return false;

	// 更新时间
	_task->task_state.last_update_time = time(nullptr);

	return true;
}

bool TaskImpl::is_task_alive(const uint64_t &tid)
{
	auto _task = search_task(tid);

	if (nullptr == _task)
    {
		return false;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 检测状态与实际线程
	return e_task_alive == _task->task_state.state && thread_exsit(tid);
}

void TaskImpl::task_stop(const uint64_t &tid)
{
	auto _task = search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	if (e_task_stop == _task->task_state.state) return;

	// 先修改状态
	_task->task_state.state = e_task_stop;

	// 解锁，等任务自己检测到退出状态
	lock.unlock();

	// 检测线程存活，如果存活等3次500ms后强制终止
	int cnt = 3;

	while (cnt-- && thread_exsit(tid))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::cout << "force destroy task [" << tid << "]." << std::endl;
		release_thread(tid);
	}
}

std::shared_ptr<TaskDesc> TaskImpl::search_task(const uint64_t &tid) noexcept
{
	std::unique_lock<std::mutex> lck(mtx_);

	for (auto item : tasks_)
	{
		if (tid == item->tid)
		{
			return item;
		}
	}

	std::cout << "not find task info = " << tid << ", can not run task!" << std::endl;

	return static_cast<std::shared_ptr<TaskDesc>>(nullptr);
}

/**
 * @brief 任务运行
 *
 * @param tasks : 任务管理器
 *
 * @return : none
 */
static void *task_run(TaskImpl *taskImpl)
{
    uint64_t tid = thread_id();

    // 等线程加入任务池
	taskImpl->wait();

    auto _task = taskImpl->search_task(tid);

    if (nullptr == _task)
    {
		std::cout << "not find task info = " << tid << ", can not run task!" << std::endl;
        return (void *)0;
    }

    set_thread_name(_task->task_attr.task_name.c_str());

	std::unique_lock<std::mutex> lck(_task->mtx);

	// 等待任务启动
	while (e_task_wait == _task->task_state.state) _task->condition.wait(lck);

	std::cout << "task " << tid << " run." << std::endl;

	lck.unlock();

    _task->calls.task();

    return (void *)0;
}

} // namespace wotsen

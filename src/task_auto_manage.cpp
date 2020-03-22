/**
 * @file task_auto_manage.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-15
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <algorithm>
#include "task_auto_manage.h"

namespace wotsen
{
extern task_dbg_cb __dbg;

// 任务超时最大次数
static const int MAX_CNT_TASK_TIMEOUT = 3;
// 异常时间差s
static const time_t MAX_ERROR_TIME = 60;

void TaskAutoManage::task_update(void) noexcept
{
	// 清理死亡任务
	clean_dead();
	// 异常标记
	dead_mark();
    // 超时标记
    timeout_mark();
	// 异常处理
	except_do();

	if (system_reboot_)
	{
		// TODO:reboot;先让所有任务安全退出
	}
}

void TaskAutoManage::task_correction_time(void) noexcept
{
	static time_t last_time = now();
	time_t now_t = now();

	// 时间向前跳变和时间向后跳变超过一分钟，重置任务时间
    if (now_t < last_time || (now_t - last_time) > MAX_ERROR_TIME)
	{
		std::unique_lock<std::mutex> lock(task_->mtx_);

		for (auto item : task_->tasks_)
		{
			std::unique_lock<std::mutex> i_lock(item->mtx);

			// 重置时间和次数
			item->task_state.last_update_time = now_t;
			item->task_state.timeout_times = 0;
		}
	}

	last_time = now_t;
}

bool TaskAutoManage::task_filter(std::shared_ptr<TaskDesc> &task)
{
	// 非存活任务或任务已经销毁则过滤掉
	return INVALID_TASK_ID == task->tid
			|| e_task_alive != task->task_state.state
			|| !is_task_alive(task->tid);
}

void TaskAutoManage::dead_mark(void)
{
	std::unique_lock<std::mutex> lock(task_->mtx_);

	for (auto item : task_->tasks_)
	{
		// 无效任务及退出任务
		if (INVALID_TASK_ID == item->tid ||
			(e_task_dead != item->task_state.state
			&& e_task_stop != item->task_state.state
			&& !is_task_alive(item->tid)))
		{
			std::unique_lock<std::mutex> i_lock(item->mtx);
			item->task_state.state = e_task_dead;
		}
	}
}

void TaskAutoManage::task_dead_handler(std::shared_ptr<TaskDesc> &task)
{
	switch (task->reg_info.e_action)
	{
	case e_task_ignore:
		break;
	case e_task_reboot_system:
		if (task->calls.e_action) task->calls.e_action();
		system_reboot_ = true;
		break;
	case e_task_default:
	default:
		if (task->calls.e_action) task->calls.e_action();
		break;
	}
}

void TaskAutoManage::except_do(void)
{
	TaskExceptInfo ex_info;

	for (auto item : task_->tasks_)
	{
		std::unique_lock<std::mutex> lock(item->mtx);

		switch (item->task_state.state)
		{
		case e_task_timeout:
			ex_info.tid = item->tid;
			ex_info.task_name =	item->reg_info.task_attr.task_name;
			ex_info.reason = "timeout";

			lock.unlock();
			
			// 通知任务异常信息
			if (Task::except_fun) Task::except_fun(ex_info);

			// 执行超时接口
			if (item->calls.timout_action) item->calls.timout_action();

			lock.lock();
			// 下个周期做异常处理
			item->task_state.state = e_task_dead;
			lock.unlock();

			break;

		case e_task_dead:
			lock.unlock();

			ex_info.tid = item->tid;
			ex_info.task_name =	item->reg_info.task_attr.task_name;
			ex_info.reason = "except dead";

			if (Task::except_fun) Task::except_fun(ex_info);

			task_dead_handler(item);

			break;

		default:
			lock.unlock();
			break;
		}
	}
}

void TaskAutoManage::timeout_mark(void) noexcept
{
	// 系统时间异常矫正
    task_correction_time();

	std::unique_lock<std::mutex> lock(task_->mtx_);
	
	for (auto item : task_->tasks_)
	{
		if (task_filter(item)) continue;

		std::unique_lock<std::mutex> i_lock(item->mtx);

		// 超时判断
		if (now() - item->task_state.last_update_time > item->reg_info.alive_time)
		{
			task_dbg("task [%s][%ld] timeout\n", item->reg_info.task_attr.task_name.c_str(), item->tid);

			if (item->task_state.timeout_times++ >= MAX_CNT_TASK_TIMEOUT)
			{
				// 先置超时，下次进行处理
				item->task_state.state = e_task_timeout;
			}
		}
		else
		{
			item->task_state.timeout_times = 0;
		}

		i_lock.unlock();
	}
}

void TaskAutoManage::clean_dead(void)
{
	std::unique_lock<std::mutex> lock(task_->mtx_);

	task_->tasks_.erase(std::remove_if(task_->tasks_.begin(),
										task_->tasks_.end(),
										[](auto item) -> bool {
											return e_task_dead == item->task_state.state;
										}),
						task_->tasks_.end());
}

TaskKey<int> task_auto_manage(Task *task)
{
	TaskAttribute attr;
	attr.task_name = "task manage";
	attr.stacksize = TASK_STACKSIZE(8);
	attr.priority = e_sys_task_pri_lv;

	TaskKey<int> ret = new_task(attr, [task](void) -> int {
		std::shared_ptr<TaskAutoManage> manage(new TaskAutoManage(task));

		// 检测任务组件退出
		for (; !Task::stop ;)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			manage->task_update();
		}

		task_dbg("task auto manage exit.\n");

		return 0;
	});

	return ret;
}

} // namespace wotsen

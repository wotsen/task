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

#include <ctime>
#include <thread>
#include <chrono>
#include <algorithm>
#include "posix_thread.h"
#include "task.h"
#include "task_auto_manage.h"

namespace wotsen
{

#if INVALID_TASK_ID != INVALID_PTHREAD_TID
#error INVALID_TASK_ID defined not equal INVALID_PTHREAD_TID
#endif

#if TASK_STACKSIZE(1) != STACKSIZE(1)
#error TASK_STACKSIZE defined not equal STACKSIZE
#endif

task_dbg_cb __dbg = nullptr;

// 强制退出次数
static const int MAX_CNT_TASK_FORCE_EXIT = 3;

static void *_task_run(Task *taskImpl);

bool Task::stop = false;
uint32_t Task::max_tasks = 128;
abnormal_task_do Task::except_fun = nullptr;

Task::Task()
{
	// 优先级校验
	static_assert((int)e_max_task_pri_lv == (int)e_max_thread_pri_lv, "e_max_task_pri_lv != e_max_thread_pri_lv");
	static_assert((int)e_sys_task_pri_lv == (int)e_sys_thread_pri_lv, "e_sys_task_pri_lv != e_sys_thread_pri_lv");
	static_assert((int)e_run_task_pri_lv == (int)e_run_thread_pri_lv, "e_run_task_pri_lv != e_run_thread_pri_lv");
	static_assert((int)e_fun_task_pri_lv == (int)e_fun_thread_pri_lv, "e_fun_task_pri_lv != e_fun_thread_pri_lv");
	static_assert((int)e_thr_task_pri_lv == (int)e_thr_thread_pri_lv, "e_thr_task_pri_lv != e_thr_thread_pri_lv");
	static_assert((int)e_min_task_pri_lv == (int)e_min_thread_pri_lv, "e_min_task_pri_lv != e_min_thread_pri_lv");

	Task::stop = false;

	// 启动任务管理
	auto ret = task_auto_manage(this);

	if (INVALID_TASK_ID == ret.tid)
	{
		Task::stop = true;
		throw std::runtime_error("create manage task failed.");
	}

	manage_exit_fut_ = std::move(ret.fut);
}

Task::~Task()
{
	// 通知任务管理退出
	Task::stop = true;

	// 同步任务管理退出，防止非法内存访问
	manage_exit_fut_.get();

	// 强制所有任务退出
	for (auto item : tasks_)
	{
		task_exit(item->tid);
	}

	tasks_.clear();
}

// 等待任务创建结束
void Task::wait(void)
{
	std::unique_lock<std::mutex> lock(mtx_);
}

// 查找任务
std::shared_ptr<TaskDesc> Task::search_task(const uint64_t &tid) noexcept
{
	// std::unique_lock<std::mutex> lck(mtx_);

	for (auto item : tasks_)
	{
		if (tid == item->tid)
		{
			return item;
		}
	}

	task_dbg("not find task = %ld!\n", tid);

	return static_cast<std::shared_ptr<TaskDesc>>(nullptr);
}

// 添加任务异常处理
bool Task::add_e_action(const uint64_t &tid, const std::function<void()> &e_action)
{
	auto item = search_task(tid);

	if (!item) return false;

	std::unique_lock<std::mutex> lck(item->mtx);

	item->calls.e_action = e_action;

	return true;
}

// 超时处理
bool Task::add_timeout_action(const uint64_t &tid, const std::function<void()> &timeout)
{
	auto item = search_task(tid);

	if (!item) return false;

	std::unique_lock<std::mutex> lck(item->mtx);

	item->calls.timout_action = timeout;

	return true;
}

// 添加任务退出处理
bool Task::add_clean(const uint64_t &tid, const std::function<void()> &clean)
{
	auto item = search_task(tid);

	if (!item) return false;

	std::unique_lock<std::mutex> lck(item->mtx);

	item->calls.clean = clean;

	return true;
}

// 添加任务
bool Task::add_task(uint64_t &tid, const TaskRegisterInfo &reg_info, const std::function<void()> &task)
{
	if (tasks_.size() >= max_tasks)
	{
		task_dbg("task full.\n");
		return false;
	}

	uint64_t _tid = INVALID_TASK_ID;
	// 资源申请
	std::shared_ptr<TaskDesc> task_desc(new TaskDesc);
	std::unique_lock<std::mutex> lck(mtx_);

	// 创建线程
	if (!create_thread(&_tid, reg_info.task_attr.stacksize, reg_info.task_attr.priority, (thread_func)_task_run, this))
    {
		task_dbg("create thread failed.\n");
        return false;
    }

	// 查找队列中是否有相同id的任务，如果有则认为是已经退出，删除任务
	del_task(_tid);

	tid = _tid;

	// 任务描述记录
	task_desc->tid = _tid;
	task_desc->reg_info = reg_info;
	task_desc->reg_info.task_attr.task_name = reg_info.task_attr.task_name;
	task_desc->calls.task = task;
	task_desc->task_state.create_time = now();
	task_desc->task_state.last_update_time = task_desc->task_state.create_time;
	task_desc->task_state.timeout_times = 0;
	task_desc->task_state.state = e_task_wait;

	// 入栈
	tasks_.push_back(task_desc);

	return true;
}

// 启动任务
void Task::task_run(const uint64_t &tid)
{
	task_continue(tid);
}

// 任务结束
void Task::task_exit(const uint64_t &tid)
{
	auto _task = task_ptr()->search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	if (e_task_stop == _task->task_state.state || e_task_dead == _task->task_state.state) return;

	// 先修改状态
	_task->task_state.state = e_task_stop;

	// 解锁，等任务自己检测到退出状态
	lock.unlock();

	// 检测线程存活，如果存活等3次500ms后强制终止
	int cnt = MAX_CNT_TASK_FORCE_EXIT;

	while (cnt-- && thread_exsit(tid))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	// 强制退出
	if (thread_exsit(tid)) {
		task_dbg("force destroy task [%ld].\n", tid);
		release_thread(tid);
	}

	lock.lock();

	// 执行清理工作
	if (_task->calls.clean) _task->calls.clean();

	std::vector<std::shared_ptr<TaskDesc>> &tasks_ = task_ptr()->tasks_;

	std::unique_lock<std::mutex> t_lock(task_ptr()->mtx_);

	// 移除队列
	tasks_.erase(std::remove_if(tasks_.begin(),
								tasks_.end(),
								[&](auto &item) -> bool {
									if (_task->tid == item->tid)
									{
										return true;
									}

									return false;
								}),
				tasks_.end());
}

// 任务心跳
bool Task::task_alive(const uint64_t &tid)
{
	auto _task = task_ptr()->search_task(tid);

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
	_task->task_state.last_update_time = now();

	return true;
}

// 任务暂停
void Task::task_wait(const uint64_t &tid)
{
	auto _task = task_ptr()->search_task(tid);

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
	auto _task = task_ptr()->search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 只有等待状态才能切换到继续执行
	if (e_task_wait != _task->task_state.state) return;

	_task->task_state.last_update_time = now();
	_task->task_state.state = e_task_alive;

	_task->condition.notify_one();
}

// 任务是否存活
bool Task::is_task_alive(const uint64_t &tid)
{
	auto _task = task_ptr()->search_task(tid);

	if (nullptr == _task)
    {
		return false;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 检测状态与实际线程
	return e_task_alive == _task->task_state.state && thread_exsit(tid);
}

// 获取任务状态
enum task_state Task::task_state(const uint64_t &tid)
{
	auto _task = task_ptr()->search_task(tid);

	if (nullptr == _task)
    {
		return e_task_stop;
    }

	return _task->task_state.state;
}

std::shared_ptr<Task> &Task::task_ptr(void)
{
	static std::shared_ptr<Task> task_instance(new Task);

	return task_instance;
}

void Task::task_init(const uint32_t &max_tasks, abnormal_task_do except_fun)
{
	Task::max_tasks = max_tasks;
	Task::except_fun = except_fun;
}

void Task::del_task(const uint64_t &tid)
{
	for (auto item : tasks_)
	{
		std::unique_lock<std::mutex> lck(item->mtx);

		if (tid == item->tid)
		{
			// 只是将任务id标记为无效，有任务管理进行处理
			item->tid = INVALID_TASK_ID;
			break;
		}
	}
}

/**
 * @brief 任务运行
 *
 * @param tasks : 任务管理器
 *
 * @return : none
 */
static void *_task_run(Task *taskImpl)
{
    uint64_t tid = thread_id();

    // 等线程加入任务池
	taskImpl->wait();

    auto _task = taskImpl->search_task(tid);

    if (nullptr == _task)
    {
		task_dbg("not find task info = %ld can not run task!\n", tid);
        return (void *)0;
    }

    set_thread_name(_task->reg_info.task_attr.task_name.c_str());

	std::unique_lock<std::mutex> lck(_task->mtx);

	// 等待任务启动
	while (e_task_wait == _task->task_state.state) _task->condition.wait(lck);

	task_dbg("task %s run.\n", _task->reg_info.task_attr.task_name.c_str());

	lck.unlock();

	// 实际任务调用
    _task->calls.task();

    return (void *)0;
}

void set_task_debug_cb(const task_dbg_cb cb)
{
	__dbg = cb;
}

const char* get_task_version(void)
{
	#define __TASK_VERSION "v1.0.0"

	return __TASK_VERSION;
}

} // namespace wotsen

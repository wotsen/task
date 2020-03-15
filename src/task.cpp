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
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include "posix_thread.h"
#include "task.h"

namespace wotsen
{

#if INVALID_TASK_ID != INVALID_PTHREAD_TID
#error INVALID_TASK_ID defined not equal INVALID_PTHREAD_TID
#endif

#if TASK_STACKSIZE(1) != STACKSIZE(1)
#error TASK_STACKSIZE defined not equal STACKSIZE
#endif

// 强制退出次数
static const int MAX_CNT_TASK_FORCE_EXIT = 3;
// 任务超时最大次数
static const int MAX_CNT_TASK_TIMEOUT = 3;
// 异常时间差s
static const time_t MAX_ERROR_TIME = 60;

static void *_task_run(Task *taskImpl);

bool Task::stop = false;

Task::Task(const uint32_t &max_tasks, abnormal_task_do except_fun) :
	max_tasks_(max_tasks), except_fun_(except_fun)
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
	auto ret = task_manage(this);

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

	// 同步任务管理退出
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

	std::cout << "not find task info = " << tid << ", can not run task!" << std::endl;

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
	if (tasks_.size() >= max_tasks_)
	{
		std::cout << "max full." << std::endl;
		return false;
	}

	uint64_t _tid = INVALID_TASK_ID;
	// 资源申请
	std::shared_ptr<TaskDesc> task_desc(new TaskDesc);
	std::unique_lock<std::mutex> lck(mtx_);

	// 创建线程
	if (!create_thread(&_tid, reg_info.task_attr.stacksize, reg_info.task_attr.priority, (thread_func)_task_run, this))
    {
		std::cout << "create thread failed." << std::endl;
        return false;
    }

	tid = _tid;

	// 任务描述记录
	task_desc->tid = _tid;
	task_desc->reg_info = reg_info;
	task_desc->calls.task = task;
	task_desc->task_state.create_time = time(nullptr);
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
	auto _task = search_task(tid);

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
		std::cout << "force destroy task [" << tid << "]." << std::endl;
		release_thread(tid);
	}

	// 执行清理工作
	if (_task->calls.clean) _task->calls.clean();

	lock.lock();

	std::unique_lock<std::mutex> t_lock(mtx_);

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

// 任务暂停
void Task::task_wait(const uint64_t &tid)
{
	auto _task = search_task(tid);

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
	auto _task = search_task(tid);

	if (nullptr == _task)
    {
		return ;
    }

	std::unique_lock<std::mutex> lock(_task->mtx);

	// 只有等待状态才能切换到继续执行
	if (e_task_wait != _task->task_state.state) return;

	_task->task_state.state = e_task_alive;

	_task->condition.notify_one();
}

// 任务是否存活
bool Task::is_task_alive(const uint64_t &tid)
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

// 获取任务状态
enum task_state Task::task_state(const uint64_t &tid)
{
	auto _task = search_task(tid);

	if (nullptr == _task)
    {
		return e_task_stop;
    }

	return _task->task_state.state;
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
		std::cout << "not find task info = " << tid << ", can not run task!" << std::endl;
        return (void *)0;
    }

    set_thread_name(_task->reg_info.task_attr.task_name.c_str());

	std::unique_lock<std::mutex> lck(_task->mtx);

	// 等待任务启动
	while (e_task_wait == _task->task_state.state) _task->condition.wait(lck);

	std::cout << "task " << _task->reg_info.task_attr.task_name << " run." << std::endl;

	lck.unlock();

	// 实际任务调用
    _task->calls.task();

    return (void *)0;
}

// 任务自动管理
class TaskManage
{
public:
	TaskManage(Task *task) : task_(task), system_reboot_(false) {}
	~TaskManage() {}

public:
	// 任务更新
	void task_update(void) noexcept;

private:
	// 任务时间矫正
	void task_correction_time(void) noexcept;
	// 异常任务过滤
	bool task_filter(std::shared_ptr<TaskDesc> &task);

	// 超时标记
	void timeout_mark(void) noexcept;
	// 崩溃标记
	void dead_mark(void);
	// 异常处理
	void except_do(void);
	
	// 任务崩溃处理
	void task_dead_handler(std::shared_ptr<TaskDesc> &task);
	// 清理崩溃任务
	void clean_dead(void);

private:
	Task *task_;			///< 任务
	time_t last_time_;		///< 最新记录时间
	bool system_reboot_;	///< 系统重启
};

void TaskManage::task_update(void) noexcept
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
		// reboot;
	}
}

void TaskManage::task_correction_time(void) noexcept
{
	static time_t last_time = time(nullptr);
	time_t now = time(nullptr);

	// 时间向前跳变和时间向后跳变超过一分钟，重置任务时间
    if (now < last_time || (now - last_time) > MAX_ERROR_TIME)
	{
		std::unique_lock<std::mutex> lock(task_->mtx_);

		for (auto item : task_->tasks_)
		{
			std::unique_lock<std::mutex> i_lock(item->mtx);

			// 重置时间和次数
			item->task_state.last_update_time = now;
			item->task_state.timeout_times = 0;
		}
	}

	last_time = now;
}

bool TaskManage::task_filter(std::shared_ptr<TaskDesc> &task)
{
	// 非存活任务或任务已经销毁则过滤掉
	return e_task_alive != task->task_state.state || !is_task_alive(task->tid);
}

void TaskManage::dead_mark(void)
{
	std::unique_lock<std::mutex> lock(task_->mtx_);
	
	for (auto item : task_->tasks_)
	{
		if (e_task_dead != item->task_state.state
			&& e_task_stop != item->task_state.state
			&& !is_task_alive(item->tid))
		{
			std::unique_lock<std::mutex> i_lock(item->mtx);
			item->task_state.state = e_task_dead;
		}
	}
}

void TaskManage::task_dead_handler(std::shared_ptr<TaskDesc> &task)
{
	switch (task->reg_info.e_action)
	{
	case e_task_ignore:
		break;
	case e_task_restart:
		if (task->calls.e_action)
			task->calls.e_action();
		// TODO:重新创建任务
		break;
	case e_task_reboot_system:
		if (task->calls.e_action)
			task->calls.e_action();
		system_reboot_ = true;
		break;
	case e_task_default:
	default:
		if (task->calls.e_action)
			task->calls.e_action();
		break;
	}
}

void TaskManage::except_do(void)
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
			
			// 通知任务异常信息
			if (task_->except_fun_) task_->except_fun_(ex_info);

			// 执行超时接口
			if (item->calls.timout_action) item->calls.timout_action();

			// 下个周期做异常处理
			item->task_state.state = e_task_dead;
			lock.unlock();

			break;

		case e_task_dead:
			lock.unlock();

			ex_info.tid = item->tid;
			ex_info.task_name =	item->reg_info.task_attr.task_name;
			ex_info.reason = "except dead";

			if (task_->except_fun_) task_->except_fun_(ex_info);

			task_dead_handler(item);

			break;

		default:
			lock.unlock();
			break;
		}
	}
}

void TaskManage::timeout_mark(void) noexcept
{
	// 系统时间异常矫正
    task_correction_time();

	std::unique_lock<std::mutex> lock(task_->mtx_);
	
	for (auto item : task_->tasks_)
	{
		if (task_filter(item)) continue;

		std::unique_lock<std::mutex> i_lock(item->mtx);

		// 超时判断
		if (time(nullptr) - item->task_state.last_update_time > item->reg_info.alive_time)
		{
			std::cout << "task [" << item->reg_info.task_attr.task_name << ", " << item->tid << "] timeout" << std::endl;

			if (item->task_state.timeout_times++ > MAX_CNT_TASK_TIMEOUT)
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

void TaskManage::clean_dead(void)
{
	std::unique_lock<std::mutex> lock(task_->mtx_);

	task_->tasks_.erase(std::remove_if(task_->tasks_.begin(),
										task_->tasks_.end(),
										[](auto item) -> bool {
											return e_task_dead == item->task_state.state;
										}),
						task_->tasks_.end());
}

TaskKey<int> task_manage(Task *task)
{
	TaskAttribute attr;
	attr.task_name = "task manage";
	attr.stacksize = TASK_STACKSIZE(8);
	attr.priority = e_sys_task_pri_lv;

	TaskKey<int> ret = new_task(attr, [task](void) -> int {
		std::shared_ptr<TaskManage> manage(new TaskManage(task));

		// 检测任务组件退出
		for (; !Task::stop ;)
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));
			manage->task_update();
		}

		return 0;
	});

	return ret;
}

} // namespace wotsen

/************************************************测试代码*********************************************************/

#include <cstring>
#include <thread>
#include <chrono>
// #include "posix_thread.h"

int main(void)
{
	using namespace wotsen;

	std::shared_ptr<Task> task(new Task);

	TaskRegisterInfo reg_info;

	reg_info.task_attr.task_name = "test task";
	reg_info.task_attr.stacksize = TASK_STACKSIZE(50);
	reg_info.task_attr.priority = e_sys_task_pri_lv;
	reg_info.alive_time = 3 * 60;
	reg_info.e_action = e_task_default;

	auto ret = task->register_task(reg_info, [&](int a, int b)->int{
		for (int i = 3; task->is_task_alive(task_id()) && i; i--)
		{
			std::cout << "alive......." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	
		return 100;
	}, 10, 20);

	task->add_task_exit_action(ret.tid, [](){ std::cout << "task exit!" << std::endl; });

	task->task_run(ret.tid);

	task->task_exit(ret.tid);

	TaskAttribute attr;
	attr.task_name = "test2 task";
	attr.stacksize = TASK_STACKSIZE(50);
	attr.priority = e_sys_task_pri_lv;

	auto ret2 = new_task(attr, [](int a, int b)->int{
		for (int i = 3; i; i--)
		{
			std::cout << "alive2......." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	
		return 200;
	}, 10, 20);

	std::cout << ret.fut.get() << std::endl;
	std::cout << ret2.fut.get() << std::endl;

	return 0;
}
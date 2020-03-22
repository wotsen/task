/**
 * @file task_auto_manage.h
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-15
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#pragma once

#include "task.h"

namespace wotsen
{

#define task_dbg(fmt, args...) __dbg ? __dbg("[%s][%d][%s]" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##args) : (void)0

static inline time_t now(void)
{
	return time(nullptr);
}

// 任务自动管理
class TaskAutoManage
{
public:
	TaskAutoManage(Task *task) : task_(task), system_reboot_(false) {}
	~TaskAutoManage() {}

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

TaskKey<int> task_auto_manage(Task *task);

} // namespace wotsen

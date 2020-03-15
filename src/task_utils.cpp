/**
 * @file task_utils.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-15
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <functional>
#include <iostream>
#include "posix_thread.h"
#include "task_utils.h"

namespace wotsen
{

bool _create_util_task(uint64_t *tid, const size_t &stacksize, const int &priority, task_util_call fn, void *arg)
{
	return create_thread(tid, stacksize, priority, (thread_func)fn, arg);
}

// 获取任务id
uint64_t task_id(void)
{
	return thread_id();
}

// 任务检测
bool is_task_alive(const uint64_t &tid)
{
	return thread_exsit(tid);
}

// 设置任务名
void set_task_name(const std::string &name)
{
	set_task_name(name.c_str());
}

void set_task_name(const char *name)
{
	set_thread_name(name);
}

// 结束任务
void kill_task(const uint64_t &tid)
{
	release_thread(tid);
}

} // namespace wotsen
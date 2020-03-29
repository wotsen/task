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
#include "posix_thread.h"
#include "task_utils.h"

namespace wotsen
{

bool _create_util_task(uint64_t *tid, const size_t &stacksize,
						const int &priority, task_util_call fn, void *arg)
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
void set_task_name(const std::string &name, const uint64_t &tid)
{
	set_task_name(name.empty() ? nullptr : name.c_str(), tid);
}

void set_task_name(const char *name, const uint64_t &tid)
{
	set_thread_name(name, tid);
}

// 获取任务名
std::string &&get_task_name(const uint64_t &tid)
{
	return get_thread_name(tid);
}

// 结束任务
void kill_task(const uint64_t &tid)
{
	release_thread(tid);
}

} // namespace wotsen
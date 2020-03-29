/**
 * @file posix_thread.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <cstring>
#include <cerrno>
#include <csignal>
#include <pthread.h>
#include <sys/prctl.h>
#include "posix_thread.h"

namespace wotsen
{

enum
{
	MAX_THREAD_NAME_LEN = 15		///< 线程名最长长度
};

///< 最小栈
#ifndef PTHREAD_STACK_MIN
	#define PTHREAD_STACK_MIN 16384
#endif

/**
 * @brief 创建线程
 * 
 * @param tid 线程id
 * @param stacksize 线程栈大小
 * @param priority 线程优先级
 * @param fn 线程人物接口
 * @param arg 传递给线程的参数
 * @return true 创建成功
 * @return false 创建失败
 */
bool create_thread(uint64_t *tid, const size_t &stacksize,
					const int &priority, thread_func fn, void *arg)
{
	pthread_t _tid = INVALID_PTHREAD_TID;
	pthread_attr_t attr;
	struct sched_param param;
	int min_pri = sched_get_priority_min(SCHED_RR);
	int max_pri = sched_get_priority_max(SCHED_RR);
	size_t _stacksize = stacksize;
	int _pri = priority;

	memset(&attr, 0, sizeof(attr));
	memset(&param, 0, sizeof(param));

	/* 矫正线程栈 */
	_stacksize = _stacksize < PTHREAD_STACK_MIN ? PTHREAD_STACK_MIN : _stacksize;

	/* 矫正优先级 */
	if (min_pri > _pri)
	{
		_pri = min_pri;
	}
	else if (max_pri < _pri)
	{
		_pri = max_pri;
	}
	else
	{
		// PASS
	}

	param.sched_priority = _pri;

	if (pthread_attr_init(&attr) < 0)
	{
		return false;
	}

	/* 设置线程分离 */
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) < 0)
	{
		pthread_attr_destroy(&attr);
		return false;
	}

	/* 设置调度策略FIFO */
	if (pthread_attr_setschedpolicy(&attr, SCHED_RR) < 0)
	{
		pthread_attr_destroy(&attr);
        return false;
	}

	/* 设置线程优先级 */
	if (pthread_attr_setschedparam(&attr, &param) < 0)
	{
		pthread_attr_destroy(&attr);
        return false;
	}

	/* 设置线程栈大小 */
	if (pthread_attr_setstacksize(&attr, stacksize) < 0)
    {
		pthread_attr_destroy(&attr);
        return false;
    }

	/* 创建线程 */
	if (pthread_create(&_tid, &attr, fn, arg) < 0)
	{
		pthread_attr_destroy(&attr);
		return false;
	}

	if (tid)
	{
		*tid = static_cast<uint64_t>(_tid);
	}

	pthread_attr_destroy(&attr);

	return true;
}

uint64_t thread_id(void)
{
	return static_cast<uint64_t>(pthread_self());
}

/**
 * @brief 设置线程名称
 * 
 * @param name 
 */
void set_thread_name(const char *name, const uint64_t &tid)
{
	char pname[MAX_THREAD_NAME_LEN + 1] = {'\0'};
	pthread_t _tid = tid != INVALID_PTHREAD_TID ? tid : pthread_self();

	if (name)
    {
        sprintf(pname, "%s", (char *)name);
        // prctl(PR_SET_NAME, pname);
    }
    else
    {
        sprintf(pname, "p%zu", _tid);
        // prctl(PR_SET_NAME, pname);
    }

	pthread_setname_np(_tid, pname);
}

///< 获取线程名
std::string &&get_thread_name(const uint64_t &tid)
{
	char pname[MAX_THREAD_NAME_LEN + 1] = {'\0'};
	pthread_t _tid = tid != INVALID_PTHREAD_TID ? tid : pthread_self();

	pthread_getname_np(_tid, pname, sizeof(pname));

	return std::move(std::string(pname));
}

/**
 * @brief 释放线程
 * 
 * @param tid 线程号
 * @return true 释放成功
 * @return false 释放失败
 */
bool release_thread(const uint64_t &tid)
{
	if (thread_exsit(tid))
	{
		return pthread_cancel(tid) == 0;
	}

	return true;
}

/**
 * @brief 检测线程存活
 * 
 * @param tid 线程号
 * @return true 线程存在
 * @return false 线程不存在
 */
bool thread_exsit(const uint64_t &tid)
{
	if (INVALID_PTHREAD_TID == tid)
	{
		return false;
	}

	int pthread_kill_err = pthread_kill(tid, 0);

	if (ESRCH == pthread_kill_err)
    {
        return false;
    }
    else if (EINVAL == pthread_kill_err)
    {
        return false;
    }
    else
    {
        return true;
    }
}

} // namespace wotsen

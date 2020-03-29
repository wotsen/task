/**
 * @file posix_thread.h
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-13
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#pragma once

#include <cinttypes>
#include <cstdio>
#include <string>

namespace wotsen
{

#define INVALID_PTHREAD_TID 0		///< 无效tid

///< 栈长度，传入数字以k为单位
#define STACKSIZE(k) ((k)*1024)

/**
 * @brief 线程优先级
 * 
 */
enum thread_priority : int
{
	e_max_thread_pri_lv	= 100,
	e_sys_thread_pri_lv	= 90,
	e_run_thread_pri_lv	= 80,
	e_fun_thread_pri_lv	= 70,
	e_thr_thread_pri_lv	= 60,
	e_min_thread_pri_lv	= 50,
};

///< 线程毁掉接口
typedef void *(*thread_func)(void *);

///< 创建线程
bool create_thread(uint64_t *tid, const size_t &stacksize, const int &priority, thread_func fn, void *arg=nullptr);

///< 获取本线程的id
uint64_t thread_id(void);

///< 设置线程名
void set_thread_name(const char *name, const uint64_t &tid=INVALID_PTHREAD_TID);

///< 获取线程名
std::string &&get_thread_name(const uint64_t &tid=INVALID_PTHREAD_TID);

///< 释放线程
bool release_thread(const uint64_t &tid);

///< 检测线程存活
bool thread_exsit(const uint64_t &tid);

} // namespace wotsen

/**
 * @file demo.cpp
 * @author 余王亮 (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-03-15
 * 
 * @copyright Copyright (c) 2020
 * 
 */

/************************************************测试代码*********************************************************/

#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdarg>
#include <cassert>
#include "src/task.h"

using namespace wotsen;

static void my_print(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	return (void)0;
}

int main(void)
{
	// 版本
	std::string version(get_task_version());
	printf("version : %s\n", version.c_str());

	// 设置回调
	set_task_debug_cb(my_print);

	/*************************************任务1********************************************/

	TaskRegisterInfo reg_info;

	reg_info.task_attr.task_name = "test task1";
	reg_info.task_attr.stacksize = TASK_STACKSIZE(50);
	reg_info.task_attr.priority = e_sys_task_pri_lv;
	reg_info.alive_time = 3 * 60;
	reg_info.e_action = e_task_default;

	auto ret = Task::register_task(reg_info, [&](int a, int b) -> int {
		for (int i = 6; Task::is_task_alive(task_id()) && i; i--)
		{
			std::cout << "alive1....... " << task_id() << std::endl;
			assert(Task::task_state(task_id()) == wotsen::e_task_alive);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		return 100;
	},
								   10, 20);

	// 启动
	Task::task_run(ret.tid);

	// 添加异常
	Task::add_task_except_action(ret.tid, [](const uint64_t &tid) {
		std::cout << "task error!" << tid << std::endl;
		assert(Task::task_state(tid) == wotsen::e_task_dead);
	},
								 ret.tid);

	// 暂停
	Task::task_wait(ret.tid);
	std::this_thread::sleep_for(std::chrono::seconds(1));

	/*************************************任务2********************************************/
	reg_info.task_attr.task_name = "test task2";
	reg_info.alive_time = 90;

	auto ret2 = Task::register_task(reg_info, [](int a, int b) -> int {
		for (int i = 3; Task::is_task_alive(task_id()) && i; i--)
		{
			std::cout << "alive2....... " << task_id() << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		return 200;
	},
									10, 20);

	// 添加退出回调
	Task::add_task_exit_action(ret2.tid, []() { std::cout << "task2 exit!" << std::endl; });
	// 启动
	Task::task_run(ret2.tid);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	// 手动停止
	Task::task_exit(ret2.tid);

	/*************************************任务3********************************************/

	TaskRegisterInfo reg_info1;

	reg_info1.task_attr.task_name = "test task3";
	reg_info1.task_attr.stacksize = TASK_STACKSIZE(50);
	reg_info1.task_attr.priority = e_sys_task_pri_lv;
	reg_info1.alive_time = 1;
	reg_info1.e_action = e_task_default;

	auto ret3 = Task::register_task(reg_info1, [](int a, int b) -> int {
		for (int i = 30; Task::is_task_alive(task_id()) && i; i--)
		{
			std::cout << "alive3....... " << task_id() << std::endl;
			assert(Task::task_state(task_id()) == wotsen::e_task_alive);
			std::this_thread::sleep_for(std::chrono::seconds(8));
		}

		return 300;
	},
									10, 20);

	// // 添加超时
	Task::add_task_timeout_action(ret3.tid, []() { std::cout << "task3 timeout!" << std::endl; });
	// 启动
	Task::task_run(ret3.tid);

	//////////////////////////////////////////////////////////////////////////////////////////

	// 任务n，使用通用接口
	TaskAttribute attr;
	attr.task_name = "test task n";
	attr.stacksize = TASK_STACKSIZE(50);
	attr.priority = e_sys_task_pri_lv;

	auto retn = new_task(attr, [](int a, int b) -> int {
		for (int i = 3; i; i--)
		{
			std::cout << "alive n......." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		return 400;
	},
						 10, 20);

	std::cout << "task2 ret" << ret2.fut.get() << std::endl;
	std::cout << "task3 ret" << ret3.fut.get() << std::endl;
	std::cout << "task4 ret" << retn.fut.get() << std::endl;

	// 继续
	Task::task_continue(ret.tid);
	std::cout << "task1 ret" << ret.fut.get() << std::endl;

	return 0;
}

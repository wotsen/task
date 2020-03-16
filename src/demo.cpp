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
#include "task.h"

using namespace wotsen;

static void my_print(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("about_stdarg >> my_print : ");
    vprintf(fmt, ap);
    va_end(ap);

    return (void)0;
}

int main(void)
{
	set_task_debug_cb(my_print);

	TaskRegisterInfo reg_info;

	reg_info.task_attr.task_name = "test task";
	reg_info.task_attr.stacksize = TASK_STACKSIZE(50);
	reg_info.task_attr.priority = e_sys_task_pri_lv;
	reg_info.alive_time = 3 * 60;
	reg_info.e_action = e_task_default;

	auto ret = Task::register_task(reg_info, [&](int a, int b)->int{
		for (int i = 3; Task::is_task_alive(task_id()) && i; i--)
		{
			std::cout << "alive......." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	
		return 100;
	}, 10, 20);

	Task::add_task_exit_action(ret.tid, [](){ std::cout << "task exit!" << std::endl; });

	Task::task_run(ret.tid);

	Task::task_exit(ret.tid);

	//////////////////////////////////////////////////////////////////////////////////////////

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

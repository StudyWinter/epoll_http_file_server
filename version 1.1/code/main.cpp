/*************************************************************************
 > File Name: main.cpp
 > Author: Winter
 > Created Time: 2022年05月07日 星期六 15时36分08秒
 ************************************************************************/

#include <iostream>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include "Server.h"


int main(int argc, char* argv[])
{
	if (argc < 3) {
		std::cout << "please input as ./server port /dir" << std::endl;
	}

	// 获得用户输入的端口
	int port = atoi(argv[1]);

	// 改变工作目录
	int res = chdir(argv[2]);
	if (res != 0) {
		perror("chdir error");
		exit(1);
	}

	// 定义服务器对象
	Server server;

	// 启动epoll
	server.epoll_run(port);

	return 0;
}


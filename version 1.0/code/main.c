/*************************************************************************
  > File Name: main.c
  > Author: Winter
  > Created Time: 2022年03月15日 星期二 10时40分34秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "server.h"
#include "tool.h"

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("please input as ./server port dir\n");
	}
	// 获取用户输入的端口
	int port = atoi(argv[1]);

	// 改变工作目录
	int res = chdir(argv[2]);
	if (res != 0) {
		perror("chdr error");
		exit(1);
	}

	// 启动epoll
	epoll_run(port);

	return 0;
}

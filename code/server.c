/*************************************************************************
  > File Name: server.c
  > Author: Winter
  > Created Time: 2022年03月13日 星期日 10时38分18秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "server.h"
#include "tool.h"

#define MAXSIZE 2048


// 运行epoll
/*
 * port:表示端口
 */
void epoll_run(int port) {
	int i = 0;
	struct epoll_event all_event[MAXSIZE];             // epoll_wait函数的参数

	// 创建一个epoll监听树根，返回值是监听红黑树的文件描述符
	int epfd = epoll_create(MAXSIZE);
	if (epfd == -1) {
		perror("epoll_create error");
		exit(1);
	}

	// 创建监听文件描述符lfd，并添加到监听树上
	int lfd = init_listen_fd(port, epfd);
	while (1) {
		// 监听对应事件
		int res = epoll_wait(epfd, all_event, MAXSIZE, -1);
		if (res == -1) {
			perror("epoll_wait error");
			exit(1);
		}

		// 遍历
		for (i = 0; i < res; i++) {
			// 只处理读事件，其他事件默认不处理
			struct epoll_event* pev = &all_event[i];          // 取出数据

			// 如果不是读事件，继续处理
			if (!(pev->events & EPOLLIN)) {
				continue;
			}
			// 接受连接请求
			if (pev->data.fd == lfd) {
				// 调用接受请求连接函数
				do_accept(lfd, epfd);
			} else {
				// 读数据
				printf("========================before do_read, res = %d\n", res);
				do_read(pev->data.fd, epfd);
				printf("========================after do_read==========\n");
			}
		}
	}
}


// 初始化监听
/*
 * port是端口号
 * epfd是监听红黑树的文件描述符
 */
int init_listen_fd(int port, int epfd) {
	// 创建监听的lfd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 端口复用
	int opt = 1;
	int rr = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (rr == -1) {
		perror("setsockopt error");
		exit(1);
	}

	// 服务器绑定地址结构
	int res = bind(lfd, (struct sockaddr*)(&server_addr), sizeof(server_addr));
	if (res == -1) {
		perror("bind error");
		exit(1);
	}
	// 设置监听上限
	res = listen(lfd, 128);
	if (res == -1) {
		perror("listen error");
		exit(1);
	}

	// 将lfd添加到epoll树上
	struct epoll_event ev;
	ev.events = EPOLLIN;             // 监听读事件
	ev.data.fd = lfd;                // 监听建立连接的文件描述符

	res = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (res == -1) {
		perror("epoll_ctl add lfd error");
		exit(1);
	}
	return lfd;
}

// epoll阻塞监听
/*
 * lfd是连接的文件描述符
 * epfd是监听红黑树的文件描述符
 */
void do_accept(int lfd, int epfd) {
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	// 阻塞监听
	int cfd = accept(lfd, (struct sockaddr*)(&client_addr), &client_addr_len);
	if (cfd == -1) {
		perror("accept error");
		exit(1);
	}

	// 打印客户端信息，ip+port
	char client_ip[64] = {0};
	printf("the new client ip: %s, port = %d, cfd = %d\n",
			inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)), 
			ntohs(client_addr.sin_port),
			cfd
	      );

	// 设置cfd为非阻塞
	int flag = fcntl(cfd, F_GETFL);
	flag = flag | O_NONBLOCK;                 // 设置非阻塞
	fcntl(cfd, F_SETFL, flag);

	// 将新节点cfd挂到epoll树上
	struct epoll_event ev;
	ev.data.fd = cfd;

	// 设置边沿非阻塞模式，默认是水平模式
	ev.events = EPOLLIN | EPOLLET;

	int res = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (res == -1) {
		perror("epoll_ctl add cfd error");
		exit(1);
	}
}

// 读文件
/*
 * cfd是通信的文件描述符
 * epfd是监听红黑树的文件描述符
 */
void do_read(int cfd, int epfd) {
	// 读取一行http协议，拆分，获取get文件名和协议号
	char line[1024] = {0};
	int len = get_line(cfd, line, sizeof(line));                // 读的是一行，读http请求协议首行  GET /hello.c HTTP/1.1
	if (len == 0) {
		printf("服务器检查到客户端已经关闭......\n");
		// 关闭套接字，cfd从epoll树上摘下
		disconnect(cfd, epfd);
	} else {
		printf("================请求头===============\n");
		printf("请求行数据：%s", line);

		// 数据还没有读完
		while (1) {
			char buff[1024] = {0};
			len = get_line(cfd, buff, sizeof(buff));
			if (buff[0] == '\n') {
				break;
			} else if (len == -1)
			{
				break;
			}
		}
		printf("===============THE END================\n");
	}
	// 判断get请求
	// 请求行：get /hello.c http/1.1
	if (strncasecmp(line, "GET", 3) == 0) {
		// 处理http请求
		http_request(cfd, line);
		// 关闭套接字，将cfd从epoll红黑树上摘下
		disconnect(cfd, epfd);
	}
}

// 获取一行 \r\n结尾的数据
/*
 * cfd是通信的文件描述符
 * buff是缓冲区
 * size是缓冲区大小
 * 返回值是去掉\r\n的数据长度
 */
int get_line(int cfd, char* buff, int size) {
	int i = 0;
	char c = '\0';
	int n;
	while ((i < size - 1) && (c != '\n')) {
		n = recv(cfd, &c, 1, 0);                    // 从cfd读数据，每次读一个数据，读到c中
		if (n > 0) {
			if (c == '\r') {
				n = recv(cfd, &c, 1, MSG_PEEK);     // MSG_PEEK表示只查看数据，不取走
				if ((n > 0) && (c == '\n')) {
					recv(cfd, &c, 1, 0);
				} else {
					c = '\n';
				}
			}
			buff[i] = c;
			i++;
		} else {
			c = '\n';
		}
	}
	buff[i] = '\0';
	if (n == -1) {
		i = n;
	}
	return i;
}

// 处理http请求
/*
 * cfd是通信文件描述符
 * request是http协议头
 */
void http_request(int cfd, const char* request) {
	// 拆分http协议
	char method[12], path[1024], protocol[12];
	sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);                   // 利用正则表达式取出对应的文件
	printf("method = %s, path = %s, protocol = %s\n", method, path, protocol);

	// 转码，将不能识别的中文乱码转成中文
	// 解码 %23 %34 %5f
	decode_str(path, path);
	char* file = path + 1;          // 去掉path中的/ 获取访问文件名

	// 如果没有指定访问的资源，默认显示资源目录中的内容
	if (strcmp(path, "/") == 0) {
		file = "./";
	}

	struct stat sbuf;
	// 判断文件是否存在
	int res = stat(file, &sbuf);
	if (res == -1) {
		// 回发给浏览器404页面
		send_error(cfd, 404, "Not Found", "No such file or directory1111");
		return;
	}

	// 判断文件类型
	if (S_ISREG(sbuf.st_mode)) {
		// 普通文件
		printf("It's a file\n");
		// 回发http协议应答
		send_respond(cfd, 200, "OK", get_file_type(file), sbuf.st_size);
		// 回发给客户端请求数据内容
		send_file(cfd, file);
	} else if (S_ISDIR(sbuf.st_mode)) {
		printf("It's a dir\n");
		// 是目录文件
		// 发送消息报头
		send_respond(cfd, 200, "OK", get_file_type(".html"), -1);
		// 发送目录信息
		send_dir(cfd, file);
	}
}

// 发生头响应给浏览器
/*
 * cfd是通信的文件描述符
 * no是错误号
 * disp是错误描述
 * type是回发的文件类型
 * len是文件的长度
 */
void send_respond(int cfd, int no, char* disp, char* type, long len) {
	char buff[4096] = {0};
	// 状态行
	sprintf(buff, "HTTP/1.1 %d %s\r\n", no, disp);
	send(cfd, buff, strlen(buff), 0);
	// 消息报头
	sprintf(buff, "Content-Type:%s\r\n", type);
	sprintf(buff + strlen(buff), "Content-Length:%ld\r\n", len);
	send(cfd, buff, strlen(buff), 0);
	// 空行
	send(cfd, "\r\n", 2, 0);
}

// 通过文件名获取文件类型
/*
 * name是路径+name
 */
const char* get_file_type(const char* name) {
	char* dot;

	// 自右向左查找‘.’字符, 如不存在返回NULL
	dot = strrchr(name, '.');   
	if (dot == NULL) {
		return "text/plain; charset=utf-8";
	}
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) {
		return "text/html; charset=utf-8";
	}
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
		return "image/jpeg";
	}
	if (strcmp(dot, ".gif") == 0) {
		return "image/gif";
	}
	if (strcmp(dot, ".png") == 0) {
		return "image/png";
	}
	if (strcmp(dot, ".css") == 0) {
		return "text/css";
	}
	if (strcmp(dot, ".au") == 0) {
		return "audio/basic";
	}
	if (strcmp( dot, ".wav" ) == 0) {
		return "audio/wav";
	}
	if (strcmp(dot, ".avi") == 0) {
		return "video/x-msvideo";
	}
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0) {
		return "video/quicktime";
	}
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0) {
		return "video/mpeg";
	}
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0) {
		return "model/vrml";
	}
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0) {
		return "audio/midi";
	}
	if (strcmp(dot, ".mp3") == 0) {
		return "audio/mpeg";
	}
	if (strcmp(dot, ".ogg") == 0) {
		return "application/ogg";
	}
	if (strcmp(dot, ".pac") == 0) {
		return "application/x-ns-proxy-autoconfig";
	}
	return "text/plain; charset=utf-8";
}

// 发送文件内容给浏览器
/*
* cfd是通信的文件描述符
* file是文件路径+文件名
*/
void send_file(int cfd, const char* file) {
	int n = 0, res = 0;
	char buff[4096] = {0};
	// 打开服务器本地文件
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		// 发送404页面
		send_error(cfd, 404, "Not Found", "No such file or direntry222");
		exit(1);
	}

	// 读数据
	while ((n = read(fd, buff, sizeof(buff))) > 0) {
		res = send(cfd, buff, n, 0);               // 发送给客户端
		if (res == -1) {
			printf("errno = %d\n", errno);
			if (errno == EAGAIN) {
				printf("===============EAGAIN\n");
				continue;
			} else if (errno == EINTR) {
				printf("===============EINTR\n");
				continue;
			} else {
				perror("send error");
				exit(1);
			}
		}
	}

	if (n == -1) {
		perror("read file errno");
		exit(1);
	}
	close(fd);
}

// 发送目录内容给浏览器
/*
* cfd是通信文件描述符
* dirname是目录路径+名字
*/
void send_dir(int cfd, const char* dirname) {
	int i, res;
	// 拼一个html页面
	char buff[4096] = {0};
	sprintf(buff, "<html><head><title>目录名:%s</title></head>", dirname);
	sprintf(buff + strlen(buff), "<body><h1>当前目录:%s</h1><table>", dirname);

	char enstr[1024] = {0};
	char path[1024] = {0};

	// 目录项二级指针
	struct dirent** ptr;
	int num = scandir(dirname, &ptr, NULL, alphasort);

	// 遍历
	for (i = 0; i < num; i++) {
		char* name = ptr[i]->d_name;        // 得到目录名字

		// 拼接文件的完整路径
		sprintf(path, "%s%s", dirname, name);
		printf("path = %s   ========\n", path);
		struct stat st;
		stat(path, &st);

		// 编码生成%E5之类的东西
		encode_str(enstr, sizeof(enstr), name);

		// 如果是文件
		if (S_ISREG(st.st_mode)) {
			sprintf(buff + strlen(buff), 
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		} else if (S_ISDIR(st.st_mode)) {
			// 目录
			sprintf(buff + strlen(buff),
				"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}

		res = send(cfd, buff, strlen(buff), 0);
		if (res == -1) {
			printf("errno = %d\n", errno);
			if (errno == EAGAIN) {
				printf("-----------EAGAIN\n");
				continue;
			} else if (errno == EINTR) {
				printf("-----------EINTR\n");
				continue;
			}
			else {
				perror("send error\n");
				exit(1);
			}
		}
		memset(buff, 0, sizeof(buff));
	}
	sprintf(buff + strlen(buff), "</table></body></html>");
	send(cfd, buff, strlen(buff), 0);
	printf("dir message send ok......\n");
}

// 发送错误页面
void send_error(int cfd, int status, char* title, char* text) {
	char buff[4096] = {0};

	sprintf(buff, "%s %d %s\r\n", "HTTP/1.1", status, title);
	sprintf(buff + strlen(buff), "Content-Type:%s\r\n", "text/html");
	sprintf(buff + strlen(buff), "Content-Length:%d\r\n", -1);
	sprintf(buff + strlen(buff), "Connection: close\r\n");
	send(cfd, buff, strlen(buff), 0);
	send(cfd, "\r\n", 2, 0);

	memset(buff, 0, sizeof(buff));

	sprintf(buff, "<html><head><title>%d %s</title></head>\n", status, title);
	sprintf(buff + strlen(buff), "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
	sprintf(buff + strlen(buff), "%s\n", text);
	sprintf(buff + strlen(buff), "<hr>\n</body>\n</html>\n");
	send(cfd, buff, strlen(buff), 0);

	return ;
}

// 封装一个断开连接的函数
void disconnect(int cfd, int epfd) {
	int res = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (res != 0) {
		perror("epoll_ctl del cfd error");
		exit(1);
	}
	printf("the %d client has been closed......\n");
	close(cfd);
}
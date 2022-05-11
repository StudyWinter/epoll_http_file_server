/*************************************************************************
 > File Name: Server.cpp
 > Author: Winter
 > Created Time: 2022年05月07日 星期六 16时22分51秒
 ************************************************************************/

#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "Server.h"
#include "Tool.h"

#define MAXSIZE 2048

// 运行epoll
/*
 *	port:表示端口号
 *	功能：传入port，运行epoll。是整个程序的起始位置
 */
void Server::epoll_run(int port) {
	int i = 0;
	struct epoll_event all_event[MAXSIZE];             // epoll_wait函数的参数

	// 创建一个epoll监听红黑树树根，返回值是监听红黑树的文件描述符
	int epfd = epoll_create(MAXSIZE);
	if (epfd == -1) {
		perror("epoll_create error");
		exit(1);
	}

	// 创建监听文件描述符fd，并添加到红黑树上
	int lfd = init_listen_fd(port, epfd);

	// 循环监听
	while (1) {
		// 监听对应的事件，返回值是监听发生事件的个数
		int res = epoll_wait(epfd, all_event, MAXSIZE, -1);
		if (res == -1) {
			perror("epoll_wait error");
			exit(1);
		}

		for (i = 0; i < res; i++) {
			// 只处理读事件
			struct epoll_event* pev = &all_event[i];    			// 取出数据

			// 跳过非读事件
			if (!(pev->events & EPOLLIN)) {
				continue;
			}
			// 接受连接请求
			if (pev->data.fd == lfd) {
				// 调用处理连接请求的函数
				do_accept(lfd, epfd);
			} else {
				// 处理通信数据
				std::cout << "==============before do_read, res = " << res << std::endl;
				do_read(pev->data.fd, epfd);
				std::cout << "==============after do_read=============="<< std::endl;
			}
		}
	}
}

// 初始化监听
/*
 *	port:端口号
 * 	epfd:epoll监听红黑树的根节点
 * 	功能：根据port创建监听文件描述符lfd，再返回lfd
 * 	返回值：lfd
 */
int Server::init_listen_fd(int port, int epfd) {
	// 1 创建socket监听
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	// 定义服务器地址结构
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));              // 地址结构清零
	server_addr.sin_family = AF_INET;                      // ipv4网络协议
	server_addr.sin_port = htons(port);                    // 端口号
	server_addr.sin_addr.s_addr =htonl(INADDR_ANY);        // ip地址

	// 端口复用
	int opt = 1;
	int rr = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (rr == -1) {
		perror("setsockopt error");
		exit(1);
	}

	// 2 服务器绑定地址结构
	int res = bind(lfd, (struct sockaddr*)(&server_addr), sizeof(server_addr));
	if (res == -1) {
		perror("bind error");
		exit(1);
	}

	// 3 设置监听上限
	res = listen(lfd, 128);
	if (res == -1) {
		perror("listen error");
		exit(1);
	}

	// 4 将监听文件描述符lfd添加到红黑树上，红黑树的根节点已经传进来了
	struct epoll_event ev;            // epoll_ctl的参数
	ev.data.fd = lfd;                 // 监听建立连接的文件描述符lfd
	ev.events = EPOLLIN;              // 监听读事件

	res = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (res == -1) {
		perror("epoll_ctl add lfd error");
		exit(1);
	}
	return lfd;
}


// epoll阻塞监听
/*
 *	lfd：连接文件描述符 
 * 	epfd:epoll监听红黑树的根节点
 * 	功能：处理连接，并将连接文件描述符挂到红黑树上
 */
void Server::do_accept(int lfd, int epfd) {
	struct sockaddr_in client_addr;             // 客户端地址
	socklen_t client_len = sizeof(client_addr);

	// 阻塞监听
	int cfd = accept(lfd, (struct sockaddr*)(&client_addr), &client_len);
	if (cfd == -1) {
		perror("accept error");
		exit(1);
	}

	// 打印客户端信息
	char client_ip[64] = {0};
	std::cout << "the new client ip =" << inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, sizeof(client_ip))
				<< ", port = " << ntohs(client_addr.sin_port)
				<< "cfd = " << cfd << std::endl;

	// 设置cfd为非阻塞
	int flag = fcntl(cfd, F_GETFL);
	flag = flag | O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	// 将新节点挂在epoll监听红黑树上
	struct epoll_event ev;
	ev.data.fd = cfd;

	// 设置成边沿阻塞模式
	ev.events = EPOLLIN | EPOLLET;

	// 将通信文件描述符cfd添加到epoll红黑树上
	int res = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (res == -1) {
		perror("epoll_ctl add cfd error111");
		exit(1);
	}
}


// 读文件
/*
 *	cfd是通信的文件描述符 
 * 	epfd是监听红黑树的文件描述符
 * 	功能：处理通信的数据，即读文件
 */
void Server::do_read(int cfd, int epfd) {
	// 读取一行http协议，拆分，获取get文件名和协议号
	char line[1024] = {0};
	int len = get_line(cfd, line, sizeof(line));                   // 读的是一行，读http请求协议首行
	if (len == 0) {
		std::cout << "服务器检查到客户端已经关闭......" << std::endl;
		// 关闭套接字，将通信的cfd从epoll红黑树上摘下
		disconnect(cfd, epfd);
	} else {
		// 有数据
		std::cout << "======================请求头===================" << std::endl;
		std::cout << "请求行数据:" << line;

		// 数据还没有读完，继续读
		while (1) {
			char buff[1024] = {0};
			len = get_line(cfd, buff, sizeof(buff));
			if (buff[0] == '\n') {
				break;
			} else if (len == -1) {
				break;
			}
		}
		std::cout << "================end================" << std::endl;
	}
	// 判断get请求
	// 请求行： get /hello.c http/1.1
	// strncasecmp用来比较line和GET前3个字符，忽略大小
	if (strncasecmp(line, "GET", 3) == 0) {
		// 处理http请求
		http_request(cfd, line);
		// 关闭套接字，将cfd从epoll红黑树上摘下
		disconnect(cfd, epfd);
	}
}


// 获取一行\r\n结尾的数据
/*
 * cfd是通信的文件描述符 
 * buff是缓冲区
 * size是缓冲区大小
 * 返回值是去掉\r\n的数据长度
 */
int Server::get_line(int cfd, char* buff, int size) {
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n')) {
		n = recv(cfd, &c, 1, 0);                    // 从cfd读取数据，每次读取一个数据，读到c中
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
 *	cfd是通信的文件描述符 
 * 	request是http协议头
 */
void Server::http_request(int cfd, const char* request) {
	char method[12], path[1024], protocol[12];
	sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);      // 利用正则表达式取出对应的文件，存储
	std::cout << "method = " << method <<", path = " << path <<", protocol = " << protocol << std::endl;

	// 转码，将不能识别的中文乱码转换成中文
	Tool tool;
	tool.decode_str(path, path);
	char* file = path + 1;            // 去除path中的/ 获取访问文件名

	// 如果没有指定访问的资源，默认显示资源目录中的内容
	if (strcmp(path, "/") == 0) {
		file = "./";
	}

	// 处理文件的结构体
	struct stat sbuf;
	int res = stat(file, &sbuf);
	if (res == -1) {
		// 回发给浏览器404页面
		send_error(cfd, 404, "Not Found", "No such file or directory");
		return;
	}

	// 判断文件类型
	if (S_ISREG(sbuf.st_mode)) {
		// 普通文件
		std::cout << "It's a file" << std::endl;
		// 回发http协议应答
		send_respond(cfd, 200, "OK", get_file_type(file), sbuf.st_size);
		// 回发给客户端请求数据内容
		send_file(cfd, file);
	} else if (S_ISDIR(sbuf.st_mode)) {
		// 是目录文件
		std::cout << "It's a dir" << std::endl;
		// 回发消息报头
		send_respond(cfd, 200, "OK", get_file_type(".html"), -1);
		// 回发目录信息
		send_dir(cfd, file);
	}
}


// 发送头响应给浏览器
/*
 *	cfd是通信的文件描述符 
 * 	no是状态号
 * 	disp是描述
 * 	type是回发的文件类型
 * 	len是文件的长度
 */
void Server::send_respond(int cfd, int no, char* disp, const char* type, long len) {
	char buff[4096] = {0};
	// 状态行
	sprintf(buff, "HTTP/1.1 %d %s\r\n", no, disp);         // 拼接
	send(cfd, buff, strlen(buff), 0);                      // 将数据发送给cfd

	// 消息报头
	sprintf(buff, "Content-Type:%s\r\n", type);
	sprintf(buff + strlen(buff), "Content-Length:%ld\r\n", len);
	send(cfd, buff, strlen(buff), 0);

	// 空行
	send(cfd, "\r\n", 2, 0);
}

// 通过文件名获取文件类型
/*
 *	name是path+name 
 */ 
const char* Server::get_file_type(const char* name) {
	const char* dot;

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
 *	cfd是通信的文件描述符
 * 	file是文件路径+文件名
 */
void Server::send_file(int cfd, const char* file) {
	int n = 0, res = 0;
	char buff[4096] = {0};
	// 以只读的方式打开服务器的本地文件
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		// 发送404页面
		send_error(cfd, 404, "Not Found", "No such file or direntrt");
		exit(1);
	}

	// 读数据
	while ((n = read(fd, buff, sizeof(buff))) > 0) {
		// 发送给客户端
		res = send(cfd, buff, n, 0);
		if (res == -1) {
			std::cout << "errno = " << errno << std::endl;
			if (errno == EAGAIN) {
				std::cout << "===========EAGAIN" << std::endl;
				continue;
			} else if (errno == EINTR) {
				std::cout << "===========EINTR" << std::endl;
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
void Server::send_dir(int cfd, const char* dirname) {
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
		char* name = ptr[i]->d_name;              // 得到目录名字

		// 拼接文件的完整路径
		sprintf(path, "%s%s", dirname, name);
		std::cout << "path = " << path << "============" << std::endl;
		struct stat st;
		stat(path, &st);

		// 编码生成%E5之类的东西
		Tool tool;
		tool.encode_str(enstr, sizeof(enstr), name);

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
		if (res  == -1) {
			std::cout << "errno = " << errno << std::endl;
			if (errno == EAGAIN) {
				std::cout << "===========EAGAIN" << std::endl;
				continue;
			} else if (errno == EINTR) {
				std::cout << "===========EINTR" << std::endl;
				continue;
			} else {
				perror("send error");
				exit(1);
			}
		}
		memset(buff, 0 ,sizeof(buff));
	}
	sprintf(buff + strlen(buff), "</table></body></html>");
	send(cfd, buff, strlen(buff), 0);
	printf("dir message send ok......\n");
}

// 发送错误页面
void Server::send_error(int cfd, int status, char* title, char* text) {
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

// 断开一个连接的函数
void Server::disconnect(int cfd, int epfd) {
	int res = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (res != 0) {
		perror("epoll_ctl del cfd error222");
		exit(1);
	}
	// std::cout << "the" << cfd << "client has benen closed......" << std::endl;
	close(cfd);
}
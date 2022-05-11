/*************************************************************************
 > File Name: Server.cpp
 > Author: Winter
 > Created Time: 2022年05月07日 星期六 15时46分26秒
 ************************************************************************/

#ifndef _SERVER_H_
#define _SERVER_H_

class Server {
public:
	int init_listen_fd(int port, int epfd);                                       // 初始化监听
	void epoll_run(int port);                                                     // 运行epoll
	void do_read(int cfd, int epfd);                                              // 读文件
	void do_accept(int lfd, int epfd);                                            // epoll阻塞监听

	void send_dir(int cfd, const char* dirname);                                  // 发送目录内容给浏览器
	void send_file(int cfd, const char* file);                              	  // 发送文件内容给浏览器
	void send_respond(int cfd, int no, char* disp, const char* type, long len);   // 发送头响应给浏览器
	void send_error(int cfd, int status, char* title, char* text);                // 发送错误页面

	void http_request(int cfd, const char* request);                              // 处理http请求
	const char* get_file_type(const char* name);                                  // 通过文件名获取文件类型
	int get_line(int cfd, char* buff, int size);                                  // 获取一行\r\n结尾的数据

	void disconnect(int cfd, int epfd);                                           // 断开连接函数
};

#endif
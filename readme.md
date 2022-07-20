# epoll_http_server

### 0 文件夹介绍

- 思路.txt是本人总结的思路，方便理解，仅做参考；
- version 1.0是C语言版本写的项目
- version 1.1是CPP版本写的项目
- epoll_http_server.assets是readme.md的图片

### 1 项目目标

写一个浏览器用户访问服务器文件的WEB服务器

### 2 需求分析

（1）基于epoll的多路IO复用技术实现多用户访问；

（2）可以访问不同类型的文件；

（3）可以访问目录；

### 3 模块介绍

#### 3.1 tool头文件

文件夹的tool.h中声明了三个函数：16进制转化10进制、编码、解码。

这里的编码是指将汉字处理成unicode码，解码是其反过程。

16进制转换成10进制，hexit函数

```c++
int hexit(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
}
```

编码函数，encode_str函数

```c++
void encode_str(char* to, int tosize, const char* from){
	int tolen;

	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
		if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {      
			*to = *from;
			++to;
			++tolen;
		} else {
			sprintf(to, "%%%02x", (int) *from & 0xff);
			to += 3;
			tolen += 3;
		}
	}
	*to = '\0';
}
```

解码函数，decode_str函数

```c++
void decode_str(char* to, char* from) {
	for ( ; *from != '\0'; ++to, ++from  ) {     
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {       
			*to = hexit(from[1])*16 + hexit(from[2]);
			from += 2;                      
		} else {
			*to = *from;
		}
	}
	*to = '\0';
}
```

#### 3.2 server文件

1、epoll_run函数
	功能：
	（1）创建epoll监听红黑树；
	（2）调用init_listen_fd函数；
	（3）只处理读事件，是连接请求，调用do_accept函数处理连接请求；是读数据请求，调用do_read函数处理。

读数据请求。

2、init_listen_fd函数
	功能：
	（1）创建监听文件描述符lfd；
	（2）设置端口复用、绑定地址结构、设置监听上限、创建epoll监听红黑树等操作；
	（3）将监听文件描述符添加到epoll红黑树上。

3、do_accept函数
	功能：
	（1）阻塞监听监听文件描述符lfd，得到通信文件描述符cfd；
	（2）将通信文件描述符设置成非阻塞方式，并添加到epoll红黑树上，将红黑树设置成边沿模式（默认是非阻塞

方式）。

4、do_read函数
	功能：
	（1）读取一行来自浏览器的http协议，调用http_request处理get请求，并将通信文件描述符从epoll红黑树上

del掉。

5、get_line函数
	功能：
	（1）获取一行 \r\n结尾的数据

6、http_request函数
	功能：
	（2）拆分http协议请求行，利用sscanf和正则表达式分成请求方法（GET）、路径（path）、协议

（protocol）
	（2）调用decode_str函数将路径解码成unicode码%23 %34 %5f
	（3）判断文件类型；
	（4）如果是普通文件，调用send_respond发送http应答协议头，并调用get_file_type判断文件类型，调用

send_file发送文件内容；
	（5）如果是目录文件，调用send_respond发送http应答协议头，并调用get_file_type判断文件类型，调用

send_dir发送文件内容；

7、send_respond函数
	功能：
	（1）发送头响应给浏览器，包括状态行、消息报头、空行

8、get_file_type函数
	功能：
	（1）判断文件类型，确定文件的打开方式

9、send_file函数
	功能：
	（1）打开服务器本地文件，将服务器本地文件发送给浏览器；打开失败，发送404页面给浏览器。

10、send_dir函数
	功能：
	（1）打开目录文件，显示当前目录下的文件（目录或者文件）；打开失败，发送404页面给浏览器。

11、send_error函数
	功能：
	（1）发送错误页面。

### 4 函数关系

<img src="epoll_http_server.assets/image-20220316103511418.png" alt="image-20220316103511418" style="zoom: 50%;" />

本图只是方便展示流程，并不标准。

### 5 测试

编译

```
gcc tool.c server.c main.c -o server
```

运行

```
./server 9527 /home/winter/networkProject/epoll_http_server/dir/
```

<img src="epoll_http_server.assets/image-20220315152135920.png" alt="image-20220315152135920" style="zoom: 50%;" />

提前在/home/winter/networkProject/epoll_http_server/dir/路径下存储一写文件

case 1:打开1.2.3文件

<img src="epoll_http_server.assets/image-20220315152246428.png" alt="image-20220315152246428" style="zoom:80%;" />

case 2:打开gif文件

<img src="epoll_http_server.assets/image-20220315152308512.png" alt="image-20220315152308512" style="zoom:50%;" />

case 3:打开mp3文件

<img src="epoll_http_server.assets/image-20220315152352888.png" alt="image-20220315152352888" style="zoom:50%;" />


### 6 总结
（1）、创建监听红黑树，创建监听文件描述符lfd，将监听文件描述符挂到红黑树上；

（2）、使用红黑树监听文件描述符，如果监听文件描述符lfd有读事件，则处理连接事件，将得到的通信文件描述符cfd挂到红黑树上进行监听，如果通信文件描述符cfd有读事件，则处理通信事件；

（3）、有通信事件表示浏览器有http请求，这里使用的是get协议，解析get协议，得到浏览器请求的内容，判断是文件请求还是目录请求；

（4）、如果是文件请求，则服务器读本地文件，并按照http响应格式发送响应头、请求内容给浏览器；

（5）、如果是目录请求，则浏览器需要遍历目录，并按照http响应格式发送响应头、文件目录给浏览器；

（6）、（4）（5）中还需要判断文件/目录是否存在，如果不存在，则需要发送404页面；

（7）、因为文件种类较多，需要针对不同的文件类型进行判断解析；

（8）、因为http协议每行是以/r/n结尾，所有（3）中解析http协议时需要特别判断；

（9）、需要注意中文乱码的问题，编写编码和解码函数；



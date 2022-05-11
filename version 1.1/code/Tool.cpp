/*************************************************************************
 > File Name: Tool.cpp
 > Author: Winter
 > Created Time: 2022年05月07日 星期六 16时06分30秒
 ************************************************************************/

#include "Tool.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>


// 16进制转10进制
int Tool::hexit(char c) {
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

// 编码函数
/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void Tool::encode_str(char* to, int tosize, const char* from){
	// int tolen;
	for (int tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
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


// 解码函数
void Tool::decode_str(char* to, char* from) {
	for ( ; *from != '\0'; ++to, ++from) {     
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {       
			*to = hexit(from[1])*16 + hexit(from[2]);
			from += 2;                      
		} else {
			*to = *from;
		}
	}
	*to = '\0';
}
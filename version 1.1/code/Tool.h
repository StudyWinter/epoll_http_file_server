/*************************************************************************
 > File Name: Tool.h
 > Author: Winter
 > Created Time: 2022年05月07日 星期六 16时02分41秒
 ************************************************************************/

#ifndef _TOOL_H_
#define _TOOL_H_

#include <iostream>

class Tool {
public:
	int hexit(char c);                                              // 16进制转成10进制
	void encode_str(char* to, int tosize, const char* from);        // 编码函数
	void decode_str(char* to, char* from);                          // 解码函数                  
};

#endif
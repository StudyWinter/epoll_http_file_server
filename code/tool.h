/*************************************************************************
 > File Name: tool.h
 > Author: Winter
 > Created Time: 2022年03月12日 星期六 16时37分39秒
 ************************************************************************/

#ifndef _TOOL_H
#define _TOOL_H

int hexit(char c);                                             // 16进制转换成10进制
void encode_str(char* to, int tosize, const char* from);       // 编码函数
void decode_str(char* to, char* from);                         // 解码函数

#endif
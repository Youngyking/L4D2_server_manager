#pragma once
#ifndef _ENCODING_CONVERT_H
#define _ENCODING_CONVERT_H

#include <Windows.h>
#include "encoding_convert.h"

// 将一段数据视为UTF-16编码，将其转化为系统默认编码（多为GBK）并返回
char* U16toGBK(LPCWSTR wstr, char* buf, int buf_len);

WCHAR* GBKtoU16(const char* str, WCHAR* buf, int buf_len);

// 将一段数据视为UTF-16编码，将其转化为UTF-8编码并返回
static char* U16toU8(LPCWSTR wstr, char* buf, int buf_len);


WCHAR* U8toU16(const char* str, WCHAR* buf, int buf_len);

// 将GBK编码转换为UTF-8编码
char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len);

// 将UTF-8编码转换为GBK编码
char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len);

#endif
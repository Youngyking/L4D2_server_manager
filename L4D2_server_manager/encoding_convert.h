#pragma once
#ifndef _ENCODING_CONVERT_H
#define _ENCODING_CONVERT_H

#include <Windows.h>
#include "encoding_convert.h"

// ��һ��������ΪUTF-16���룬����ת��ΪϵͳĬ�ϱ��루��ΪGBK��������
char* U16toGBK(LPCWSTR wstr, char* buf, int buf_len);

WCHAR* GBKtoU16(const char* str, WCHAR* buf, int buf_len);

// ��һ��������ΪUTF-16���룬����ת��ΪUTF-8���벢����
static char* U16toU8(LPCWSTR wstr, char* buf, int buf_len);


WCHAR* U8toU16(const char* str, WCHAR* buf, int buf_len);

// ��GBK����ת��ΪUTF-8����
char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len);

// ��UTF-8����ת��ΪGBK����
char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len);

#endif
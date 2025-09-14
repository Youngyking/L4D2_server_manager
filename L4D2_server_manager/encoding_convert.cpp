#include <Windows.h>
#include "encoding_convert.h"

// 将一段数据视为UTF-16编码，将其转化为系统默认编码（多为GBK）并返回
char* U16toGBK(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// 将一段数据视为系统默认编码（多为GBK），将其转化为UTF-16编码并返回
WCHAR* GBKtoU16(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
    return buf;
}

// 将一段数据视为UTF-16编码，将其转化为UTF-8编码并返回
char* U16toU8(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// 将一段数据视为UTF-8编码，将其转化为UTF-16编码并返回
WCHAR* U8toU16(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, buf_len);
    return buf;
}

// 将GBK编码转换为UTF-8编码
char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len) {
    if (!gbk_str || !u8_buf || buf_len <= 0) {
        return nullptr; // 无效参数检查
    }

    // 步骤1: 先将GBK转换为UTF-16（中间过渡编码）
    int u16_len = MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // GBK编码无效
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // 转换UTF-16失败
    }

    // 步骤2: 将UTF-16转换为UTF-8
    int u8_len = WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (u8_len == 0 || u8_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // 缓冲区不足或转换失败
    }

    WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, u8_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return u8_buf;
}

// 将UTF-8编码转换为GBK编码
char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len) {
    if (!u8_str || !gbk_buf || buf_len <= 0) {
        return nullptr; // 无效参数检查
    }

    // 步骤1: 先将UTF-8转换为UTF-16（中间过渡编码）
    int u16_len = MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // UTF-8编码无效
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // 转换UTF-16失败
    }

    // 步骤2: 将UTF-16转换为GBK
    int gbk_len = WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (gbk_len == 0 || gbk_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // 缓冲区不足或转换失败
    }

    WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, gbk_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return gbk_buf;
}
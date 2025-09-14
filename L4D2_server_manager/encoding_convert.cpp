#include <Windows.h>
#include "encoding_convert.h"

// ��һ��������ΪUTF-16���룬����ת��ΪϵͳĬ�ϱ��루��ΪGBK��������
char* U16toGBK(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// ��һ��������ΪϵͳĬ�ϱ��루��ΪGBK��������ת��ΪUTF-16���벢����
WCHAR* GBKtoU16(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
    return buf;
}

// ��һ��������ΪUTF-16���룬����ת��ΪUTF-8���벢����
char* U16toU8(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// ��һ��������ΪUTF-8���룬����ת��ΪUTF-16���벢����
WCHAR* U8toU16(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, buf_len);
    return buf;
}

// ��GBK����ת��ΪUTF-8����
char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len) {
    if (!gbk_str || !u8_buf || buf_len <= 0) {
        return nullptr; // ��Ч�������
    }

    // ����1: �Ƚ�GBKת��ΪUTF-16���м���ɱ��룩
    int u16_len = MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // GBK������Ч
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // ת��UTF-16ʧ��
    }

    // ����2: ��UTF-16ת��ΪUTF-8
    int u8_len = WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (u8_len == 0 || u8_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // �����������ת��ʧ��
    }

    WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, u8_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return u8_buf;
}

// ��UTF-8����ת��ΪGBK����
char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len) {
    if (!u8_str || !gbk_buf || buf_len <= 0) {
        return nullptr; // ��Ч�������
    }

    // ����1: �Ƚ�UTF-8ת��ΪUTF-16���м���ɱ��룩
    int u16_len = MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // UTF-8������Ч
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // ת��UTF-16ʧ��
    }

    // ����2: ��UTF-16ת��ΪGBK
    int gbk_len = WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (gbk_len == 0 || gbk_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // �����������ת��ʧ��
    }

    WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, gbk_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return gbk_buf;
}
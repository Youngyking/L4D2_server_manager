#include "config.h"
#include <string>
#include <windows.h>
#include <iostream>

// Ĭ��Զ����Ŀ��·��
const std::string DEFAULT_REMOTE_ROOT = "/home/L4D2_Manager";

// ��ȡ�����ļ�������·������Ŀ��Ŀ¼�µ�config.ini��
std::wstring GetConfigFilePath() {
    WCHAR exePath[MAX_PATH] = { 0 };
    WCHAR rootPath[MAX_PATH] = { 0 };

    // ��ȡ��ǰ��ִ���ļ�·��
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        std::cerr << "��ȡ����·��ʧ�ܣ��������: " << GetLastError() << std::endl;
        return L"config.ini"; // ʧ��ʱʹ��Ĭ��ֵ
    }

    // ��ȡĿ¼·������Ŀ��Ŀ¼��
    WCHAR* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash != NULL) {
        *lastSlash = L'\0';
        wcscpy_s(rootPath, MAX_PATH, exePath);
        wcscat_s(rootPath, MAX_PATH, L"\\config.ini");
        return std::wstring(rootPath);
    }

    return L"config.ini";
}

// ��ȡԶ����Ŀ��·������
std::string GetRemoteRootPath() {
    std::wstring configPath = GetConfigFilePath();
    WCHAR pathW[256] = { 0 };

    // ��INI�ļ���ȡ���ã�����������ʹ��Ĭ��ֵ
    DWORD result = GetPrivateProfileStringW(
        L"RemoteConfig",
        L"RootPath",
        std::wstring(DEFAULT_REMOTE_ROOT.begin(), DEFAULT_REMOTE_ROOT.end()).c_str(),
        pathW,
        sizeof(pathW) / sizeof(WCHAR),
        configPath.c_str()
    );

    if (result == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) { // �ļ����������������������Ҫ����
            std::cerr << "��ȡ����ʧ�ܣ��������: " << error << std::endl;
        }
    }

    // ת��ΪANSI�ַ���
    char pathA[256] = { 0 };
    WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, sizeof(pathA), NULL, NULL);
    return std::string(pathA);
}

// ����Զ����Ŀ��·������
void SaveRemoteRootPath(const std::string& path) {
    std::wstring configPath = GetConfigFilePath();

    // ת��Ϊ���ַ�
    WCHAR pathW[256] = { 0 };
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, pathW, sizeof(pathW) / sizeof(WCHAR));

    // д��INI�ļ�
    BOOL result = WritePrivateProfileStringW(
        L"RemoteConfig",
        L"RootPath",
        pathW,
        configPath.c_str()
    );

    if (!result) {
        std::cerr << "��������ʧ�ܣ��������: " << GetLastError() << std::endl;
    }
}

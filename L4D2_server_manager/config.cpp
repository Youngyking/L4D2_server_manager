#include "config.h"
#include <string>
#include <windows.h>
#include <iostream>

// 默认远程项目根路径
const std::string DEFAULT_REMOTE_ROOT = "/home/L4D2_Manager";

// 获取配置文件的完整路径（项目根目录下的config.ini）
std::wstring GetConfigFilePath() {
    WCHAR exePath[MAX_PATH] = { 0 };
    WCHAR rootPath[MAX_PATH] = { 0 };

    // 获取当前可执行文件路径
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        std::cerr << "获取程序路径失败，错误代码: " << GetLastError() << std::endl;
        return L"config.ini"; // 失败时使用默认值
    }

    // 提取目录路径（项目根目录）
    WCHAR* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash != NULL) {
        *lastSlash = L'\0';
        wcscpy_s(rootPath, MAX_PATH, exePath);
        wcscat_s(rootPath, MAX_PATH, L"\\config.ini");
        return std::wstring(rootPath);
    }

    return L"config.ini";
}

// 读取远程项目根路径配置
std::string GetRemoteRootPath() {
    std::wstring configPath = GetConfigFilePath();
    WCHAR pathW[256] = { 0 };

    // 从INI文件读取配置，若不存在则使用默认值
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
        if (error != ERROR_FILE_NOT_FOUND) { // 文件不存在是正常情况，不需要报错
            std::cerr << "读取配置失败，错误代码: " << error << std::endl;
        }
    }

    // 转换为ANSI字符串
    char pathA[256] = { 0 };
    WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, sizeof(pathA), NULL, NULL);
    return std::string(pathA);
}

// 保存远程项目根路径配置
void SaveRemoteRootPath(const std::string& path) {
    std::wstring configPath = GetConfigFilePath();

    // 转换为宽字符
    WCHAR pathW[256] = { 0 };
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, pathW, sizeof(pathW) / sizeof(WCHAR));

    // 写入INI文件
    BOOL result = WritePrivateProfileStringW(
        L"RemoteConfig",
        L"RootPath",
        pathW,
        configPath.c_str()
    );

    if (!result) {
        std::cerr << "保存配置失败，错误代码: " << GetLastError() << std::endl;
    }
}

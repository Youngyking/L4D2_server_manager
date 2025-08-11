/*
基于ssh.cpp提供的ssh与sftp接口封装窗口控件的功能
*/


#define WIN32_LEAN_AND_MEAN  // 禁用 Windows 旧版头文件中的冗余定义
#define _CRT_SECURE_NO_WARNINGS //允许使用fopen

#include <winsock2.h>
#include <ws2tcpip.h>
#include <commdlg.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comdlg32.lib")
#include "manager.h"
#include "cJSON.h"
#include "gui.h"
#include "resource.h"
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <cmath>
#include <map>

// 全局端口-实例名称映射表（动态更新）
std::map<std::string, std::string> g_portToInstance;

// 将一段数据视为UTF-16编码，将其转化为GBK编码并返回
static char* WCharToChar(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// 将一段数据视为GBK编码，将其转化为UTF-16编码并返回
static WCHAR* CharToWChar(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
    return buf;
}

// 将一段数据视为UTF-8编码，将其转化为UTF-16编码并返回
static WCHAR* CharToWChar_ser(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, buf_len);
    return buf;
}

// 处理SSH连接请求（线程函数）
DWORD WINAPI HandleConnectRequest(LPVOID param) {
    HWND hWnd = (HWND)param;
    WCHAR ip_w[64], user_w[64], pass_w[64];
    char ip[64], user[64], pass[64], err_msg[256];

    // 获取输入框内容
    GetWindowTextW(GetDlgItem(hWnd, IDC_IP_EDIT), ip_w, sizeof(ip_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_USER_EDIT), user_w, sizeof(user_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_PASS_EDIT), pass_w, sizeof(pass_w) / sizeof(WCHAR));

    // 转换为多字节
    WCharToChar(ip_w, ip, sizeof(ip));
    WCharToChar(user_w, user, sizeof(user));
    WCharToChar(pass_w, pass, sizeof(pass));

    // 初始化SSH
    if (g_ssh_ctx) {
        l4d2_ssh_cleanup(g_ssh_ctx);
    }
    g_ssh_ctx = l4d2_ssh_init();
    if (!g_ssh_ctx) {
        AddLog(hWnd, L"初始化SSH失败");
        UpdateConnectionStatus(hWnd, L"连接失败", FALSE);
        return 0;
    }

    // 连接服务器
    AddLog(hWnd, L"正在连接到服务器...");
    bool success = l4d2_ssh_connect(g_ssh_ctx, ip, user, pass, err_msg, sizeof(err_msg));
    WCHAR err_msg_w[256];
    CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
    AddLog(hWnd, err_msg_w);

    if (success) {
        // 连接成功后上传API脚本
        AddLog(hWnd, L"正在检查并上传API脚本...");
        if (l4d2_ssh_upload_api_script(g_ssh_ctx, err_msg, sizeof(err_msg))) {
            CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
            AddLog(hWnd, err_msg_w);

            // 新增：执行依赖检查与安装
            AddLog(hWnd, L"正在检查并安装脚本依赖...");
            char dep_output[4096] = { 0 };
            WCHAR output_w[4096];
            bool dep_success = l4d2_ssh_exec_command(
                g_ssh_ctx,
                "bash /home/L4D2_Manager/L4D2_Manager_API.sh check_deps",
                dep_output, sizeof(dep_output),
                err_msg, sizeof(err_msg)
            );
            CharToWChar_ser(dep_output, output_w, sizeof(output_w) / sizeof(WCHAR));
            AddLog(hWnd, output_w);

            if (dep_success) {
                UpdateConnectionStatus(hWnd, L"已连接，且依赖安装成功", TRUE);
                HandleGetStatus(hWnd);  // 刷新状态
                HandleGetInstances(hWnd);
            }
            else {
                CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
                AddLog(hWnd, err_msg_w);
                UpdateConnectionStatus(hWnd, L"已连接，但依赖安装失败", FALSE);
            }
        }
    }
    else {
        UpdateConnectionStatus(hWnd, L"连接失败", FALSE);
    }

    return 0;
}

// 启动实例处理函数
DWORD WINAPI HandleStartInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        WCHAR logMsg[256];
        CharToWChar("未连接到服务器，无法启动实例", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // 获取用户输入的端口号
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WCharToChar(port_w, port, sizeof(port));

    // 从全局映射表中查找实例名称
    std::string instanceName;
    if (g_portToInstance.find(port) != g_portToInstance.end()) {
        instanceName = g_portToInstance[port];
    }
    else {
        WCHAR logMsg[256];
        CharToWChar("未找到对应端口的实例配置", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // 根据端口查找实例名称
    if (g_portToInstance.find(port) != g_portToInstance.end()) {
        instanceName = g_portToInstance[port];
    }
    else {
        // 未找到对应实例
        WCHAR logMsg[256];
        CharToWChar("未找到对应端口的实例配置", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // 输出启动日志（包含实例名称和端口）
    WCHAR logMsg[256];
    WCHAR instanceNameW[128];

    CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"正在启动实例: %ls（端口: %s）...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // 构造SSH命令（使用start_instance动作和实例名称，修正原命令错误）
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh start_instance %s", instanceName.c_str());

    // 执行命令并处理结果
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // 转换输出日志并显示
    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        WCHAR successMsg[64];
        CharToWChar("实例启动成功", successMsg, sizeof(successMsg) / sizeof(WCHAR));
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // 刷新实例列表
    }
    else {
        WCHAR failPrefix[64];
        CharToWChar("启动失败: ", failPrefix, sizeof(failPrefix) / sizeof(WCHAR));
        AddLog(hWnd, failPrefix);
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// 停止实例处理函数（使用全局映射表获取实例名称）
DWORD WINAPI HandleStopInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        WCHAR logMsg[256];
        CharToWChar("未连接到服务器，无法停止实例", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // 获取端口号
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WCharToChar(port_w, port, sizeof(port));

    // 从全局映射表查找实例名称
    std::string instanceName;
    auto it = g_portToInstance.find(port);
    if (it != g_portToInstance.end()) {
        instanceName = it->second;
    }
    else {
        WCHAR logMsg[256];

        swprintf_s(logMsg, L"未找到端口 %s 对应的实例配置", port_w);
        AddLog(hWnd, logMsg);
        return 0;
    }

    // 输出停止日志
    WCHAR logMsg[256];
    WCHAR instanceNameW[128];

    CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"正在停止实例: %ls（端口: %s）...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // 构造SSH命令
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh stop_instance %s", instanceName.c_str());

    // 执行命令并处理结果
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // 转换输出日志并显示
    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        Sleep(1000);
        AddLog(hWnd, L"请耐心等待，关闭服务器大致需要半分钟");
        Sleep(15000); //非常重要，实例的关闭需要时间，否则get_status返回的就还是在运行
        WCHAR successMsg[256];
        WCHAR instanceNameW[128];

        CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
        swprintf_s(successMsg, L"实例 %ls（端口: %s）停止成功", instanceNameW, port_w);
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // 刷新实例列表
    }
    else {
        WCHAR failMsg[256];
        WCHAR instanceNameW[128];

        CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
        swprintf_s(failMsg, L"实例 %ls（端口: %s）停止失败: %s",
            instanceNameW, port_w, err_msg_w);
        AddLog(hWnd, failMsg);
    }

    return 0;
}

// 处理部署服务器请求（线程函数）
DWORD WINAPI HandleDeployServer(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"未连接到服务器，无法部署");
        return 0;
    }

    AddLog(hWnd, L"开始部署/更新服务器(已部署则检查更新，若可更新则需要耗费一定时间，取决于你服务器带宽)...");
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh deploy_server",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"服务器部署/更新完成");
        HandleGetStatus(hWnd);  // 刷新状态
    }
    else {
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// 处理获取服务器状态请求（线程函数）
DWORD WINAPI HandleGetStatus(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) return 0;

    char output[1024] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh get_status",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (success) {
        cJSON* root = cJSON_Parse(output);
        if (root) {
            cJSON* serverDeployed = cJSON_GetObjectItem(root, "serverDeployed");
            LPCWSTR serverStatus = (serverDeployed && serverDeployed->type == cJSON_True) ? L"已部署" : L"未部署";

            cJSON* smInstalled = cJSON_GetObjectItem(root, "smInstalled");
            LPCWSTR smStatus = (smInstalled && smInstalled->type == cJSON_True) ? L"已安装" : L"未安装";

            cJSON* mmInstalled = cJSON_GetObjectItem(root, "mmInstalled");
            LPCWSTR mmStatus = (mmInstalled && mmInstalled->type == cJSON_True) ? L"已安装" : L"未安装";

            UpdateSystemStatus(hWnd, serverStatus, smStatus, mmStatus);

            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"解析系统状态JSON失败");
        }
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// 处理获取实例列表请求（线程函数）
DWORD WINAPI HandleGetInstances(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) return 0;

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh get_instances",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (success) {
        // 解析JSON并更新全局映射表
        cJSON* root = cJSON_Parse(output);
        if (root) {
            // 清空旧映射（避免残留无效数据）
            g_portToInstance.clear();

            // 遍历JSON中所有实例（键为实例名称，值为实例详情）
            cJSON* instance = NULL;
            cJSON_ArrayForEach(instance, root) {
                if (instance->type == cJSON_Object) {
                    // 获取实例名称（JSON键）
                    std::string instanceName = instance->string;
                    // 获取端口号（实例详情中的port字段）
                    cJSON* portItem = cJSON_GetObjectItem(instance, "port");
                    if (portItem && portItem->type == cJSON_String) {
                        std::string port = portItem->valuestring;
                        // 存入映射表（端口 -> 实例名称）
                        g_portToInstance[port] = instanceName;
                    }
                }
            }
            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"解析实例列表JSON失败");
        }

        // 刷新UI列表
        UpdateInstanceList(hWnd, output);
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// 上传SourceMod安装包
DWORD WINAPI HandleUploadSourceMod(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"未连接到服务器，无法上传SourceMod");
        return 0;
    }

    // 远程目录
    const char* remote_dir = "/home/L4D2_Manager/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // 检查并创建远程目录
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"远程目录不存在，尝试创建...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            AddLog(hWnd, err_w);
            return 0;
        }
    }

    // 打开文件选择对话框
    WCHAR filter[] = L"所有文件 (*.*)\0*.*\0tar.gz压缩包 (*.tar.gz)\0*.tar.gz\0\0";
    WCHAR fileName[256] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = sizeof(fileName) / sizeof(WCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // 设置默认路径为当前目录的resource文件夹
    WCHAR currentDir[256];
    GetCurrentDirectoryW(sizeof(currentDir) / sizeof(WCHAR), currentDir);
    WCHAR defaultPath[256];
    swprintf_s(defaultPath, L"%s\\resource", currentDir);
    ofn.lpstrInitialDir = defaultPath;

    if (!GetOpenFileNameW(&ofn)) {
        AddLog(hWnd, L"取消选择文件");
        return 0;
    }

    // 转换路径为多字节
    char local_path[256];
    WCharToChar(fileName, local_path, sizeof(local_path));

    // 获取文件名
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // 构建远程路径
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir, file_name);

    // 上传文件
    AddLog(hWnd, L"开始上传SourceMod安装包...");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        CharToWChar(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"上传成功");
    }
    else {
        WCHAR err_w[256];
        CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        AddLog(hWnd, err_w);
    }

    return 0;
}

// 上传MetaMod安装包
DWORD WINAPI HandleUploadMetaMod(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"未连接到服务器，无法上传MetaMod");
        return 0;
    }

    // 远程目录
    const char* remote_dir = "/home/L4D2_Manager/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // 检查并创建远程目录
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"远程目录不存在或ssh连接失败，尝试连接并创建...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            AddLog(hWnd, err_w);
            return 0;
        }
    }

    // 打开文件选择对话框
    WCHAR filter[] = L"所有文件 (*.*)\0*.*\0tar.gz压缩包 (*.tar.gz)\0*.tar.gz\0\0";
    WCHAR fileName[256] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = sizeof(fileName) / sizeof(WCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // 设置默认路径为当前目录的resource文件夹
    WCHAR currentDir[256];
    GetCurrentDirectoryW(sizeof(currentDir) / sizeof(WCHAR), currentDir);
    WCHAR defaultPath[256];
    swprintf_s(defaultPath, L"%s\\resource", currentDir);
    ofn.lpstrInitialDir = defaultPath;

    if (!GetOpenFileNameW(&ofn)) {
        AddLog(hWnd, L"取消选择文件");
        return 0;
    }

    // 转换路径为多字节
    char local_path[256];
    WCharToChar(fileName, local_path, sizeof(local_path));

    // 获取文件名
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // 构建远程路径
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir, file_name);

    // 上传文件
    AddLog(hWnd, L"开始上传MetaMod安装包...");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        CharToWChar(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"上传成功");
    }
    else {
        WCHAR err_w[256];
        CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        AddLog(hWnd, err_w);
    }

    return 0;
}

// 安装SourceMod与MetaMod
DWORD WINAPI HandleInstallSourceMetaMod(LPVOID param) {
    HWND hWnd = (HWND)param;

    // 检查SSH连接状态
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"未连接到服务器，无法安装SourceMod");
        return 0;
    }

    // 记录开始安装的日志
    AddLog(hWnd, L"开始执行SourceMod安装脚本...");

    // 定义命令、输出缓冲区和错误信息缓冲区
    const char* installCmd = "/home/L4D2_Manager/L4D2_Manager_API.sh install_sourcemod";
    char output[4096] = { 0 };  // 足够大的缓冲区存储命令输出
    char err_msg[256] = { 0 };  // 存储错误信息

    // 执行SSH命令
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        installCmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // 将命令输出从UTF-8转换为宽字符并打印到日志
    WCHAR output_w[4096];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    AddLog(hWnd, output_w);

    // 如果命令执行失败，额外打印错误信息
    if (!success) {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, L"安装过程中出现错误:");
        AddLog(hWnd, err_msg_w);
    }
    else {
        // 安装成功后刷新状态
        AddLog(hWnd, L"SourceMod安装命令执行完成");
        HandleGetStatus(hWnd);  // 刷新系统状态显示
    }

    return 0;
}

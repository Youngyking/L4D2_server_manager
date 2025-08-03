/*
基于ssh.cpp提供的ssh与sftp接口封装窗口控件的功能
*/

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include "manager.h"
#include "cJSON.h"
#include "gui.h"
#include "resource.h"
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <string.h>
#include <string>

// 转换宽字符串到多字节
static char* WCharToChar(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// 转换多字节到宽字符串
static WCHAR* CharToWChar(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
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
            CharToWChar(dep_output, output_w, sizeof(output_w) / sizeof(WCHAR));
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
        AddLog(hWnd, L"未连接到服务器，无法启动实例");
        return 0;
    }

    // 获取端口号
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WideCharToMultiByte(CP_ACP, 0, port_w, -1, port, sizeof(port), NULL, NULL);

    AddLog(hWnd, L"正在启动端口为 ");
    AddLog(hWnd, port_w);
    AddLog(hWnd, L" 的实例...");

    // 构造SSH命令
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh start %s", port);

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // 转换编码并输出日志
    WCHAR output_w[4096], err_msg_w[256];
    MultiByteToWideChar(CP_ACP, 0, output, -1, output_w, sizeof(output_w) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, err_msg, -1, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"实例启动成功");
        HandleGetInstances(hWnd);  // 刷新实例列表
    }
    else {
        AddLog(hWnd, L"启动失败: ");
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// 停止实例处理函数
DWORD WINAPI HandleStopInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"未连接到服务器，无法停止实例");
        return 0;
    }

    // 获取端口号
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WideCharToMultiByte(CP_ACP, 0, port_w, -1, port, sizeof(port), NULL, NULL);

    AddLog(hWnd, L"正在停止端口为 ");
    AddLog(hWnd, port_w);
    AddLog(hWnd, L" 的实例...");

    // 构造SSH命令
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh stop %s", port);

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // 转换编码并输出日志
    WCHAR output_w[4096], err_msg_w[256];
    MultiByteToWideChar(CP_ACP, 0, output, -1, output_w, sizeof(output_w) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, err_msg, -1, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"实例停止成功");
        HandleGetInstances(hWnd);  // 刷新实例列表
    }
    else {
        AddLog(hWnd, L"停止失败: ");
        AddLog(hWnd, err_msg_w);
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

    AddLog(hWnd, L"开始部署/更新服务器...");
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh deploy_server",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"服务器部署完成");
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

            cJSON* instanceCount = cJSON_GetObjectItem(root, "instanceCount");
            int instCount = instanceCount ? instanceCount->valueint : 0;
            WCHAR instCountStr[32];
            swprintf_s(instCountStr, L"%d", instCount);

            cJSON* pluginCount = cJSON_GetObjectItem(root, "pluginCount");
            int plugCount = pluginCount ? pluginCount->valueint : 0;
            WCHAR plugCountStr[32];
            swprintf_s(plugCountStr, L"%d", plugCount);

            UpdateSystemStatus(hWnd, serverStatus, smStatus, instCountStr, plugCountStr);

            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"解析系统状态JSON失败");
        }
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
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
        UpdateInstanceList(hWnd, output);
    }

    return 0;
}
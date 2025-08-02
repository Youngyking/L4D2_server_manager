// 在所有头文件包含前添加（建议放在stdafx.h 或 pch.h 中）
#define WIN32_LEAN_AND_MEAN  // 禁用 Windows 旧版头文件中的冗余定义
#include <winsock2.h>        // 只包含新版 Winsock 2.0
#include <ws2tcpip.h>        // 如需使用更现代的网络函数（如 getaddrinfo），可包含此文件
#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 2.0 库
#include "framework.h"
#include "L4D2_server_manager.h"
#include "resource.h"
#include "gui.h"
#include <fcntl.h>
#include <string.h>

// 初始化公共控件（ListView等需要）
void InitCommonControlsExWrapper() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;  // 初始化ListView
    InitCommonControlsEx(&icex);
}

// 创建所有窗口控件
void CreateAllControls(HWND hWnd, HINSTANCE hInst) {
    // 1. SSH连接栏
    CreateWindowW(L"BUTTON", L"SSH连接设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 980, 80, hWnd, (HMENU)IDC_SSH_GROUP, hInst, NULL);

    // 1.1 IP输入框
    CreateWindowW(L"STATIC", L"服务器IP:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"192.168.1.1",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        120, 30, 200, 25, hWnd, (HMENU)IDC_IP_EDIT, hInst, NULL);

    // 1.2 用户名输入框
    CreateWindowW(L"STATIC", L"用户名:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        350, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"steam",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        440, 30, 150, 25, hWnd, (HMENU)IDC_USER_EDIT, hInst, NULL);

    // 1.3 密码输入框
    CreateWindowW(L"STATIC", L"密码:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        620, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
        700, 30, 150, 25, hWnd, (HMENU)IDC_PASS_EDIT, hInst, NULL);

    // 1.4 连接按钮
    CreateWindowW(L"BUTTON", L"连接服务器",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        880, 30, 100, 25, hWnd, (HMENU)IDC_CONNECT_BTN, hInst, NULL);

    // 1.5 连接状态
    CreateWindowW(L"STATIC", L"未连接",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 60, 200, 20, hWnd, (HMENU)IDC_CONN_STATUS, hInst, NULL);

    // 2. 系统状态卡片
    CreateWindowW(L"BUTTON", L"系统状态 & 核心操作",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 100, 480, 220, hWnd, (HMENU)IDC_STATUS_GROUP, hInst, NULL);

    // 2.1 状态指标（4个网格）
    // 服务器文件状态
    CreateWindowW(L"STATIC", L"服务器文件:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 130, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"未部署",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 130, 100, 20, hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);

    // SourceMod状态
    CreateWindowW(L"STATIC", L"SourceMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 170, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"未安装",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 170, 100, 20, hWnd, (HMENU)IDC_SM_STATUS, hInst, NULL);

    // 运行中实例数
    CreateWindowW(L"STATIC", L"运行中实例:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 210, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 210, 100, 20, hWnd, (HMENU)IDC_INSTANCE_COUNT, hInst, NULL);

    // 已安装插件数
    CreateWindowW(L"STATIC", L"已安装插件:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 250, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 250, 100, 20, hWnd, (HMENU)IDC_PLUGIN_COUNT, hInst, NULL);

    // 2.2 部署按钮
    CreateWindowW(L"BUTTON", L"部署/更新服务器",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 280, 440, 30, hWnd, (HMENU)IDC_DEPLOY_BTN, hInst, NULL);

    // 3. 服务器实例卡片
    CreateWindowW(L"BUTTON", L"服务器实例",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 100, 490, 220, hWnd, (HMENU)IDC_INSTANCE_GROUP, hInst, NULL);

    // 3.1 实例列表（ListView）
    HWND hInstanceList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        520, 130, 450, 160, hWnd, (HMENU)IDC_INSTANCE_LIST, hInst, NULL);
    // 添加列表列
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    // 第0列
    lvc.cx = 60;
    lvc.pszText = const_cast<LPWSTR>(L"状态");  // 强制转换常量为可修改指针
    ListView_InsertColumn(hInstanceList, 0, &lvc);

    // 第1列
    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"名称");
    ListView_InsertColumn(hInstanceList, 1, &lvc);

    // 第2列
    lvc.cx = 80;
    lvc.pszText = const_cast<LPWSTR>(L"端口");
    ListView_InsertColumn(hInstanceList, 2, &lvc);

    // 第3列
    lvc.cx = 120;
    lvc.pszText = const_cast<LPWSTR>(L"地图");
    ListView_InsertColumn(hInstanceList, 3, &lvc);

    // 第4列
    lvc.cx = 80;
    lvc.pszText = const_cast<LPWSTR>(L"操作");
    ListView_InsertColumn(hInstanceList, 4, &lvc);

    // 3.2 端口输入相关控件
    CreateWindowW(L"STATIC", L"端口号:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        520, 300, 60, 20, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"EDIT", L"27015",  // 默认L4D2端口
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        590, 300, 80, 25, hWnd, (HMENU)IDC_PORT_EDIT, hInst, NULL);

    CreateWindowW(L"BUTTON", L"启动实例",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        690, 300, 100, 25, hWnd, (HMENU)IDC_START_INSTANCE, hInst, NULL);

    CreateWindowW(L"BUTTON", L"停止实例",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        810, 300, 100, 25, hWnd, (HMENU)IDC_STOP_INSTANCE, hInst, NULL);

    // 4. 操作入口卡片
    CreateWindowW(L"BUTTON", L"插件管理 & 日志",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 330, 980, 200, hWnd, (HMENU)IDC_ACTION_GROUP, hInst, NULL);

    // 4.1 插件管理按钮
    CreateWindowW(L"BUTTON", L"前往插件管理",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 360, 200, 30, hWnd, (HMENU)IDC_PLUGIN_BTN, hInst, NULL);

    // 4.2 日志查看按钮
    CreateWindowW(L"BUTTON", L"清空日志",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        250, 360, 200, 30, hWnd, (HMENU)IDC_LOG_BTN, hInst, NULL);

    // 4.3 日志显示框（多行编辑框）
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        30, 400, 940, 100, hWnd, (HMENU)IDC_LOG_VIEW, hInst, NULL);
}

// 更新连接状态
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected) {
    HWND hStatus = GetDlgItem(hWnd, IDC_CONN_STATUS);
    SetWindowTextW(hStatus, status);
    // 连接成功时启用其他按钮
    EnableWindow(GetDlgItem(hWnd, IDC_DEPLOY_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_PLUGIN_BTN), isConnected);
}

// 更新系统状态卡片
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR instanceCount, LPCWSTR pluginCount) {
    // 更新服务器文件状态
    SetWindowTextW(GetDlgItem(hWnd, IDC_SERVER_STATUS), serverStatus);
    // 更新SourceMod状态
    SetWindowTextW(GetDlgItem(hWnd, IDC_SM_STATUS), smStatus);
    // 新增：更新实例数量
    SetWindowTextW(GetDlgItem(hWnd, IDC_INSTANCE_COUNT), instanceCount);
    // 新增：更新插件数量
    SetWindowTextW(GetDlgItem(hWnd, IDC_PLUGIN_COUNT), pluginCount);
}

// 更新实例列表（简化示例，实际需解析JSON）
void UpdateInstanceList(HWND hWnd, const char* instancesJson) {
    HWND hList = GetDlgItem(hWnd, IDC_INSTANCE_LIST);
    ListView_DeleteAllItems(hList);  // 清空现有项

    // 此处应解析JSON（建议使用cJSON等库），示例直接添加测试项
   // 定义一次LVITEMW变量，作用域覆盖所有测试项
    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;  // 统一设置mask，指定使用文本字段

    // 测试项1
    lvi.iItem = 0;  // 修改行索引
    WCHAR status0[20] = L"运行中";
    lvi.pszText = status0;  // 赋值状态文本
    ListView_InsertItem(hList, &lvi);

    // 设置其他列
    ListView_SetItemText(hList, 0, 1, const_cast<LPWSTR>(L"主服_战役"));
    ListView_SetItemText(hList, 0, 2, const_cast<LPWSTR>(L"27015"));
    ListView_SetItemText(hList, 0, 3, const_cast<LPWSTR>(L"c1m1_hotel"));
    ListView_SetItemText(hList, 0, 4, const_cast<LPWSTR>(L"停止"));


    // 测试项2（复用同一个lvi变量）
    lvi.iItem = 1;  // 仅修改行索引
    WCHAR status1[20] = L"已停止";
    lvi.pszText = status1;  // 赋值新的状态文本
    ListView_InsertItem(hList, &lvi);

    // 设置其他列
    ListView_SetItemText(hList, 1, 1, const_cast<LPWSTR>(L"副服_对抗"));
    ListView_SetItemText(hList, 1, 2, const_cast<LPWSTR>(L"27016"));
    ListView_SetItemText(hList, 1, 3, const_cast<LPWSTR>(L"c5m1_waterfront"));
    ListView_SetItemText(hList, 1, 4, const_cast<LPWSTR>(L"启动"));
}

// 添加日志到日志框
void AddLog(HWND hWnd, LPCWSTR logText) {
    HWND hLog = GetDlgItem(hWnd, IDC_LOG_VIEW);
    int len = GetWindowTextLengthW(hLog);
    SendMessageW(hLog, EM_SETSEL, len, len);  // 移动到末尾
    WCHAR timeStr[32] = { 0 };
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(timeStr, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)timeStr);  // 添加时间
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)logText);  // 添加日志内容
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");  // 换行
}

// 清空日志
void ClearLog(HWND hWnd) {
    SetWindowTextW(GetDlgItem(hWnd, IDC_LOG_VIEW), L"");
}
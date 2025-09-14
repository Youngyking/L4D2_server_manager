/*
    为主程序文件L4D2_server_manager.cpp提供GUI界面的实现，负责创建和管理界面元素，
    并为manager.cpp提供界面更新接口
*/


#define WIN32_LEAN_AND_MEAN  // 减少 Windows 头文件中的冗余内容
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 2.0 库
#include <fcntl.h>
#include <string.h>
#include <string>
#include "cJSON.h"
#include "framework.h"
#include "resource.h"
#include "gui.h"
#include "ssh.h"
#include "config.h"
#include "encoding_convert.h"

// 初始化通用控件（ListView需要）
void InitCommonControlsExWrapper() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;  // 初始化ListView
    InitCommonControlsEx(&icex);
}

// 创建所有界面控件
void CreateAllControls(HWND hWnd, HINSTANCE hInst) {
    // 1. SSH连接区
    CreateWindowW(L"BUTTON", L"SSH连接设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 980, 80, hWnd, (HMENU)IDC_SSH_GROUP, hInst, NULL);

    // 1.1 IP地址输入
    CreateWindowW(L"STATIC", L"服务器IP:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"xxx.xxx.xxx.xxx",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        120, 30, 200, 25, hWnd, (HMENU)IDC_IP_EDIT, hInst, NULL);

    // 1.2 用户名输入
    CreateWindowW(L"STATIC", L"用户名:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        350, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"root",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        440, 30, 150, 25, hWnd, (HMENU)IDC_USER_EDIT, hInst, NULL);

    // 1.3 密码输入
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

    // 2. 系统状态面板（调整高度以完全包含所有按钮）
    CreateWindowW(L"BUTTON", L"系统状态 & 核心操作",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 100, 480, 420, hWnd, (HMENU)IDC_STATUS_GROUP, hInst, NULL);

    // 2.1 状态显示（5条信息）
    // 服务器文件状态
    CreateWindowW(L"STATIC", L"服务器文件:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 130, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"未部署",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 130, 100, 30, hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);

    // SourceMod状态
    CreateWindowW(L"STATIC", L"SourceMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 200, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"未安装",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 200, 100, 30, hWnd, (HMENU)IDC_SM_STATUS, hInst, NULL);

    // MetaMod状态
    CreateWindowW(L"STATIC", L"MetaMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 270, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"未安装",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 270, 100, 30, hWnd, (HMENU)IDC_MM_STATUS, hInst, NULL);

    // 2.2 操作按钮
    CreateWindowW(L"BUTTON", L"部署/更新服务器",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 320, 440, 30, hWnd, (HMENU)IDC_DEPLOY_BTN, hInst, NULL);

    // 上传SourceMod按钮
    CreateWindowW(L"BUTTON", L"上传SourceMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 370, 440, 30, hWnd, (HMENU)IDC_UPLOAD_SM_BTN, hInst, NULL);

    // 上传MetaMod按钮
    CreateWindowW(L"BUTTON", L"上传MetaMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 420, 440, 30, hWnd, (HMENU)IDC_UPLOAD_MM_BTN, hInst, NULL);

    // 安装SourceMod与MetaMod按钮
    CreateWindowW(L"BUTTON", L"安装SourceMod与MetaMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 470, 440, 30, hWnd, (HMENU)IDC_UPLOAD_SMM_BTN, hInst, NULL);

    // 3. 服务器实例面板
    CreateWindowW(L"BUTTON", L"服务器实例",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 100, 490, 220, hWnd, (HMENU)IDC_INSTANCE_GROUP, hInst, NULL);

    // 3.1 实例列表（ListView）
    HWND hInstanceList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        520, 130, 450, 160, hWnd, (HMENU)IDC_INSTANCE_LIST, hInst, NULL);
    // 设置列表列
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    // 第0列
    lvc.cx = 60;
    lvc.pszText = const_cast<LPWSTR>(L"状态");
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

    // 3.2 端口输入和操作按钮
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

    // 4. 操作与日志面板（放在服务器实例面板下方，与左侧系统状态面板高度匹配）
    CreateWindowW(L"BUTTON", L"插件管理 & 日志",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 330, 490, 210, hWnd, (HMENU)IDC_ACTION_GROUP, hInst, NULL);

    // 4.1 插件管理按钮
    CreateWindowW(L"BUTTON", L"插件管理",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        520, 360, 140, 30, hWnd, (HMENU)IDC_PLUGIN_BTN, hInst, NULL);

    // 4.2 日志查看按钮
    CreateWindowW(L"BUTTON", L"清除日志",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        680, 360, 140, 30, hWnd, (HMENU)IDC_LOG_BTN, hInst, NULL);

    // 4.3 地图管理按钮
    CreateWindowW(
        L"BUTTON", L"地图管理",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        840, 360, 140, 30,  // 位置在插件管理按钮右侧
        hWnd, (HMENU)IDC_MAP_BTN, hInst, NULL
    );

    // 4.4 日志显示框（多行编辑框）
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        520, 400, 450, 120, hWnd, (HMENU)IDC_LOG_VIEW, hInst, NULL);

    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_CHANGE_REMOTE_PATH, L"更改远程项目部署位置");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"文件");
    SetMenu(hWnd, hMenu);
}

// 更新连接状态
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected) {
    HWND hStatus = GetDlgItem(hWnd, IDC_CONN_STATUS);
    SetWindowTextW(hStatus, status);
    // 连接成功时启用操作按钮
    EnableWindow(GetDlgItem(hWnd, IDC_DEPLOY_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_PLUGIN_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_UPLOAD_SM_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_UPLOAD_MM_BTN), isConnected);
}

// 更新系统状态面板
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR mmStatus) {
    // 更新服务器文件状态
    SetWindowTextW(GetDlgItem(hWnd, IDC_SERVER_STATUS), serverStatus);
    // 更新SourceMod状态
    SetWindowTextW(GetDlgItem(hWnd, IDC_SM_STATUS), smStatus);
    // 更新MetaMod状态
    SetWindowTextW(GetDlgItem(hWnd, IDC_MM_STATUS), mmStatus);
}

// 更新实例列表
void UpdateInstanceList(HWND hWnd, const char* instancesJson) {
    HWND hList = GetDlgItem(hWnd, IDC_INSTANCE_LIST);
    ListView_DeleteAllItems(hList);  // 清空现有项

    if (!instancesJson || *instancesJson == '\0') {
        AddLog(hWnd, L"获取实例列表失败：空JSON数据");
        return;
    }

    // 解析JSON根对象
    cJSON* root = cJSON_Parse(instancesJson);
    if (!root) {
        AddLog(hWnd, L"解析实例列表JSON失败");
        return;
    }

    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;  // 统一设置文本字段
    int itemIndex = 0;

    // 遍历所有实例对象
    cJSON* currentInstance = NULL;
    cJSON_ArrayForEach(currentInstance, root) {
        // 获取实例名称（键名）
        const char* instanceName = currentInstance->string;
        if (!instanceName) continue;

        // 转换实例名称为宽字符（UTF-8到Unicode）
        WCHAR nameW[256] = { 0 };
        GBKtoU16(instanceName, nameW, 256);

        // 获取实例属性
        cJSON* port = cJSON_GetObjectItem(currentInstance, "port");
        cJSON* map = cJSON_GetObjectItem(currentInstance, "map");
        cJSON* running = cJSON_GetObjectItem(currentInstance, "running");

        // 验证必要字段
        if (!port || !map || !running || !cJSON_IsString(port) || !cJSON_IsString(map) || !cJSON_IsBool(running)) {
            AddLog(hWnd, L"实例数据格式错误，跳过该实例");
            continue;
        }

        // 转换端口为宽字符（UTF-8到Unicode）
        WCHAR portW[16] = { 0 };
        GBKtoU16(port->valuestring, portW, 16);

        // 转换地图为宽字符（UTF-8到Unicode）
        WCHAR mapW[256] = { 0 };
        GBKtoU16(map->valuestring, mapW, 256);

        // 根据running的布尔值设置状态文本（true/false判断）
        WCHAR statusW[20] = { 0 };
        wcscpy_s(statusW, (running->type == cJSON_True) ? L"运行中" : L"已停止");

        // 设置操作文本
        WCHAR actionW[20] = { 0 };
        wcscpy_s(actionW, (running->type == cJSON_True) ? L"停止" : L"启动");

        // 插入列表项
        lvi.iItem = itemIndex;
        lvi.pszText = statusW;
        ListView_InsertItem(hList, &lvi);

        // 设置其他列内容
        ListView_SetItemText(hList, itemIndex, 1, nameW);       // 实例名称
        ListView_SetItemText(hList, itemIndex, 2, portW);        // 端口
        ListView_SetItemText(hList, itemIndex, 3, mapW);         // 地图
        ListView_SetItemText(hList, itemIndex, 4, actionW);      // 操作

        itemIndex++;
    }

    // 清理JSON资源
    cJSON_Delete(root);
    AddLog(hWnd, L"实例列表已更新");
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
/*
专门用于管理员管理的功能实现
*/
#define _WIN32_WINNT 0x0501  // 定义最低支持的Windows版本
#include "ssh.h"
#include "manager.h"
#include "admin_manager.h"
#include "cJSON.h"
#include "config.h"
#include "encoding_convert.h"
#include "Resource.h"
#include <shlwapi.h>
#include <vector>
#include <string>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <tchar.h>


#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

// 管理员管理窗口类名和控件ID
#define ADMIN_WINDOW_CLASS L"AdminManagerClass"
#define IDC_ADMIN_LIST      3001  // 管理员列表
#define IDC_PERMISSION_LABEL 3002 // 权限标签
#define IDC_STEAMID_EDIT    3003  // SteamID/用户名输入框
#define IDC_ADD_BTN         3004  // 添加管理员按钮
#define IDC_REMOVE_BTN      3005  // 删除管理员按钮
#define IDC_REFRESH_BTN     3006  // 刷新列表按钮

// 权限复选框ID
#define IDC_PERM_0          3100  // 保留权限
#define IDC_PERM_1          3101  // 管理员菜单
#define IDC_PERM_2          3102  // 踢人权限
#define IDC_PERM_3          3103  // 封禁权限
#define IDC_PERM_4          3104  // 地图管理
#define IDC_PERM_5          3105  // 服务器设置
#define IDC_PERM_6          3106  // 插件管理
#define IDC_PERM_7          3107  // 作弊权限

// 全局SSH上下文
extern L4D2_SSH_Context* g_ssh_ctx;

// 管理员窗口消息处理
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 创建管理员窗口控件
void CreateAdminWindowControls(HWND hWnd, HINSTANCE hInst);

// 更新管理员列表
void UpdateAdminList(HWND hWnd);

// 获取选中的管理员
std::wstring GetSelectedAdmin(HWND hList);

// 获取选中的权限位
std::string GetSelectedPermissions(HWND hWnd);

// 添加管理员处理
void OnAddAdmin(HWND hWnd);

// 删除管理员处理
void OnRemoveAdmin(HWND hWnd);

//得到选中的管理员存储进vector便于删除
std::vector<std::wstring> GetAllSelectedAdmins(HWND hListBox);

// 管理员管理窗口创建函数
DWORD WINAPI HandleManageAdmin(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // 检查窗口类是否已注册
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, ADMIN_WINDOW_CLASS, &existingClass)) {
        // 类未注册，执行注册
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = AdminWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = ADMIN_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"窗口类注册失败", L"错误", MB_ICONERROR);
            return 0;
        }
    }

    // 创建管理员管理窗口
    HWND hWnd = CreateWindowExW(
        0,
        ADMIN_WINDOW_CLASS,
        L"服务器管理员配置",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 550,
        hParent,
        NULL,
        hInst,
        NULL
    );

    if (!hWnd) {
        MessageBoxW(hParent, L"窗口创建失败", L"错误", MB_ICONERROR);
        return 0;
    }

    // 显示窗口
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// 创建管理员窗口控件
void CreateAdminWindowControls(HWND hWnd, HINSTANCE hInst) {
    // 上部分左侧：管理员列表
    CreateWindowW(
        L"STATIC", L"当前管理员列表",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 350, 20,
        hWnd, NULL, hInst, NULL
    );

    HWND hAdminList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        20, 45, 350, 350,
        hWnd, (HMENU)IDC_ADMIN_LIST, hInst, NULL
    );
    // 设置扩展样式：支持勾选框
    ListView_SetExtendedListViewStyle(hAdminList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    // 设置列表头
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;

    lvc.cx = 180;  // 第一列宽
    lvc.pszText = const_cast<LPWSTR>(L"SteamID/用户名");
    ListView_InsertColumn(hAdminList, 0, &lvc);

    lvc.cx = 160;  // 第二列宽
    lvc.pszText = const_cast<LPWSTR>(L"权限");
    ListView_InsertColumn(hAdminList, 1, &lvc);

    // 上部分右侧：权限选择
    CreateWindowW(
        L"STATIC", L"管理员权限",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        430, 20, 350, 20,
        hWnd, (HMENU)IDC_PERMISSION_LABEL, hInst, NULL
    );

    // 权限复选框 (8个)
    const WCHAR* permTexts[8] = {
        L"保留权限 (z) - 最高权限",
        L"管理员菜单 (a) - 使用管理员菜单",
        L"踢人权限 (b) - 踢出玩家",
        L"封禁权限 (c) - 封禁玩家",
        L"地图管理 (d) - 更改地图",
        L"服务器设置 (e) - 修改服务器设置",
        L"插件管理 (f) - 管理插件",
        L"作弊权限 (h) - 使用作弊命令"
    };

    for (int i = 0; i < 8; i++) {
        CreateWindowW(
            L"BUTTON", permTexts[i],
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            450, 50 + (i * 40), 320, 25,
            hWnd, (HMENU)(IDC_PERM_0 + i), hInst, NULL
        );
    }

    // 下部分：操作区
    // SteamID/用户名输入框
    CreateWindowW(
        L"STATIC", L"SteamID/用户名:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 420, 120, 20,
        hWnd, NULL, hInst, NULL
    );

    CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        140, 418, 230, 25,
        hWnd, (HMENU)IDC_STEAMID_EDIT, hInst, NULL
    );

    // 添加管理员按钮
    CreateWindowW(
        L"BUTTON", L"添加管理员",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 460, 150, 30,
        hWnd, (HMENU)IDC_ADD_BTN, hInst, NULL
    );

    // 删除管理员按钮
    CreateWindowW(
        L"BUTTON", L"删除选中管理员",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        190, 460, 180, 30,
        hWnd, (HMENU)IDC_REMOVE_BTN, hInst, NULL
    );

    // 刷新按钮
    CreateWindowW(
        L"BUTTON", L"刷新列表",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        630, 460, 150, 30,
        hWnd, (HMENU)IDC_REFRESH_BTN, hInst, NULL
    );
}

// 管理员窗口消息处理
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // 初始化通用控件
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建控件
        CreateAdminWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // 初始加载管理员列表
        UpdateAdminList(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ADD_BTN:
            OnAddAdmin(hWnd);
            break;
        case IDC_REMOVE_BTN:
            OnRemoveAdmin(hWnd);
            break;
        case IDC_REFRESH_BTN:
            UpdateAdminList(hWnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// 更新管理员列表
void UpdateAdminList(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_admin";

    // 执行远程命令获取管理员列表
    char output[8192] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (!success) {
        WCHAR err_w[256];
        GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"获取管理员列表失败", MB_ICONERROR);
        return;
    }

    // 解析JSON结果
    std::vector<std::pair<std::string, std::string>> admins;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON解析失败", L"错误", MB_ICONERROR);
        return;
    }

    // 解析管理员数组
    if (cJSON_IsArray(root)) {
        int array_size = cJSON_GetArraySize(root);
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (cJSON_IsObject(item)) {
                cJSON* name = cJSON_GetObjectItem(item, "name");
                cJSON* flags = cJSON_GetObjectItem(item, "flags");

                if (cJSON_IsString(name) && name->valuestring &&
                    cJSON_IsString(flags) && flags->valuestring) {
                    admins.emplace_back(name->valuestring, flags->valuestring);
                }
            }
        }
    }

    // 释放JSON对象
    cJSON_Delete(root);

    // 更新管理员列表
    HWND hAdminList = GetDlgItem(hWnd, IDC_ADMIN_LIST);
    ListView_DeleteAllItems(hAdminList);

    for (size_t i = 0; i < admins.size(); ++i) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;

        // 管理员名称/SteamID
        WCHAR name[256];
        item.pszText = GBKtoU16(admins[i].first.c_str(), name, 256);
        ListView_InsertItem(hAdminList, &item);

        // 管理员权限
        WCHAR flags[256];
        ListView_SetItemText(hAdminList, i, 1, GBKtoU16(admins[i].second.c_str(), flags, 256));
    }
}

// 获取选中的管理员
std::wstring GetSelectedAdmin(HWND hList) {
    int selected = -1;
    int itemCount = ListView_GetItemCount(hList);

    // 查找勾选的项
    for (int i = 0; i < itemCount; ++i) {
        if (ListView_GetCheckState(hList, i)) {
            selected = i;
            break;
        }
    }

    // 如果没有勾选的项，查找选中的项
    if (selected == -1) {
        selected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    }

    if (selected == -1) {
        return L"";
    }

    WCHAR itemText[256] = { 0 };
    ListView_GetItemText(hList, selected, 0, itemText, sizeof(itemText) / sizeof(WCHAR));
    return std::wstring(itemText);
}

// 获取选中的权限位（完全移除BS_CHECKED依赖）
std::string GetSelectedPermissions(HWND hWnd) {
    char bits[9] = { 0 }; // 8位权限 + 终止符

    for (int i = 0; i < 8; i++) {
        HWND hCheck = GetDlgItem(hWnd, IDC_PERM_0 + i);
        // 使用按钮消息获取状态，替代BS_CHECKED宏
        // BM_GETCHECK消息返回值：0=未选中，1=选中
        LRESULT checkState = SendMessageW(hCheck, BM_GETCHECK, 0, 0);
        bits[i] = (checkState != 0) ? '1' : '0';
    }

    bits[8] = '\0';
    return std::string(bits);
}

// 添加管理员处理函数
void OnAddAdmin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取SteamID/用户名
    WCHAR steamIdW[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_STEAMID_EDIT), steamIdW, sizeof(steamIdW) / sizeof(WCHAR));

    if (wcscmp(steamIdW, L"") == 0) {
        MessageBoxW(hWnd, L"请输入SteamID或用户名", L"提示", MB_OK);
        return;
    }

    // 获取选中的权限
    std::string permissions = GetSelectedPermissions(hWnd);

    // 转换为多字节
    char steamId[256];
    U16toGBK(steamIdW, steamId, sizeof(steamId));

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();

    // 执行添加管理员命令
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s/L4D2_Manager_API.sh new_admin %s %s",
        remoteRoot.c_str(), steamId, permissions.c_str());

    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

    if (success) {
        WCHAR errW[256], steamIdW[256];
        GBKtoU16(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
        GBKtoU16(steamId, steamIdW, sizeof(steamIdW) / sizeof(WCHAR));
        // 清空输入框
        SetWindowTextW(GetDlgItem(hWnd, IDC_STEAMID_EDIT), L"");
        // 取消所有权限勾选
        for (int i = 0; i < 8; i++) {
            HWND hCheck = GetDlgItem(hWnd, IDC_PERM_0 + i);
            SendMessageW(hCheck, BM_SETCHECK, 0, 0); // 0表示未选中
        }
        // 更新列表
        UpdateAdminList(hWnd);
        WCHAR msg[512];
        swprintf_s(msg, L"添加管理员 %s 成功: %s", steamIdW, errW);
        MessageBoxW(hWnd, msg, L"添加成功", MB_OK);
    }
    else {
        WCHAR errW[256], steamIdW[256];
        GBKtoU16(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
        GBKtoU16(steamId, steamIdW, sizeof(steamIdW) / sizeof(WCHAR));

        WCHAR msg[512];
        swprintf_s(msg, L"添加管理员 %s 失败: %s", steamIdW, errW);
        MessageBoxW(hWnd, msg, L"添加失败", MB_ICONWARNING);
    }
}

// 删除管理员处理函数（支持批量删除）
void OnRemoveAdmin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取管理员列表控件
    HWND hAdminList = GetDlgItem(hWnd, IDC_ADMIN_LIST);

    // 获取所有选中的管理员（修改为返回多个选中项）
    std::vector<std::wstring> selectedAdmins = GetAllSelectedAdmins(hAdminList);

    if (selectedAdmins.empty()) {
        MessageBoxW(hWnd, L"请先勾选或选择要删除的管理员", L"提示", MB_OK);
        return;
    }

    // 批量确认提示
    WCHAR confirmMsg[512];
    swprintf_s(confirmMsg, L"确定要删除以下 %zu 个管理员吗？\n", selectedAdmins.size());
    for (const auto& admin : selectedAdmins) {
        wcscat_s(confirmMsg, admin.c_str());
        wcscat_s(confirmMsg, L"\n");
    }

    if (MessageBoxW(hWnd, confirmMsg, L"确认批量删除", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();
    std::wstring successList, failList;

    // 循环删除每个选中的管理员
    for (const auto& adminW : selectedAdmins) {
        // 转换为多字节
        char admin[256];
        if (U16toGBK(adminW.c_str(), admin, sizeof(admin)) == nullptr) {
            failList += adminW + L"（编码转换失败）\n";
            continue;
        }

        // 构建删除命令
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s/L4D2_Manager_API.sh rm_admin \"%s\"",
            remoteRoot.c_str(), admin);  // 注意添加引号处理特殊字符

        // 执行删除命令
        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        // 记录执行结果
        if (success) {
            successList += adminW + L"\n";
        }
        else {
            std::wstring errW;
            GBKtoU16(err_msg, &errW[0], sizeof(err_msg));
            failList += adminW + L"（失败：" + errW + L"）\n";
        }
    }

    // 更新管理员列表
    UpdateAdminList(hWnd);

    // 显示批量处理结果
    WCHAR resultMsg[1024] = L"批量删除完成！\n";
    if (!successList.empty()) {
        wcscat_s(resultMsg, L"\n成功删除：\n");
        wcscat_s(resultMsg, successList.c_str());
    }
    if (!failList.empty()) {
        wcscat_s(resultMsg, L"\n删除失败：\n");
        wcscat_s(resultMsg, failList.c_str());
    }
    MessageBoxW(hWnd, resultMsg, L"批量操作结果", MB_OK | (failList.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
}

// 辅助函数：获取所有勾选的管理员（适配ListView控件）
std::vector<std::wstring> GetAllSelectedAdmins(HWND hListView) {
    std::vector<std::wstring> result;
    int itemCount = ListView_GetItemCount(hListView);  // 获取ListView的项数

    for (int i = 0; i < itemCount; ++i) {
        // 检查当前项是否被勾选（ListView专用宏）
        if (ListView_GetCheckState(hListView, i)) {
            // 获取第0列的文本（管理员名称/SteamID）
            WCHAR itemText[256] = { 0 };
            ListView_GetItemText(
                hListView,  // ListView控件句柄
                i,          // 项索引
                0,          // 列索引（0列是管理员ID）
                itemText,   // 接收文本的缓冲区
                sizeof(itemText) / sizeof(WCHAR)  // 缓冲区大小
            );

            result.push_back(std::wstring(itemText));
        }
    }
    return result;
}

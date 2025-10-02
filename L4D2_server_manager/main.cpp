/*
入口程序，实现项目主逻辑
*/

#define WIN32_LEAN_AND_MEAN  // 禁用 Windows 旧版头文件中的冗余定义
#include <winsock2.h>        // 只包含新版 Winsock 2.0
#include <ws2tcpip.h>        
#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 2.0 库
#include "framework.h"
#include "resource.h"
#include "gui.h"
#include "manager.h"
#include "ssh.h"
#include "resource.h"
#include "plugin_manager.h"
#include "map_manager.h"
#include "config.h"
#include "cJSON.h"
#include "encoding_convert.h"
#include "admin_manager.h"
#include <fcntl.h>
#include <string.h>
#include <string>

#define MAX_LOADSTRING 100
#define WM_USER_PROGRESS_UPDATE (WM_USER + 100)  // 使用WM_USER加上偏移量定义自定义消息

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// 全局SSH上下文实例
L4D2_SSH_Context* g_ssh_ctx = NULL;

// 对话框回调函数
INT_PTR CALLBACK RemotePathDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::string* newPath;

    switch (msg) {
    case WM_INITDIALOG: {
        // 初始化对话框，显示当前路径
        newPath = (std::string*)lParam;
        std::string currentPath = GetRemoteRootPath();

        WCHAR currentPathW[256] = { 0 };
        MultiByteToWideChar(CP_ACP, 0, currentPath.c_str(), -1, currentPathW, sizeof(currentPathW) / sizeof(WCHAR));
        SetDlgItemTextW(hDlg, IDC_PATH_EDIT, currentPathW);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            // 用户确认，获取输入的路径
            WCHAR pathW[256] = { 0 };
            GetDlgItemTextW(hDlg, IDC_PATH_EDIT, pathW, sizeof(pathW) / sizeof(WCHAR));

            char pathA[256] = { 0 };
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, sizeof(pathA), NULL, NULL);
            *newPath = std::string(pathA);

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_L4D2SERVERMANAGER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_L4D2SERVERMANAGER));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 清理SSH资源
    if (g_ssh_ctx)
    {
        l4d2_ssh_cleanup(g_ssh_ctx);
    }

    return (int)msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_L4D2SERVERMANAGER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_L4D2SERVERMANAGER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 将实例句柄存储在全局变量中

    // 创建主窗口（固定初始大小为1024x600）
    HWND hWnd = CreateWindowW(szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, 0, 1024, 620, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    // 初始化公共控件和创建UI元素
    InitCommonControlsExWrapper();
    CreateAllControls(hWnd, hInstance);

    // 初始化日志显示
    AddLog(hWnd, L"程序已启动，等待连接服务器...");

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_CONNECT_BTN:
            CreateThread(NULL, 0, HandleConnectRequest, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_DEPLOY_BTN:
            CreateThread(NULL, 0, HandleDeployServer, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_START_INSTANCE:
            CreateThread(NULL, 0, HandleStartInstance, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_STOP_INSTANCE:
            CreateThread(NULL, 0, HandleStopInstance, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_LOG_BTN:
            ClearLog(hWnd);
            break;
        case IDC_UPLOAD_SM_BTN:
            CreateThread(NULL, 0, HandleUploadSourceMod, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_UPLOAD_MM_BTN:
            CreateThread(NULL, 0, HandleUploadMetaMod, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_UPLOAD_SMM_BTN:
            CreateThread(NULL, 0, HandleInstallSourceMetaMod, (LPVOID)hWnd, 0, NULL);
            break;
        case IDC_PLUGIN_BTN:
            // 检查是否已连接到服务器
            if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
                // 未连接时添加日志并禁止操作
                AddLog(hWnd, L"未连接无法打开插件管理面板");
            }
            else {
                // 已连接时创建线程处理插件管理
                CreateThread(NULL, 0, HandleManagePlugin, (LPVOID)hWnd, 0, NULL);
            }
            break;
        case IDC_MAP_BTN:
            if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
                // 未连接时添加日志并禁止操作
                AddLog(hWnd, L"未连接无法打开地图管理面板");
            }
            else {
                CreateThread(NULL, 0, HandleManageMaps, (LPVOID)hWnd, 0, NULL);
            }
            break;
        case ID_CHANGE_REMOTE_PATH: {
            std::string newPath;
            // 创建对话框
            INT_PTR result = DialogBoxParamW(
                hInst,
                MAKEINTRESOURCE(IDD_REMOTE_PATH_DIALOG),
                hWnd,
                RemotePathDialogProc,
                (LPARAM)&newPath
            );

            if (result == IDOK && !newPath.empty()) {
                // 保存新路径
                SaveRemoteRootPath(newPath);

                // 提示用户重新连接
                MessageBoxW(hWnd, L"远程项目路径已更新，请重新连接服务器使设置生效", L"提示", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
                                  // 在消息处理函数的WM_COMMAND部分添加
        case IDC_ADMIN_CONFIG_BTN:
            // 检查SourceMod是否已安装
        {
            std::string remoteRoot = GetRemoteRootPath();
            std::string command = remoteRoot + "/L4D2_Manager_API.sh get_status";

            char output[8192] = { 0 };
            char err_msg[256] = { 0 };

            bool success = l4d2_ssh_exec_command(
                g_ssh_ctx,
                command.c_str(),
                output, sizeof(output),
                err_msg, sizeof(err_msg)
            );

            if (success) {
                cJSON* root = cJSON_Parse(output);
                if (root) {
                    cJSON* smInstalled = cJSON_GetObjectItem(root, "smInstalled");
                    bool smStatus = (smInstalled && smInstalled->type == cJSON_True);

                    cJSON_Delete(root);

                    if (smStatus) {
                        // SourceMod已安装，打开管理员配置窗口
                        CreateThread(NULL, 0, HandleManageAdmin, hWnd, 0, NULL);
                    }
                    else {
                        AddLog(hWnd, L"sourcemod未安装，无法设置管理员");
                    }
                }
                else {
                    AddLog(hWnd, L"解析系统状态JSON失败");
                }
            }
            else {
                WCHAR err_msg_w[256];
                GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
                AddLog(hWnd, err_msg_w);
            }
        }
        break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_NOTIFY:
    {
        // 处理ListView控件通知（如实例操作按钮）
        LPNMHDR pNMHDR = (LPNMHDR)lParam;
        if (pNMHDR->hwndFrom == GetDlgItem(hWnd, IDC_INSTANCE_LIST) &&
            pNMHDR->code == LVN_ITEMACTIVATE)
        {
            // 获取选中的项
            LPNMLISTVIEW pNMListView = (LPNMLISTVIEW)lParam;
            if (pNMListView->iItem != -1)
            {
                WCHAR itemText[256];
                ListView_GetItemText(pNMListView->hdr.hwndFrom, pNMListView->iItem, 1, itemText, 256);
                ListView_GetItemText(pNMListView->hdr.hwndFrom, pNMListView->iItem, 4, itemText, 256);

                WCHAR logText[256];
                swprintf_s(logText, L"实例操作: %s %s", itemText, L"功能待实现");
                AddLog(hWnd, logText);
            }
        }
    }
    break;
    case WM_CTLCOLORSTATIC:
    {
        // 设置静态文本控件样式
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(50, 50, 50)); // 深灰色文本
        SetBkColor(hdcStatic, RGB(245, 245, 245)); // 浅灰色背景
        return (LRESULT)CreateSolidBrush(RGB(245, 245, 245));
    }
    break;
    case WM_CTLCOLORBTN:
    {
        // 设置按钮控件样式
        HDC hdcBtn = (HDC)wParam;
        SetTextColor(hdcBtn, RGB(255, 255, 255)); // 白色文本
        SetBkColor(hdcBtn, RGB(0, 120, 215)); // 蓝色背景
        return (LRESULT)CreateSolidBrush(RGB(0, 120, 215));
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // 绘制窗口背景
        RECT rect;
        GetClientRect(hWnd, &rect);
        HBRUSH hBrush = CreateSolidBrush(RGB(245, 245, 245));
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_USER_PROGRESS_UPDATE: {  // 现在可以正确识别这个消息
        // wParam 包含进度值（0-10000，代表0.00%-100.00%）
        HWND hProgressWnd = FindWindowEx(hWnd, NULL, L"STATIC", L"正在部署服务器...");
        if (hProgressWnd) {
            HWND hProgress = GetDlgItem(hProgressWnd, 1001);
            if (hProgress) {
                SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 10000));
                SendMessage(hProgress, PBM_SETPOS, wParam, 0);
            }
        }
    }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

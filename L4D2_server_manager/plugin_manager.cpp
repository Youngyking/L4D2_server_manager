/*
专门用于插件管理的功能实现
*/

#include "ssh.h"
#include "manager.h"
#include "plugin_manager.h"
#include <shlwapi.h>
#include <vector>
#include <string>


#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

// 插件管理窗口类名和控件ID
#define PLUGIN_WINDOW_CLASS L"PluginManagerClass"
#define IDC_AVAILABLE_LIST  1001  // 可用插件列表
#define IDC_INSTALLED_LIST  1002  // 已安装插件列表
#define IDC_UPLOAD_BTN      1003  // 上传插件按钮
#define IDC_INSTALL_EDIT    1004  // 安装插件输入框
#define IDC_INSTALL_BTN     1005  // 安装插件按钮
#define IDC_UNINSTALL_EDIT  1006  // 卸载插件输入框
#define IDC_UNINSTALL_BTN   1007  // 卸载插件按钮

// 全局SSH上下文（假设从主窗口传递）
extern L4D2_SSH_Context* g_ssh_ctx;

// 字符串转换辅助函数
static WCHAR* CharToWChar_ser(const char* str, WCHAR* out, int out_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, out, out_len);
    return out;
}

static char* WCharToChar_ser(const WCHAR* wstr, char* out, int out_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, out, out_len, NULL, NULL);
    return out;
}

// 插件窗口消息处理
LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 更新插件列表
void UpdatePluginLists(HWND hWnd);

// 上传插件处理
void OnUploadPlugins(HWND hWnd);

// 安装插件处理
void OnInstallPlugin(HWND hWnd);

// 卸载插件处理
void OnUninstallPlugin(HWND hWnd);

// 递归上传目录
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

// 插件管理窗口创建函数
DWORD WINAPI HandleManagePlugin(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // 检查窗口类是否已注册
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, PLUGIN_WINDOW_CLASS, &existingClass)) {
        // 类未注册，执行注册
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = PluginWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = PLUGIN_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"窗口类注册失败", L"错误", MB_ICONERROR);
            return 0;
        }
    }

    // 创建插件管理窗口
    HWND hWnd = CreateWindowExW(
        0,
        PLUGIN_WINDOW_CLASS,
        L"插件管理",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
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

// 创建插件管理窗口控件
void CreatePluginWindowControls(HWND hWnd, HINSTANCE hInst) {
    // 1. 可用插件列表（左栏）
    HWND hAvailableList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        20, 20, 350, 400,
        hWnd, (HMENU)IDC_AVAILABLE_LIST, hInst, NULL
    );

    // 设置列表头
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 320;  // 列宽
    lvc.pszText = const_cast<LPWSTR>(L"可用插件");
    ListView_InsertColumn(hAvailableList, 0, &lvc);

    // 2. 已安装插件列表（右栏）
    HWND hInstalledList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        430, 20, 350, 400,
        hWnd, (HMENU)IDC_INSTALLED_LIST, hInst, NULL
    );

    lvc.pszText = const_cast<LPWSTR>(L"已安装插件");
    ListView_InsertColumn(hInstalledList, 0, &lvc);

    // 3. 操作区控件（下栏）
    // 上传插件按钮
    CreateWindowW(
        L"BUTTON", L"上传插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 450, 150, 30,
        hWnd, (HMENU)IDC_UPLOAD_BTN, hInst, NULL
    );

    // 安装插件输入框和按钮
    CreateWindowW(
        L"EDIT", L"输入插件名",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        200, 450, 150, 30,
        hWnd, (HMENU)IDC_INSTALL_EDIT, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"安装插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 450, 100, 30,
        hWnd, (HMENU)IDC_INSTALL_BTN, hInst, NULL
    );

    // 卸载插件输入框和按钮
    CreateWindowW(
        L"EDIT", L"输入插件名",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        500, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_EDIT, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"卸载插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 450, 100, 30,
        hWnd, (HMENU)IDC_UNINSTALL_BTN, hInst, NULL
    );
}

// 插件窗口消息处理
LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // 初始化通用控件
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建控件
        CreatePluginWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // 初始加载插件列表
        UpdatePluginLists(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_UPLOAD_BTN:
            OnUploadPlugins(hWnd);
            break;
        case IDC_INSTALL_BTN:
            OnInstallPlugin(hWnd);
            break;
        case IDC_UNINSTALL_BTN:
            OnUninstallPlugin(hWnd);
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

// 更新插件列表（解析get_plugins命令结果）
void UpdatePluginLists(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 执行远程命令获取插件列表
    char output[8192] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh get_plugins",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (!success) {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"获取插件列表失败", MB_ICONERROR);
        return;
    }

    // 解析JSON结果（简化版解析，实际项目建议使用JSON库）
    std::vector<std::string> available_plugins;
    std::vector<std::string> installed_plugins;
    std::string json(output);

    // 提取可用插件列表
    size_t avail_start = json.find("\"available\": [") + 13;
    size_t avail_end = json.find("]", avail_start);
    if (avail_start < avail_end) {
        std::string avail_str = json.substr(avail_start, avail_end - avail_start);
        size_t pos = 0;
        while (pos < avail_str.size()) {
            size_t start = avail_str.find("\"", pos) + 1;
            if (start == std::string::npos) break;
            size_t end = avail_str.find("\"", start);
            if (end == std::string::npos) break;
            available_plugins.push_back(avail_str.substr(start, end - start));
            pos = end + 1;
        }
    }

    // 提取已安装插件列表
    size_t inst_start = json.find("\"installed\": [") + 14;
    size_t inst_end = json.find("]", inst_start);
    if (inst_start < inst_end) {
        std::string inst_str = json.substr(inst_start, inst_end - inst_start);
        size_t pos = 0;
        while (pos < inst_str.size()) {
            size_t start = inst_str.find("\"", pos) + 1;
            if (start == std::string::npos) break;
            size_t end = inst_str.find("\"", start);
            if (end == std::string::npos) break;
            installed_plugins.push_back(inst_str.substr(start, end - start));
            pos = end + 1;
        }
    }

    // 更新可用插件列表
    HWND hAvailable = GetDlgItem(hWnd, IDC_AVAILABLE_LIST);
    ListView_DeleteAllItems(hAvailable);
    for (size_t i = 0; i < available_plugins.size(); ++i) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;
        WCHAR name[256];
        item.pszText = CharToWChar_ser(available_plugins[i].c_str(), name, 256);
        ListView_InsertItem(hAvailable, &item);
    }

    // 更新已安装插件列表
    HWND hInstalled = GetDlgItem(hWnd, IDC_INSTALLED_LIST);
    ListView_DeleteAllItems(hInstalled);
    for (size_t i = 0; i < installed_plugins.size(); ++i) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;
        WCHAR name[256];
        item.pszText = CharToWChar_ser(installed_plugins[i].c_str(), name, 256);
        ListView_InsertItem(hInstalled, &item);
    }
}

// 上传插件处理函数
void OnUploadPlugins(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取本地resource目录路径（项目根目录下的resource文件夹）
    WCHAR local_res_path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, local_res_path, MAX_PATH)) {
        MessageBoxW(hWnd, L"获取程序路径失败", L"错误", MB_ICONERROR);
        return;
    }
    PathRemoveFileSpecW(local_res_path);  // 移除可执行文件名
    PathAppendW(local_res_path, L"resource");  // 拼接resource目录

    // 检查目录是否存在
    if (!PathIsDirectoryW(local_res_path)) {
        MessageBoxW(hWnd, L"本地resource目录不存在", L"错误", MB_ICONERROR);
        return;
    }

    // 遍历resource目录下的所有插件文件夹
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[MAX_PATH];
    swprintf_s(search_path, L"%s\\*", local_res_path);
    HANDLE hFind = FindFirstFileW(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBoxW(hWnd, L"遍历插件目录失败", L"错误", MB_ICONERROR);
        return;
    }

    do {
        // 只处理目录（跳过.和..）
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // 插件名称（文件夹名）
            char plugin_name[256];
            WCharToChar_ser(find_data.cFileName, plugin_name, 256);

            // 远程插件目录
            std::string remote_dir = "/home/L4D2_Manager/Available_Plugins/" + std::string(plugin_name);

            // 创建远程目录
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_dir.c_str());
            char err_msg[256] = { 0 };
            if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, nullptr, 0, err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, 256);
                MessageBoxW(hWnd, err_w, L"创建远程目录失败", MB_ICONERROR);
                continue;
            }

            // 本地插件目录
            WCHAR local_plugin_dir[MAX_PATH];
            swprintf_s(local_plugin_dir, L"%s\\%s", local_res_path, find_data.cFileName);

            // 递归上传目录内容
            if (!UploadDirectory(hWnd, local_plugin_dir, remote_dir)) {
                WCHAR msg[512];
                swprintf_s(msg, L"插件 %s 上传失败", find_data.cFileName);
                MessageBoxW(hWnd, msg, L"警告", MB_ICONWARNING);
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    UpdatePluginLists(hWnd);  // 上传完成后更新列表
    MessageBoxW(hWnd, L"插件上传完成", L"提示", MB_OK);
}

// 递归上传目录内容
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir) {
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[MAX_PATH];
    swprintf_s(search_path, L"%s\\*", local_dir);
    HANDLE hFind = FindFirstFileW(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool result = true;
    do {
        // 处理子目录
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // 子目录路径
            WCHAR local_subdir[MAX_PATH];
            swprintf_s(local_subdir, L"%s\\%s", local_dir, find_data.cFileName);

            // 远程子目录路径
            char subdir_name[256];
            WCharToChar_ser(find_data.cFileName, subdir_name, 256);
            std::string remote_subdir = remote_dir + "/" + subdir_name;

            // 创建远程子目录
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_subdir.c_str());
            char err_msg[256] = { 0 };
            if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, nullptr, 0, err_msg, sizeof(err_msg))) {
                result = false;
                continue;
            }

            // 递归上传子目录
            if (!UploadDirectory(hWnd, local_subdir, remote_subdir)) {
                result = false;
            }
        }
        // 处理文件
        else if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // 本地文件路径
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // 远程文件路径
            char filename[256];
            WCharToChar_ser(find_data.cFileName, filename, 256);
            std::string remote_file = remote_dir + "/" + filename;

            // 转换本地路径为多字节
            char local_path[512];
            WCharToChar_ser(local_file, local_path, 512);

            // 上传文件
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, 256);
                MessageBoxW(hWnd, err_w, L"文件上传失败", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// 安装插件处理函数
void OnInstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取输入的插件名
    WCHAR plugin_name_w[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_INSTALL_EDIT), plugin_name_w, 256);
    if (wcscmp(plugin_name_w, L"") == 0) {
        MessageBoxW(hWnd, L"请输入插件名称", L"提示", MB_OK);
        return;
    }

    // 转换为多字节
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, 256);

    // 执行安装命令
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh install_plugin %s", plugin_name);
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

    if (success) {
        MessageBoxW(hWnd, L"插件安装成功", L"提示", MB_OK);
        UpdatePluginLists(hWnd);  // 更新列表
    }
    else {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, 256);
        MessageBoxW(hWnd, err_w, L"安装失败", MB_ICONERROR);
    }
}

// 卸载插件处理函数
void OnUninstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取输入的插件名
    WCHAR plugin_name_w[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_UNINSTALL_EDIT), plugin_name_w, 256);
    if (wcscmp(plugin_name_w, L"") == 0) {
        MessageBoxW(hWnd, L"请输入插件名称", L"提示", MB_OK);
        return;
    }

    // 转换为多字节
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, 256);

    // 执行卸载命令
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh uninstall_plugin %s", plugin_name);
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

    if (success) {
        MessageBoxW(hWnd, L"插件卸载成功", L"提示", MB_OK);
        UpdatePluginLists(hWnd);  // 更新列表
    }
    else {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, 256);
        MessageBoxW(hWnd, err_w, L"卸载失败", MB_ICONERROR);
    }
}
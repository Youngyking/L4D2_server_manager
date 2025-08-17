/*
专门用于插件管理的功能实现
*/

#include "ssh.h"
#include "manager.h"
#include "plugin_manager.h"
#include "cJSON.h"
#include "config.h"
#include <shlwapi.h>
#include <vector>
#include <string>
#include <commdlg.h>
#include <shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")  // 用于文件夹选择对话框

// 插件管理窗口类名和控件ID
#define PLUGIN_WINDOW_CLASS L"PluginManagerClass"
#define IDC_AVAILABLE_LIST  1001  // 可用插件列表
#define IDC_INSTALLED_LIST  1002  // 已安装插件列表
#define IDC_UPLOAD_BTN      1003  // 上传插件按钮
#define IDC_INSTALL_BTN     1005  // 安装插件按钮
#define IDC_UNINSTALL_EDIT  1006  // 卸载插件输入框
#define IDC_UNINSTALL_BTN   1007  // 卸载插件按钮
#define IDC_DELETE_BTN      1009  // 删除可用插件按钮

// 全局SSH上下文
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

// 删除可用插件处理
void OnDeleteAvailablePlugin(HWND hWnd);

// 递归上传目录
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

// 获取列表中勾选的项
std::vector<std::wstring> GetCheckedItems(HWND hList) {
    std::vector<std::wstring> checkedItems;
    int itemCount = ListView_GetItemCount(hList);

    for (int i = 0; i < itemCount; ++i) {
        // 检查项是否被勾选
        if (ListView_GetCheckState(hList, i)) {
            WCHAR itemText[256] = { 0 };
            ListView_GetItemText(hList, i, 0, itemText, sizeof(itemText) / sizeof(WCHAR));
            checkedItems.push_back(std::wstring(itemText));
        }
    }

    return checkedItems;
}

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
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 550,  // 调整窗口高度
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
    // 1. 可用插件列表（左栏）- 支持勾选和多选
    HWND hAvailableList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        20, 20, 350, 400,
        hWnd, (HMENU)IDC_AVAILABLE_LIST, hInst, NULL
    );
    // 设置扩展样式：支持勾选框
    ListView_SetExtendedListViewStyle(hAvailableList, LVS_EX_CHECKBOXES);

    // 设置列表头
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 320;  // 列宽
    lvc.pszText = const_cast<LPWSTR>(L"可用插件");
    ListView_InsertColumn(hAvailableList, 0, &lvc);

    // 2. 已安装插件列表（右栏）- 支持勾选和多选
    HWND hInstalledList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        430, 20, 350, 400,
        hWnd, (HMENU)IDC_INSTALLED_LIST, hInst, NULL
    );
    // 设置扩展样式：支持勾选框
    ListView_SetExtendedListViewStyle(hInstalledList, LVS_EX_CHECKBOXES);

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

    // 安装插件按钮（移除输入框，调整位置）
    CreateWindowW(
        L"BUTTON", L"安装选中插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 450, 150, 30,
        hWnd, (HMENU)IDC_INSTALL_BTN, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"卸载选中插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_BTN, hInst, NULL
    );

    // 删除可用插件按钮（移除输入框，调整位置）
    CreateWindowW(
        L"BUTTON", L"删除选中可用插件",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        610, 450, 150, 30,
        hWnd, (HMENU)IDC_DELETE_BTN, hInst, NULL
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
        case IDC_DELETE_BTN:
            OnDeleteAvailablePlugin(hWnd);
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

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_plugins";

    // 执行远程命令获取插件列表
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
        CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"获取插件列表失败", MB_ICONERROR);
        return;
    }

    // 解析JSON结果（使用cJSON库）
    std::vector<std::string> available_plugins;
    std::vector<std::string> installed_plugins;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON解析失败", L"错误", MB_ICONERROR);
        return;
    }

    // 解析可用插件列表
    cJSON* available_array = cJSON_GetObjectItem(root, "available");
    if (cJSON_IsArray(available_array)) {
        int array_size = cJSON_GetArraySize(available_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(available_array, i);
            if (cJSON_IsString(item) && item->valuestring) {
                available_plugins.push_back(item->valuestring);
            }
        }
    }

    // 解析已安装插件列表
    cJSON* installed_array = cJSON_GetObjectItem(root, "installed");
    if (cJSON_IsArray(installed_array)) {
        int array_size = cJSON_GetArraySize(installed_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(installed_array, i);
            if (cJSON_IsString(item) && item->valuestring) {
                installed_plugins.push_back(item->valuestring);
            }
        }
    }

    // 释放JSON对象
    cJSON_Delete(root);

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

// 上传插件处理函数（修改为用户选择单个文件夹）
void OnUploadPlugins(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取项目根目录下的resource文件夹路径
    WCHAR local_res_path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, local_res_path, MAX_PATH)) {
        MessageBoxW(hWnd, L"获取程序路径失败", L"错误", MB_ICONERROR);
        return;
    }
    PathRemoveFileSpecW(local_res_path);  // 移除可执行文件名，获取程序所在目录
    PathAppendW(local_res_path, L"resource");  // 拼接resource目录

    // 检查resource目录是否存在
    if (!PathIsDirectoryW(local_res_path)) {
        MessageBoxW(hWnd, L"本地resource目录不存在", L"错误", MB_ICONERROR);
        return;
    }

    // 文件夹选择对话框配置
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"请选择要上传的插件文件夹（位于resource目录下）";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;  // 只显示目录，使用新UI
    bi.lParam = (LPARAM)local_res_path;  // 传递初始目录

    // 设置初始目录回调
    bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) -> int {
        if (uMsg == BFFM_INITIALIZED) {
            // 初始化完成后设置初始目录
            SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
        }
        return 0;
        };

    // 显示文件夹选择对话框
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        return;  // 用户取消选择
    }

    // 获取选择的文件夹路径
    WCHAR local_plugin_dir[MAX_PATH];
    if (!SHGetPathFromIDListW(pidl, local_plugin_dir)) {
        MessageBoxW(hWnd, L"获取文件夹路径失败", L"错误", MB_ICONERROR);
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);  // 释放内存

    // 验证选择的文件夹是否在resource目录下
    if (wcsncmp(local_plugin_dir, local_res_path, wcslen(local_res_path)) != 0) {
        MessageBoxW(hWnd, L"请选择resource目录下的插件文件夹", L"提示", MB_OK);
        return;
    }

    // 提取插件文件夹名称（最后一级目录名）
    WCHAR* plugin_name_w = PathFindFileNameW(local_plugin_dir);
    if (!plugin_name_w || *plugin_name_w == L'\0') {
        MessageBoxW(hWnd, L"无效的插件文件夹名称", L"错误", MB_ICONERROR);
        return;
    }

    // 转换插件名称为多字节字符串
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, sizeof(plugin_name));

    // 服务器端目标目录：使用配置的根路径
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_dir = remoteRoot + "/Available_Plugins/" + std::string(plugin_name);

    // 创建服务器端目录
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_dir.c_str());
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"创建远程目录失败", MB_ICONERROR);
        return;
    }

    // 递归上传文件夹内容
    if (UploadDirectory(hWnd, local_plugin_dir, remote_dir)) {
        UpdatePluginLists(hWnd);  // 上传成功后更新列表
        MessageBoxW(hWnd, L"插件上传成功", L"提示", MB_OK);
    }
    else {
        MessageBoxW(hWnd, L"插件上传失败", L"错误", MB_ICONERROR);
    }
}

// 递归上传目录内容
static bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir) {
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[MAX_PATH];
    swprintf_s(search_path, L"%s\\*", local_dir);
    HANDLE hFind = FindFirstFileW(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool result = true;
    do {
        // 处理子目录（跳过.和..）
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // 构建本地子目录路径
            WCHAR local_subdir[MAX_PATH];
            swprintf_s(local_subdir, L"%s\\%s", local_dir, find_data.cFileName);

            // 构建远程子目录路径
            char subdir_name[256];
            WCharToChar_ser(find_data.cFileName, subdir_name, sizeof(subdir_name));
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
            // 构建本地文件路径
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // 构建远程文件路径
            char filename[256];
            WCharToChar_ser(find_data.cFileName, filename, sizeof(filename));
            std::string remote_file = remote_dir + "/" + filename;

            // 转换本地路径为多字节
            char local_path[512];
            WCharToChar_ser(local_file, local_path, sizeof(local_path));

            // 上传文件
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
                MessageBoxW(hWnd, err_w, L"文件上传失败", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// 安装插件处理函数（批量安装勾选的插件）
void OnInstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取可用列表中勾选的插件
    HWND hAvailableList = GetDlgItem(hWnd, IDC_AVAILABLE_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hAvailableList);

    if (checkedPlugins.empty()) {
        MessageBoxW(hWnd, L"请先勾选要安装的插件", L"提示", MB_OK);
        return;
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();

    // 批量安装勾选的插件
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // 转换为多字节
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // 执行安装命令
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s/L4D2_Manager_API.sh install_plugin %s",
            remoteRoot.c_str(), pluginName);
        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        if (success) {
            successCount++;
        }
        else {
            WCHAR errW[256], pluginNameW[256];
            CharToWChar_ser(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
            CharToWChar_ser(pluginName, pluginNameW, sizeof(pluginNameW) / sizeof(WCHAR));

            WCHAR msg[512];
            swprintf_s(msg, L"安装插件 %s 失败: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"安装失败", MB_ICONWARNING);
        }
    }

    // 显示安装结果
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"共%d个插件，成功安装%d个", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"安装完成", MB_OK);
    UpdatePluginLists(hWnd);  // 安装后更新列表
}

// 卸载插件处理函数（支持勾选和手动输入）
void OnUninstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取已安装列表中勾选的插件
    HWND hInstalledList = GetDlgItem(hWnd, IDC_INSTALLED_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hInstalledList);

    // 如果没有勾选，则尝试从输入框获取
    if (checkedPlugins.empty()) {
        WCHAR pluginNameW[256];
        GetWindowTextW(GetDlgItem(hWnd, IDC_UNINSTALL_EDIT), pluginNameW, sizeof(pluginNameW) / sizeof(WCHAR));
        if (wcscmp(pluginNameW, L"") != 0) {
            checkedPlugins.push_back(std::wstring(pluginNameW));
        }
        else {
            MessageBoxW(hWnd, L"请先勾选或输入要卸载的插件", L"提示", MB_OK);
            return;
        }
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();

    // 批量卸载勾选的插件
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // 转换为多字节
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // 执行卸载命令
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s/L4D2_Manager_API.sh uninstall_plugin %s",
            remoteRoot.c_str(), pluginName);
        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        if (success) {
            successCount++;
        }
        else {
            WCHAR errW[256], pluginNameW[256];
            CharToWChar_ser(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
            CharToWChar_ser(pluginName, pluginNameW, sizeof(pluginNameW) / sizeof(WCHAR));

            WCHAR msg[512];
            swprintf_s(msg, L"卸载插件 %s 失败: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"卸载失败", MB_ICONWARNING);
        }
    }

    // 显示卸载结果
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"共%d个插件，成功卸载%d个", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"卸载完成", MB_OK);
    UpdatePluginLists(hWnd);  // 卸载后更新列表
}

// 删除可用插件处理函数（批量删除勾选的插件）
void OnDeleteAvailablePlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取可用列表中勾选的插件
    HWND hAvailableList = GetDlgItem(hWnd, IDC_AVAILABLE_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hAvailableList);

    if (checkedPlugins.empty()) {
        MessageBoxW(hWnd, L"请先勾选要删除的可用插件", L"提示", MB_OK);
        return;
    }

    // 确认删除
    WCHAR confirmMsg[256];
    swprintf_s(confirmMsg, L"确定要删除选中的%d个插件吗？", checkedPlugins.size());
    if (MessageBoxW(hWnd, confirmMsg, L"确认删除", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();

    // 批量删除勾选的插件
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // 转换为多字节
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // 执行删除命令
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s/Available_Plugins/%s",
            remoteRoot.c_str(), pluginName);
        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        if (success) {
            successCount++;
        }
        else {
            WCHAR errW[256], pluginNameW[256];
            CharToWChar_ser(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
            CharToWChar_ser(pluginName, pluginNameW, sizeof(pluginNameW) / sizeof(WCHAR));

            WCHAR msg[512];
            swprintf_s(msg, L"删除插件 %s 失败: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"删除失败", MB_ICONWARNING);
        }
    }

    // 显示删除结果
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"共%d个插件，成功删除%d个", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"删除完成", MB_OK);
    UpdatePluginLists(hWnd);  // 删除后更新列表
}

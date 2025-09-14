#include "ssh.h"
#include "manager.h"
#include "map_manager.h"
#include "cJSON.h"
#include "encoding_convert.h"
#include "config.h"
#include <shlwapi.h>
#include <vector>
#include <string>
#include <commdlg.h>
#include <shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

// 全局SSH上下文
extern L4D2_SSH_Context* g_ssh_ctx;

// 地图窗口消息处理
LRESULT CALLBACK MapWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 更新地图列表
void UpdateMapList(HWND hWnd);

// 上传地图处理
void OnUploadMap(HWND hWnd);

// 卸载地图处理
void OnUninstallMap(HWND hWnd);

// 递归上传目录
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

// 获取列表中选中的地图名称
std::vector<std::string> GetSelectedMaps(HWND hList);

// 地图管理窗口创建函数
DWORD WINAPI HandleManageMaps(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // 检查窗口类是否已注册
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, MAP_WINDOW_CLASS, &existingClass)) {
        // 类未注册，执行注册
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MapWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = MAP_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"窗口类注册失败", L"错误", MB_ICONERROR);
            return 0;
        }
    }

    // 创建地图管理窗口
    HWND hWnd = CreateWindowExW(
        0,
        MAP_WINDOW_CLASS,
        L"地图管理",
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

// 创建地图管理窗口控件
void CreateMapWindowControls(HWND hWnd, HINSTANCE hInst) {
    // 1. 已安装地图列表 - 修改为支持多选
    HWND hInstalledMapsList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EX_CHECKBOXES,
        20, 20, 740, 400,
        hWnd, (HMENU)IDC_INSTALLED_MAPS_LIST, hInst, NULL
    );

    // 设置列表扩展样式以支持复选框
    ListView_SetExtendedListViewStyle(hInstalledMapsList, LVS_EX_CHECKBOXES);

    // 设置列表头
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 700;  // 列宽
    lvc.pszText = const_cast<LPWSTR>(L"已安装地图");
    ListView_InsertColumn(hInstalledMapsList, 0, &lvc);

    // 2. 操作区控件
    // 刷新地图按钮
    CreateWindowW(
        L"BUTTON", L"刷新已安装地图",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 450, 150, 30,
        hWnd, (HMENU)IDC_REFRESH_MAPS_BTN, hInst, NULL
    );

    // 上传地图按钮
    CreateWindowW(
        L"BUTTON", L"上传地图文件夹",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        190, 450, 150, 30,
        hWnd, (HMENU)IDC_UPLOAD_MAP_BTN, hInst, NULL
    );

    // 卸载地图按钮 - 调整位置，移除文本框
    CreateWindowW(
        L"BUTTON", L"卸载选中地图",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_MAP_BTN, hInst, NULL
    );
}

// 地图窗口消息处理
LRESULT CALLBACK MapWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // 初始化通用控件
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // 创建控件
        CreateMapWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // 初始加载地图列表
        UpdateMapList(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_REFRESH_MAPS_BTN:
            UpdateMapList(hWnd);
            break;
        case IDC_UPLOAD_MAP_BTN:
            OnUploadMap(hWnd);
            break;
        case IDC_UNINSTALL_MAP_BTN:
            OnUninstallMap(hWnd);
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

// 更新地图列表（解析list_maps命令结果）
void UpdateMapList(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh list_maps";

    // 执行远程命令获取地图列表
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
        MessageBoxW(hWnd, err_w, L"获取地图列表失败", MB_ICONERROR);
        return;
    }

    // 解析JSON结果
    std::vector<std::string> installed_maps;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON解析失败", L"错误", MB_ICONERROR);
        return;
    }

    // 解析已安装地图列表
    cJSON* maps_array = cJSON_GetObjectItem(root, "installed_maps");
    if (cJSON_IsArray(maps_array)) {
        int array_size = cJSON_GetArraySize(maps_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(maps_array, i);
            if (cJSON_IsString(item) && item->valuestring) {
                installed_maps.push_back(item->valuestring);
            }
        }
    }

    // 释放JSON对象
    cJSON_Delete(root);

    // 更新已安装地图列表
    HWND hInstalled = GetDlgItem(hWnd, IDC_INSTALLED_MAPS_LIST);
    ListView_DeleteAllItems(hInstalled);
    for (size_t i = 0; i < installed_maps.size(); ++i) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;
        WCHAR name[256];
        item.pszText = GBKtoU16(installed_maps[i].c_str(), name, 256);
        ListView_InsertItem(hInstalled, &item);
    }
}

// 获取列表中选中的地图名称
std::vector<std::string> GetSelectedMaps(HWND hList) {
    std::vector<std::string> selectedMaps;
    int itemCount = ListView_GetItemCount(hList);

    for (int i = 0; i < itemCount; i++) {
        // 检查项目是否被勾选
        if (ListView_GetCheckState(hList, i)) {
            WCHAR mapName[256];
            ListView_GetItemText(hList, i, 0, mapName, sizeof(mapName) / sizeof(WCHAR));

            char mapNameA[256];
            U16toGBK(mapName, mapNameA, sizeof(mapNameA));
            selectedMaps.push_back(std::string(mapNameA));
        }
    }

    return selectedMaps;
}

// 上传地图处理函数
void OnUploadMap(HWND hWnd) {
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

    // 检查resource目录是否存在，不存在则创建
    if (!PathIsDirectoryW(local_res_path)) {
        CreateDirectoryW(local_res_path, NULL);
        if (!PathIsDirectoryW(local_res_path)) {
            MessageBoxW(hWnd, L"本地resource目录不存在且无法创建", L"错误", MB_ICONERROR);
            return;
        }
    }

    // 文件夹选择对话框配置
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"请选择要上传的地图文件夹（位于resource目录下）";
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
    WCHAR local_map_dir[MAX_PATH];
    if (!SHGetPathFromIDListW(pidl, local_map_dir)) {
        MessageBoxW(hWnd, L"获取文件夹路径失败", L"错误", MB_ICONERROR);
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);  // 释放内存

    // 验证选择的文件夹是否在resource目录下
    if (wcsncmp(local_map_dir, local_res_path, wcslen(local_res_path)) != 0) {
        MessageBoxW(hWnd, L"请选择resource目录下的地图文件夹", L"提示", MB_OK);
        return;
    }

    // 提取地图文件夹名称（最后一级目录名）
    WCHAR* map_name_w = PathFindFileNameW(local_map_dir);
    if (!map_name_w || *map_name_w == L'\0') {
        MessageBoxW(hWnd, L"无效的地图文件夹名称", L"错误", MB_ICONERROR);
        return;
    }

    // 转换地图名称为多字节字符串
    char map_name[256];
    U16toGBK(map_name_w, map_name, sizeof(map_name));

    // 服务器端临时目录 - 使用配置的根路径
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_tmp_dir = remoteRoot + "/map_tmp/" + std::string(map_name);

    // 创建服务器端临时目录
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_tmp_dir.c_str());
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
        WCHAR err_w[256];
        GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"创建远程临时目录失败", MB_ICONERROR);
        return;
    }

    // 递归上传文件夹内容
    if (UploadDirectory(hWnd, local_map_dir, remote_tmp_dir)) {
        // 上传成功后，调用脚本处理地图安装
        std::string installCmd = remoteRoot + "/L4D2_Manager_API.sh install_map " + std::string(map_name);
        snprintf(cmd, sizeof(cmd), "%s", installCmd.c_str());
        if (l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
            UpdateMapList(hWnd);  // 更新地图列表
            MessageBoxW(hWnd, L"地图上传并安装成功", L"提示", MB_OK);
        }
        else {
            WCHAR err_w[256];
            GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            MessageBoxW(hWnd, err_w, L"地图安装失败", MB_ICONERROR);
        }
    }
    else {
        MessageBoxW(hWnd, L"地图文件上传失败", L"错误", MB_ICONERROR);
    }
}

// 递归上传目录内容，将所有文件（包括子目录中的）上传到同一个远程目录，不保留目录结构
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

            // 递归上传子目录，但远程目录仍使用同一个根目录
            if (!UploadDirectory(hWnd, local_subdir, remote_dir)) {
                result = false;
            }
        }
        // 处理文件
        else if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // 构建本地文件路径
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // 构建远程文件路径 - 所有文件都直接放到remote_dir下
            char filename[256];
            U16toGBK(find_data.cFileName, filename, sizeof(filename));
            std::string remote_file = remote_dir + "/" + filename;

            // 转换本地路径为GBK
            char local_path[512];
            U16toGBK(local_file, local_path, sizeof(local_path));

            // 上传文件
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
                MessageBoxW(hWnd, err_w, L"文件上传失败", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// 卸载地图处理函数
void OnUninstallMap(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"未连接到服务器", L"错误", MB_ICONERROR);
        return;
    }

    // 获取已安装地图列表控件
    HWND hInstalledList = GetDlgItem(hWnd, IDC_INSTALLED_MAPS_LIST);

    // 获取所有选中的地图
    std::vector<std::string> selectedMaps = GetSelectedMaps(hInstalledList);

    if (selectedMaps.empty()) {
        MessageBoxW(hWnd, L"请先勾选要卸载的地图", L"提示", MB_OK);
        return;
    }

    // 确认卸载
    WCHAR confirmMsg[512];
    swprintf_s(confirmMsg, L"确定要卸载选中的 %d 个地图吗？", selectedMaps.size());
    if (MessageBoxW(hWnd, confirmMsg, L"确认卸载", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    // 记录卸载成功和失败的数量
    int successCount = 0;
    int failCount = 0;

    // 获取配置的远程路径
    std::string remoteRoot = GetRemoteRootPath();

    // 逐个卸载选中的地图
    for (const std::string& mapName : selectedMaps) {
        // 执行卸载命令
        std::string uninstallCmd = remoteRoot + "/L4D2_Manager_API.sh uninstall_map " + mapName;
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s", uninstallCmd.c_str());

        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        if (success) {
            successCount++;
        }
        else {
            failCount++;
            WCHAR err_w[256];
            GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));

            WCHAR errorMsg[512];
            swprintf_s(errorMsg, L"卸载地图 %s 失败:\n%s", mapName.c_str(), err_w);
            MessageBoxW(hWnd, errorMsg, L"卸载失败", MB_ICONERROR);
        }
    }

    // 显示卸载结果
    WCHAR resultMsg[512];
    swprintf_s(resultMsg, L"卸载完成！成功: %d 个, 失败: %d 个", successCount, failCount);
    MessageBoxW(hWnd, resultMsg, L"结果", MB_OK);

    // 卸载后更新列表
    UpdateMapList(hWnd);
}

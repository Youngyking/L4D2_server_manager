/*
ר�����ڲ������Ĺ���ʵ��
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
#pragma comment(lib, "ole32.lib")  // �����ļ���ѡ��Ի���

// ��������������Ϳؼ�ID
#define PLUGIN_WINDOW_CLASS L"PluginManagerClass"
#define IDC_AVAILABLE_LIST  1001  // ���ò���б�
#define IDC_INSTALLED_LIST  1002  // �Ѱ�װ����б�
#define IDC_UPLOAD_BTN      1003  // �ϴ������ť
#define IDC_INSTALL_BTN     1005  // ��װ�����ť
#define IDC_UNINSTALL_EDIT  1006  // ж�ز�������
#define IDC_UNINSTALL_BTN   1007  // ж�ز����ť
#define IDC_DELETE_BTN      1009  // ɾ�����ò����ť

// ȫ��SSH������
extern L4D2_SSH_Context* g_ssh_ctx;

// �ַ���ת����������
static WCHAR* CharToWChar_ser(const char* str, WCHAR* out, int out_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, out, out_len);
    return out;
}

static char* WCharToChar_ser(const WCHAR* wstr, char* out, int out_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, out, out_len, NULL, NULL);
    return out;
}

// ���������Ϣ����
LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ���²���б�
void UpdatePluginLists(HWND hWnd);

// �ϴ��������
void OnUploadPlugins(HWND hWnd);

// ��װ�������
void OnInstallPlugin(HWND hWnd);

// ж�ز������
void OnUninstallPlugin(HWND hWnd);

// ɾ�����ò������
void OnDeleteAvailablePlugin(HWND hWnd);

// �ݹ��ϴ�Ŀ¼
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

// ��ȡ�б��й�ѡ����
std::vector<std::wstring> GetCheckedItems(HWND hList) {
    std::vector<std::wstring> checkedItems;
    int itemCount = ListView_GetItemCount(hList);

    for (int i = 0; i < itemCount; ++i) {
        // ������Ƿ񱻹�ѡ
        if (ListView_GetCheckState(hList, i)) {
            WCHAR itemText[256] = { 0 };
            ListView_GetItemText(hList, i, 0, itemText, sizeof(itemText) / sizeof(WCHAR));
            checkedItems.push_back(std::wstring(itemText));
        }
    }

    return checkedItems;
}

// ��������ڴ�������
DWORD WINAPI HandleManagePlugin(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // ��鴰�����Ƿ���ע��
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, PLUGIN_WINDOW_CLASS, &existingClass)) {
        // ��δע�ᣬִ��ע��
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = PluginWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = PLUGIN_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"������ע��ʧ��", L"����", MB_ICONERROR);
            return 0;
        }
    }

    // �������������
    HWND hWnd = CreateWindowExW(
        0,
        PLUGIN_WINDOW_CLASS,
        L"�������",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 550,  // �������ڸ߶�
        hParent,
        NULL,
        hInst,
        NULL
    );

    if (!hWnd) {
        MessageBoxW(hParent, L"���ڴ���ʧ��", L"����", MB_ICONERROR);
        return 0;
    }

    // ��ʾ����
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // ��Ϣѭ��
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// ������������ڿؼ�
void CreatePluginWindowControls(HWND hWnd, HINSTANCE hInst) {
    // 1. ���ò���б�������- ֧�ֹ�ѡ�Ͷ�ѡ
    HWND hAvailableList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        20, 20, 350, 400,
        hWnd, (HMENU)IDC_AVAILABLE_LIST, hInst, NULL
    );
    // ������չ��ʽ��֧�ֹ�ѡ��
    ListView_SetExtendedListViewStyle(hAvailableList, LVS_EX_CHECKBOXES);

    // �����б�ͷ
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 320;  // �п�
    lvc.pszText = const_cast<LPWSTR>(L"���ò��");
    ListView_InsertColumn(hAvailableList, 0, &lvc);

    // 2. �Ѱ�װ����б�������- ֧�ֹ�ѡ�Ͷ�ѡ
    HWND hInstalledList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        430, 20, 350, 400,
        hWnd, (HMENU)IDC_INSTALLED_LIST, hInst, NULL
    );
    // ������չ��ʽ��֧�ֹ�ѡ��
    ListView_SetExtendedListViewStyle(hInstalledList, LVS_EX_CHECKBOXES);

    lvc.pszText = const_cast<LPWSTR>(L"�Ѱ�װ���");
    ListView_InsertColumn(hInstalledList, 0, &lvc);

    // 3. �������ؼ���������
    // �ϴ������ť
    CreateWindowW(
        L"BUTTON", L"�ϴ����",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 450, 150, 30,
        hWnd, (HMENU)IDC_UPLOAD_BTN, hInst, NULL
    );

    // ��װ�����ť���Ƴ�����򣬵���λ�ã�
    CreateWindowW(
        L"BUTTON", L"��װѡ�в��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        200, 450, 150, 30,
        hWnd, (HMENU)IDC_INSTALL_BTN, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"ж��ѡ�в��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_BTN, hInst, NULL
    );

    // ɾ�����ò����ť���Ƴ�����򣬵���λ�ã�
    CreateWindowW(
        L"BUTTON", L"ɾ��ѡ�п��ò��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        610, 450, 150, 30,
        hWnd, (HMENU)IDC_DELETE_BTN, hInst, NULL
    );
}

// ���������Ϣ����
LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // ��ʼ��ͨ�ÿؼ�
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // �����ؼ�
        CreatePluginWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // ��ʼ���ز���б�
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

// ���²���б�����get_plugins��������
void UpdatePluginLists(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_plugins";

    // ִ��Զ�������ȡ����б�
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
        MessageBoxW(hWnd, err_w, L"��ȡ����б�ʧ��", MB_ICONERROR);
        return;
    }

    // ����JSON�����ʹ��cJSON�⣩
    std::vector<std::string> available_plugins;
    std::vector<std::string> installed_plugins;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON����ʧ��", L"����", MB_ICONERROR);
        return;
    }

    // �������ò���б�
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

    // �����Ѱ�װ����б�
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

    // �ͷ�JSON����
    cJSON_Delete(root);

    // ���¿��ò���б�
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

    // �����Ѱ�װ����б�
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

// �ϴ�������������޸�Ϊ�û�ѡ�񵥸��ļ��У�
void OnUploadPlugins(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ��Ŀ��Ŀ¼�µ�resource�ļ���·��
    WCHAR local_res_path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, local_res_path, MAX_PATH)) {
        MessageBoxW(hWnd, L"��ȡ����·��ʧ��", L"����", MB_ICONERROR);
        return;
    }
    PathRemoveFileSpecW(local_res_path);  // �Ƴ���ִ���ļ�������ȡ��������Ŀ¼
    PathAppendW(local_res_path, L"resource");  // ƴ��resourceĿ¼

    // ���resourceĿ¼�Ƿ����
    if (!PathIsDirectoryW(local_res_path)) {
        MessageBoxW(hWnd, L"����resourceĿ¼������", L"����", MB_ICONERROR);
        return;
    }

    // �ļ���ѡ��Ի�������
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"��ѡ��Ҫ�ϴ��Ĳ���ļ��У�λ��resourceĿ¼�£�";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;  // ֻ��ʾĿ¼��ʹ����UI
    bi.lParam = (LPARAM)local_res_path;  // ���ݳ�ʼĿ¼

    // ���ó�ʼĿ¼�ص�
    bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) -> int {
        if (uMsg == BFFM_INITIALIZED) {
            // ��ʼ����ɺ����ó�ʼĿ¼
            SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
        }
        return 0;
        };

    // ��ʾ�ļ���ѡ��Ի���
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        return;  // �û�ȡ��ѡ��
    }

    // ��ȡѡ����ļ���·��
    WCHAR local_plugin_dir[MAX_PATH];
    if (!SHGetPathFromIDListW(pidl, local_plugin_dir)) {
        MessageBoxW(hWnd, L"��ȡ�ļ���·��ʧ��", L"����", MB_ICONERROR);
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);  // �ͷ��ڴ�

    // ��֤ѡ����ļ����Ƿ���resourceĿ¼��
    if (wcsncmp(local_plugin_dir, local_res_path, wcslen(local_res_path)) != 0) {
        MessageBoxW(hWnd, L"��ѡ��resourceĿ¼�µĲ���ļ���", L"��ʾ", MB_OK);
        return;
    }

    // ��ȡ����ļ������ƣ����һ��Ŀ¼����
    WCHAR* plugin_name_w = PathFindFileNameW(local_plugin_dir);
    if (!plugin_name_w || *plugin_name_w == L'\0') {
        MessageBoxW(hWnd, L"��Ч�Ĳ���ļ�������", L"����", MB_ICONERROR);
        return;
    }

    // ת���������Ϊ���ֽ��ַ���
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, sizeof(plugin_name));

    // ��������Ŀ��Ŀ¼��ʹ�����õĸ�·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_dir = remoteRoot + "/Available_Plugins/" + std::string(plugin_name);

    // ������������Ŀ¼
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_dir.c_str());
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"����Զ��Ŀ¼ʧ��", MB_ICONERROR);
        return;
    }

    // �ݹ��ϴ��ļ�������
    if (UploadDirectory(hWnd, local_plugin_dir, remote_dir)) {
        UpdatePluginLists(hWnd);  // �ϴ��ɹ�������б�
        MessageBoxW(hWnd, L"����ϴ��ɹ�", L"��ʾ", MB_OK);
    }
    else {
        MessageBoxW(hWnd, L"����ϴ�ʧ��", L"����", MB_ICONERROR);
    }
}

// �ݹ��ϴ�Ŀ¼����
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
        // ������Ŀ¼������.��..��
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // ����������Ŀ¼·��
            WCHAR local_subdir[MAX_PATH];
            swprintf_s(local_subdir, L"%s\\%s", local_dir, find_data.cFileName);

            // ����Զ����Ŀ¼·��
            char subdir_name[256];
            WCharToChar_ser(find_data.cFileName, subdir_name, sizeof(subdir_name));
            std::string remote_subdir = remote_dir + "/" + subdir_name;

            // ����Զ����Ŀ¼
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_subdir.c_str());
            char err_msg[256] = { 0 };
            if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, nullptr, 0, err_msg, sizeof(err_msg))) {
                result = false;
                continue;
            }

            // �ݹ��ϴ���Ŀ¼
            if (!UploadDirectory(hWnd, local_subdir, remote_subdir)) {
                result = false;
            }
        }
        // �����ļ�
        else if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // ���������ļ�·��
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // ����Զ���ļ�·��
            char filename[256];
            WCharToChar_ser(find_data.cFileName, filename, sizeof(filename));
            std::string remote_file = remote_dir + "/" + filename;

            // ת������·��Ϊ���ֽ�
            char local_path[512];
            WCharToChar_ser(local_file, local_path, sizeof(local_path));

            // �ϴ��ļ�
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
                MessageBoxW(hWnd, err_w, L"�ļ��ϴ�ʧ��", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// ��װ�����������������װ��ѡ�Ĳ����
void OnInstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ�����б��й�ѡ�Ĳ��
    HWND hAvailableList = GetDlgItem(hWnd, IDC_AVAILABLE_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hAvailableList);

    if (checkedPlugins.empty()) {
        MessageBoxW(hWnd, L"���ȹ�ѡҪ��װ�Ĳ��", L"��ʾ", MB_OK);
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();

    // ������װ��ѡ�Ĳ��
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // ת��Ϊ���ֽ�
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // ִ�а�װ����
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
            swprintf_s(msg, L"��װ��� %s ʧ��: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"��װʧ��", MB_ICONWARNING);
        }
    }

    // ��ʾ��װ���
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"��%d��������ɹ���װ%d��", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"��װ���", MB_OK);
    UpdatePluginLists(hWnd);  // ��װ������б�
}

// ж�ز����������֧�ֹ�ѡ���ֶ����룩
void OnUninstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ�Ѱ�װ�б��й�ѡ�Ĳ��
    HWND hInstalledList = GetDlgItem(hWnd, IDC_INSTALLED_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hInstalledList);

    // ���û�й�ѡ�����Դ�������ȡ
    if (checkedPlugins.empty()) {
        WCHAR pluginNameW[256];
        GetWindowTextW(GetDlgItem(hWnd, IDC_UNINSTALL_EDIT), pluginNameW, sizeof(pluginNameW) / sizeof(WCHAR));
        if (wcscmp(pluginNameW, L"") != 0) {
            checkedPlugins.push_back(std::wstring(pluginNameW));
        }
        else {
            MessageBoxW(hWnd, L"���ȹ�ѡ������Ҫж�صĲ��", L"��ʾ", MB_OK);
            return;
        }
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();

    // ����ж�ع�ѡ�Ĳ��
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // ת��Ϊ���ֽ�
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // ִ��ж������
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
            swprintf_s(msg, L"ж�ز�� %s ʧ��: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"ж��ʧ��", MB_ICONWARNING);
        }
    }

    // ��ʾж�ؽ��
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"��%d��������ɹ�ж��%d��", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"ж�����", MB_OK);
    UpdatePluginLists(hWnd);  // ж�غ�����б�
}

// ɾ�����ò��������������ɾ����ѡ�Ĳ����
void OnDeleteAvailablePlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ�����б��й�ѡ�Ĳ��
    HWND hAvailableList = GetDlgItem(hWnd, IDC_AVAILABLE_LIST);
    std::vector<std::wstring> checkedPlugins = GetCheckedItems(hAvailableList);

    if (checkedPlugins.empty()) {
        MessageBoxW(hWnd, L"���ȹ�ѡҪɾ���Ŀ��ò��", L"��ʾ", MB_OK);
        return;
    }

    // ȷ��ɾ��
    WCHAR confirmMsg[256];
    swprintf_s(confirmMsg, L"ȷ��Ҫɾ��ѡ�е�%d�������", checkedPlugins.size());
    if (MessageBoxW(hWnd, confirmMsg, L"ȷ��ɾ��", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();

    // ����ɾ����ѡ�Ĳ��
    int successCount = 0;
    for (const auto& pluginW : checkedPlugins) {
        // ת��Ϊ���ֽ�
        char pluginName[256];
        WCharToChar_ser(pluginW.c_str(), pluginName, sizeof(pluginName));

        // ִ��ɾ������
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
            swprintf_s(msg, L"ɾ����� %s ʧ��: %s", pluginNameW, errW);
            MessageBoxW(hWnd, msg, L"ɾ��ʧ��", MB_ICONWARNING);
        }
    }

    // ��ʾɾ�����
    WCHAR resultMsg[256];
    swprintf_s(resultMsg, L"��%d��������ɹ�ɾ��%d��", checkedPlugins.size(), successCount);
    MessageBoxW(hWnd, resultMsg, L"ɾ�����", MB_OK);
    UpdatePluginLists(hWnd);  // ɾ��������б�
}

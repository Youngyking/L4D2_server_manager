/*
ר�����ڲ������Ĺ���ʵ��
*/

#include "ssh.h"
#include "manager.h"
#include "plugin_manager.h"
#include <shlwapi.h>
#include <vector>
#include <string>


#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

// ��������������Ϳؼ�ID
#define PLUGIN_WINDOW_CLASS L"PluginManagerClass"
#define IDC_AVAILABLE_LIST  1001  // ���ò���б�
#define IDC_INSTALLED_LIST  1002  // �Ѱ�װ����б�
#define IDC_UPLOAD_BTN      1003  // �ϴ������ť
#define IDC_INSTALL_EDIT    1004  // ��װ��������
#define IDC_INSTALL_BTN     1005  // ��װ�����ť
#define IDC_UNINSTALL_EDIT  1006  // ж�ز�������
#define IDC_UNINSTALL_BTN   1007  // ж�ز����ť

// ȫ��SSH�����ģ�����������ڴ��ݣ�
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

// �ݹ��ϴ�Ŀ¼
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

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
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
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
    // 1. ���ò���б�������
    HWND hAvailableList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        20, 20, 350, 400,
        hWnd, (HMENU)IDC_AVAILABLE_LIST, hInst, NULL
    );

    // �����б�ͷ
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 320;  // �п�
    lvc.pszText = const_cast<LPWSTR>(L"���ò��");
    ListView_InsertColumn(hAvailableList, 0, &lvc);

    // 2. �Ѱ�װ����б�������
    HWND hInstalledList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        430, 20, 350, 400,
        hWnd, (HMENU)IDC_INSTALLED_LIST, hInst, NULL
    );

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

    // ��װ��������Ͱ�ť
    CreateWindowW(
        L"EDIT", L"��������",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        200, 450, 150, 30,
        hWnd, (HMENU)IDC_INSTALL_EDIT, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"��װ���",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        370, 450, 100, 30,
        hWnd, (HMENU)IDC_INSTALL_BTN, hInst, NULL
    );

    // ж�ز�������Ͱ�ť
    CreateWindowW(
        L"EDIT", L"��������",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        500, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_EDIT, hInst, NULL
    );

    CreateWindowW(
        L"BUTTON", L"ж�ز��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 450, 100, 30,
        hWnd, (HMENU)IDC_UNINSTALL_BTN, hInst, NULL
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

    // ִ��Զ�������ȡ����б�
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
        MessageBoxW(hWnd, err_w, L"��ȡ����б�ʧ��", MB_ICONERROR);
        return;
    }

    // ����JSON������򻯰������ʵ����Ŀ����ʹ��JSON�⣩
    std::vector<std::string> available_plugins;
    std::vector<std::string> installed_plugins;
    std::string json(output);

    // ��ȡ���ò���б�
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

    // ��ȡ�Ѱ�װ����б�
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

// �ϴ����������
void OnUploadPlugins(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ����resourceĿ¼·������Ŀ��Ŀ¼�µ�resource�ļ��У�
    WCHAR local_res_path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, local_res_path, MAX_PATH)) {
        MessageBoxW(hWnd, L"��ȡ����·��ʧ��", L"����", MB_ICONERROR);
        return;
    }
    PathRemoveFileSpecW(local_res_path);  // �Ƴ���ִ���ļ���
    PathAppendW(local_res_path, L"resource");  // ƴ��resourceĿ¼

    // ���Ŀ¼�Ƿ����
    if (!PathIsDirectoryW(local_res_path)) {
        MessageBoxW(hWnd, L"����resourceĿ¼������", L"����", MB_ICONERROR);
        return;
    }

    // ����resourceĿ¼�µ����в���ļ���
    WIN32_FIND_DATAW find_data;
    WCHAR search_path[MAX_PATH];
    swprintf_s(search_path, L"%s\\*", local_res_path);
    HANDLE hFind = FindFirstFileW(search_path, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBoxW(hWnd, L"�������Ŀ¼ʧ��", L"����", MB_ICONERROR);
        return;
    }

    do {
        // ֻ����Ŀ¼������.��..��
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // ������ƣ��ļ�������
            char plugin_name[256];
            WCharToChar_ser(find_data.cFileName, plugin_name, 256);

            // Զ�̲��Ŀ¼
            std::string remote_dir = "/home/L4D2_Manager/Available_Plugins/" + std::string(plugin_name);

            // ����Զ��Ŀ¼
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_dir.c_str());
            char err_msg[256] = { 0 };
            if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, nullptr, 0, err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, 256);
                MessageBoxW(hWnd, err_w, L"����Զ��Ŀ¼ʧ��", MB_ICONERROR);
                continue;
            }

            // ���ز��Ŀ¼
            WCHAR local_plugin_dir[MAX_PATH];
            swprintf_s(local_plugin_dir, L"%s\\%s", local_res_path, find_data.cFileName);

            // �ݹ��ϴ�Ŀ¼����
            if (!UploadDirectory(hWnd, local_plugin_dir, remote_dir)) {
                WCHAR msg[512];
                swprintf_s(msg, L"��� %s �ϴ�ʧ��", find_data.cFileName);
                MessageBoxW(hWnd, msg, L"����", MB_ICONWARNING);
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    UpdatePluginLists(hWnd);  // �ϴ���ɺ�����б�
    MessageBoxW(hWnd, L"����ϴ����", L"��ʾ", MB_OK);
}

// �ݹ��ϴ�Ŀ¼����
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
        // ������Ŀ¼
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {

            // ��Ŀ¼·��
            WCHAR local_subdir[MAX_PATH];
            swprintf_s(local_subdir, L"%s\\%s", local_dir, find_data.cFileName);

            // Զ����Ŀ¼·��
            char subdir_name[256];
            WCharToChar_ser(find_data.cFileName, subdir_name, 256);
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
            // �����ļ�·��
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // Զ���ļ�·��
            char filename[256];
            WCharToChar_ser(find_data.cFileName, filename, 256);
            std::string remote_file = remote_dir + "/" + filename;

            // ת������·��Ϊ���ֽ�
            char local_path[512];
            WCharToChar_ser(local_file, local_path, 512);

            // �ϴ��ļ�
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                CharToWChar_ser(err_msg, err_w, 256);
                MessageBoxW(hWnd, err_w, L"�ļ��ϴ�ʧ��", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// ��װ���������
void OnInstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ����Ĳ����
    WCHAR plugin_name_w[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_INSTALL_EDIT), plugin_name_w, 256);
    if (wcscmp(plugin_name_w, L"") == 0) {
        MessageBoxW(hWnd, L"������������", L"��ʾ", MB_OK);
        return;
    }

    // ת��Ϊ���ֽ�
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, 256);

    // ִ�а�װ����
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh install_plugin %s", plugin_name);
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

    if (success) {
        MessageBoxW(hWnd, L"�����װ�ɹ�", L"��ʾ", MB_OK);
        UpdatePluginLists(hWnd);  // �����б�
    }
    else {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, 256);
        MessageBoxW(hWnd, err_w, L"��װʧ��", MB_ICONERROR);
    }
}

// ж�ز��������
void OnUninstallPlugin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ����Ĳ����
    WCHAR plugin_name_w[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_UNINSTALL_EDIT), plugin_name_w, 256);
    if (wcscmp(plugin_name_w, L"") == 0) {
        MessageBoxW(hWnd, L"������������", L"��ʾ", MB_OK);
        return;
    }

    // ת��Ϊ���ֽ�
    char plugin_name[256];
    WCharToChar_ser(plugin_name_w, plugin_name, 256);

    // ִ��ж������
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh uninstall_plugin %s", plugin_name);
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

    if (success) {
        MessageBoxW(hWnd, L"���ж�سɹ�", L"��ʾ", MB_OK);
        UpdatePluginLists(hWnd);  // �����б�
    }
    else {
        WCHAR err_w[256];
        CharToWChar_ser(err_msg, err_w, 256);
        MessageBoxW(hWnd, err_w, L"ж��ʧ��", MB_ICONERROR);
    }
}
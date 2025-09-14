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

// ȫ��SSH������
extern L4D2_SSH_Context* g_ssh_ctx;

// ��ͼ������Ϣ����
LRESULT CALLBACK MapWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ���µ�ͼ�б�
void UpdateMapList(HWND hWnd);

// �ϴ���ͼ����
void OnUploadMap(HWND hWnd);

// ж�ص�ͼ����
void OnUninstallMap(HWND hWnd);

// �ݹ��ϴ�Ŀ¼
bool UploadDirectory(HWND hWnd, const WCHAR* local_dir, const std::string& remote_dir);

// ��ȡ�б���ѡ�еĵ�ͼ����
std::vector<std::string> GetSelectedMaps(HWND hList);

// ��ͼ�����ڴ�������
DWORD WINAPI HandleManageMaps(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // ��鴰�����Ƿ���ע��
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, MAP_WINDOW_CLASS, &existingClass)) {
        // ��δע�ᣬִ��ע��
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MapWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = MAP_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"������ע��ʧ��", L"����", MB_ICONERROR);
            return 0;
        }
    }

    // ������ͼ������
    HWND hWnd = CreateWindowExW(
        0,
        MAP_WINDOW_CLASS,
        L"��ͼ����",
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

// ������ͼ�����ڿؼ�
void CreateMapWindowControls(HWND hWnd, HINSTANCE hInst) {
    // 1. �Ѱ�װ��ͼ�б� - �޸�Ϊ֧�ֶ�ѡ
    HWND hInstalledMapsList = CreateWindowW(
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EX_CHECKBOXES,
        20, 20, 740, 400,
        hWnd, (HMENU)IDC_INSTALLED_MAPS_LIST, hInst, NULL
    );

    // �����б���չ��ʽ��֧�ָ�ѡ��
    ListView_SetExtendedListViewStyle(hInstalledMapsList, LVS_EX_CHECKBOXES);

    // �����б�ͷ
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 700;  // �п�
    lvc.pszText = const_cast<LPWSTR>(L"�Ѱ�װ��ͼ");
    ListView_InsertColumn(hInstalledMapsList, 0, &lvc);

    // 2. �������ؼ�
    // ˢ�µ�ͼ��ť
    CreateWindowW(
        L"BUTTON", L"ˢ���Ѱ�װ��ͼ",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 450, 150, 30,
        hWnd, (HMENU)IDC_REFRESH_MAPS_BTN, hInst, NULL
    );

    // �ϴ���ͼ��ť
    CreateWindowW(
        L"BUTTON", L"�ϴ���ͼ�ļ���",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        190, 450, 150, 30,
        hWnd, (HMENU)IDC_UPLOAD_MAP_BTN, hInst, NULL
    );

    // ж�ص�ͼ��ť - ����λ�ã��Ƴ��ı���
    CreateWindowW(
        L"BUTTON", L"ж��ѡ�е�ͼ",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 450, 150, 30,
        hWnd, (HMENU)IDC_UNINSTALL_MAP_BTN, hInst, NULL
    );
}

// ��ͼ������Ϣ����
LRESULT CALLBACK MapWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // ��ʼ��ͨ�ÿؼ�
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // �����ؼ�
        CreateMapWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // ��ʼ���ص�ͼ�б�
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

// ���µ�ͼ�б�����list_maps��������
void UpdateMapList(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh list_maps";

    // ִ��Զ�������ȡ��ͼ�б�
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
        MessageBoxW(hWnd, err_w, L"��ȡ��ͼ�б�ʧ��", MB_ICONERROR);
        return;
    }

    // ����JSON���
    std::vector<std::string> installed_maps;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON����ʧ��", L"����", MB_ICONERROR);
        return;
    }

    // �����Ѱ�װ��ͼ�б�
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

    // �ͷ�JSON����
    cJSON_Delete(root);

    // �����Ѱ�װ��ͼ�б�
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

// ��ȡ�б���ѡ�еĵ�ͼ����
std::vector<std::string> GetSelectedMaps(HWND hList) {
    std::vector<std::string> selectedMaps;
    int itemCount = ListView_GetItemCount(hList);

    for (int i = 0; i < itemCount; i++) {
        // �����Ŀ�Ƿ񱻹�ѡ
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

// �ϴ���ͼ������
void OnUploadMap(HWND hWnd) {
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

    // ���resourceĿ¼�Ƿ���ڣ��������򴴽�
    if (!PathIsDirectoryW(local_res_path)) {
        CreateDirectoryW(local_res_path, NULL);
        if (!PathIsDirectoryW(local_res_path)) {
            MessageBoxW(hWnd, L"����resourceĿ¼���������޷�����", L"����", MB_ICONERROR);
            return;
        }
    }

    // �ļ���ѡ��Ի�������
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"��ѡ��Ҫ�ϴ��ĵ�ͼ�ļ��У�λ��resourceĿ¼�£�";
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
    WCHAR local_map_dir[MAX_PATH];
    if (!SHGetPathFromIDListW(pidl, local_map_dir)) {
        MessageBoxW(hWnd, L"��ȡ�ļ���·��ʧ��", L"����", MB_ICONERROR);
        CoTaskMemFree(pidl);
        return;
    }
    CoTaskMemFree(pidl);  // �ͷ��ڴ�

    // ��֤ѡ����ļ����Ƿ���resourceĿ¼��
    if (wcsncmp(local_map_dir, local_res_path, wcslen(local_res_path)) != 0) {
        MessageBoxW(hWnd, L"��ѡ��resourceĿ¼�µĵ�ͼ�ļ���", L"��ʾ", MB_OK);
        return;
    }

    // ��ȡ��ͼ�ļ������ƣ����һ��Ŀ¼����
    WCHAR* map_name_w = PathFindFileNameW(local_map_dir);
    if (!map_name_w || *map_name_w == L'\0') {
        MessageBoxW(hWnd, L"��Ч�ĵ�ͼ�ļ�������", L"����", MB_ICONERROR);
        return;
    }

    // ת����ͼ����Ϊ���ֽ��ַ���
    char map_name[256];
    U16toGBK(map_name_w, map_name, sizeof(map_name));

    // ����������ʱĿ¼ - ʹ�����õĸ�·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_tmp_dir = remoteRoot + "/map_tmp/" + std::string(map_name);

    // ��������������ʱĿ¼
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", remote_tmp_dir.c_str());
    char output[4096] = { 0 };
    char err_msg[256] = { 0 };
    if (!l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
        WCHAR err_w[256];
        GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        MessageBoxW(hWnd, err_w, L"����Զ����ʱĿ¼ʧ��", MB_ICONERROR);
        return;
    }

    // �ݹ��ϴ��ļ�������
    if (UploadDirectory(hWnd, local_map_dir, remote_tmp_dir)) {
        // �ϴ��ɹ��󣬵��ýű������ͼ��װ
        std::string installCmd = remoteRoot + "/L4D2_Manager_API.sh install_map " + std::string(map_name);
        snprintf(cmd, sizeof(cmd), "%s", installCmd.c_str());
        if (l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg))) {
            UpdateMapList(hWnd);  // ���µ�ͼ�б�
            MessageBoxW(hWnd, L"��ͼ�ϴ�����װ�ɹ�", L"��ʾ", MB_OK);
        }
        else {
            WCHAR err_w[256];
            GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            MessageBoxW(hWnd, err_w, L"��ͼ��װʧ��", MB_ICONERROR);
        }
    }
    else {
        MessageBoxW(hWnd, L"��ͼ�ļ��ϴ�ʧ��", L"����", MB_ICONERROR);
    }
}

// �ݹ��ϴ�Ŀ¼���ݣ��������ļ���������Ŀ¼�еģ��ϴ���ͬһ��Զ��Ŀ¼��������Ŀ¼�ṹ
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

            // �ݹ��ϴ���Ŀ¼����Զ��Ŀ¼��ʹ��ͬһ����Ŀ¼
            if (!UploadDirectory(hWnd, local_subdir, remote_dir)) {
                result = false;
            }
        }
        // �����ļ�
        else if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // ���������ļ�·��
            WCHAR local_file[MAX_PATH];
            swprintf_s(local_file, L"%s\\%s", local_dir, find_data.cFileName);

            // ����Զ���ļ�·�� - �����ļ���ֱ�ӷŵ�remote_dir��
            char filename[256];
            U16toGBK(find_data.cFileName, filename, sizeof(filename));
            std::string remote_file = remote_dir + "/" + filename;

            // ת������·��ΪGBK
            char local_path[512];
            U16toGBK(local_file, local_path, sizeof(local_path));

            // �ϴ��ļ�
            char err_msg[256] = { 0 };
            if (!upload_file_normal(g_ssh_ctx->session, local_path, remote_file.c_str(), err_msg, sizeof(err_msg))) {
                WCHAR err_w[256];
                GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
                MessageBoxW(hWnd, err_w, L"�ļ��ϴ�ʧ��", MB_ICONWARNING);
                result = false;
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return result;
}

// ж�ص�ͼ������
void OnUninstallMap(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ�Ѱ�װ��ͼ�б�ؼ�
    HWND hInstalledList = GetDlgItem(hWnd, IDC_INSTALLED_MAPS_LIST);

    // ��ȡ����ѡ�еĵ�ͼ
    std::vector<std::string> selectedMaps = GetSelectedMaps(hInstalledList);

    if (selectedMaps.empty()) {
        MessageBoxW(hWnd, L"���ȹ�ѡҪж�صĵ�ͼ", L"��ʾ", MB_OK);
        return;
    }

    // ȷ��ж��
    WCHAR confirmMsg[512];
    swprintf_s(confirmMsg, L"ȷ��Ҫж��ѡ�е� %d ����ͼ��", selectedMaps.size());
    if (MessageBoxW(hWnd, confirmMsg, L"ȷ��ж��", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    // ��¼ж�سɹ���ʧ�ܵ�����
    int successCount = 0;
    int failCount = 0;

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();

    // ���ж��ѡ�еĵ�ͼ
    for (const std::string& mapName : selectedMaps) {
        // ִ��ж������
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
            swprintf_s(errorMsg, L"ж�ص�ͼ %s ʧ��:\n%s", mapName.c_str(), err_w);
            MessageBoxW(hWnd, errorMsg, L"ж��ʧ��", MB_ICONERROR);
        }
    }

    // ��ʾж�ؽ��
    WCHAR resultMsg[512];
    swprintf_s(resultMsg, L"ж����ɣ��ɹ�: %d ��, ʧ��: %d ��", successCount, failCount);
    MessageBoxW(hWnd, resultMsg, L"���", MB_OK);

    // ж�غ�����б�
    UpdateMapList(hWnd);
}

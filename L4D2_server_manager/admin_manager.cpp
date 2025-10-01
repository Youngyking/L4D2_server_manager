/*
ר�����ڹ���Ա����Ĺ���ʵ��
*/
#define _WIN32_WINNT 0x0501  // �������֧�ֵ�Windows�汾
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

// ����Ա�����������Ϳؼ�ID
#define ADMIN_WINDOW_CLASS L"AdminManagerClass"
#define IDC_ADMIN_LIST      3001  // ����Ա�б�
#define IDC_PERMISSION_LABEL 3002 // Ȩ�ޱ�ǩ
#define IDC_STEAMID_EDIT    3003  // SteamID/�û��������
#define IDC_ADD_BTN         3004  // ��ӹ���Ա��ť
#define IDC_REMOVE_BTN      3005  // ɾ������Ա��ť
#define IDC_REFRESH_BTN     3006  // ˢ���б�ť

// Ȩ�޸�ѡ��ID
#define IDC_PERM_0          3100  // ����Ȩ��
#define IDC_PERM_1          3101  // ����Ա�˵�
#define IDC_PERM_2          3102  // ����Ȩ��
#define IDC_PERM_3          3103  // ���Ȩ��
#define IDC_PERM_4          3104  // ��ͼ����
#define IDC_PERM_5          3105  // ����������
#define IDC_PERM_6          3106  // �������
#define IDC_PERM_7          3107  // ����Ȩ��

// ȫ��SSH������
extern L4D2_SSH_Context* g_ssh_ctx;

// ����Ա������Ϣ����
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ��������Ա���ڿؼ�
void CreateAdminWindowControls(HWND hWnd, HINSTANCE hInst);

// ���¹���Ա�б�
void UpdateAdminList(HWND hWnd);

// ��ȡѡ�еĹ���Ա
std::wstring GetSelectedAdmin(HWND hList);

// ��ȡѡ�е�Ȩ��λ
std::string GetSelectedPermissions(HWND hWnd);

// ��ӹ���Ա����
void OnAddAdmin(HWND hWnd);

// ɾ������Ա����
void OnRemoveAdmin(HWND hWnd);

//�õ�ѡ�еĹ���Ա�洢��vector����ɾ��
std::vector<std::wstring> GetAllSelectedAdmins(HWND hListBox);

// ����Ա�����ڴ�������
DWORD WINAPI HandleManageAdmin(LPVOID param) {
    HWND hParent = (HWND)param;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);

    // ��鴰�����Ƿ���ע��
    WNDCLASSEXW existingClass = { 0 };
    existingClass.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoExW(hInst, ADMIN_WINDOW_CLASS, &existingClass)) {
        // ��δע�ᣬִ��ע��
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = AdminWindowProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = ADMIN_WINDOW_CLASS;

        if (!RegisterClassExW(&wc)) {
            MessageBoxW(hParent, L"������ע��ʧ��", L"����", MB_ICONERROR);
            return 0;
        }
    }

    // ��������Ա������
    HWND hWnd = CreateWindowExW(
        0,
        ADMIN_WINDOW_CLASS,
        L"����������Ա����",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 550,
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

// ��������Ա���ڿؼ�
void CreateAdminWindowControls(HWND hWnd, HINSTANCE hInst) {
    // �ϲ�����ࣺ����Ա�б�
    CreateWindowW(
        L"STATIC", L"��ǰ����Ա�б�",
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
    // ������չ��ʽ��֧�ֹ�ѡ��
    ListView_SetExtendedListViewStyle(hAdminList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    // �����б�ͷ
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;

    lvc.cx = 180;  // ��һ�п�
    lvc.pszText = const_cast<LPWSTR>(L"SteamID/�û���");
    ListView_InsertColumn(hAdminList, 0, &lvc);

    lvc.cx = 160;  // �ڶ��п�
    lvc.pszText = const_cast<LPWSTR>(L"Ȩ��");
    ListView_InsertColumn(hAdminList, 1, &lvc);

    // �ϲ����ҲࣺȨ��ѡ��
    CreateWindowW(
        L"STATIC", L"����ԱȨ��",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        430, 20, 350, 20,
        hWnd, (HMENU)IDC_PERMISSION_LABEL, hInst, NULL
    );

    // Ȩ�޸�ѡ�� (8��)
    const WCHAR* permTexts[8] = {
        L"����Ȩ�� (z) - ���Ȩ��",
        L"����Ա�˵� (a) - ʹ�ù���Ա�˵�",
        L"����Ȩ�� (b) - �߳����",
        L"���Ȩ�� (c) - ������",
        L"��ͼ���� (d) - ���ĵ�ͼ",
        L"���������� (e) - �޸ķ���������",
        L"������� (f) - ������",
        L"����Ȩ�� (h) - ʹ����������"
    };

    for (int i = 0; i < 8; i++) {
        CreateWindowW(
            L"BUTTON", permTexts[i],
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            450, 50 + (i * 40), 320, 25,
            hWnd, (HMENU)(IDC_PERM_0 + i), hInst, NULL
        );
    }

    // �²��֣�������
    // SteamID/�û��������
    CreateWindowW(
        L"STATIC", L"SteamID/�û���:",
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

    // ��ӹ���Ա��ť
    CreateWindowW(
        L"BUTTON", L"��ӹ���Ա",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 460, 150, 30,
        hWnd, (HMENU)IDC_ADD_BTN, hInst, NULL
    );

    // ɾ������Ա��ť
    CreateWindowW(
        L"BUTTON", L"ɾ��ѡ�й���Ա",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        190, 460, 180, 30,
        hWnd, (HMENU)IDC_REMOVE_BTN, hInst, NULL
    );

    // ˢ�°�ť
    CreateWindowW(
        L"BUTTON", L"ˢ���б�",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        630, 460, 150, 30,
        hWnd, (HMENU)IDC_REFRESH_BTN, hInst, NULL
    );
}

// ����Ա������Ϣ����
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // ��ʼ��ͨ�ÿؼ�
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // �����ؼ�
        CreateAdminWindowControls(hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));
        // ��ʼ���ع���Ա�б�
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

// ���¹���Ա�б�
void UpdateAdminList(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_admin";

    // ִ��Զ�������ȡ����Ա�б�
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
        MessageBoxW(hWnd, err_w, L"��ȡ����Ա�б�ʧ��", MB_ICONERROR);
        return;
    }

    // ����JSON���
    std::vector<std::pair<std::string, std::string>> admins;

    cJSON* root = cJSON_Parse(output);
    if (!root) {
        MessageBoxW(hWnd, L"JSON����ʧ��", L"����", MB_ICONERROR);
        return;
    }

    // ��������Ա����
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

    // �ͷ�JSON����
    cJSON_Delete(root);

    // ���¹���Ա�б�
    HWND hAdminList = GetDlgItem(hWnd, IDC_ADMIN_LIST);
    ListView_DeleteAllItems(hAdminList);

    for (size_t i = 0; i < admins.size(); ++i) {
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;

        // ����Ա����/SteamID
        WCHAR name[256];
        item.pszText = GBKtoU16(admins[i].first.c_str(), name, 256);
        ListView_InsertItem(hAdminList, &item);

        // ����ԱȨ��
        WCHAR flags[256];
        ListView_SetItemText(hAdminList, i, 1, GBKtoU16(admins[i].second.c_str(), flags, 256));
    }
}

// ��ȡѡ�еĹ���Ա
std::wstring GetSelectedAdmin(HWND hList) {
    int selected = -1;
    int itemCount = ListView_GetItemCount(hList);

    // ���ҹ�ѡ����
    for (int i = 0; i < itemCount; ++i) {
        if (ListView_GetCheckState(hList, i)) {
            selected = i;
            break;
        }
    }

    // ���û�й�ѡ�������ѡ�е���
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

// ��ȡѡ�е�Ȩ��λ����ȫ�Ƴ�BS_CHECKED������
std::string GetSelectedPermissions(HWND hWnd) {
    char bits[9] = { 0 }; // 8λȨ�� + ��ֹ��

    for (int i = 0; i < 8; i++) {
        HWND hCheck = GetDlgItem(hWnd, IDC_PERM_0 + i);
        // ʹ�ð�ť��Ϣ��ȡ״̬�����BS_CHECKED��
        // BM_GETCHECK��Ϣ����ֵ��0=δѡ�У�1=ѡ��
        LRESULT checkState = SendMessageW(hCheck, BM_GETCHECK, 0, 0);
        bits[i] = (checkState != 0) ? '1' : '0';
    }

    bits[8] = '\0';
    return std::string(bits);
}

// ��ӹ���Ա������
void OnAddAdmin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡSteamID/�û���
    WCHAR steamIdW[256];
    GetWindowTextW(GetDlgItem(hWnd, IDC_STEAMID_EDIT), steamIdW, sizeof(steamIdW) / sizeof(WCHAR));

    if (wcscmp(steamIdW, L"") == 0) {
        MessageBoxW(hWnd, L"������SteamID���û���", L"��ʾ", MB_OK);
        return;
    }

    // ��ȡѡ�е�Ȩ��
    std::string permissions = GetSelectedPermissions(hWnd);

    // ת��Ϊ���ֽ�
    char steamId[256];
    U16toGBK(steamIdW, steamId, sizeof(steamId));

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();

    // ִ����ӹ���Ա����
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
        // ��������
        SetWindowTextW(GetDlgItem(hWnd, IDC_STEAMID_EDIT), L"");
        // ȡ������Ȩ�޹�ѡ
        for (int i = 0; i < 8; i++) {
            HWND hCheck = GetDlgItem(hWnd, IDC_PERM_0 + i);
            SendMessageW(hCheck, BM_SETCHECK, 0, 0); // 0��ʾδѡ��
        }
        // �����б�
        UpdateAdminList(hWnd);
        WCHAR msg[512];
        swprintf_s(msg, L"��ӹ���Ա %s �ɹ�: %s", steamIdW, errW);
        MessageBoxW(hWnd, msg, L"��ӳɹ�", MB_OK);
    }
    else {
        WCHAR errW[256], steamIdW[256];
        GBKtoU16(err_msg, errW, sizeof(errW) / sizeof(WCHAR));
        GBKtoU16(steamId, steamIdW, sizeof(steamIdW) / sizeof(WCHAR));

        WCHAR msg[512];
        swprintf_s(msg, L"��ӹ���Ա %s ʧ��: %s", steamIdW, errW);
        MessageBoxW(hWnd, msg, L"���ʧ��", MB_ICONWARNING);
    }
}

// ɾ������Ա��������֧������ɾ����
void OnRemoveAdmin(HWND hWnd) {
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        MessageBoxW(hWnd, L"δ���ӵ�������", L"����", MB_ICONERROR);
        return;
    }

    // ��ȡ����Ա�б�ؼ�
    HWND hAdminList = GetDlgItem(hWnd, IDC_ADMIN_LIST);

    // ��ȡ����ѡ�еĹ���Ա���޸�Ϊ���ض��ѡ���
    std::vector<std::wstring> selectedAdmins = GetAllSelectedAdmins(hAdminList);

    if (selectedAdmins.empty()) {
        MessageBoxW(hWnd, L"���ȹ�ѡ��ѡ��Ҫɾ���Ĺ���Ա", L"��ʾ", MB_OK);
        return;
    }

    // ����ȷ����ʾ
    WCHAR confirmMsg[512];
    swprintf_s(confirmMsg, L"ȷ��Ҫɾ������ %zu ������Ա��\n", selectedAdmins.size());
    for (const auto& admin : selectedAdmins) {
        wcscat_s(confirmMsg, admin.c_str());
        wcscat_s(confirmMsg, L"\n");
    }

    if (MessageBoxW(hWnd, confirmMsg, L"ȷ������ɾ��", MB_YESNO | MB_ICONWARNING) != IDYES) {
        return;
    }

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::wstring successList, failList;

    // ѭ��ɾ��ÿ��ѡ�еĹ���Ա
    for (const auto& adminW : selectedAdmins) {
        // ת��Ϊ���ֽ�
        char admin[256];
        if (U16toGBK(adminW.c_str(), admin, sizeof(admin)) == nullptr) {
            failList += adminW + L"������ת��ʧ�ܣ�\n";
            continue;
        }

        // ����ɾ������
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s/L4D2_Manager_API.sh rm_admin \"%s\"",
            remoteRoot.c_str(), admin);  // ע��������Ŵ��������ַ�

        // ִ��ɾ������
        char output[4096] = { 0 };
        char err_msg[256] = { 0 };
        bool success = l4d2_ssh_exec_command(g_ssh_ctx, cmd, output, sizeof(output), err_msg, sizeof(err_msg));

        // ��¼ִ�н��
        if (success) {
            successList += adminW + L"\n";
        }
        else {
            std::wstring errW;
            GBKtoU16(err_msg, &errW[0], sizeof(err_msg));
            failList += adminW + L"��ʧ�ܣ�" + errW + L"��\n";
        }
    }

    // ���¹���Ա�б�
    UpdateAdminList(hWnd);

    // ��ʾ����������
    WCHAR resultMsg[1024] = L"����ɾ����ɣ�\n";
    if (!successList.empty()) {
        wcscat_s(resultMsg, L"\n�ɹ�ɾ����\n");
        wcscat_s(resultMsg, successList.c_str());
    }
    if (!failList.empty()) {
        wcscat_s(resultMsg, L"\nɾ��ʧ�ܣ�\n");
        wcscat_s(resultMsg, failList.c_str());
    }
    MessageBoxW(hWnd, resultMsg, L"�����������", MB_OK | (failList.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
}

// ������������ȡ���й�ѡ�Ĺ���Ա������ListView�ؼ���
std::vector<std::wstring> GetAllSelectedAdmins(HWND hListView) {
    std::vector<std::wstring> result;
    int itemCount = ListView_GetItemCount(hListView);  // ��ȡListView������

    for (int i = 0; i < itemCount; ++i) {
        // ��鵱ǰ���Ƿ񱻹�ѡ��ListViewר�ú꣩
        if (ListView_GetCheckState(hListView, i)) {
            // ��ȡ��0�е��ı�������Ա����/SteamID��
            WCHAR itemText[256] = { 0 };
            ListView_GetItemText(
                hListView,  // ListView�ؼ����
                i,          // ������
                0,          // ��������0���ǹ���ԱID��
                itemText,   // �����ı��Ļ�����
                sizeof(itemText) / sizeof(WCHAR)  // ��������С
            );

            result.push_back(std::wstring(itemText));
        }
    }
    return result;
}

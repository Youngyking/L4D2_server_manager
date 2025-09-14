/*
    Ϊ�������ļ�L4D2_server_manager.cpp�ṩGUI�����ʵ�֣����𴴽��͹������Ԫ�أ�
    ��Ϊmanager.cpp�ṩ������½ӿ�
*/


#define WIN32_LEAN_AND_MEAN  // ���� Windows ͷ�ļ��е���������
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")  // ���� Winsock 2.0 ��
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

// ��ʼ��ͨ�ÿؼ���ListView��Ҫ��
void InitCommonControlsExWrapper() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;  // ��ʼ��ListView
    InitCommonControlsEx(&icex);
}

// �������н���ؼ�
void CreateAllControls(HWND hWnd, HINSTANCE hInst) {
    // 1. SSH������
    CreateWindowW(L"BUTTON", L"SSH��������",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 980, 80, hWnd, (HMENU)IDC_SSH_GROUP, hInst, NULL);

    // 1.1 IP��ַ����
    CreateWindowW(L"STATIC", L"������IP:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"xxx.xxx.xxx.xxx",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        120, 30, 200, 25, hWnd, (HMENU)IDC_IP_EDIT, hInst, NULL);

    // 1.2 �û�������
    CreateWindowW(L"STATIC", L"�û���:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        350, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"root",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        440, 30, 150, 25, hWnd, (HMENU)IDC_USER_EDIT, hInst, NULL);

    // 1.3 ��������
    CreateWindowW(L"STATIC", L"����:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        620, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
        700, 30, 150, 25, hWnd, (HMENU)IDC_PASS_EDIT, hInst, NULL);

    // 1.4 ���Ӱ�ť
    CreateWindowW(L"BUTTON", L"���ӷ�����",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        880, 30, 100, 25, hWnd, (HMENU)IDC_CONNECT_BTN, hInst, NULL);

    // 1.5 ����״̬
    CreateWindowW(L"STATIC", L"δ����",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 60, 200, 20, hWnd, (HMENU)IDC_CONN_STATUS, hInst, NULL);

    // 2. ϵͳ״̬��壨�����߶�����ȫ�������а�ť��
    CreateWindowW(L"BUTTON", L"ϵͳ״̬ & ���Ĳ���",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 100, 480, 420, hWnd, (HMENU)IDC_STATUS_GROUP, hInst, NULL);

    // 2.1 ״̬��ʾ��5����Ϣ��
    // �������ļ�״̬
    CreateWindowW(L"STATIC", L"�������ļ�:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 130, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"δ����",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 130, 100, 30, hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);

    // SourceMod״̬
    CreateWindowW(L"STATIC", L"SourceMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 200, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"δ��װ",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 200, 100, 30, hWnd, (HMENU)IDC_SM_STATUS, hInst, NULL);

    // MetaMod״̬
    CreateWindowW(L"STATIC", L"MetaMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 270, 100, 30, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"δ��װ",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 270, 100, 30, hWnd, (HMENU)IDC_MM_STATUS, hInst, NULL);

    // 2.2 ������ť
    CreateWindowW(L"BUTTON", L"����/���·�����",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 320, 440, 30, hWnd, (HMENU)IDC_DEPLOY_BTN, hInst, NULL);

    // �ϴ�SourceMod��ť
    CreateWindowW(L"BUTTON", L"�ϴ�SourceMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 370, 440, 30, hWnd, (HMENU)IDC_UPLOAD_SM_BTN, hInst, NULL);

    // �ϴ�MetaMod��ť
    CreateWindowW(L"BUTTON", L"�ϴ�MetaMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 420, 440, 30, hWnd, (HMENU)IDC_UPLOAD_MM_BTN, hInst, NULL);

    // ��װSourceMod��MetaMod��ť
    CreateWindowW(L"BUTTON", L"��װSourceMod��MetaMod",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 470, 440, 30, hWnd, (HMENU)IDC_UPLOAD_SMM_BTN, hInst, NULL);

    // 3. ������ʵ�����
    CreateWindowW(L"BUTTON", L"������ʵ��",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 100, 490, 220, hWnd, (HMENU)IDC_INSTANCE_GROUP, hInst, NULL);

    // 3.1 ʵ���б�ListView��
    HWND hInstanceList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        520, 130, 450, 160, hWnd, (HMENU)IDC_INSTANCE_LIST, hInst, NULL);
    // �����б���
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    // ��0��
    lvc.cx = 60;
    lvc.pszText = const_cast<LPWSTR>(L"״̬");
    ListView_InsertColumn(hInstanceList, 0, &lvc);

    // ��1��
    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"����");
    ListView_InsertColumn(hInstanceList, 1, &lvc);

    // ��2��
    lvc.cx = 80;
    lvc.pszText = const_cast<LPWSTR>(L"�˿�");
    ListView_InsertColumn(hInstanceList, 2, &lvc);

    // ��3��
    lvc.cx = 120;
    lvc.pszText = const_cast<LPWSTR>(L"��ͼ");
    ListView_InsertColumn(hInstanceList, 3, &lvc);

    // ��4��
    lvc.cx = 80;
    lvc.pszText = const_cast<LPWSTR>(L"����");
    ListView_InsertColumn(hInstanceList, 4, &lvc);

    // 3.2 �˿�����Ͳ�����ť
    CreateWindowW(L"STATIC", L"�˿ں�:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        520, 300, 60, 20, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"EDIT", L"27015",  // Ĭ��L4D2�˿�
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        590, 300, 80, 25, hWnd, (HMENU)IDC_PORT_EDIT, hInst, NULL);

    CreateWindowW(L"BUTTON", L"����ʵ��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        690, 300, 100, 25, hWnd, (HMENU)IDC_START_INSTANCE, hInst, NULL);

    CreateWindowW(L"BUTTON", L"ֹͣʵ��",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        810, 300, 100, 25, hWnd, (HMENU)IDC_STOP_INSTANCE, hInst, NULL);

    // 4. ��������־��壨���ڷ�����ʵ������·��������ϵͳ״̬���߶�ƥ�䣩
    CreateWindowW(L"BUTTON", L"������� & ��־",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 330, 490, 210, hWnd, (HMENU)IDC_ACTION_GROUP, hInst, NULL);

    // 4.1 �������ť
    CreateWindowW(L"BUTTON", L"�������",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        520, 360, 140, 30, hWnd, (HMENU)IDC_PLUGIN_BTN, hInst, NULL);

    // 4.2 ��־�鿴��ť
    CreateWindowW(L"BUTTON", L"�����־",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        680, 360, 140, 30, hWnd, (HMENU)IDC_LOG_BTN, hInst, NULL);

    // 4.3 ��ͼ����ť
    CreateWindowW(
        L"BUTTON", L"��ͼ����",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        840, 360, 140, 30,  // λ���ڲ������ť�Ҳ�
        hWnd, (HMENU)IDC_MAP_BTN, hInst, NULL
    );

    // 4.4 ��־��ʾ�򣨶��б༭��
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        520, 400, 450, 120, hWnd, (HMENU)IDC_LOG_VIEW, hInst, NULL);

    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_CHANGE_REMOTE_PATH, L"����Զ����Ŀ����λ��");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"�ļ�");
    SetMenu(hWnd, hMenu);
}

// ��������״̬
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected) {
    HWND hStatus = GetDlgItem(hWnd, IDC_CONN_STATUS);
    SetWindowTextW(hStatus, status);
    // ���ӳɹ�ʱ���ò�����ť
    EnableWindow(GetDlgItem(hWnd, IDC_DEPLOY_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_PLUGIN_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_UPLOAD_SM_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_UPLOAD_MM_BTN), isConnected);
}

// ����ϵͳ״̬���
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR mmStatus) {
    // ���·������ļ�״̬
    SetWindowTextW(GetDlgItem(hWnd, IDC_SERVER_STATUS), serverStatus);
    // ����SourceMod״̬
    SetWindowTextW(GetDlgItem(hWnd, IDC_SM_STATUS), smStatus);
    // ����MetaMod״̬
    SetWindowTextW(GetDlgItem(hWnd, IDC_MM_STATUS), mmStatus);
}

// ����ʵ���б�
void UpdateInstanceList(HWND hWnd, const char* instancesJson) {
    HWND hList = GetDlgItem(hWnd, IDC_INSTANCE_LIST);
    ListView_DeleteAllItems(hList);  // ���������

    if (!instancesJson || *instancesJson == '\0') {
        AddLog(hWnd, L"��ȡʵ���б�ʧ�ܣ���JSON����");
        return;
    }

    // ����JSON������
    cJSON* root = cJSON_Parse(instancesJson);
    if (!root) {
        AddLog(hWnd, L"����ʵ���б�JSONʧ��");
        return;
    }

    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;  // ͳһ�����ı��ֶ�
    int itemIndex = 0;

    // ��������ʵ������
    cJSON* currentInstance = NULL;
    cJSON_ArrayForEach(currentInstance, root) {
        // ��ȡʵ�����ƣ�������
        const char* instanceName = currentInstance->string;
        if (!instanceName) continue;

        // ת��ʵ������Ϊ���ַ���UTF-8��Unicode��
        WCHAR nameW[256] = { 0 };
        GBKtoU16(instanceName, nameW, 256);

        // ��ȡʵ������
        cJSON* port = cJSON_GetObjectItem(currentInstance, "port");
        cJSON* map = cJSON_GetObjectItem(currentInstance, "map");
        cJSON* running = cJSON_GetObjectItem(currentInstance, "running");

        // ��֤��Ҫ�ֶ�
        if (!port || !map || !running || !cJSON_IsString(port) || !cJSON_IsString(map) || !cJSON_IsBool(running)) {
            AddLog(hWnd, L"ʵ�����ݸ�ʽ����������ʵ��");
            continue;
        }

        // ת���˿�Ϊ���ַ���UTF-8��Unicode��
        WCHAR portW[16] = { 0 };
        GBKtoU16(port->valuestring, portW, 16);

        // ת����ͼΪ���ַ���UTF-8��Unicode��
        WCHAR mapW[256] = { 0 };
        GBKtoU16(map->valuestring, mapW, 256);

        // ����running�Ĳ���ֵ����״̬�ı���true/false�жϣ�
        WCHAR statusW[20] = { 0 };
        wcscpy_s(statusW, (running->type == cJSON_True) ? L"������" : L"��ֹͣ");

        // ���ò����ı�
        WCHAR actionW[20] = { 0 };
        wcscpy_s(actionW, (running->type == cJSON_True) ? L"ֹͣ" : L"����");

        // �����б���
        lvi.iItem = itemIndex;
        lvi.pszText = statusW;
        ListView_InsertItem(hList, &lvi);

        // ��������������
        ListView_SetItemText(hList, itemIndex, 1, nameW);       // ʵ������
        ListView_SetItemText(hList, itemIndex, 2, portW);        // �˿�
        ListView_SetItemText(hList, itemIndex, 3, mapW);         // ��ͼ
        ListView_SetItemText(hList, itemIndex, 4, actionW);      // ����

        itemIndex++;
    }

    // ����JSON��Դ
    cJSON_Delete(root);
    AddLog(hWnd, L"ʵ���б��Ѹ���");
}


// �����־����־��
void AddLog(HWND hWnd, LPCWSTR logText) {
    HWND hLog = GetDlgItem(hWnd, IDC_LOG_VIEW);
    int len = GetWindowTextLengthW(hLog);
    SendMessageW(hLog, EM_SETSEL, len, len);  // �ƶ���ĩβ
    WCHAR timeStr[32] = { 0 };
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(timeStr, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)timeStr);  // ���ʱ��
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)logText);  // �����־����
    SendMessageW(hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");  // ����
}

// �����־
void ClearLog(HWND hWnd) {
    SetWindowTextW(GetDlgItem(hWnd, IDC_LOG_VIEW), L"");
}
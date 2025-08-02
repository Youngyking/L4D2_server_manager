// ������ͷ�ļ�����ǰ��ӣ��������stdafx.h �� pch.h �У�
#define WIN32_LEAN_AND_MEAN  // ���� Windows �ɰ�ͷ�ļ��е����ඨ��
#include <winsock2.h>        // ֻ�����°� Winsock 2.0
#include <ws2tcpip.h>        // ����ʹ�ø��ִ������纯������ getaddrinfo�����ɰ������ļ�
#pragma comment(lib, "ws2_32.lib")  // ���� Winsock 2.0 ��
#include "framework.h"
#include "L4D2_server_manager.h"
#include "resource.h"
#include "gui.h"
#include <fcntl.h>
#include <string.h>

// ��ʼ�������ؼ���ListView����Ҫ��
void InitCommonControlsExWrapper() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;  // ��ʼ��ListView
    InitCommonControlsEx(&icex);
}

// �������д��ڿؼ�
void CreateAllControls(HWND hWnd, HINSTANCE hInst) {
    // 1. SSH������
    CreateWindowW(L"BUTTON", L"SSH��������",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 980, 80, hWnd, (HMENU)IDC_SSH_GROUP, hInst, NULL);

    // 1.1 IP�����
    CreateWindowW(L"STATIC", L"������IP:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"192.168.1.1",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        120, 30, 200, 25, hWnd, (HMENU)IDC_IP_EDIT, hInst, NULL);

    // 1.2 �û��������
    CreateWindowW(L"STATIC", L"�û���:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        350, 30, 80, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"EDIT", L"steam",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        440, 30, 150, 25, hWnd, (HMENU)IDC_USER_EDIT, hInst, NULL);

    // 1.3 ���������
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

    // 2. ϵͳ״̬��Ƭ
    CreateWindowW(L"BUTTON", L"ϵͳ״̬ & ���Ĳ���",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 100, 480, 220, hWnd, (HMENU)IDC_STATUS_GROUP, hInst, NULL);

    // 2.1 ״ָ̬�꣨4������
    // �������ļ�״̬
    CreateWindowW(L"STATIC", L"�������ļ�:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 130, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"δ����",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 130, 100, 20, hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);

    // SourceMod״̬
    CreateWindowW(L"STATIC", L"SourceMod:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 170, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"δ��װ",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 170, 100, 20, hWnd, (HMENU)IDC_SM_STATUS, hInst, NULL);

    // ������ʵ����
    CreateWindowW(L"STATIC", L"������ʵ��:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 210, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 210, 100, 20, hWnd, (HMENU)IDC_INSTANCE_COUNT, hInst, NULL);

    // �Ѱ�װ�����
    CreateWindowW(L"STATIC", L"�Ѱ�װ���:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 250, 100, 20, hWnd, NULL, hInst, NULL);
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        140, 250, 100, 20, hWnd, (HMENU)IDC_PLUGIN_COUNT, hInst, NULL);

    // 2.2 ����ť
    CreateWindowW(L"BUTTON", L"����/���·�����",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 280, 440, 30, hWnd, (HMENU)IDC_DEPLOY_BTN, hInst, NULL);

    // 3. ������ʵ����Ƭ
    CreateWindowW(L"BUTTON", L"������ʵ��",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        500, 100, 490, 220, hWnd, (HMENU)IDC_INSTANCE_GROUP, hInst, NULL);

    // 3.1 ʵ���б�ListView��
    HWND hInstanceList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
        520, 130, 450, 160, hWnd, (HMENU)IDC_INSTANCE_LIST, hInst, NULL);
    // ����б���
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    // ��0��
    lvc.cx = 60;
    lvc.pszText = const_cast<LPWSTR>(L"״̬");  // ǿ��ת������Ϊ���޸�ָ��
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

    // 3.2 �˿�������ؿؼ�
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

    // 4. ������ڿ�Ƭ
    CreateWindowW(L"BUTTON", L"������� & ��־",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 330, 980, 200, hWnd, (HMENU)IDC_ACTION_GROUP, hInst, NULL);

    // 4.1 �������ť
    CreateWindowW(L"BUTTON", L"ǰ���������",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        30, 360, 200, 30, hWnd, (HMENU)IDC_PLUGIN_BTN, hInst, NULL);

    // 4.2 ��־�鿴��ť
    CreateWindowW(L"BUTTON", L"�����־",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        250, 360, 200, 30, hWnd, (HMENU)IDC_LOG_BTN, hInst, NULL);

    // 4.3 ��־��ʾ�򣨶��б༭��
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        30, 400, 940, 100, hWnd, (HMENU)IDC_LOG_VIEW, hInst, NULL);
}

// ��������״̬
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected) {
    HWND hStatus = GetDlgItem(hWnd, IDC_CONN_STATUS);
    SetWindowTextW(hStatus, status);
    // ���ӳɹ�ʱ����������ť
    EnableWindow(GetDlgItem(hWnd, IDC_DEPLOY_BTN), isConnected);
    EnableWindow(GetDlgItem(hWnd, IDC_PLUGIN_BTN), isConnected);
}

// ����ϵͳ״̬��Ƭ
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR instanceCount, LPCWSTR pluginCount) {
    // ���·������ļ�״̬
    SetWindowTextW(GetDlgItem(hWnd, IDC_SERVER_STATUS), serverStatus);
    // ����SourceMod״̬
    SetWindowTextW(GetDlgItem(hWnd, IDC_SM_STATUS), smStatus);
    // ����������ʵ������
    SetWindowTextW(GetDlgItem(hWnd, IDC_INSTANCE_COUNT), instanceCount);
    // ���������²������
    SetWindowTextW(GetDlgItem(hWnd, IDC_PLUGIN_COUNT), pluginCount);
}

// ����ʵ���б���ʾ����ʵ�������JSON��
void UpdateInstanceList(HWND hWnd, const char* instancesJson) {
    HWND hList = GetDlgItem(hWnd, IDC_INSTANCE_LIST);
    ListView_DeleteAllItems(hList);  // ���������

    // �˴�Ӧ����JSON������ʹ��cJSON�ȿ⣩��ʾ��ֱ����Ӳ�����
   // ����һ��LVITEMW�����������򸲸����в�����
    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;  // ͳһ����mask��ָ��ʹ���ı��ֶ�

    // ������1
    lvi.iItem = 0;  // �޸�������
    WCHAR status0[20] = L"������";
    lvi.pszText = status0;  // ��ֵ״̬�ı�
    ListView_InsertItem(hList, &lvi);

    // ����������
    ListView_SetItemText(hList, 0, 1, const_cast<LPWSTR>(L"����_ս��"));
    ListView_SetItemText(hList, 0, 2, const_cast<LPWSTR>(L"27015"));
    ListView_SetItemText(hList, 0, 3, const_cast<LPWSTR>(L"c1m1_hotel"));
    ListView_SetItemText(hList, 0, 4, const_cast<LPWSTR>(L"ֹͣ"));


    // ������2������ͬһ��lvi������
    lvi.iItem = 1;  // ���޸�������
    WCHAR status1[20] = L"��ֹͣ";
    lvi.pszText = status1;  // ��ֵ�µ�״̬�ı�
    ListView_InsertItem(hList, &lvi);

    // ����������
    ListView_SetItemText(hList, 1, 1, const_cast<LPWSTR>(L"����_�Կ�"));
    ListView_SetItemText(hList, 1, 2, const_cast<LPWSTR>(L"27016"));
    ListView_SetItemText(hList, 1, 3, const_cast<LPWSTR>(L"c5m1_waterfront"));
    ListView_SetItemText(hList, 1, 4, const_cast<LPWSTR>(L"����"));
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
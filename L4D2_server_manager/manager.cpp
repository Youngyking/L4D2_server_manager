/*
����ssh.cpp�ṩ��ssh��sftp�ӿڷ�װ���ڿؼ��Ĺ���
*/

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include "manager.h"
#include "cJSON.h"
#include "gui.h"
#include "resource.h"
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <string.h>
#include <string>

// ת�����ַ��������ֽ�
static char* WCharToChar(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// ת�����ֽڵ����ַ���
static WCHAR* CharToWChar(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
    return buf;
}

// ����SSH���������̺߳�����
DWORD WINAPI HandleConnectRequest(LPVOID param) {
    HWND hWnd = (HWND)param;
    WCHAR ip_w[64], user_w[64], pass_w[64];
    char ip[64], user[64], pass[64], err_msg[256];

    // ��ȡ���������
    GetWindowTextW(GetDlgItem(hWnd, IDC_IP_EDIT), ip_w, sizeof(ip_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_USER_EDIT), user_w, sizeof(user_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_PASS_EDIT), pass_w, sizeof(pass_w) / sizeof(WCHAR));

    // ת��Ϊ���ֽ�
    WCharToChar(ip_w, ip, sizeof(ip));
    WCharToChar(user_w, user, sizeof(user));
    WCharToChar(pass_w, pass, sizeof(pass));

    // ��ʼ��SSH
    if (g_ssh_ctx) {
        l4d2_ssh_cleanup(g_ssh_ctx);
    }
    g_ssh_ctx = l4d2_ssh_init();
    if (!g_ssh_ctx) {
        AddLog(hWnd, L"��ʼ��SSHʧ��");
        UpdateConnectionStatus(hWnd, L"����ʧ��", FALSE);
        return 0;
    }

    // ���ӷ�����
    AddLog(hWnd, L"�������ӵ�������...");
    bool success = l4d2_ssh_connect(g_ssh_ctx, ip, user, pass, err_msg, sizeof(err_msg));
    WCHAR err_msg_w[256];
    CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
    AddLog(hWnd, err_msg_w);

    if (success) {
        // ���ӳɹ����ϴ�API�ű�
        AddLog(hWnd, L"���ڼ�鲢�ϴ�API�ű�...");
        if (l4d2_ssh_upload_api_script(g_ssh_ctx, err_msg, sizeof(err_msg))) {
            CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
            AddLog(hWnd, err_msg_w);

            // ������ִ����������밲װ
            AddLog(hWnd, L"���ڼ�鲢��װ�ű�����...");
            char dep_output[4096] = { 0 };
            WCHAR output_w[4096];
            bool dep_success = l4d2_ssh_exec_command(
                g_ssh_ctx,
                "bash /home/L4D2_Manager/L4D2_Manager_API.sh check_deps",
                dep_output, sizeof(dep_output),
                err_msg, sizeof(err_msg)
            );
            CharToWChar(dep_output, output_w, sizeof(output_w) / sizeof(WCHAR));
            AddLog(hWnd, output_w);

            if (dep_success) {
                UpdateConnectionStatus(hWnd, L"�����ӣ���������װ�ɹ�", TRUE);
                HandleGetStatus(hWnd);  // ˢ��״̬
                HandleGetInstances(hWnd);
            }
            else {
                CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
                AddLog(hWnd, err_msg_w);
                UpdateConnectionStatus(hWnd, L"�����ӣ���������װʧ��", FALSE);
            }
        }
    }
    else {
        UpdateConnectionStatus(hWnd, L"����ʧ��", FALSE);
    }

    return 0;
}

// ����ʵ��������
DWORD WINAPI HandleStartInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷�����ʵ��");
        return 0;
    }

    // ��ȡ�˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WideCharToMultiByte(CP_ACP, 0, port_w, -1, port, sizeof(port), NULL, NULL);

    AddLog(hWnd, L"���������˿�Ϊ ");
    AddLog(hWnd, port_w);
    AddLog(hWnd, L" ��ʵ��...");

    // ����SSH����
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh start %s", port);

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�����벢�����־
    WCHAR output_w[4096], err_msg_w[256];
    MultiByteToWideChar(CP_ACP, 0, output, -1, output_w, sizeof(output_w) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, err_msg, -1, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"ʵ�������ɹ�");
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        AddLog(hWnd, L"����ʧ��: ");
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// ֹͣʵ��������
DWORD WINAPI HandleStopInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷�ֹͣʵ��");
        return 0;
    }

    // ��ȡ�˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WideCharToMultiByte(CP_ACP, 0, port_w, -1, port, sizeof(port), NULL, NULL);

    AddLog(hWnd, L"����ֹͣ�˿�Ϊ ");
    AddLog(hWnd, port_w);
    AddLog(hWnd, L" ��ʵ��...");

    // ����SSH����
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh stop %s", port);

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�����벢�����־
    WCHAR output_w[4096], err_msg_w[256];
    MultiByteToWideChar(CP_ACP, 0, output, -1, output_w, sizeof(output_w) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, err_msg, -1, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"ʵ��ֹͣ�ɹ�");
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        AddLog(hWnd, L"ֹͣʧ��: ");
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// ����������������̺߳�����
DWORD WINAPI HandleDeployServer(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷�����");
        return 0;
    }

    AddLog(hWnd, L"��ʼ����/���·�����...");
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh deploy_server",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"�������������");
        HandleGetStatus(hWnd);  // ˢ��״̬
    }
    else {
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// �����ȡ������״̬�����̺߳�����
DWORD WINAPI HandleGetStatus(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) return 0;

    char output[1024] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh get_status",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (success) {
        cJSON* root = cJSON_Parse(output);
        if (root) {
            cJSON* serverDeployed = cJSON_GetObjectItem(root, "serverDeployed");
            LPCWSTR serverStatus = (serverDeployed && serverDeployed->type == cJSON_True) ? L"�Ѳ���" : L"δ����";

            cJSON* smInstalled = cJSON_GetObjectItem(root, "smInstalled");
            LPCWSTR smStatus = (smInstalled && smInstalled->type == cJSON_True) ? L"�Ѱ�װ" : L"δ��װ";

            cJSON* instanceCount = cJSON_GetObjectItem(root, "instanceCount");
            int instCount = instanceCount ? instanceCount->valueint : 0;
            WCHAR instCountStr[32];
            swprintf_s(instCountStr, L"%d", instCount);

            cJSON* pluginCount = cJSON_GetObjectItem(root, "pluginCount");
            int plugCount = pluginCount ? pluginCount->valueint : 0;
            WCHAR plugCountStr[32];
            swprintf_s(plugCountStr, L"%d", plugCount);

            UpdateSystemStatus(hWnd, serverStatus, smStatus, instCountStr, plugCountStr);

            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"����ϵͳ״̬JSONʧ��");
        }
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// �����ȡʵ���б������̺߳�����
DWORD WINAPI HandleGetInstances(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) return 0;

    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh get_instances",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    if (success) {
        UpdateInstanceList(hWnd, output);
    }

    return 0;
}
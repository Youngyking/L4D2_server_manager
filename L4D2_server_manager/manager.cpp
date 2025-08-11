/*
����ssh.cpp�ṩ��ssh��sftp�ӿڷ�װ���ڿؼ��Ĺ���
*/


#define WIN32_LEAN_AND_MEAN  // ���� Windows �ɰ�ͷ�ļ��е����ඨ��
#define _CRT_SECURE_NO_WARNINGS //����ʹ��fopen

#include <winsock2.h>
#include <ws2tcpip.h>
#include <commdlg.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comdlg32.lib")
#include "manager.h"
#include "cJSON.h"
#include "gui.h"
#include "resource.h"
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <cmath>
#include <map>

// ȫ�ֶ˿�-ʵ������ӳ�����̬���£�
std::map<std::string, std::string> g_portToInstance;

// ��һ��������ΪUTF-16���룬����ת��ΪGBK���벢����
static char* WCharToChar(LPCWSTR wstr, char* buf, int buf_len) {
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, buf_len, NULL, NULL);
    return buf;
}

// ��һ��������ΪGBK���룬����ת��ΪUTF-16���벢����
static WCHAR* CharToWChar(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf, buf_len);
    return buf;
}

// ��һ��������ΪUTF-8���룬����ת��ΪUTF-16���벢����
static WCHAR* CharToWChar_ser(const char* str, WCHAR* buf, int buf_len) {
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, buf_len);
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
            CharToWChar_ser(dep_output, output_w, sizeof(output_w) / sizeof(WCHAR));
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
        WCHAR logMsg[256];
        CharToWChar("δ���ӵ����������޷�����ʵ��", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ��ȡ�û�����Ķ˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WCharToChar(port_w, port, sizeof(port));

    // ��ȫ��ӳ����в���ʵ������
    std::string instanceName;
    if (g_portToInstance.find(port) != g_portToInstance.end()) {
        instanceName = g_portToInstance[port];
    }
    else {
        WCHAR logMsg[256];
        CharToWChar("δ�ҵ���Ӧ�˿ڵ�ʵ������", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ���ݶ˿ڲ���ʵ������
    if (g_portToInstance.find(port) != g_portToInstance.end()) {
        instanceName = g_portToInstance[port];
    }
    else {
        // δ�ҵ���Ӧʵ��
        WCHAR logMsg[256];
        CharToWChar("δ�ҵ���Ӧ�˿ڵ�ʵ������", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ���������־������ʵ�����ƺͶ˿ڣ�
    WCHAR logMsg[256];
    WCHAR instanceNameW[128];

    CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"��������ʵ��: %ls���˿�: %s��...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // ����SSH���ʹ��start_instance������ʵ�����ƣ�����ԭ�������
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh start_instance %s", instanceName.c_str());

    // ִ�����������
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�������־����ʾ
    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        WCHAR successMsg[64];
        CharToWChar("ʵ�������ɹ�", successMsg, sizeof(successMsg) / sizeof(WCHAR));
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        WCHAR failPrefix[64];
        CharToWChar("����ʧ��: ", failPrefix, sizeof(failPrefix) / sizeof(WCHAR));
        AddLog(hWnd, failPrefix);
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// ֹͣʵ����������ʹ��ȫ��ӳ����ȡʵ�����ƣ�
DWORD WINAPI HandleStopInstance(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        WCHAR logMsg[256];
        CharToWChar("δ���ӵ����������޷�ֹͣʵ��", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ��ȡ�˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    WCharToChar(port_w, port, sizeof(port));

    // ��ȫ��ӳ������ʵ������
    std::string instanceName;
    auto it = g_portToInstance.find(port);
    if (it != g_portToInstance.end()) {
        instanceName = it->second;
    }
    else {
        WCHAR logMsg[256];

        swprintf_s(logMsg, L"δ�ҵ��˿� %s ��Ӧ��ʵ������", port_w);
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ���ֹͣ��־
    WCHAR logMsg[256];
    WCHAR instanceNameW[128];

    CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"����ֹͣʵ��: %ls���˿�: %s��...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // ����SSH����
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/home/L4D2_Manager/L4D2_Manager_API.sh stop_instance %s", instanceName.c_str());

    // ִ�����������
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        cmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�������־����ʾ
    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        Sleep(1000);
        AddLog(hWnd, L"�����ĵȴ����رշ�����������Ҫ�����");
        Sleep(15000); //�ǳ���Ҫ��ʵ���Ĺر���Ҫʱ�䣬����get_status���صľͻ���������
        WCHAR successMsg[256];
        WCHAR instanceNameW[128];

        CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
        swprintf_s(successMsg, L"ʵ�� %ls���˿�: %s��ֹͣ�ɹ�", instanceNameW, port_w);
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        WCHAR failMsg[256];
        WCHAR instanceNameW[128];

        CharToWChar_ser(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
        swprintf_s(failMsg, L"ʵ�� %ls���˿�: %s��ֹͣʧ��: %s",
            instanceNameW, port_w, err_msg_w);
        AddLog(hWnd, failMsg);
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

    AddLog(hWnd, L"��ʼ����/���·�����(�Ѳ���������£����ɸ�������Ҫ�ķ�һ��ʱ�䣬ȡ���������������)...");
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        "/home/L4D2_Manager/L4D2_Manager_API.sh deploy_server",
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    WCHAR output_w[4096], err_msg_w[256];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        AddLog(hWnd, L"����������/�������");
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

            cJSON* mmInstalled = cJSON_GetObjectItem(root, "mmInstalled");
            LPCWSTR mmStatus = (mmInstalled && mmInstalled->type == cJSON_True) ? L"�Ѱ�װ" : L"δ��װ";

            UpdateSystemStatus(hWnd, serverStatus, smStatus, mmStatus);

            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"����ϵͳ״̬JSONʧ��");
        }
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
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
        // ����JSON������ȫ��ӳ���
        cJSON* root = cJSON_Parse(output);
        if (root) {
            // ��վ�ӳ�䣨���������Ч���ݣ�
            g_portToInstance.clear();

            // ����JSON������ʵ������Ϊʵ�����ƣ�ֵΪʵ�����飩
            cJSON* instance = NULL;
            cJSON_ArrayForEach(instance, root) {
                if (instance->type == cJSON_Object) {
                    // ��ȡʵ�����ƣ�JSON����
                    std::string instanceName = instance->string;
                    // ��ȡ�˿ںţ�ʵ�������е�port�ֶΣ�
                    cJSON* portItem = cJSON_GetObjectItem(instance, "port");
                    if (portItem && portItem->type == cJSON_String) {
                        std::string port = portItem->valuestring;
                        // ����ӳ����˿� -> ʵ�����ƣ�
                        g_portToInstance[port] = instanceName;
                    }
                }
            }
            cJSON_Delete(root);
        }
        else {
            AddLog(hWnd, L"����ʵ���б�JSONʧ��");
        }

        // ˢ��UI�б�
        UpdateInstanceList(hWnd, output);
    }
    else {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// �ϴ�SourceMod��װ��
DWORD WINAPI HandleUploadSourceMod(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷��ϴ�SourceMod");
        return 0;
    }

    // Զ��Ŀ¼
    const char* remote_dir = "/home/L4D2_Manager/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"Զ��Ŀ¼�����ڣ����Դ���...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            AddLog(hWnd, err_w);
            return 0;
        }
    }

    // ���ļ�ѡ��Ի���
    WCHAR filter[] = L"�����ļ� (*.*)\0*.*\0tar.gzѹ���� (*.tar.gz)\0*.tar.gz\0\0";
    WCHAR fileName[256] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = sizeof(fileName) / sizeof(WCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // ����Ĭ��·��Ϊ��ǰĿ¼��resource�ļ���
    WCHAR currentDir[256];
    GetCurrentDirectoryW(sizeof(currentDir) / sizeof(WCHAR), currentDir);
    WCHAR defaultPath[256];
    swprintf_s(defaultPath, L"%s\\resource", currentDir);
    ofn.lpstrInitialDir = defaultPath;

    if (!GetOpenFileNameW(&ofn)) {
        AddLog(hWnd, L"ȡ��ѡ���ļ�");
        return 0;
    }

    // ת��·��Ϊ���ֽ�
    char local_path[256];
    WCharToChar(fileName, local_path, sizeof(local_path));

    // ��ȡ�ļ���
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // ����Զ��·��
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir, file_name);

    // �ϴ��ļ�
    AddLog(hWnd, L"��ʼ�ϴ�SourceMod��װ��...");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        CharToWChar(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"�ϴ��ɹ�");
    }
    else {
        WCHAR err_w[256];
        CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        AddLog(hWnd, err_w);
    }

    return 0;
}

// �ϴ�MetaMod��װ��
DWORD WINAPI HandleUploadMetaMod(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷��ϴ�MetaMod");
        return 0;
    }

    // Զ��Ŀ¼
    const char* remote_dir = "/home/L4D2_Manager/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"Զ��Ŀ¼�����ڻ�ssh����ʧ�ܣ��������Ӳ�����...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir, err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
            AddLog(hWnd, err_w);
            return 0;
        }
    }

    // ���ļ�ѡ��Ի���
    WCHAR filter[] = L"�����ļ� (*.*)\0*.*\0tar.gzѹ���� (*.tar.gz)\0*.tar.gz\0\0";
    WCHAR fileName[256] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = sizeof(fileName) / sizeof(WCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // ����Ĭ��·��Ϊ��ǰĿ¼��resource�ļ���
    WCHAR currentDir[256];
    GetCurrentDirectoryW(sizeof(currentDir) / sizeof(WCHAR), currentDir);
    WCHAR defaultPath[256];
    swprintf_s(defaultPath, L"%s\\resource", currentDir);
    ofn.lpstrInitialDir = defaultPath;

    if (!GetOpenFileNameW(&ofn)) {
        AddLog(hWnd, L"ȡ��ѡ���ļ�");
        return 0;
    }

    // ת��·��Ϊ���ֽ�
    char local_path[256];
    WCharToChar(fileName, local_path, sizeof(local_path));

    // ��ȡ�ļ���
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // ����Զ��·��
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir, file_name);

    // �ϴ��ļ�
    AddLog(hWnd, L"��ʼ�ϴ�MetaMod��װ��...");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        CharToWChar(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"�ϴ��ɹ�");
    }
    else {
        WCHAR err_w[256];
        CharToWChar(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
        AddLog(hWnd, err_w);
    }

    return 0;
}

// ��װSourceMod��MetaMod
DWORD WINAPI HandleInstallSourceMetaMod(LPVOID param) {
    HWND hWnd = (HWND)param;

    // ���SSH����״̬
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) {
        AddLog(hWnd, L"δ���ӵ����������޷���װSourceMod");
        return 0;
    }

    // ��¼��ʼ��װ����־
    AddLog(hWnd, L"��ʼִ��SourceMod��װ�ű�...");

    // �����������������ʹ�����Ϣ������
    const char* installCmd = "/home/L4D2_Manager/L4D2_Manager_API.sh install_sourcemod";
    char output[4096] = { 0 };  // �㹻��Ļ������洢�������
    char err_msg[256] = { 0 };  // �洢������Ϣ

    // ִ��SSH����
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        installCmd,
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // �����������UTF-8ת��Ϊ���ַ�����ӡ����־
    WCHAR output_w[4096];
    CharToWChar_ser(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    AddLog(hWnd, output_w);

    // �������ִ��ʧ�ܣ������ӡ������Ϣ
    if (!success) {
        WCHAR err_msg_w[256];
        CharToWChar_ser(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, L"��װ�����г��ִ���:");
        AddLog(hWnd, err_msg_w);
    }
    else {
        // ��װ�ɹ���ˢ��״̬
        AddLog(hWnd, L"SourceMod��װ����ִ�����");
        HandleGetStatus(hWnd);  // ˢ��ϵͳ״̬��ʾ
    }

    return 0;
}

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
#include "encoding_convert.h"
#include "config.h"
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <cmath>
#include <map>

// ȫ�ֶ˿�-ʵ������ӳ�����̬���£�
std::map<std::string, std::string> g_portToInstance;



// ����SSH���������̺߳�����
DWORD WINAPI HandleConnectRequest(LPVOID param) {
    HWND hWnd = (HWND)param;
    WCHAR ip_w[64], user_w[64], pass_w[64], port_w[16];
    char ip[64], user[64], pass[64], port[16], err_msg[256];
    int port_num = 22;  // Ĭ�϶˿�

    // ��ȡ���������
    GetWindowTextW(GetDlgItem(hWnd, IDC_IP_EDIT), ip_w, sizeof(ip_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_USER_EDIT), user_w, sizeof(user_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_PASS_EDIT), pass_w, sizeof(pass_w) / sizeof(WCHAR));
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT_SSH), port_w, sizeof(port_w) / sizeof(WCHAR));  // ��ȡ�˿�

    // ת��Ϊ���ֽ�
    U16toGBK(ip_w, ip, sizeof(ip));
    U16toGBK(user_w, user, sizeof(user));
    U16toGBK(pass_w, pass, sizeof(pass));
    U16toGBK(port_w, port, sizeof(port));

    // �����˿ں�
    if (strlen(port) > 0) {
        port_num = atoi(port);
        if (port_num <= 0 || port_num > 65535) {
            AddLog(hWnd, L"��Ч�Ķ˿ںţ�ʹ��Ĭ�϶˿�22");
            port_num = 22;
        }
    }

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

    // ���ssh_key�ļ����Ƿ���ڣ��������򴴽�
    bool use_key_auth = false;
    char key_dir[MAX_PATH] = "ssh_key";
    char key_path[MAX_PATH] = { 0 };

    // ��鲢����ssh_keyĿ¼
    if (!CreateDirectoryA(key_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        AddLog(hWnd, L"����ssh_key�ļ���ʧ�ܣ���ʹ�������¼");
    }
    else {
        // ����ssh_keyĿ¼�еĵ�һ���ļ���Ϊ˽Կ
        WIN32_FIND_DATAA findData;
        // ƴ��·����key_dir + "\*"��
        char search_path[MAX_PATH];
        sprintf_s(search_path, MAX_PATH, "%s\\*", key_dir);  // ʹ�� sprintf_s ��ȫƴ��

        // ��ȷ���� FindFirstFileA
        HANDLE hFind = FindFirstFileA(search_path, &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                // ����Ŀ¼�͵�ǰ/�ϼ�Ŀ¼
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    strcmp(findData.cFileName, ".") != 0 &&
                    strcmp(findData.cFileName, "..") != 0) {

                    // �ҵ���һ���ļ�����Ϊ˽Կ
                    sprintf_s(key_path, MAX_PATH, "%s\\%s", key_dir, findData.cFileName);
                    use_key_auth = true;
                    break;
                }
            } while (FindNextFileA(hFind, &findData));

            FindClose(hFind);
        }

        if (use_key_auth) {
            WCHAR key_path_w[MAX_PATH];
            GBKtoU16(key_path, key_path_w, MAX_PATH);
            AddLog(hWnd, L"�ҵ�˽Կ�ļ�����ʹ����Կ��¼:");
            AddLog(hWnd, key_path_w);
        }
        else {
            AddLog(hWnd, L"ssh_key�ļ�����δ�ҵ�˽Կ�ļ�����ʹ�������¼");
        }
    }

    // ���ӷ�����
    AddLog(hWnd, L"�������ӵ�������...");
    bool success = false;

    if (use_key_auth) {
        // ʹ����Կ��¼
        success = l4d2_ssh_connect_with_key(
            g_ssh_ctx,
            ip,
            port_num,
            user,
            key_path,
            pass,
            err_msg,
            sizeof(err_msg)
        );
    }
    else {
        // ʹ�������¼
        success = l4d2_ssh_connect(
            g_ssh_ctx,
            ip,
            port_num,
            user,
            pass,
            err_msg,
            sizeof(err_msg)
        );
    }

    WCHAR err_msg_w[256];
    GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
    AddLog(hWnd, err_msg_w);

    if (success) {
        // ���ӳɹ����ϴ�API�ű�
        AddLog(hWnd, L"���ڼ��API�ű��Ƿ�װΪ����...");
        if (l4d2_ssh_upload_api_script(g_ssh_ctx, err_msg, sizeof(err_msg))) {
            GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
            AddLog(hWnd, err_msg_w);

            // ������ִ����������밲װ
            AddLog(hWnd, L"���ڼ�鲢��װ�ű�����...");
            char dep_output[4096] = { 0 };
            WCHAR output_w[4096];
            // ��ȡ���õ�Զ��·��
            std::string remoteRoot = GetRemoteRootPath();
            std::string command = remoteRoot + "/L4D2_Manager_API.sh check_deps";
            bool dep_success = l4d2_ssh_exec_command(
                g_ssh_ctx,
                command.c_str(),
                dep_output, sizeof(dep_output),
                err_msg, sizeof(err_msg)
            );
            GBKtoU16(dep_output, output_w, sizeof(output_w) / sizeof(WCHAR));
            AddLog(hWnd, output_w);

            if (dep_success) {
                UpdateConnectionStatus(hWnd, L"�����ӣ���������װ�ɹ�", TRUE);
                HandleGetStatus(hWnd);  // ˢ��״̬
                HandleGetInstances(hWnd);
            }
            else {
                GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
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
        GBKtoU16("δ���ӵ����������޷�����ʵ��", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ��ȡ�û�����Ķ˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    U16toGBK(port_w, port, sizeof(port));

    // ��ȫ��ӳ����в���ʵ������
    std::string instanceName;
    if (g_portToInstance.find(port) != g_portToInstance.end()) {
        instanceName = g_portToInstance[port];
    }
    else {
        WCHAR logMsg[256];
        GBKtoU16("δ�ҵ���Ӧ�˿ڵ�ʵ������", logMsg, sizeof(logMsg) / sizeof(WCHAR));
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
        GBKtoU16("δ�ҵ���Ӧ�˿ڵ�ʵ������", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ���������־������ʵ�����ƺͶ˿ڣ�
    WCHAR logMsg[256];
    WCHAR instanceNameW[128];

    GBKtoU16(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"��������ʵ��: %ls���˿�: %s��...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // ����SSH���ʹ��start_instance������ʵ�����ƣ�
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh start_instance " + instanceName;

    // ִ�����������
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�������־����ʾ
    WCHAR output_w[4096], err_msg_w[256];
    GBKtoU16(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        WCHAR successMsg[64];
        GBKtoU16("ʵ�������ɹ�", successMsg, sizeof(successMsg) / sizeof(WCHAR));
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        WCHAR failPrefix[64];
        GBKtoU16("����ʧ��: ", failPrefix, sizeof(failPrefix) / sizeof(WCHAR));
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
        GBKtoU16("δ���ӵ����������޷�ֹͣʵ��", logMsg, sizeof(logMsg) / sizeof(WCHAR));
        AddLog(hWnd, logMsg);
        return 0;
    }

    // ��ȡ�˿ں�
    WCHAR port_w[16];
    GetWindowTextW(GetDlgItem(hWnd, IDC_PORT_EDIT), port_w, sizeof(port_w) / sizeof(WCHAR));
    char port[16];
    U16toGBK(port_w, port, sizeof(port));

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

    GBKtoU16(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
    swprintf_s(logMsg, L"����ֹͣʵ��: %ls���˿�: %s��...", instanceNameW, port_w);
    AddLog(hWnd, logMsg);

    // ����SSH����
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh stop_instance " + instanceName;

    // ִ�����������
    char output[4096] = { 0 }, err_msg[256] = { 0 };
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // ת�������־����ʾ
    WCHAR output_w[4096], err_msg_w[256];
    GBKtoU16(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

    AddLog(hWnd, output_w);
    if (success) {
        Sleep(1000);
        AddLog(hWnd, L"�����ĵȴ����رշ�����������Ҫ�����");
        Sleep(15000); //�ǳ���Ҫ��ʵ���Ĺر���Ҫʱ�䣬����get_status���صľͻ���������
        WCHAR successMsg[256];
        WCHAR instanceNameW[128];

        GBKtoU16(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
        swprintf_s(successMsg, L"ʵ�� %ls���˿�: %s��ֹͣ�ɹ�", instanceNameW, port_w);
        AddLog(hWnd, successMsg);
        HandleGetInstances(hWnd);  // ˢ��ʵ���б�
    }
    else {
        WCHAR failMsg[256];
        WCHAR instanceNameW[128];

        GBKtoU16(instanceName.c_str(), instanceNameW, sizeof(instanceNameW) / sizeof(WCHAR));
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

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh deploy_server";

    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    WCHAR output_w[4096], err_msg_w[256];
    GBKtoU16(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));

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

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_status";

    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
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
        GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
        AddLog(hWnd, err_msg_w);
    }

    return 0;
}

// �����ȡʵ���б������̺߳�����
DWORD WINAPI HandleGetInstances(LPVOID param) {
    HWND hWnd = (HWND)param;
    if (!g_ssh_ctx || !g_ssh_ctx->is_connected) return 0;

    char output[4096] = { 0 }, err_msg[256] = { 0 };

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string command = remoteRoot + "/L4D2_Manager_API.sh get_instances";

    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        command.c_str(),
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
        GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
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

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_dir = remoteRoot + "/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir.c_str(), err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"Զ��Ŀ¼�����ڣ����Դ���...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir.c_str(), err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
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
    U16toGBK(fileName, local_path, sizeof(local_path));

    // ��ȡ�ļ���
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // ����Զ��·��
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir.c_str(), file_name);

    // �ϴ��ļ�
    AddLog(hWnd, L"��ʼ�ϴ�SourceMod��װ�������ļ��ϴ��������ĵȺ�...");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        GBKtoU16(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"�ϴ��ɹ�");
    }
    else {
        WCHAR err_w[256];
        GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
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

    // ��ȡ���õ�Զ��·��
    std::string remoteRoot = GetRemoteRootPath();
    std::string remote_dir = remoteRoot + "/SourceMod_Installers";
    char err_msg[256] = { 0 };

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(g_ssh_ctx->session, remote_dir.c_str(), err_msg, sizeof(err_msg))) {
        AddLog(hWnd, L"Զ��Ŀ¼�����ڻ�ssh����ʧ�ܣ��������Ӳ�����...");
        if (!create_remote_dir(g_ssh_ctx->session, remote_dir.c_str(), err_msg, sizeof(err_msg))) {
            WCHAR err_w[256];
            GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
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
    U16toGBK(fileName, local_path, sizeof(local_path));

    // ��ȡ�ļ���
    char* file_name = strrchr(local_path, '\\');
    if (!file_name) file_name = local_path;
    else file_name++;

    // ����Զ��·��
    char remote_path[256];
    snprintf(remote_path, sizeof(remote_path), "%s/%s", remote_dir.c_str(), file_name);

    // �ϴ��ļ�
    AddLog(hWnd, L"��ʼ�ϴ�MetaMod��װ�������ļ��ϴ��������ĵȺ�......");
    if (upload_file_normal(g_ssh_ctx->session, local_path, remote_path, err_msg, sizeof(err_msg))) {
        WCHAR success_msg[256];
        GBKtoU16(err_msg, success_msg, sizeof(success_msg) / sizeof(WCHAR));
        AddLog(hWnd, success_msg);
        AddLog(hWnd, L"�ϴ��ɹ�");
    }
    else {
        WCHAR err_w[256];
        GBKtoU16(err_msg, err_w, sizeof(err_w) / sizeof(WCHAR));
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
    std::string remoteRoot = GetRemoteRootPath();
    std::string installCmd = remoteRoot + "/L4D2_Manager_API.sh install_sourcemod";
    char output[4096] = { 0 };  // �㹻��Ļ������洢�������
    char err_msg[256] = { 0 };  // �洢������Ϣ

    // ִ��SSH����
    bool success = l4d2_ssh_exec_command(
        g_ssh_ctx,
        installCmd.c_str(),
        output, sizeof(output),
        err_msg, sizeof(err_msg)
    );

    // �����������UTF-8ת��Ϊ���ַ�����ӡ����־
    WCHAR output_w[4096];
    GBKtoU16(output, output_w, sizeof(output_w) / sizeof(WCHAR));
    AddLog(hWnd, output_w);

    // �������ִ��ʧ�ܣ������ӡ������Ϣ
    if (!success) {
        WCHAR err_msg_w[256];
        GBKtoU16(err_msg, err_msg_w, sizeof(err_msg_w) / sizeof(WCHAR));
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

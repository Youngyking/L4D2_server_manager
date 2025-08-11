#pragma once
#ifndef SSH_H
#define SSH_H

#include <libssh/libssh.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h> 
#pragma comment(lib, "ws2_32.lib")  // ���� Winsock 2.0 ��

// SSH����������
typedef struct {
    ssh_session session;
    bool is_connected;
} L4D2_SSH_Context;

// ��ʼ��SSH������
L4D2_SSH_Context* l4d2_ssh_init();

// ���Զ��Ŀ¼�Ƿ����
bool check_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len);

// ����Զ��Ŀ¼
bool create_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len);

// �ϴ��ļ���Զ�̷������������з�ת�����ܣ�
bool upload_file_txt(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len);

// �ϴ��ļ���Զ�̷�������һ���ļ���
bool upload_file_normal(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len);

// ���ӵ�SSH������
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len);

// ��鲢�ϴ�API�ű������Ĺ��ܣ�
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len);

// ִ��Զ�������ȡ���
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len);

// �Ͽ����Ӳ��ͷ���Դ
void l4d2_ssh_cleanup(L4D2_SSH_Context* ctx);

#endif
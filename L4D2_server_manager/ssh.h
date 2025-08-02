#pragma once
#ifndef SSH_H
#define SSH_H

#include <libssh/libssh.h>
#include <stdbool.h>

// SSH����������
typedef struct {
    ssh_session session;
    bool is_connected;
} L4D2_SSH_Context;

// ��ʼ��SSH������
L4D2_SSH_Context* l4d2_ssh_init();

// ���ӵ�SSH������
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len);

// ��鲢�ϴ�API�ű������Ĺ��ܣ�
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len);

// ִ��Զ�������ȡ���
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len);

// �Ͽ����Ӳ��ͷ���Դ
void l4d2_ssh_cleanup(L4D2_SSH_Context* ctx);

#endif
#pragma once
#ifndef SSH_H
#define SSH_H

#include <libssh/libssh.h>
#include <stdbool.h>

// SSH连接上下文
typedef struct {
    ssh_session session;
    bool is_connected;
} L4D2_SSH_Context;

// 初始化SSH上下文
L4D2_SSH_Context* l4d2_ssh_init();

// 连接到SSH服务器
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len);

// 检查并上传API脚本（核心功能）
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len);

// 执行远程命令并获取输出
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len);

// 断开连接并释放资源
void l4d2_ssh_cleanup(L4D2_SSH_Context* ctx);

#endif
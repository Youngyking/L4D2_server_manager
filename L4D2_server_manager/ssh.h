#pragma once
#ifndef SSH_H
#define SSH_H

#include <libssh/libssh.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h> 
#pragma comment(lib, "ws2_32.lib")  // 链接 Winsock 2.0 库

// SSH连接上下文
typedef struct {
    ssh_session session;
    bool is_connected;
} L4D2_SSH_Context;

// 初始化SSH上下文
L4D2_SSH_Context* l4d2_ssh_init();

// 检查远程目录是否存在
bool check_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len);

// 创建远程目录
bool create_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len);

// 上传文件到远程服务器（带换行符转换功能）
bool upload_file_txt(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len);

// 上传文件到远程服务器（一般文件）
bool upload_file_normal(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len);

// 连接到SSH服务器
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len);

// 检查并上传API脚本（核心功能）
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len);

// 执行远程命令并获取输出
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len);

// 断开连接并释放资源
void l4d2_ssh_cleanup(L4D2_SSH_Context* ctx);

#endif
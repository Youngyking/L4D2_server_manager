/*
通过封装底层协议实现细节，实现对manager.cpp和gui.cpp的底层协议支持
*/

#define WIN32_LEAN_AND_MEAN  // 禁用 Windows 旧版头文件中的冗余定义
#define _CRT_SECURE_NO_WARNINGS //允许使用fopen

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/stat.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include "ssh.h"

//doc_to_unix
static bool convert_crlf_to_lf(const char* input_path, const char* temp_path) {
    FILE* in_file = fopen(input_path, "rb");
    if (!in_file) return false;

    FILE* out_file = fopen(temp_path, "wb");
    if (!out_file) {
        fclose(in_file);
        return false;
    }

    int c;
    while ((c = fgetc(in_file)) != EOF) {
        if (c == '\r') {
            // 跳过CR，只保留LF
            continue;
        }
        fputc(c, out_file);
    }

    fclose(in_file);
    fclose(out_file);
    return true;
}


// 初始化SSH上下文
L4D2_SSH_Context* l4d2_ssh_init() {
    L4D2_SSH_Context* ctx = (L4D2_SSH_Context*)malloc(sizeof(L4D2_SSH_Context));
    if (!ctx) return NULL;
    ctx->session = ssh_new();
    ctx->is_connected = false;
    return ctx;
}

// 连接到SSH服务器
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len) {
    if (!ctx || !ctx->session) {
        snprintf(err_msg, err_len, "SSH上下文未初始化");
        return false;
    }

    // 设置连接参数
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);

    // 建立连接
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "连接失败: %s", ssh_get_error(ctx->session));
        return false;
    }

    // 密码认证
    rc = ssh_userauth_password(ctx->session, NULL, pass);
    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "认证失败: %s", ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    ctx->is_connected = true;
    snprintf(err_msg, err_len, "连接成功");
    return true;
}

// 检查远程目录是否存在
bool check_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "test -d %s", dir);
    ssh_channel channel = ssh_channel_new(session);
    if (!channel) {
        snprintf(err_msg, err_len, "创建通道失败");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "打开通道失败: %s", ssh_get_error(session));
        ssh_channel_free(channel);
        return false;
    }

    // 将ssh_channel_exec替换为ssh_channel_request_exec
    rc = ssh_channel_request_exec(channel, cmd);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "执行命令失败: %s", ssh_get_error(session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return exit_status == 0;  // 0表示目录存在
}

// 创建远程目录
bool create_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
    ssh_channel channel = ssh_channel_new(session);
    if (!channel) return false;

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "创建目录失败: %s", ssh_get_error(session));
        ssh_channel_free(channel);
        return false;
    }

    // 将ssh_channel_exec替换为ssh_channel_request_exec
    rc = ssh_channel_request_exec(channel, cmd);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return rc == SSH_OK;
}

// 上传文件到远程服务器（带换行符转换功能）
bool upload_file(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
    // 创建临时文件路径
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", local_path);

    // 转换CRLF为LF
    FILE* in_file = fopen(local_path, "rb");
    if (!in_file) {
        snprintf(err_msg, err_len, "无法打开本地文件: %s", local_path);
        return false;
    }

    FILE* out_file = fopen(temp_path, "wb");
    if (!out_file) {
        snprintf(err_msg, err_len, "无法创建临时文件: %s", temp_path);
        fclose(in_file);
        return false;
    }

    int c;
    while ((c = fgetc(in_file)) != EOF) {
        if (c == '\r') {
            // 跳过CR，只保留LF
            continue;
        }
        fputc(c, out_file);
    }

    fclose(in_file);
    fclose(out_file);

    // 检查临时文件是否存在
    struct stat file_stat;
    if (stat(temp_path, &file_stat) != 0) {
        snprintf(err_msg, err_len, "无法访问临时文件: %s", temp_path);
        remove(temp_path);
        return false;
    }

    // 初始化SFTP会话
    sftp_session sftp = sftp_new(session);
    if (!sftp) {
        snprintf(err_msg, err_len, "创建SFTP会话失败: %s", ssh_get_error(session));
        remove(temp_path);
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "初始化SFTP失败: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // 检查远程文件是否已存在
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        snprintf(err_msg, err_len, "远程文件已存在，跳过上传");
        sftp_free(sftp);
        remove(temp_path);
        return true;
    }

    // 打开临时文件进行读取
    FILE* local_file;
    if (fopen_s(&local_file, temp_path, "rb") != 0) {
        snprintf(err_msg, err_len, "打开临时文件失败");
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // 创建远程文件
    sftp_file remote_file = sftp_open(sftp, remote_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!remote_file) {
        snprintf(err_msg, err_len, "创建远程文件失败: %s", sftp_get_error(sftp));
        fclose(local_file);
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // 传输文件内容
    char buffer[4096];
    size_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), local_file)) > 0) {
        if (sftp_write(remote_file, buffer, nread) != (int)nread) {
            snprintf(err_msg, err_len, "文件传输失败: %s", sftp_get_error(sftp));
            fclose(local_file);
            sftp_close(remote_file);
            sftp_free(sftp);
            remove(temp_path);
            return false;
        }
    }

    // 清理资源
    fclose(local_file);
    sftp_close(remote_file);
    sftp_free(sftp);
    remove(temp_path); // 删除临时文件

    snprintf(err_msg, err_len, "文件上传成功");
    return true;
}

// 检查并上传API脚本
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "未连接SSH服务器");
        return false;
    }

    const char* remote_dir = "/home/L4D2_Manager";
    const char* remote_file = "/home/L4D2_Manager/L4D2_Manager_API.sh";
    const char* local_file = "scripts/L4D2_Manager_API.sh";  // 本地脚本路径

    // 检查并创建远程目录
    if (!check_remote_dir(ctx->session, remote_dir, err_msg, err_len)) {
        if (!create_remote_dir(ctx->session, remote_dir, err_msg, err_len)) {
            return false;
        }
    }

    // 上传脚本文件
    if (!upload_file(ctx->session, local_file, remote_file, err_msg, err_len)) {
        return false;  // 上传失败直接返回
    }

    // 新增：授予脚本可执行权限（关键修改）
    char chmod_cmd[256];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", remote_file);
    ssh_channel channel = ssh_channel_new(ctx->session);
    if (!channel) {
        snprintf(err_msg, err_len, "创建通道失败（授权权限时）");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "打开通道失败（授权权限时）: %s", ssh_get_error(ctx->session));
        ssh_channel_free(channel);
        return false;
    }

    rc = ssh_channel_request_exec(channel, chmod_cmd);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "授予执行权限失败: %s", ssh_get_error(ctx->session));
        return false;
    }

    snprintf(err_msg, err_len, "脚本上传并授予执行权限成功");

    return true;
}

// 执行远程命令并获取输出
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "未建立SSH连接");
        return false;
    }

    ssh_channel channel = ssh_channel_new(ctx->session);
    if (!channel) {
        snprintf(err_msg, err_len, "创建通道失败");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "打开通道失败: %s", ssh_get_error(ctx->session));
        ssh_channel_free(channel);
        return false;
    }

    // 将ssh_channel_exec替换为ssh_channel_request_exec
    rc = ssh_channel_request_exec(channel, cmd);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "执行命令失败: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // 读取命令输出
    output[0] = '\0';
    char buffer[1024];
    int nbytes;
    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[nbytes] = '\0';
        // 使用更安全的字符串拼接方式
        size_t current_len = strlen(output);
        if (current_len + nbytes < (size_t)output_len - 1) {
            strcat_s(output, output_len, buffer);
        }
        else {
            break; // 防止缓冲区溢出
        }
    }

    if (nbytes < 0) {
        snprintf(err_msg, err_len, "读取输出失败: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    ssh_channel_close(channel);
    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_free(channel);

    if (exit_status != 0) {
        snprintf(err_msg, err_len, "命令执行失败（退出码: %d）", exit_status);
        return false;
    }

    return true;
}

// 断开连接并释放资源
void l4d2_ssh_cleanup(L4D2_SSH_Context* ctx) {
    if (ctx) {
        if (ctx->session) {
            if (ctx->is_connected) {
                ssh_disconnect(ctx->session);
            }
            ssh_free(ctx->session);
        }
        free(ctx);
    }
}

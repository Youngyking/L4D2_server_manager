/*
通过封装底层协议实现细节，实现对manager.cpp和gui.cpp的底层协议支持
*/

#define WIN32_LEAN_AND_MEAN  // 禁用 Windows 旧版头文件中的冗余定义
#define _CRT_SECURE_NO_WARNINGS //允许使用fopen

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/stat.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include "ssh.h"
#include "config.h"

// 声明外部函数
std::string GetRemoteRootPath();

// 将GBK编码转换为UTF-8编码
static char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len) {
    if (!gbk_str || !u8_buf || buf_len <= 0) {
        return nullptr; // 无效参数检查
    }

    // 步骤1: 先将GBK转换为UTF-16（中间过渡编码）
    int u16_len = MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // GBK编码无效
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // 转换UTF-16失败
    }

    // 步骤2: 将UTF-16转换为UTF-8
    int u8_len = WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (u8_len == 0 || u8_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // 缓冲区不足或转换失败
    }

    WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, u8_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return u8_buf;
}

// 将UTF-8编码转换为GBK编码
static char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len) {
    if (!u8_str || !gbk_buf || buf_len <= 0) {
        return nullptr; // 无效参数检查
    }

    // 步骤1: 先将UTF-8转换为UTF-16（中间过渡编码）
    int u16_len = MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // UTF-8编码无效
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // 转换UTF-16失败
    }

    // 步骤2: 将UTF-16转换为GBK
    int gbk_len = WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (gbk_len == 0 || gbk_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // 缓冲区不足或转换失败
    }

    WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, gbk_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return gbk_buf;
}


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

// 读取远程文件内容到缓冲区（内部辅助函数）
static bool read_remote_file_content(sftp_session sftp, const char* remote_path,
    char** content, size_t* len,
    char* err_msg, int err_len) {
    sftp_file file = sftp_open(sftp, remote_path, O_RDONLY, 0);
    if (!file) {
        snprintf(err_msg, err_len, "打开远程文件失败: %s", sftp_get_error(sftp));
        return false;
    }

    // 动态缓冲区读取文件内容
    size_t buffer_size = 4096;
    *len = 0;
    *content = (char*)malloc(buffer_size);
    if (!*content) {
        snprintf(err_msg, err_len, "内存分配失败");
        sftp_close(file);
        return false;
    }

    ssize_t nread;
    while ((nread = sftp_read(file, *content + *len, buffer_size - *len)) > 0) {
        *len += nread;
        // 缓冲区不足时扩容
        if (*len >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buf = (char*)realloc(*content, buffer_size);
            if (!new_buf) {
                snprintf(err_msg, err_len, "内存重分配失败");
                free(*content);
                *content = nullptr;
                *len = 0;
                sftp_close(file);
                return false;
            }
            *content = new_buf;
        }
    }

    if (nread < 0) {
        snprintf(err_msg, err_len, "读取远程文件失败: %s", sftp_get_error(sftp));
        free(*content);
        *content = nullptr;
        *len = 0;
        sftp_close(file);
        return false;
    }

    // 调整缓冲区到实际大小
    char* new_buf = (char*)realloc(*content, *len);
    if (new_buf) {
        *content = new_buf;
    }

    sftp_close(file);
    return true;
}

// 处理本地文件（转换CRLF为LF）并读取内容（内部辅助函数）
static bool process_local_file(const char* local_path, char** content, size_t* len,
    char* err_msg, int err_len) {
    FILE* in_file = fopen(local_path, "rb");
    if (!in_file) {
        snprintf(err_msg, err_len, "无法打开本地文件: %s", local_path);
        return false;
    }

    // 动态缓冲区读取并处理换行符
    size_t buffer_size = 4096;
    *len = 0;
    *content = (char*)malloc(buffer_size);
    if (!*content) {
        snprintf(err_msg, err_len, "内存分配失败");
        fclose(in_file);
        return false;
    }

    int c;
    while ((c = fgetc(in_file)) != EOF) {
        if (c == '\r') {
            continue;  // 跳过CR，只保留LF
        }
        // 缓冲区不足时扩容
        if (*len >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buf = (char*)realloc(*content, buffer_size);
            if (!new_buf) {
                snprintf(err_msg, err_len, "内存重分配失败");
                free(*content);
                *content = nullptr;
                *len = 0;
                fclose(in_file);
                return false;
            }
            *content = new_buf;
        }
        (*content)[*len] = (char)c;
        (*len)++;
    }

    fclose(in_file);

    // 调整缓冲区到实际大小
    char* new_buf = (char*)realloc(*content, *len);
    if (new_buf) {
        *content = new_buf;
    }

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


// 支持IP+端口+密码的SSH连接函数
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, int port_num,
    const char* user, const char* pass,
    char* err_msg, int err_len) {
    // 参数合法性检查
    if (!ctx || !ctx->session || !ip || !user) {
        snprintf(err_msg, err_len, "无效的参数（上下文或会话未初始化）");
        return false;
    }

    // 设置SSH连接选项（主机、端口、用户名）
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);               // 设置IP地址
    ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port_num);        // 设置端口（注意传地址）
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);             // 设置用户名

    // 建立TCP连接
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "TCP连接失败: %s（IP: %s, 端口: %d）",
            ssh_get_error(ctx->session), ip, port_num);
        return false;
    }

    // 密码认证
    rc = ssh_userauth_password(ctx->session, NULL, pass);
    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "密码认证失败: %s（用户: %s）",
            ssh_get_error(ctx->session), user);
        ssh_disconnect(ctx->session);  // 认证失败需断开连接
        return false;
    }

    // 连接成功
    ctx->is_connected = true;
    snprintf(err_msg, err_len, "密码认证成功（IP: %s, 端口: %d, 用户: %s）",
        ip, port_num, user);
    return true;
}


// 使用密钥连接
bool l4d2_ssh_connect_with_key(L4D2_SSH_Context* ctx, const char* ip, int port,
    const char* user, const char* key_path,
    const char* key_pass,  // 新增：密钥密码（可为NULL）
    char* err_msg, int err_len) {
    if (!ctx || !ctx->session || !ip || !user || !key_path) {
        snprintf(err_msg, err_len, "无效的参数（上下文、会话或路径为空）");
        return false;
    }

    // 1. 检查密钥文件是否存在
    FILE* key_file = fopen(key_path, "r");
    if (!key_file) {
        snprintf(err_msg, err_len, "无法打开私钥文件: %s", strerror(errno));
        return false;
    }
    fclose(key_file);

    // 2. 设置连接参数
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);
    ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);

    // 3. 建立TCP连接
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "TCP连接失败: %s", ssh_get_error(ctx->session));
        return false;
    }

    // 4. 手动读取私钥（使用密钥密码解锁）
    ssh_key priv_key = NULL;
    rc = ssh_pki_import_privkey_file(
        key_path,          // 私钥文件路径
        key_pass,          // 新增：使用传入的密钥密码解锁（无密码则为NULL）
        NULL, NULL,        // 回调函数（无需处理时为NULL）
        &priv_key          // 输出：解析后的私钥
    );
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "解析私钥失败（密码错误或密钥损坏）: %s",
            ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    // 5. 使用解析后的私钥进行认证
    rc = ssh_userauth_publickey(ctx->session, user, priv_key);
    ssh_key_free(priv_key);  // 释放私钥资源

    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "密钥认证失败: %s", ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    // 6. 认证成功
    ctx->is_connected = true;
    snprintf(err_msg, err_len, "密钥认证成功（IP: %s, 端口: %d, 用户: %s）",
        ip, port, user);
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
bool upload_file_txt(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
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

    // 检查远程文件是否存在，若存在则删除
    char remote_path_tmp[256];
    GBKtoU8(remote_path, remote_path_tmp, 256);
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path_tmp);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        // 尝试删除远程文件
        if (sftp_unlink(sftp, remote_path_tmp) != SSH_OK) {
            snprintf(err_msg, err_len, "删除远程文件失败: %s", sftp_get_error(sftp));
            sftp_free(sftp);
            remove(temp_path);
            return false;
        }
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
    sftp_file remote_file = sftp_open(sftp, remote_path_tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

// 上传一般文件到远程服务器（不处理换行符，直接二进制传输）
bool upload_file_normal(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
    // 检查本地文件是否存在
    struct stat file_stat;
    if (stat(local_path, &file_stat) != 0) {
        snprintf(err_msg, err_len, "无法访问本地文件: %s", local_path);
        return false;
    }

    // 初始化SFTP会话
    sftp_session sftp = sftp_new(session);
    if (!sftp) {
        snprintf(err_msg, err_len, "创建SFTP会话失败: %s", ssh_get_error(session));
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "初始化SFTP失败: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        return false;
    }

    // 检查远程文件是否存在，若存在则删除
    char remote_path_tep[256];
    GBKtoU8(remote_path, remote_path_tep, 256);
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path_tep);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        // 尝试删除远程文件
        if (sftp_unlink(sftp, remote_path) != SSH_OK) {
            snprintf(err_msg, err_len, "删除远程文件失败: %s", sftp_get_error(sftp));
            sftp_free(sftp);
            return false;
        }
    }

    // 打开本地文件（二进制模式）
    FILE* local_file;
    if (fopen_s(&local_file, local_path, "rb") != 0) {
        snprintf(err_msg, err_len, "无法打开本地文件: %s", local_path);
        sftp_free(sftp);
        return false;
    }

    // 创建远程文件
    sftp_file remote_file = sftp_open(sftp, remote_path_tep, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!remote_file) {
        snprintf(err_msg, err_len, "创建远程文件失败: %s", sftp_get_error(sftp));
        fclose(local_file);
        sftp_free(sftp);
        return false;
    }

    // 传输文件内容（二进制方式，不做任何转换）
    const size_t BUFFER_SIZE = 65536;  // ssh协议对缓冲区大小有限制
    char* buffer = (char*)malloc(BUFFER_SIZE);
    size_t nread;
    ssize_t nwrite;
    while ((nread = fread(buffer, 1, BUFFER_SIZE, local_file)) > 0) {
        nwrite = sftp_write(remote_file, buffer, nread);
        if (nwrite < 0) {
            snprintf(err_msg, err_len, "文件传输失败: %s", sftp_get_error(sftp));
            // 清理资源
            fclose(local_file);
            sftp_close(remote_file);
            sftp_free(sftp);
            free(buffer);
            return false;
        }
    }

    // 检查本地文件读取是否出错
    if (ferror(local_file) != 0) {
        snprintf(err_msg, err_len, "读取本地文件时出错");
        fclose(local_file);
        sftp_close(remote_file);
        sftp_free(sftp);
        return false;
    }

    // 清理资源
    free(buffer);
    fclose(local_file);
    sftp_close(remote_file);
    sftp_free(sftp);

    snprintf(err_msg, err_len, "上传成功");
    return true;
}

// 检查并上传API脚本
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "未连接SSH服务器");
        return false;
    }

    // 获取远程根目录路径
    std::string remote_root = GetRemoteRootPath();

    // 构建远程目录和文件路径
    std::string remote_dir = remote_root;
    std::string remote_file = remote_root + "/L4D2_Manager_API.sh";
    const char* local_file = "scripts/L4D2_Manager_API.sh";  // 本地脚本路径

    // 检查并创建远程目录
    if (!check_remote_dir(ctx->session, remote_dir.c_str(), err_msg, err_len)) {
        if (!create_remote_dir(ctx->session, remote_dir.c_str(), err_msg, err_len)) {
            return false;
        }
    }

    // 初始化SFTP会话用于文件操作
    sftp_session sftp = sftp_new(ctx->session);
    if (!sftp) {
        snprintf(err_msg, err_len, "创建SFTP会话失败: %s", ssh_get_error(ctx->session));
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "初始化SFTP失败: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        return false;
    }

    bool need_upload = true;
    char* remote_content = nullptr;
    size_t remote_len = 0;
    char* local_content = nullptr;
    size_t local_len = 0;

    // 检查远程文件是否存在
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_file.c_str());
    bool file_exists = (remote_attrs != nullptr);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
    }

    // 如果文件存在，读取远程文件内容
    if (file_exists) {
        if (!read_remote_file_content(sftp, remote_file.c_str(), &remote_content, &remote_len, err_msg, err_len)) {
            // 读取远程文件失败，视为需要上传
            need_upload = true;
        }
        else {
            // 处理本地文件（转换CRLF为LF）
            if (!process_local_file(local_file, &local_content, &local_len, err_msg, err_len)) {
                free(remote_content);
                sftp_free(sftp);
                return false;
            }

            // 对比内容
            if (remote_len == local_len && memcmp(remote_content, local_content, remote_len) == 0) {
                need_upload = false;  // 内容相同，不需要上传
                snprintf(err_msg, err_len, "服务器脚本内容为最新，不需要上传");
            }
            else {
                need_upload = true;   // 内容不同，需要上传
            }
        }
    }

    // 释放内容缓冲区
    free(remote_content);
    free(local_content);

    // 需要上传时执行上传操作
    if (need_upload) {
        if (!upload_file_txt(ctx->session, local_file, remote_file.c_str(), err_msg, err_len)) {
            sftp_free(sftp);
            return false;  // 上传失败直接返回
        }

        // 授予脚本可执行权限
        char chmod_cmd[512];  // 增大缓冲区防止路径过长
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", remote_file.c_str());
        ssh_channel channel = ssh_channel_new(ctx->session);
        if (!channel) {
            snprintf(err_msg, err_len, "创建通道失败（授权权限时）");
            sftp_free(sftp);
            return false;
        }

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            snprintf(err_msg, err_len, "打开通道失败（授权权限时）: %s", ssh_get_error(ctx->session));
            ssh_channel_free(channel);
            sftp_free(sftp);
            return false;
        }

        rc = ssh_channel_request_exec(channel, chmod_cmd);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        if (rc != SSH_OK) {
            snprintf(err_msg, err_len, "授予执行权限失败: %s", ssh_get_error(ctx->session));
            sftp_free(sftp);
            return false;
        }

        snprintf(err_msg, err_len, "服务器脚本不存在或不为最新，上传/更新成功");
    }
    // 释放SFTP会话
    sftp_free(sftp);
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
    char cmd_u8[256];
    GBKtoU8(cmd, cmd_u8, 256);
    rc = ssh_channel_request_exec(channel, cmd_u8);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "执行命令失败: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // 读取命令输出
    if ((output != nullptr) && (output_len != 0)) {
        output[0] = '\0';
        char buffer_u8[1024];
        char buffer_gbk[1024];
        int nbytes;
        while ((nbytes = ssh_channel_read(channel, buffer_u8, sizeof(buffer_u8) - 1, 0)) > 0) {
            buffer_u8[nbytes] = '\0';
            U8toGBK(buffer_u8, buffer_gbk, 1024);
            // 使用更安全的字符串拼接方式
            size_t current_len = strlen(output);
            if (current_len + nbytes < (size_t)output_len - 1) {
                strcat_s(output, output_len, buffer_gbk);
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


bool l4d2_ssh_exec_command_rl(L4D2_SSH_Context* ctx, const char* cmd,
    void (*line_callback)(HWND, const char*),
    HWND hWnd, char* err_msg, int err_len) {
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

    // 准备命令（编码转换）
    char cmd_u8[256];
    GBKtoU8(cmd, cmd_u8, 256);
    rc = ssh_channel_request_exec(channel, cmd_u8);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "执行命令失败: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // 逐行读取输出
    char buffer_u8[1024];
    char buffer_gbk[1024];
    char line_buf[2048] = { 0 };
    int nbytes;
    int line_pos = 0;

    while ((nbytes = ssh_channel_read(channel, buffer_u8, sizeof(buffer_u8) - 1, 0)) > 0) {
        buffer_u8[nbytes] = '\0';
        U8toGBK(buffer_u8, buffer_gbk, 1024);

        // 按行分割
        for (int i = 0; i < nbytes && buffer_gbk[i] != '\0'; i++) {
            if (buffer_gbk[i] == '\n' || buffer_gbk[i] == '\r') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';
                    // 调用行处理回调函数
                    if (line_callback) {
                        line_callback(hWnd, line_buf);
                    }
                    line_pos = 0;
                    memset(line_buf, 0, sizeof(line_buf));
                }
            }
            else {
                if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = buffer_gbk[i];
                }
            }
        }
    }

    // 处理最后一行（如果存在）
    if (line_pos > 0) {
        line_buf[line_pos] = '\0';
        if (line_callback) {
            line_callback(hWnd, line_buf);
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
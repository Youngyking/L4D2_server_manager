/*
ͨ����װ�ײ�Э��ʵ��ϸ�ڣ�ʵ�ֶ�manager.cpp��gui.cpp�ĵײ�Э��֧��
*/

#define WIN32_LEAN_AND_MEAN  // ���� Windows �ɰ�ͷ�ļ��е����ඨ��
#define _CRT_SECURE_NO_WARNINGS //����ʹ��fopen

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

// �����ⲿ����
std::string GetRemoteRootPath();

// ��GBK����ת��ΪUTF-8����
static char* GBKtoU8(const char* gbk_str, char* u8_buf, int buf_len) {
    if (!gbk_str || !u8_buf || buf_len <= 0) {
        return nullptr; // ��Ч�������
    }

    // ����1: �Ƚ�GBKת��ΪUTF-16���м���ɱ��룩
    int u16_len = MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // GBK������Ч
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // ת��UTF-16ʧ��
    }

    // ����2: ��UTF-16ת��ΪUTF-8
    int u8_len = WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (u8_len == 0 || u8_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // �����������ת��ʧ��
    }

    WideCharToMultiByte(CP_UTF8, 0, u16_buf, -1, u8_buf, buf_len, nullptr, nullptr);
    delete[] u16_buf;
    return u8_buf;
}

// ��UTF-8����ת��ΪGBK����
static char* U8toGBK(const char* u8_str, char* gbk_buf, int buf_len) {
    if (!u8_str || !gbk_buf || buf_len <= 0) {
        return nullptr; // ��Ч�������
    }

    // ����1: �Ƚ�UTF-8ת��ΪUTF-16���м���ɱ��룩
    int u16_len = MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, nullptr, 0);
    if (u16_len == 0) {
        return nullptr; // UTF-8������Ч
    }

    WCHAR* u16_buf = new WCHAR[u16_len];
    if (!MultiByteToWideChar(CP_UTF8, 0, u8_str, -1, u16_buf, u16_len)) {
        delete[] u16_buf;
        return nullptr; // ת��UTF-16ʧ��
    }

    // ����2: ��UTF-16ת��ΪGBK
    int gbk_len = WideCharToMultiByte(CP_ACP, 0, u16_buf, -1, nullptr, 0, nullptr, nullptr);
    if (gbk_len == 0 || gbk_len > buf_len) {
        delete[] u16_buf;
        return nullptr; // �����������ת��ʧ��
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
            // ����CR��ֻ����LF
            continue;
        }
        fputc(c, out_file);
    }

    fclose(in_file);
    fclose(out_file);
    return true;
}

// ��ȡԶ���ļ����ݵ����������ڲ�����������
static bool read_remote_file_content(sftp_session sftp, const char* remote_path,
    char** content, size_t* len,
    char* err_msg, int err_len) {
    sftp_file file = sftp_open(sftp, remote_path, O_RDONLY, 0);
    if (!file) {
        snprintf(err_msg, err_len, "��Զ���ļ�ʧ��: %s", sftp_get_error(sftp));
        return false;
    }

    // ��̬��������ȡ�ļ�����
    size_t buffer_size = 4096;
    *len = 0;
    *content = (char*)malloc(buffer_size);
    if (!*content) {
        snprintf(err_msg, err_len, "�ڴ����ʧ��");
        sftp_close(file);
        return false;
    }

    ssize_t nread;
    while ((nread = sftp_read(file, *content + *len, buffer_size - *len)) > 0) {
        *len += nread;
        // ����������ʱ����
        if (*len >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buf = (char*)realloc(*content, buffer_size);
            if (!new_buf) {
                snprintf(err_msg, err_len, "�ڴ��ط���ʧ��");
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
        snprintf(err_msg, err_len, "��ȡԶ���ļ�ʧ��: %s", sftp_get_error(sftp));
        free(*content);
        *content = nullptr;
        *len = 0;
        sftp_close(file);
        return false;
    }

    // ������������ʵ�ʴ�С
    char* new_buf = (char*)realloc(*content, *len);
    if (new_buf) {
        *content = new_buf;
    }

    sftp_close(file);
    return true;
}

// �������ļ���ת��CRLFΪLF������ȡ���ݣ��ڲ�����������
static bool process_local_file(const char* local_path, char** content, size_t* len,
    char* err_msg, int err_len) {
    FILE* in_file = fopen(local_path, "rb");
    if (!in_file) {
        snprintf(err_msg, err_len, "�޷��򿪱����ļ�: %s", local_path);
        return false;
    }

    // ��̬��������ȡ�������з�
    size_t buffer_size = 4096;
    *len = 0;
    *content = (char*)malloc(buffer_size);
    if (!*content) {
        snprintf(err_msg, err_len, "�ڴ����ʧ��");
        fclose(in_file);
        return false;
    }

    int c;
    while ((c = fgetc(in_file)) != EOF) {
        if (c == '\r') {
            continue;  // ����CR��ֻ����LF
        }
        // ����������ʱ����
        if (*len >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buf = (char*)realloc(*content, buffer_size);
            if (!new_buf) {
                snprintf(err_msg, err_len, "�ڴ��ط���ʧ��");
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

    // ������������ʵ�ʴ�С
    char* new_buf = (char*)realloc(*content, *len);
    if (new_buf) {
        *content = new_buf;
    }

    return true;
}

// ��ʼ��SSH������
L4D2_SSH_Context* l4d2_ssh_init() {
    L4D2_SSH_Context* ctx = (L4D2_SSH_Context*)malloc(sizeof(L4D2_SSH_Context));
    if (!ctx) return NULL;
    ctx->session = ssh_new();
    ctx->is_connected = false;
    return ctx;
}


// ֧��IP+�˿�+�����SSH���Ӻ���
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, int port_num,
    const char* user, const char* pass,
    char* err_msg, int err_len) {
    // �����Ϸ��Լ��
    if (!ctx || !ctx->session || !ip || !user) {
        snprintf(err_msg, err_len, "��Ч�Ĳ����������Ļ�Ựδ��ʼ����");
        return false;
    }

    // ����SSH����ѡ��������˿ڡ��û�����
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);               // ����IP��ַ
    ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port_num);        // ���ö˿ڣ�ע�⴫��ַ��
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);             // �����û���

    // ����TCP����
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "TCP����ʧ��: %s��IP: %s, �˿�: %d��",
            ssh_get_error(ctx->session), ip, port_num);
        return false;
    }

    // ������֤
    rc = ssh_userauth_password(ctx->session, NULL, pass);
    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "������֤ʧ��: %s���û�: %s��",
            ssh_get_error(ctx->session), user);
        ssh_disconnect(ctx->session);  // ��֤ʧ����Ͽ�����
        return false;
    }

    // ���ӳɹ�
    ctx->is_connected = true;
    snprintf(err_msg, err_len, "������֤�ɹ���IP: %s, �˿�: %d, �û�: %s��",
        ip, port_num, user);
    return true;
}


// ʹ����Կ����
bool l4d2_ssh_connect_with_key(L4D2_SSH_Context* ctx, const char* ip, int port,
    const char* user, const char* key_path,
    const char* key_pass,  // ��������Կ���루��ΪNULL��
    char* err_msg, int err_len) {
    if (!ctx || !ctx->session || !ip || !user || !key_path) {
        snprintf(err_msg, err_len, "��Ч�Ĳ����������ġ��Ự��·��Ϊ�գ�");
        return false;
    }

    // 1. �����Կ�ļ��Ƿ����
    FILE* key_file = fopen(key_path, "r");
    if (!key_file) {
        snprintf(err_msg, err_len, "�޷���˽Կ�ļ�: %s", strerror(errno));
        return false;
    }
    fclose(key_file);

    // 2. �������Ӳ���
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);
    ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);

    // 3. ����TCP����
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "TCP����ʧ��: %s", ssh_get_error(ctx->session));
        return false;
    }

    // 4. �ֶ���ȡ˽Կ��ʹ����Կ���������
    ssh_key priv_key = NULL;
    rc = ssh_pki_import_privkey_file(
        key_path,          // ˽Կ�ļ�·��
        key_pass,          // ������ʹ�ô������Կ�����������������ΪNULL��
        NULL, NULL,        // �ص����������账��ʱΪNULL��
        &priv_key          // ������������˽Կ
    );
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "����˽Կʧ�ܣ�����������Կ�𻵣�: %s",
            ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    // 5. ʹ�ý������˽Կ������֤
    rc = ssh_userauth_publickey(ctx->session, user, priv_key);
    ssh_key_free(priv_key);  // �ͷ�˽Կ��Դ

    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "��Կ��֤ʧ��: %s", ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    // 6. ��֤�ɹ�
    ctx->is_connected = true;
    snprintf(err_msg, err_len, "��Կ��֤�ɹ���IP: %s, �˿�: %d, �û�: %s��",
        ip, port, user);
    return true;
}


// ���Զ��Ŀ¼�Ƿ����
bool check_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "test -d %s", dir);
    ssh_channel channel = ssh_channel_new(session);
    if (!channel) {
        snprintf(err_msg, err_len, "����ͨ��ʧ��");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ͨ��ʧ��: %s", ssh_get_error(session));
        ssh_channel_free(channel);
        return false;
    }

    // ��ssh_channel_exec�滻Ϊssh_channel_request_exec
    rc = ssh_channel_request_exec(channel, cmd);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "ִ������ʧ��: %s", ssh_get_error(session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return exit_status == 0;  // 0��ʾĿ¼����
}

// ����Զ��Ŀ¼
bool create_remote_dir(ssh_session session, const char* dir, char* err_msg, int err_len) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
    ssh_channel channel = ssh_channel_new(session);
    if (!channel) return false;

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "����Ŀ¼ʧ��: %s", ssh_get_error(session));
        ssh_channel_free(channel);
        return false;
    }

    // ��ssh_channel_exec�滻Ϊssh_channel_request_exec
    rc = ssh_channel_request_exec(channel, cmd);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return rc == SSH_OK;
}

// �ϴ��ļ���Զ�̷������������з�ת�����ܣ�
bool upload_file_txt(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
    // ������ʱ�ļ�·��
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", local_path);

    // ת��CRLFΪLF
    FILE* in_file = fopen(local_path, "rb");
    if (!in_file) {
        snprintf(err_msg, err_len, "�޷��򿪱����ļ�: %s", local_path);
        return false;
    }

    FILE* out_file = fopen(temp_path, "wb");
    if (!out_file) {
        snprintf(err_msg, err_len, "�޷�������ʱ�ļ�: %s", temp_path);
        fclose(in_file);
        return false;
    }

    int c;
    while ((c = fgetc(in_file)) != EOF) {
        if (c == '\r') {
            // ����CR��ֻ����LF
            continue;
        }
        fputc(c, out_file);
    }

    fclose(in_file);
    fclose(out_file);

    // �����ʱ�ļ��Ƿ����
    struct stat file_stat;
    if (stat(temp_path, &file_stat) != 0) {
        snprintf(err_msg, err_len, "�޷�������ʱ�ļ�: %s", temp_path);
        remove(temp_path);
        return false;
    }

    // ��ʼ��SFTP�Ự
    sftp_session sftp = sftp_new(session);
    if (!sftp) {
        snprintf(err_msg, err_len, "����SFTP�Ựʧ��: %s", ssh_get_error(session));
        remove(temp_path);
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ʼ��SFTPʧ��: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // ���Զ���ļ��Ƿ���ڣ���������ɾ��
    char remote_path_tmp[256];
    GBKtoU8(remote_path, remote_path_tmp, 256);
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path_tmp);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        // ����ɾ��Զ���ļ�
        if (sftp_unlink(sftp, remote_path_tmp) != SSH_OK) {
            snprintf(err_msg, err_len, "ɾ��Զ���ļ�ʧ��: %s", sftp_get_error(sftp));
            sftp_free(sftp);
            remove(temp_path);
            return false;
        }
    }

    // ����ʱ�ļ����ж�ȡ
    FILE* local_file;
    if (fopen_s(&local_file, temp_path, "rb") != 0) {
        snprintf(err_msg, err_len, "����ʱ�ļ�ʧ��");
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // ����Զ���ļ�
    sftp_file remote_file = sftp_open(sftp, remote_path_tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!remote_file) {
        snprintf(err_msg, err_len, "����Զ���ļ�ʧ��: %s", sftp_get_error(sftp));
        fclose(local_file);
        sftp_free(sftp);
        remove(temp_path);
        return false;
    }

    // �����ļ�����
    char buffer[4096];
    size_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), local_file)) > 0) {
        if (sftp_write(remote_file, buffer, nread) != (int)nread) {
            snprintf(err_msg, err_len, "�ļ�����ʧ��: %s", sftp_get_error(sftp));
            fclose(local_file);
            sftp_close(remote_file);
            sftp_free(sftp);
            remove(temp_path);
            return false;
        }
    }

    // ������Դ
    fclose(local_file);
    sftp_close(remote_file);
    sftp_free(sftp);
    remove(temp_path); // ɾ����ʱ�ļ�

    snprintf(err_msg, err_len, "�ļ��ϴ��ɹ�");
    return true;
}

// �ϴ�һ���ļ���Զ�̷��������������з���ֱ�Ӷ����ƴ��䣩
bool upload_file_normal(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
    // ��鱾���ļ��Ƿ����
    struct stat file_stat;
    if (stat(local_path, &file_stat) != 0) {
        snprintf(err_msg, err_len, "�޷����ʱ����ļ�: %s", local_path);
        return false;
    }

    // ��ʼ��SFTP�Ự
    sftp_session sftp = sftp_new(session);
    if (!sftp) {
        snprintf(err_msg, err_len, "����SFTP�Ựʧ��: %s", ssh_get_error(session));
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ʼ��SFTPʧ��: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        return false;
    }

    // ���Զ���ļ��Ƿ���ڣ���������ɾ��
    char remote_path_tep[256];
    GBKtoU8(remote_path, remote_path_tep, 256);
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path_tep);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        // ����ɾ��Զ���ļ�
        if (sftp_unlink(sftp, remote_path) != SSH_OK) {
            snprintf(err_msg, err_len, "ɾ��Զ���ļ�ʧ��: %s", sftp_get_error(sftp));
            sftp_free(sftp);
            return false;
        }
    }

    // �򿪱����ļ���������ģʽ��
    FILE* local_file;
    if (fopen_s(&local_file, local_path, "rb") != 0) {
        snprintf(err_msg, err_len, "�޷��򿪱����ļ�: %s", local_path);
        sftp_free(sftp);
        return false;
    }

    // ����Զ���ļ�
    sftp_file remote_file = sftp_open(sftp, remote_path_tep, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!remote_file) {
        snprintf(err_msg, err_len, "����Զ���ļ�ʧ��: %s", sftp_get_error(sftp));
        fclose(local_file);
        sftp_free(sftp);
        return false;
    }

    // �����ļ����ݣ������Ʒ�ʽ�������κ�ת����
    const size_t BUFFER_SIZE = 65536;  // sshЭ��Ի�������С������
    char* buffer = (char*)malloc(BUFFER_SIZE);
    size_t nread;
    ssize_t nwrite;
    while ((nread = fread(buffer, 1, BUFFER_SIZE, local_file)) > 0) {
        nwrite = sftp_write(remote_file, buffer, nread);
        if (nwrite < 0) {
            snprintf(err_msg, err_len, "�ļ�����ʧ��: %s", sftp_get_error(sftp));
            // ������Դ
            fclose(local_file);
            sftp_close(remote_file);
            sftp_free(sftp);
            free(buffer);
            return false;
        }
    }

    // ��鱾���ļ���ȡ�Ƿ����
    if (ferror(local_file) != 0) {
        snprintf(err_msg, err_len, "��ȡ�����ļ�ʱ����");
        fclose(local_file);
        sftp_close(remote_file);
        sftp_free(sftp);
        return false;
    }

    // ������Դ
    free(buffer);
    fclose(local_file);
    sftp_close(remote_file);
    sftp_free(sftp);

    snprintf(err_msg, err_len, "�ϴ��ɹ�");
    return true;
}

// ��鲢�ϴ�API�ű�
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "δ����SSH������");
        return false;
    }

    // ��ȡԶ�̸�Ŀ¼·��
    std::string remote_root = GetRemoteRootPath();

    // ����Զ��Ŀ¼���ļ�·��
    std::string remote_dir = remote_root;
    std::string remote_file = remote_root + "/L4D2_Manager_API.sh";
    const char* local_file = "scripts/L4D2_Manager_API.sh";  // ���ؽű�·��

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(ctx->session, remote_dir.c_str(), err_msg, err_len)) {
        if (!create_remote_dir(ctx->session, remote_dir.c_str(), err_msg, err_len)) {
            return false;
        }
    }

    // ��ʼ��SFTP�Ự�����ļ�����
    sftp_session sftp = sftp_new(ctx->session);
    if (!sftp) {
        snprintf(err_msg, err_len, "����SFTP�Ựʧ��: %s", ssh_get_error(ctx->session));
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ʼ��SFTPʧ��: %s", sftp_get_error(sftp));
        sftp_free(sftp);
        return false;
    }

    bool need_upload = true;
    char* remote_content = nullptr;
    size_t remote_len = 0;
    char* local_content = nullptr;
    size_t local_len = 0;

    // ���Զ���ļ��Ƿ����
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_file.c_str());
    bool file_exists = (remote_attrs != nullptr);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
    }

    // ����ļ����ڣ���ȡԶ���ļ�����
    if (file_exists) {
        if (!read_remote_file_content(sftp, remote_file.c_str(), &remote_content, &remote_len, err_msg, err_len)) {
            // ��ȡԶ���ļ�ʧ�ܣ���Ϊ��Ҫ�ϴ�
            need_upload = true;
        }
        else {
            // �������ļ���ת��CRLFΪLF��
            if (!process_local_file(local_file, &local_content, &local_len, err_msg, err_len)) {
                free(remote_content);
                sftp_free(sftp);
                return false;
            }

            // �Ա�����
            if (remote_len == local_len && memcmp(remote_content, local_content, remote_len) == 0) {
                need_upload = false;  // ������ͬ������Ҫ�ϴ�
                snprintf(err_msg, err_len, "�������ű�����Ϊ���£�����Ҫ�ϴ�");
            }
            else {
                need_upload = true;   // ���ݲ�ͬ����Ҫ�ϴ�
            }
        }
    }

    // �ͷ����ݻ�����
    free(remote_content);
    free(local_content);

    // ��Ҫ�ϴ�ʱִ���ϴ�����
    if (need_upload) {
        if (!upload_file_txt(ctx->session, local_file, remote_file.c_str(), err_msg, err_len)) {
            sftp_free(sftp);
            return false;  // �ϴ�ʧ��ֱ�ӷ���
        }

        // ����ű���ִ��Ȩ��
        char chmod_cmd[512];  // ���󻺳�����ֹ·������
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", remote_file.c_str());
        ssh_channel channel = ssh_channel_new(ctx->session);
        if (!channel) {
            snprintf(err_msg, err_len, "����ͨ��ʧ�ܣ���ȨȨ��ʱ��");
            sftp_free(sftp);
            return false;
        }

        rc = ssh_channel_open_session(channel);
        if (rc != SSH_OK) {
            snprintf(err_msg, err_len, "��ͨ��ʧ�ܣ���ȨȨ��ʱ��: %s", ssh_get_error(ctx->session));
            ssh_channel_free(channel);
            sftp_free(sftp);
            return false;
        }

        rc = ssh_channel_request_exec(channel, chmod_cmd);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        if (rc != SSH_OK) {
            snprintf(err_msg, err_len, "����ִ��Ȩ��ʧ��: %s", ssh_get_error(ctx->session));
            sftp_free(sftp);
            return false;
        }

        snprintf(err_msg, err_len, "�������ű������ڻ�Ϊ���£��ϴ�/���³ɹ�");
    }
    // �ͷ�SFTP�Ự
    sftp_free(sftp);
    return true;
}

// ִ��Զ�������ȡ���
bool l4d2_ssh_exec_command(L4D2_SSH_Context* ctx, const char* cmd, char* output, int output_len, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "δ����SSH����");
        return false;
    }

    ssh_channel channel = ssh_channel_new(ctx->session);
    if (!channel) {
        snprintf(err_msg, err_len, "����ͨ��ʧ��");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ͨ��ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_free(channel);
        return false;
    }

    // ��ssh_channel_exec�滻Ϊssh_channel_request_exec
    char cmd_u8[256];
    GBKtoU8(cmd, cmd_u8, 256);
    rc = ssh_channel_request_exec(channel, cmd_u8);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "ִ������ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // ��ȡ�������
    if ((output != nullptr) && (output_len != 0)) {
        output[0] = '\0';
        char buffer_u8[1024];
        char buffer_gbk[1024];
        int nbytes;
        while ((nbytes = ssh_channel_read(channel, buffer_u8, sizeof(buffer_u8) - 1, 0)) > 0) {
            buffer_u8[nbytes] = '\0';
            U8toGBK(buffer_u8, buffer_gbk, 1024);
            // ʹ�ø���ȫ���ַ���ƴ�ӷ�ʽ
            size_t current_len = strlen(output);
            if (current_len + nbytes < (size_t)output_len - 1) {
                strcat_s(output, output_len, buffer_gbk);
            }
            else {
                break; // ��ֹ���������
            }
        }

        if (nbytes < 0) {
            snprintf(err_msg, err_len, "��ȡ���ʧ��: %s", ssh_get_error(ctx->session));
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return false;
        }
    }
    

    ssh_channel_close(channel);
    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_free(channel);

    if (exit_status != 0) {
        snprintf(err_msg, err_len, "����ִ��ʧ�ܣ��˳���: %d��", exit_status);
        return false;
    }

    return true;
}

// �Ͽ����Ӳ��ͷ���Դ
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
        snprintf(err_msg, err_len, "δ����SSH����");
        return false;
    }

    ssh_channel channel = ssh_channel_new(ctx->session);
    if (!channel) {
        snprintf(err_msg, err_len, "����ͨ��ʧ��");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ͨ��ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_free(channel);
        return false;
    }

    // ׼���������ת����
    char cmd_u8[256];
    GBKtoU8(cmd, cmd_u8, 256);
    rc = ssh_channel_request_exec(channel, cmd_u8);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "ִ������ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // ���ж�ȡ���
    char buffer_u8[1024];
    char buffer_gbk[1024];
    char line_buf[2048] = { 0 };
    int nbytes;
    int line_pos = 0;

    while ((nbytes = ssh_channel_read(channel, buffer_u8, sizeof(buffer_u8) - 1, 0)) > 0) {
        buffer_u8[nbytes] = '\0';
        U8toGBK(buffer_u8, buffer_gbk, 1024);

        // ���зָ�
        for (int i = 0; i < nbytes && buffer_gbk[i] != '\0'; i++) {
            if (buffer_gbk[i] == '\n' || buffer_gbk[i] == '\r') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';
                    // �����д���ص�����
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

    // �������һ�У�������ڣ�
    if (line_pos > 0) {
        line_buf[line_pos] = '\0';
        if (line_callback) {
            line_callback(hWnd, line_buf);
        }
    }

    if (nbytes < 0) {
        snprintf(err_msg, err_len, "��ȡ���ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    ssh_channel_close(channel);
    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_free(channel);

    if (exit_status != 0) {
        snprintf(err_msg, err_len, "����ִ��ʧ�ܣ��˳���: %d��", exit_status);
        return false;
    }

    return true;
}
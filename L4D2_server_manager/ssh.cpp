/*
ͨ����װ�ײ�Э��ʵ��ϸ�ڣ�ʵ�ֶ�manager.cpp��gui.cpp�ĵײ�Э��֧��
*/

#define WIN32_LEAN_AND_MEAN  // ���� Windows �ɰ�ͷ�ļ��е����ඨ��
#define _CRT_SECURE_NO_WARNINGS //����ʹ��fopen

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
            // ����CR��ֻ����LF
            continue;
        }
        fputc(c, out_file);
    }

    fclose(in_file);
    fclose(out_file);
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

// ���ӵ�SSH������
bool l4d2_ssh_connect(L4D2_SSH_Context* ctx, const char* ip, const char* user, const char* pass, char* err_msg, int err_len) {
    if (!ctx || !ctx->session) {
        snprintf(err_msg, err_len, "SSH������δ��ʼ��");
        return false;
    }

    // �������Ӳ���
    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, ip);
    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);

    // ��������
    int rc = ssh_connect(ctx->session);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "����ʧ��: %s", ssh_get_error(ctx->session));
        return false;
    }

    // ������֤
    rc = ssh_userauth_password(ctx->session, NULL, pass);
    if (rc != SSH_AUTH_SUCCESS) {
        snprintf(err_msg, err_len, "��֤ʧ��: %s", ssh_get_error(ctx->session));
        ssh_disconnect(ctx->session);
        return false;
    }

    ctx->is_connected = true;
    snprintf(err_msg, err_len, "���ӳɹ�");
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
bool upload_file(ssh_session session, const char* local_path, const char* remote_path, char* err_msg, int err_len) {
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

    // ���Զ���ļ��Ƿ��Ѵ���
    sftp_attributes remote_attrs = sftp_stat(sftp, remote_path);
    if (remote_attrs) {
        sftp_attributes_free(remote_attrs);
        snprintf(err_msg, err_len, "Զ���ļ��Ѵ��ڣ������ϴ�");
        sftp_free(sftp);
        remove(temp_path);
        return true;
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
    sftp_file remote_file = sftp_open(sftp, remote_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

// ��鲢�ϴ�API�ű�
bool l4d2_ssh_upload_api_script(L4D2_SSH_Context* ctx, char* err_msg, int err_len) {
    if (!ctx || !ctx->is_connected) {
        snprintf(err_msg, err_len, "δ����SSH������");
        return false;
    }

    const char* remote_dir = "/home/L4D2_Manager";
    const char* remote_file = "/home/L4D2_Manager/L4D2_Manager_API.sh";
    const char* local_file = "scripts/L4D2_Manager_API.sh";  // ���ؽű�·��

    // ��鲢����Զ��Ŀ¼
    if (!check_remote_dir(ctx->session, remote_dir, err_msg, err_len)) {
        if (!create_remote_dir(ctx->session, remote_dir, err_msg, err_len)) {
            return false;
        }
    }

    // �ϴ��ű��ļ�
    if (!upload_file(ctx->session, local_file, remote_file, err_msg, err_len)) {
        return false;  // �ϴ�ʧ��ֱ�ӷ���
    }

    // ����������ű���ִ��Ȩ�ޣ��ؼ��޸ģ�
    char chmod_cmd[256];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x %s", remote_file);
    ssh_channel channel = ssh_channel_new(ctx->session);
    if (!channel) {
        snprintf(err_msg, err_len, "����ͨ��ʧ�ܣ���ȨȨ��ʱ��");
        return false;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "��ͨ��ʧ�ܣ���ȨȨ��ʱ��: %s", ssh_get_error(ctx->session));
        ssh_channel_free(channel);
        return false;
    }

    rc = ssh_channel_request_exec(channel, chmod_cmd);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "����ִ��Ȩ��ʧ��: %s", ssh_get_error(ctx->session));
        return false;
    }

    snprintf(err_msg, err_len, "�ű��ϴ�������ִ��Ȩ�޳ɹ�");

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
    rc = ssh_channel_request_exec(channel, cmd);
    if (rc != SSH_OK) {
        snprintf(err_msg, err_len, "ִ������ʧ��: %s", ssh_get_error(ctx->session));
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return false;
    }

    // ��ȡ�������
    output[0] = '\0';
    char buffer[1024];
    int nbytes;
    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[nbytes] = '\0';
        // ʹ�ø���ȫ���ַ���ƴ�ӷ�ʽ
        size_t current_len = strlen(output);
        if (current_len + nbytes < (size_t)output_len - 1) {
            strcat_s(output, output_len, buffer);
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

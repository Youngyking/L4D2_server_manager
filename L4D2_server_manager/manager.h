// manager.h
#ifndef MANAGER_H
#define MANAGER_H

#include <windows.h>
#include "ssh.h"

extern L4D2_SSH_Context* g_ssh_ctx;

// 原有函数声明（保持不变）
DWORD WINAPI HandleConnectRequest(LPVOID param);
DWORD WINAPI HandleDeployServer(LPVOID param);
DWORD WINAPI HandleGetStatus(LPVOID param);
DWORD WINAPI HandleGetInstances(LPVOID param);

// 新增函数声明
DWORD WINAPI HandleStartInstance(LPVOID param);
DWORD WINAPI HandleStopInstance(LPVOID param);

#endif
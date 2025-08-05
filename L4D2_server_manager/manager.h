// manager.h
#ifndef MANAGER_H
#define MANAGER_H

#include <windows.h>
#include "ssh.h"

extern L4D2_SSH_Context* g_ssh_ctx;

// ԭ�к������������ֲ��䣩
DWORD WINAPI HandleConnectRequest(LPVOID param);
DWORD WINAPI HandleDeployServer(LPVOID param);
DWORD WINAPI HandleGetStatus(LPVOID param);
DWORD WINAPI HandleGetInstances(LPVOID param);
DWORD WINAPI HandleUploadSourceMod(LPVOID param);
DWORD WINAPI HandleUploadMetaMod(LPVOID param);
DWORD WINAPI HandleStartInstance(LPVOID param);
DWORD WINAPI HandleStopInstance(LPVOID param);

#endif
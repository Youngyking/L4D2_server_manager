#ifndef ADMIN_MANAGER_H
#define ADMIN_MANAGER_H

#include <windows.h>

// ����Ա�����ڴ�����
DWORD WINAPI HandleManageAdmin(LPVOID param);

// ���ڹ��̺�������
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ���¹���Ա�б�
void UpdateAdminList(HWND hWnd);

// ��ӹ���Ա
void OnAddAdmin(HWND hWnd);

// ɾ������Ա
void OnRemoveAdmin(HWND hWnd);

#endif // ADMIN_MANAGER_H
#pragma once

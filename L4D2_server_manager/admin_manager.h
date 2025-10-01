#ifndef ADMIN_MANAGER_H
#define ADMIN_MANAGER_H

#include <windows.h>

// 管理员管理窗口处理函数
DWORD WINAPI HandleManageAdmin(LPVOID param);

// 窗口过程函数声明
LRESULT CALLBACK AdminWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 更新管理员列表
void UpdateAdminList(HWND hWnd);

// 添加管理员
void OnAddAdmin(HWND hWnd);

// 删除管理员
void OnRemoveAdmin(HWND hWnd);

#endif // ADMIN_MANAGER_H
#pragma once

#ifndef GUI_H
#define GUI_H

#include <windows.h>
#include <commctrl.h>  // 用于ListView控件


// 初始化公共控件
void InitCommonControlsExWrapper();

// 创建所有窗口控件
void CreateAllControls(HWND hWnd, HINSTANCE hInst);

// 更新连接状态显示
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected);

// 更新系统状态面板（服务器文件/SourceMod/MetaMod状态等）
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR mmStatus); 

// 更新实例列表
void UpdateInstanceList(HWND hWnd, const char* instancesJson);

// 添加日志到日志框
void AddLog(HWND hWnd, LPCWSTR logText);

// 清空日志
void ClearLog(HWND hWnd);

#endif
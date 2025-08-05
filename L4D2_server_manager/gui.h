#ifndef GUI_H
#define GUI_H

#include <windows.h>
#include <commctrl.h>  // ����ListView�ؼ�


// ��ʼ�������ؼ�
void InitCommonControlsExWrapper();

// �������д��ڿؼ�
void CreateAllControls(HWND hWnd, HINSTANCE hInst);

// ��������״̬��ʾ
void UpdateConnectionStatus(HWND hWnd, LPCWSTR status, BOOL isConnected);

// ����ϵͳ״̬��壨�������ļ�/SourceMod/MetaMod״̬�ȣ�
void UpdateSystemStatus(HWND hWnd, LPCWSTR serverStatus, LPCWSTR smStatus,
    LPCWSTR mmStatus); 

// ����ʵ���б�
void UpdateInstanceList(HWND hWnd, const char* instancesJson);

// �����־����־��
void AddLog(HWND hWnd, LPCWSTR logText);

// �����־
void ClearLog(HWND hWnd);

#endif
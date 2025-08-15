#pragma once
#ifndef _MAP_MANAGER_H
#define _MAP_MANAGER_H

#include <windows.h>
#include <commctrl.h>

// ��ͼ�����������Ϳؼ�ID
#define MAP_WINDOW_CLASS L"MapManagerClass"
#define IDC_INSTALLED_MAPS_LIST  2001  // �Ѱ�װ��ͼ�б�
#define IDC_REFRESH_MAPS_BTN     2002  // ˢ�µ�ͼ��ť
#define IDC_UPLOAD_MAP_BTN       2003  // �ϴ���ͼ��ť
#define IDC_UNINSTALL_MAP_EDIT   2004  // ж�ص�ͼ�����
#define IDC_UNINSTALL_MAP_BTN    2005  // ж�ص�ͼ��ť

DWORD WINAPI HandleManageMaps(LPVOID param);

#endif
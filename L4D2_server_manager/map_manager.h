#pragma once
#ifndef _MAP_MANAGER_H
#define _MAP_MANAGER_H

#include <windows.h>
#include <commctrl.h>

// 地图管理窗口类名和控件ID
#define MAP_WINDOW_CLASS L"MapManagerClass"
#define IDC_INSTALLED_MAPS_LIST  2001  // 已安装地图列表
#define IDC_REFRESH_MAPS_BTN     2002  // 刷新地图按钮
#define IDC_UPLOAD_MAP_BTN       2003  // 上传地图按钮
#define IDC_UNINSTALL_MAP_EDIT   2004  // 卸载地图输入框
#define IDC_UNINSTALL_MAP_BTN    2005  // 卸载地图按钮

DWORD WINAPI HandleManageMaps(LPVOID param);

#endif
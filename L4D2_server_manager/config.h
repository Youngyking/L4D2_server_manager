#pragma once
#ifndef _CONFIG_H
#define _CONFIG_H

#include <string>
#include <windows.h>

// ��ȡԶ����Ŀ��·������
std::string GetRemoteRootPath();

// ����Զ����Ŀ��·������
void SaveRemoteRootPath(const std::string& path);

#endif
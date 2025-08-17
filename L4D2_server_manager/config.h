#pragma once
#ifndef _CONFIG_H
#define _CONFIG_H

#include <string>
#include <windows.h>

// 读取远程项目根路径配置
std::string GetRemoteRootPath();

// 保存远程项目根路径配置
void SaveRemoteRootPath(const std::string& path);

#endif
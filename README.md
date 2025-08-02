# L4D2_server_manager

一个《求生之路2》（L4D2）服务器管理工具，旨在通过图形用户界面（GUI）简化服务器连接、状态监控、实例管理等操作。

## 概述

本项目提供一个基于Windows的GUI应用程序，通过SSH协议管理L4D2服务器。用户可通过它连接远程服务器、部署服务器实例、查看服务器状态及日志等。

## 功能特点

- **SSH连接管理**：建立和管理与远程L4D2服务器的SSH连接。
- **服务器部署**：一键部署或重新安装L4D2服务器实例。
- **状态监控**：查看服务器状态，包括服务器是否已部署、SourceMod安装状态、实例数量及已安装插件数量。
- **实例管理**：查看和管理服务器实例，包括其实例状态、名称、端口、地图及操作选项。
- **日志查看**：显示和清除操作日志，便于调试和监控。

## 依赖项

项目依赖以下库：

- `ssh.lib`：用于SSH协议实现。
- `libcrypto.lib`：用于加密功能。
- `libssl.lib`：用于SSL/TLS支持。
- `zlib.lib`：用于数据压缩- `comctl32.lib`：用于GUI中的通用控件。

这些库对应用程序的正常运行至关重要，尤其是在SSH通信、数据处理和GUI组件方面。

## 技术细节

- **开发语言**：C++
- **GUI框架**：Windows API（Win32）
- **网络通信**：Winsock 2.0用于网络操作
- **SSH库**：libssh用于SSH通信和SFTP文件传输
- **许可证**：GNU通用公共许可证v3.0（GPLv3）

## 功能分解

### SSH通信（`ssh.cpp` & `ssh.h`）

- **初始化**：`l4d2_ssh_init()`初始化SSH上下文。
- **连接**：`l4d2_ssh_connect()`使用IP、用户名和密码建立与远程服务器的SSH连接。
- **文件上传**：`l4d2_ssh_upload_api_script()`通过SFTP将管理脚本上传到远程服务器。
- **命令执行**：`l4d2_ssh_exec_command()`在远程服务器上执行命令并获取输出。
- **清理**：`l4d2_ssh_cleanup()`关闭连接并释放资源。

### 服务器管理（`manager.cpp` & `manager.h`）

- **连接处理**：`HandleConnectRequest()`在单独的线程中管理SSH连接过程，避免UI阻塞。
- **服务器部署**：`HandleDeployServer()`通过远程脚本触发服务器部署。
- **状态获取**：`HandleGetStatus()`获取并更新服务器状态信息。
- **实例获取**：`HandleGetInstances()`检索并显示服务器实例。

### GUI组件（`gui.cpp`）

- **控件创建**：`CreateAllControls()`初始化所有GUI元素，包括输入字段、按钮、列表视图和日志显示。
- **状态更新**：`UpdateConnectionStatus()`、`UpdateSystemStatus()`和`UpdateInstanceList()`等函数用当前数据刷新GUI。
- **日志管理**：`AddLog()`和`ClearLog()`处理日志的显示和清除。

### 主应用流程（`L4D2_server_manager.cpp`）

- **初始化**：注册窗口类、初始化实例并设置主窗口。
- **消息处理**：处理用户交互（按钮点击、菜单选择）并分派相应操作。
- **资源清理**：确保应用程序退出时正确清理SSH资源。

## 许可证

本项目采用GNU通用公共许可证v3.0授权。详情参见[LICENSE.txt](LICENSE.txt)文件。

根据该许可证，您可以自由复制、分发和修改本软件，但前提是任何修改版本也需遵循相同许可证，并向用户提供源代码等。本软件不提供任何明示或暗示的担保。更多信息，请参阅LICENSE.txt文件。

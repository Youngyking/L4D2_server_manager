# L4D2_server_manager

一个《求生之路2》（L4D2）服务器管理工具，旨在通过图形用户界面（GUI）简化服务器连接、状态监控、实例管理等操作。
<img width="1509" height="881" alt="屏幕截图 2025-08-13 211626" src="https://github.com/user-attachments/assets/5392ab9c-023b-4ca3-90aa-d50eb29fcb30" />
<img width="1170" height="953" alt="屏幕截图 2025-08-13 211711" src="https://github.com/user-attachments/assets/3a79e47a-5266-49cf-a80f-be14e4b9a54d" />

## 概述

本项目提供一个基于Windows的GUI应用程序，通过SSH协议管理L4D2服务器。用户可通过它连接远程服务器、部署服务器实例、查看服务器状态及日志等。

## ✨ 功能特点

- **SSH连接管理**：建立和管理与远程L4D2服务器的SSH连接。
- **服务器部署**：一键部署或重新安装L4D2服务器实例。
- **状态监控**：查看服务器状态，包括服务器是否已部署、SourceMod安装状态、实例数量及已安装插件数量。
- **实例管理**：查看和管理服务器实例，包括其实例状态、名称、端口、地图及操作选项。
- **日志查看**：显示和清除操作日志，便于调试和监控。

## 🚀 快速启动

### 前置条件

- Windows 操作系统
- 远程服务器需支持SSH连接

### 运行步骤

1. **获取程序**
   - 方式一：下载预编译版本（推荐）
   - 方式二：源码编译（较为困难，需要手动配置依赖）
     ```bash
     # 克隆仓库
     git clone [仓库地址]
     cd L4D2_server_manager
     # 使用Visual Studio打开项目文件，推荐使用vcpkg安装依赖并编译生成可执行文件
     ```

2. **配置远程服务器信息**
   - 运行程序后，在SSH连接区域输入远程服务器IP、用户名和密码
   - 点击"连接服务器"按钮建立SSH连接

3. **使用功能**
   - 连接成功后，可通过界面按钮进行服务器部署、实例管理等操作
   - 服务器状态和操作日志会实时显示在界面中

## ⚙️ 配置指南

### 1. 服务器实例配置

若已经部署求生之路，可在远程服务器的`L4D2_Manager_API.sh`脚本中的用户配置区修改求生之路服务器文件根目录与steamcmd根目录与实际部署位置匹配

若未部署，也可在远程服务器的`L4D2_Manager_API.sh`脚本中的用户配置区修改求生之路服务器文件根目录与steamcmd根目录为期望部署位置，且可在远程服务器的`L4D2_Manager_API.sh`脚本中的用户配置区定义多个服务器实例，修改`ServerInstances`关联数组：

```bash
declare -A ServerInstances=(
    ["战役_服务器"]="
        Port=27015
        HostName='[CN] My L4D2 Campaign Server'
        MaxPlayers=8
        StartMap='c1m1_hotel'
        ExtraParams='+sv_gametypes \"coop,realism,survival\"'
    "
    ["对抗_服务器"]="
        Port=27016
        HostName='[CN] My L4D2 Versus Server'
        MaxPlayers=8
        StartMap='c5m1_waterfront'
        ExtraParams='+sv_gametypes \"versus,teamversus,scavenge\"'
    "
    # 可添加更多实例...
)
```

注意，未部署过服务器请先暂时关闭steam令牌。

### 2. SSH连接配置

在程序界面的SSH连接区域直接配置：
- 输入远程服务器IP地址（如`127.0.0.1`）
- 输入SSH登录用户名（默认`root`）
- 输入对应用户的密码
- 点击"连接服务器"按钮建立连接

## 🛠️ 技术细节

- **开发语言**：C++
- **GUI框架**：Windows API（Win32）
- **网络通信**：Winsock 2.0用于网络操作
- **SSH库**：libssh用于SSH通信和SFTP文件传输
- **许可证**：GNU通用公共许可证v3.0（GPLv3）

## 功能分解

### 底层ssh协议、sftp协议支持（ssh.cpp & ssh.h）
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

## 🤝 贡献

欢迎提交Pull Requests或Issues，帮助改进该工具的功能和稳定性。

## 📄 许可证

本项目采用GNU通用公共许可证v3.0授权。详情参见[LICENSE.txt](LICENSE.txt)文件。

根据该许可证，您可以自由复制、分发和修改本软件，但前提是任何修改版本也需遵循相同许可证，并向用户提供源代码等。本软件不提供任何明示或暗示的担保。更多信息，请参阅LICENSE.txt文件。

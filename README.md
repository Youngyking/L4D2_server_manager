# L4D2_server_manager

一个《求生之路2》（L4D2）服务器管理工具，旨在通过图形用户界面（GUI）简化服务器连接、状态监控、实例管理等操作。
<img width="1509" height="881" alt="屏幕截图 2025-08-13 211626" src="https://github.com/user-attachments/assets/5392ab9c-023b-4ca3-90aa-d50eb29fcb30" />
<img width="1170" height="953" alt="屏幕截图 2025-08-13 211711" src="https://github.com/user-attachments/assets/3a79e47a-5266-49cf-a80f-be14e4b9a54d" />

## 概述

本项目提供一个基于Windows的GUI应用程序，通过SSH协议管理L4D2服务器。用户可通过它连接远程服务器、部署服务器实例、查看服务器状态及日志等。

## ✨ 功能特点

- **SSH连接管理**：可通过用户名+密钥（+口令）或原始ssh用户名+密码建立和管理与远程L4D2服务器的SSH连接。
- **服务器部署**：一键部署或重新安装L4D2服务器实例。
- **状态监控与管理**：查看服务器状态，包括游戏服务器是否已搭建、SourceMod和Metamod安装状态（SourceMod几乎为游戏服务器必需）、实例（运行中的游戏服务器）数量及已安装插件数量。
  　　　　　　　　　　可以在此启动或关闭实例。
- **插件、地图上传与卸载**：可在插件管理窗口上传/卸载插件，地图管理窗口上传/卸载地图
- **日志查看**：显示和清除操作日志，可通知指令操作成功与否

## 🚀 快速启动

### 前置条件

- Windows 操作系统
- 远程服务器需开放ssh连接的端口（默认22，可修改）与游戏服务器的端口（默认27015，27016，可修改）

### 运行步骤

1. **获取程序**
   下载最新发布版本

2. **配置远程服务器信息**
   - 先根据下述配置指南修改scripts文件夹下的L4D2_Manager_API.sh脚本
   - 运行程序，在SSH连接区域根据提示输入连接参数
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

---
以下为技术实现细节，欢迎参与开发

## 🛠️开发环境配置

     前提是你拥有Visual Studio，推荐使用2022版本。同时你的Visual Studio已经集成了vcpkg包管理器。
     
     通过包管理下载好了libssh及其依赖项(libssh 依赖 openssl 和 zlib)
     ```
     vcpkg install libssh:x64-windows openssl:x64-windows zlib:x64-windows
     ```
     
     通过Visual Studio打开项目，并配置项目属性中链接器的附加库目录为$(VCPKG_ROOT)\installed\x64-windows\lib，附加依赖项为ssh.lib;libcrypto.lib;libssl.lib;zlib.lib;comctl32.lib;%(AdditionalDependencies)

     这样开发环境就搭好了

## 🛠️ 技术细节

- **GUI框架**：Windows API（Win32）
- **网络通信**：libssh用于SSH通信和SFTP文件传输

## 🛠️功能分解

1.窗口搭建：gui.cpp与gui.h

2.封装libssh函数成为容易使用的接口：ssh.cpp与ssh.h

3.响应主窗口循环的事件并做相关处理：manger.cpp与manager.h

4.地图管理和实例管理单独实现：plugin_manager.cpp与map_manager.cpp

5.程序入口函数，处理Win_32 API创建的控件的消息循环：main.cpp与main.h

6.用于处理字符编码转换：encoding_convert.cpp与encoding_convert.h

## 📄 许可证

本项目采用GNU通用公共许可证v3.0授权。详情参见[LICENSE.txt](LICENSE.txt)文件。

根据该许可证，您可以自由复制、分发和修改本软件，但前提是任何修改版本也需遵循相同许可证，并向用户提供源代码等。本软件不提供任何明示或暗示的担保。更多信息，请参阅LICENSE.txt文件。

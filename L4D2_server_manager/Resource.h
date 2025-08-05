// Resource.h（文件末尾的换行符不能删）
#ifndef RESOURCE_H
#define RESOURCE_H

// 控件ID（原有部分保持）
#define IDC_SSH_GROUP        1001  // SSH连接组框，用于包含SSH连接相关控件
#define IDC_IP_EDIT          1002  // IP地址输入框，用于输入服务器IP地址
#define IDC_USER_EDIT        1003  // 用户名输入框，用于输入SSH登录用户名
#define IDC_PASS_EDIT        1004  // 密码输入框，用于输入SSH登录密码
#define IDC_CONNECT_BTN      1005  // 连接按钮，用于触发SSH连接操作
#define IDC_CONN_STATUS      1006  // 连接状态标签，用于显示当前SSH连接状态
#define IDC_STATUS_GROUP     1007  // 系统状态组框，用于包含系统状态相关控件
#define IDC_SERVER_STATUS    1008  // 服务器状态标签，用于显示L4D2服务器运行状态
#define IDC_SM_STATUS        1009  // SourceMod状态标签，用于显示SourceMod插件系统状态
#define IDC_INSTANCE_COUNT   1010  // 实例数量标签，用于显示当前运行的服务器实例数量(废弃)
#define IDC_PLUGIN_COUNT     1011  // 插件数量标签，用于显示已安装的插件数量（废弃）
#define IDC_DEPLOY_BTN       1012  // 部署按钮，用于触发服务器部署/更新操作
#define IDC_INSTANCE_GROUP   1013  // 服务器实例组框，用于包含服务器实例相关控件
#define IDC_INSTANCE_LIST    1014  // 实例列表控件（ListView），用于显示所有服务器实例信息
#define IDC_ACTION_GROUP     1015  // 操作与日志组框，用于包含操作按钮和日志显示控件
#define IDC_PLUGIN_BTN       1016  // 插件按钮，用于打开插件管理界面
#define IDC_LOG_BTN          1017  // 日志按钮，用于清除或管理日志显示
#define IDC_LOG_VIEW         1018  // 日志视图（多行编辑框），用于显示系统操作日志
#define IDC_PORT_EDIT        1019  // 端口输入框，用于输入服务器实例的端口号
#define IDC_START_INSTANCE   1020  // 启动实例按钮，用于启动指定端口的服务器实例
#define IDC_STOP_INSTANCE    1021  // 停止实例按钮，用于停止指定的服务器实例
#define IDC_MM_STATUS        1022  // MetaMod状态标签
#define IDC_UPLOAD_SM_BTN    1023  // 上传SourceMod按钮
#define IDC_UPLOAD_MM_BTN    1024  // 上传MetaMod按钮

// 菜单/对话框/图标/字符串ID（保持不变）
#define IDM_ABOUT            40001 // 关于菜单，用于打开关于对话框
#define IDM_EXIT             40002 // 退出菜单，用于关闭应用程序
#define IDD_ABOUTBOX         100   // 关于对话框ID，定义关于对话框资源
#define IDI_L4D2SERVERMANAGER 107  // 应用程序主图标ID
#define IDI_SMALL            108   // 应用程序小图标ID
#define IDS_APP_TITLE        103   // 应用程序标题字符串ID
#define IDC_L4D2SERVERMANAGER 104  // 主窗口类名ID

#endif 

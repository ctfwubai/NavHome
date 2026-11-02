# 导航之家（NavHome）

> 本地化、可视化、带密码保护的 Windows 网页收藏管家。

「导航之家」是一款面向 Windows 桌面端的本地单机网页收藏管理工具，帮助用户把分散在多个浏览器中的收藏网址集中管理，提供卡片化展示、分组归类、关键词筛选、一键打开与密码保护能力。参考开源项目 [OneNav](https://github.com/helloxz/onenav) 的交互思路，但在技术栈与运行形态上全新实现——**无需服务端、安装即用、离线可用**。

## 功能特性

- 🗂 **分组管理**：新增 / 重命名 / 删除分组，支持拖拽上下排序，收藏按分组归类
- 🔖 **收藏管理**：增删改查、批量操作、移动分组
- 📁 **文件/文件夹/网络路径**：可收藏本地文件、文件夹、SMB/UNC 共享（`\\server\share`）与 FTP（`ftp://…`）路径，点击即打开
- 🔍 **关键词筛选**：按名称 / 网址模糊匹配，全局搜索
- 🖱 **卡片展示**：长方形圆角卡片（图标 + 名称 + 网址），点击一键打开，彩色占位图标
- 🌐 **浏览器选择**：首次启动自动探测本机浏览器并展示其图标，可随时在设置中更换
- 🔒 **密码保护**：开启后需输入密码才能查看收藏（明文 / 哈希两种存储）
- 💾 **数据本地化**：收藏存于 SQLite，配置存于 config.ini，均可手工编辑找回
- 🎨 **界面主题**：浅色 / 深色主题切换，支持自定义背景图片
- 📦 **单文件安装**：Inno Setup 打包为独立 .exe 安装程序，默认安装到 D 盘，所有用户（含 AD 域）可用

## 技术栈

| 层次 | 选型 |
|------|------|
| 开发语言 / 框架 | C++17 + Qt 6（Widgets / Sql / Network） |
| 数据库 | SQLite（Qt Sql 模块，随程序分发） |
| 配置文件 | config.ini（INI 格式，可手工编辑） |
| 构建工具 | CMake + Ninja（MSYS2 mingw64 工具链） |
| 安装打包 | Inno Setup 6（单文件 .exe 安装程序） |

## 目录结构

```
导航之家/
├── src/
│   ├── main.cpp                 # 程序入口
│   ├── app/                     # 应用服务层
│   │   ├── AppConfig.*          # config.ini 读写
│   │   ├── Database.*           # SQLite 连接与建表
│   │   ├── BrowserLauncher.*    # 浏览器探测 / 图标提取 / 打开
│   │   ├── PasswordManager.*    # 密码存储与校验
│   │   ├── BackupManager.*      # JSON 备份与恢复
│   │   ├── BookmarkImporter.*   # 浏览器书签 HTML 导入
│   │   └── IconFetcher.*        # 站点 favicon 异步抓取
│   ├── models/                  # 数据模型与数据访问层
│   │   ├── Group.h / Bookmark.h # 数据结构
│   │   └── BookmarkRepository.* # SQLite 读写封装
│   └── ui/                      # 表现层
│       ├── MainWindow.*         # 主窗口（左分组 + 右卡片）
│       ├── FlowLayout.*         # 卡片流式布局
│       ├── BookmarkCardWidget.* # 收藏卡片
│       ├── AddBookmarkDialog.*  # 添加 / 编辑收藏
│       ├── BrowserSetupDialog.* # 浏览器选择
│       ├── LoginDialog.*        # 密码登录
│       └── SettingsDialog.*     # 设置
├── resources/                   # 图标等资源
├── installer/                   # 安装打包脚本（Inno Setup）
└── docs/                        # 架构与业务流程文档
```

## 构建

### 环境要求

- Windows 10 / 11（64 位）
- [MSYS2](https://www.msys2.org/) 及以下工具链（可通过 pacman 安装）：
  ```bash
  pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
  ```

### 构建步骤

在 MSYS2 / Git Bash 中执行：

```bash
./build.sh
```

> 说明：由于 Qt 的 `moc` 工具无法写入含中文的输出路径，构建脚本会将中间产物输出到纯 ASCII 目录（默认 `E:/NavHome-build`）。源码仍保留在本目录。

构建产物为 `NavHome.exe`，可通过 [windeployqt](https://doc.qt.io/qt-6/windows-deployment.html) 收集依赖后运行：

```bash
# 在构建目录下执行
/c/msys64/mingw64/bin/windeployqt.exe NavHome.exe
```

> 注意：`windeployqt` 不会自动拷贝 icu、freetype、harfbuzz、sqlite3、brotli、zstd、libb2 等第三方 DLL，需手工补齐（或使用 `installer/deploy_deps.sh` 递归补全依赖）。

### 打包安装程序

使用 [Inno Setup 6](https://jrsoftware.org/isinfo.php) 打包为单文件安装程序（默认安装到 `D:\NavHome`）：

```bash
# 1. 将构建产物与依赖放入 installer/dist
# 2. 编译安装脚本（在 installer 目录下）
"/c/Program Files (x86)/Inno Setup 6/ISCC.exe" setup.iss
```

产物为 `installer/NavHome-Setup-2.3.0.exe`，复制到任意机器即可双击安装。

## 使用说明

1. 双击 `NavHome.exe` 启动，首次启动会引导选择默认浏览器。
2. 左侧「分组」管理收藏分类，右侧卡片展示收藏，点击卡片即可打开网址。
3. 点击「＋ 添加收藏」录入名称与网址；悬停卡片可编辑 / 删除。
4. 「设置」中可更换默认浏览器、开启密码保护。

### 数据与配置

应用启动时自动选择数据目录：

- **便携模式**：程序目录可写时，数据存于 exe 所在目录（随程序迁移）。
- **安装模式**：安装在公共目录（如 `D:\NavHome`）时，数据存于 `%APPDATA%\NavHome`，各用户独立、卸载重装不丢。

数据文件：

- 收藏数据：`navhome.db`（SQLite，含 groups / bookmarks / settings 表）
- 配置文件：`config.ini`（可手工编辑，用于密码找回、默认浏览器、主题背景等）
- 站点图标：`icons/`（抓取到的 favicon）

## 文档

- 架构说明（分层架构、模块职责、数据目录策略）：[docs/architecture.md](docs/architecture.md)
- 业务流程说明（启动 / 收藏 / 打开网址 / 设置 / 备份恢复流程图）：[docs/business-flow.md](docs/business-flow.md)
- 更新日志：[CHANGELOG.md](CHANGELOG.md)

## 许可证

[MIT](LICENSE)

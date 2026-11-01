# 业务流程说明（Business Flow）

> 本文用流程图描述「导航之家」的核心业务流程。流程图采用 Mermaid 语法，在支持 Mermaid 的 Markdown 阅读器（如 GitHub、VS Code）中可直接渲染。

## 一、启动流程

```mermaid
flowchart TD
    Start([启动程序]) --> ChooseDir[选择数据目录<br/>便携 / APPDATA]
    ChooseDir --> InitCfg[初始化 config.ini<br/>首次生成带注释模板]
    InitCfg --> InitDb[打开 SQLite<br/>初始化表结构]
    InitDb --> CheckPwd{启用密码保护?}
    CheckPwd -->|是| Login[弹出登录对话框<br/>输入密码校验]
    Login -->|校验失败| Login
    Login -->|校验通过| CheckFirst
    CheckPwd -->|否| CheckFirst
    CheckFirst{首次使用?} -->|是| Guide[引导选择默认浏览器]
    Guide --> SaveCfg[保存选择并标记已配置]
    SaveCfg --> Main
    CheckFirst -->|否| Main[进入主界面]
    Main --> End([等待用户操作])
```

## 二、收藏管理流程

### 添加 / 编辑收藏

```mermaid
flowchart TD
    A[点击 添加收藏 / 编辑] --> B[打开 AddBookmarkDialog]
    B --> C[填写名称、网址、选择分组]
    C --> D{保存?}
    D -->|取消| End([结束])
    D -->|确定| E[写入 SQLite]
    E --> F{URL 是否变化?}
    F -->|是| G[IconFetcher 异步抓取 favicon]
    F -->|否| H[刷新卡片列表]
    G -->|成功| I[回写图标路径]
    I --> H
    H --> End
```

### 删除 / 批量操作

```mermaid
flowchart TD
    A[进入多选模式] --> B[勾选目标卡片]
    B --> C{操作}
    C -->|批量删除| D[确认后删除所选]
    C -->|移动到分组| E[选择目标分组后移动]
    C -->|全选| F[选中全部卡片]
    D --> G[刷新列表]
    E --> G
    F --> B
```

## 三、打开网址流程

```mermaid
flowchart TD
    A[点击收藏卡片] --> B{当前是否多选模式?}
    B -->|是| C[切换选中状态]
    B -->|否| D[读取默认浏览器配置]
    D --> E{指定浏览器可用?}
    E -->|是| F[用命令行参数打开网址]
    E -->|否| G[用系统默认浏览器打开]
    F --> H[结束]
    G --> H
```

## 四、设置与配置流程

```mermaid
flowchart TD
    A[点击 设置] --> B[打开 SettingsDialog]
    B --> C{选择操作}
    C -->|更换默认浏览器| D[BrowserSetupDialog 探测本机浏览器<br/>展示浏览器图标]
    C -->|切换主题| E[浅色 / 深色]
    C -->|更换背景| F[预设渐变 / 自定义图片]
    C -->|密码保护| G[启用 / 关闭 / 修改密码]
    C -->|数据备份| H[导出 JSON]
    C -->|数据恢复| I[覆盖 / 合并导入]
    C -->|导入书签| J[解析浏览器 HTML 书签]
    D --> K[点击确定后持久化并刷新]
    E --> K
    F --> K
    G --> K
    H --> K
    I --> K
    J --> K
```

## 五、备份与恢复流程

```mermaid
flowchart TD
    A[立即备份] --> B[选择保存位置]
    B --> C[导出分组 + 收藏到 JSON]
    C --> D[提示备份成功]

    E[从备份恢复] --> F[选择 JSON 文件]
    F --> G[选择恢复方式]
    G -->|覆盖| H[清空后导入]
    G -->|合并| I[追加导入，同名分组复用]
    H --> J[刷新界面]
    I --> J
```

## 六、关键设计要点

- **浏览器探测**：优先读注册表 `App Paths`，失败时回退到常见安装路径；系统默认浏览器通过 `UserChoice` 注册表识别。
- **favicon 抓取**：异步进行，不阻塞界面；失败时回退为「名称首字符 + 彩色背景」的占位图标。
- **密码存储**：默认 SHA-256 哈希；也可通过 `config.ini` 切换为明文。忘记密码可通过编辑配置关闭保护。
- **数据一致性**：书签 HTML 导入在单个事务内完成，失败自动回滚。

## 七、相关文档

- 架构说明：[architecture.md](architecture.md)
- 使用与构建说明：项目根目录 [README.md](../README.md)

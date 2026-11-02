# 如何把「导航之家」上传到 GitHub

> 目标仓库：https://github.com/ctfwubai/NavHome
> 本机远程名：`origin`

本文记录从本机把源码、安装包上传到 GitHub 的完整步骤，供以后每次发版重复使用。

---

## 一、两种连接方式（HTTPS / SSH）

连 GitHub 之前要先选一种方式，两种本机都已验证可用，任选其一即可。

### 方式 A：HTTPS（需要走本地代理）

本机直连 GitHub 会超时（被墙），必须走本地代理 `127.0.0.1:7897`（Clash Verge）。

给仓库配置代理（只需做一次）：

```bash
git config http.proxy  http://127.0.0.1:7897
git config https.proxy http://127.0.0.1:7897
```

> 如果哪天代理端口变了，把 `7897` 换成新端口即可。查看/取消代理：

```bash
git config --get http.proxy      # 查看
git config --unset http.proxy    # 取消（需要时）
git config --unset https.proxy
```

远程地址用 HTTPS：

```bash
git remote set-url origin https://github.com/ctfwubai/NavHome.git
```

首次 push 时 Git Credential Manager 会弹浏览器登录 GitHub 并授权，之后凭据会被记住。

### 方式 B：SSH（密钥免密，无需代理）

本机已有密钥 `~/.ssh/id_ed25519`，且已授权到 GitHub 账号 `ctfwubai`，SSH 22 端口可直连、无需代理。

1. 确认密钥已添加到 GitHub（网页 → Settings → SSH and GPG keys），可先自测：

   ```bash
   ssh -T git@github.com
   # 出现 "Hi ctfwubai! You've successfully authenticated..." 即成功
   ```

2. 远程地址用 SSH：

   ```bash
   git remote set-url origin git@github.com:ctfwubai/NavHome.git
   ```

3. 直接 push 即可，免密免代理。

---

## 二、首次推送

首次关联远程（已做）：

```bash
git remote add origin <上面两种地址之一>
git push -u origin main
```

---

## 三、每次发版的完整流程

### 1. 改代码 + 改版本号

需要同步改这几处版本号：

| 文件 | 位置 |
|------|------|
| `src/main.cpp` | `setApplicationVersion("x.y.z")` |
| `CMakeLists.txt` | `VERSION x.y.z` |
| `installer/setup.iss` | `#define MyAppVersion "x.y.z"` 和 `OutputBaseFilename=NavHome-Setup-{#MyAppVersion}-<日期>` |
| `CHANGELOG.md` | 新增一条版本记录（新增 / 修复 / 优化） |
| `README.md` | 如需引用新安装包名，一并更新 |

### 2. 构建 + 打包安装程序

```bash
# 在 MSYS2 / Git Bash 中
./build.sh                          # 产出 E:\NavHome-build\NavHome.exe
# 把 NavHome.exe 同步到 installer\dist\（打包源是 dist\*）
# 然后在 installer 目录下编译安装脚本：
"/c/Program Files (x86)/Inno Setup 6/ISCC.exe" setup.iss
# 产物：installer\NavHome-Setup-x.y.z-<日期>.exe
```

### 3. 提交源码

```bash
git add -A
git commit -m "feat: <本次改了什么>，版本 Vx.y.z"
```

### 4. 提交安装包

安装包默认被 `.gitignore` 里的 `*.exe` 忽略，但已加了例外规则，直接 `git add` 即可：

```bash
git add installer/NavHome-Setup-*.exe
git commit -m "chore: 收录安装包 NavHome-Setup-x.y.z.exe"
```

### 5. 推送到 GitHub

```bash
git push
```

---

## 四、给每个版本打标签（推荐）

打 tag 后，GitHub 上就能按版本浏览源码快照：

```bash
git tag v2.3.0
git push origin v2.3.0        # 单个标签
# 或一次性推送所有标签：git push origin --tags
```

---

## 五、用 GitHub Releases 发布安装包（可选，更专业）

如果不想让仓库体积变大（安装包每个约 30MB），可以把安装包放到 Release，而不是提交进仓库：

1. 网页打开仓库 → 右侧 **Releases** → **Draft a new release**。
2. 「Choose a tag」选对应版本 tag（如 `v2.3.0`）。
3. 标题填 `导航之家 V2.3.0`，正文写本次更新内容。
4. 把 `installer\NavHome-Setup-2.3.0-20261102.exe` 拖进附件区上传。
5. 点 **Publish release**。

> 两种方式任选其一：仓库内直接放安装包最简单；用 Release 更规范、下载更清晰。

---

## 六、常见问题

- **HTTPS 直连超时 / 连不上**：先确认代理已开，并已 `git config http.proxy http://127.0.0.1:7897`。
- **HTTPS 首次推送要求登录 / 授权**：按浏览器提示登录 GitHub 并「Authorize」，只出现一次。
- **SSH 提示 `Permission denied (publickey)`**：密钥没加到 GitHub，去 Settings → SSH and GPG keys 添加 `~/.ssh/id_ed25519.pub` 内容。
- **提示 `Permission denied` / 没有写权限**：确认登录的是 `ctfwubai` 且该账号有仓库写权限。
- **单个文件超过 100MB 推不上去**：GitHub 限制单文件 100MB。当前安装包约 30MB，不受影响；若未来超限请改用 Release。
- **中文文件名 / 乱码**：Git 默认 UTF-8，中文文件名正常；若命令行显示乱码可执行 `git config --global core.quotepath false`。

#pragma once

#include <QIcon>
#include <QList>
#include <QPixmap>
#include <QString>

// 浏览器信息
struct BrowserInfo {
    QString name;   // 显示名称
    QString path;   // 可执行文件完整路径
    QIcon icon;     // 浏览器图标（从 exe 提取）
};

// 浏览器探测与打开工具（对应需求 3.2）
class BrowserLauncher {
public:
    // 探测本机已安装的常见浏览器（Edge/Chrome/Firefox/360/QQ 等）
    static QList<BrowserInfo> detectBrowsers();

    // 获取系统当前默认浏览器名称（用于首次引导推荐展示）
    static QString systemDefaultBrowserName();

    // 提取某可执行文件的图标（浏览器 logo）；路径无效时返回空图标
    static QIcon iconForPath(const QString& path);

    // 使用指定浏览器打开网址；browserPath 为空时回退到系统默认
    static bool openUrl(const QString& url, const QString& browserPath = QString());

    // 打开应用程序：command 可为纯路径或带参数的命令行（如 "D:\Weixin\Weixin.exe --scene=desktop"）
    static bool openApp(const QString& command);

    // 从命令行提取可执行文件路径（用于提取 exe 图标）
    static QString exePathFromCommand(const QString& command);

    // 读取 exe 版本信息中的 FileDescription（如 "BarTender Designer"）；失败返回空
    static QString fileDescription(const QString& path);

    // 解析 .lnk 快捷方式的目标命令行（含参数）；失败返回空
    static QString shortcutTarget(const QString& lnkPath);

    // 取应用的友好显示名：优先桌面/开始菜单快捷方式的中文名，其次 FileDescription，最后文件名
    static QString friendlyNameForExe(const QString& exePath);

    // 提取应用图标（Windows Shell 大图标，比 QFileIconProvider 更可靠）；失败返回空
    static QPixmap appIconPixmap(const QString& exePath, int size);

    // 补全网址协议（缺少 scheme 时补 https://）
    static QString ensureScheme(const QString& url);

private:
    // 查询注册表 App Paths 中某可执行文件的实际路径
    static QString appPathFromRegistry(const QString& exeName);

    // 在常见快捷方式位置查找指向该 exe 的 .lnk 文件名（去扩展名）
    static QString findShortcutName(const QString& exePath);
};

#pragma once

#include <QIcon>
#include <QList>
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

    // 补全网址协议（缺少 scheme 时补 https://）
    static QString ensureScheme(const QString& url);

private:
    // 查询注册表 App Paths 中某可执行文件的实际路径
    static QString appPathFromRegistry(const QString& exeName);
};

#include "BrowserLauncher.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winver.h>
#endif

namespace {

// 常见浏览器候选表：注册表 App Paths 键名 + 常见安装路径兜底
struct Candidate {
    const char* name;
    const char* exeName;
    const char* fallback;
};

const Candidate kCandidates[] = {
    { "Microsoft Edge",    "msedge.exe",    "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" },
    { "Google Chrome",     "chrome.exe",    "C:/Program Files/Google/Chrome/Application/chrome.exe" },
    { "Mozilla Firefox",   "firefox.exe",   "C:/Program Files/Mozilla Firefox/firefox.exe" },
    { "360 安全浏览器",    "360se.exe",     "C:/Program Files (x86)/360/360se6/Application/360se.exe" },
    { "360 极速浏览器",    "360chrome.exe", "C:/Program Files (x86)/360/360Chrome/Chrome/Application/360chrome.exe" },
    { "QQ 浏览器",         "QQBrowser.exe", "C:/Program Files (x86)/Tencent/QQBrowser/QQBrowser.exe" },
};

} // namespace

QList<BrowserInfo> BrowserLauncher::detectBrowsers() {
    QList<BrowserInfo> result;
    for (const Candidate& c : kCandidates) {
        QString path = appPathFromRegistry(QString::fromUtf8(c.exeName));
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            path = QString::fromUtf8(c.fallback);
        }
        if (QFileInfo::exists(path)) {
            BrowserInfo info;
            info.name = QString::fromUtf8(c.name);
            info.path = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
            info.icon = iconForPath(info.path);
            result.append(info);
        }
    }
    return result;
}

QString BrowserLauncher::systemDefaultBrowserName() {
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Shell\\Associations"
                "\\UrlAssociations\\http\\UserChoice",
                QSettings::NativeFormat);
    const QString progId = s.value("ProgId").toString();

    // 常见 ProgId 到友好名称的映射
    if (progId.contains("Chrome", Qt::CaseInsensitive)) return "Google Chrome";
    if (progId.contains("Firefox", Qt::CaseInsensitive)) return "Mozilla Firefox";
    if (progId.contains("Edge", Qt::CaseInsensitive) || progId.contains("MSEdgeHTM", Qt::CaseInsensitive))
        return "Microsoft Edge";
    if (progId.contains("360", Qt::CaseInsensitive)) return "360 浏览器";
    if (progId.contains("QQBrowser", Qt::CaseInsensitive)) return "QQ 浏览器";
    return progId;
}

QIcon BrowserLauncher::iconForPath(const QString& path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return QIcon();
    }
    QFileIconProvider provider;
    return provider.icon(QFileInfo(path));
}

bool BrowserLauncher::openUrl(const QString& url, const QString& browserPath) {
    if (url.isEmpty()) {
        return false;
    }
    const QString full = ensureScheme(url);

    // 指定浏览器可用时，通过命令行参数打开
    if (!browserPath.isEmpty() && QFileInfo::exists(browserPath)) {
        return QProcess::startDetached(browserPath, QStringList{ full });
    }

    // 回退到系统默认浏览器
    return QDesktopServices::openUrl(QUrl(full));
}

bool BrowserLauncher::openApp(const QString& command) {
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    // 快捷方式（.lnk）：直接交给系统 Shell 打开，保留原始参数/工作目录
    if (trimmed.endsWith(".lnk", Qt::CaseInsensitive)) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(trimmed));
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    if (parts.isEmpty()) {
        return false;
    }
    const QString program = parts.first();
    const QStringList args = parts.mid(1);
    if (!QFileInfo::exists(program)) {
        return false;
    }
    // 在程序所在目录启动，避免相对路径资源找不到
    return QProcess::startDetached(program, args, QFileInfo(program).absolutePath());
}

QString BrowserLauncher::exePathFromCommand(const QString& command) {
    const QString trimmed = command.trimmed();
    // 纯文件路径（无参数）直接返回，避免含空格路径被 splitCommand 误拆分
    if (QFileInfo::exists(trimmed)) {
        return trimmed;
    }
    const QStringList parts = QProcess::splitCommand(trimmed);
    if (parts.isEmpty()) {
        return QString();
    }
    return parts.first();
}

QString BrowserLauncher::fileDescription(const QString& path) {
#ifdef Q_OS_WIN
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return QString();
    }
    const QString native = QDir::toNativeSeparators(path);
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(
        reinterpret_cast<const wchar_t*>(native.utf16()), &handle);
    if (size == 0) {
        return QString();
    }
    QByteArray buffer(static_cast<int>(size), 0);
    if (!GetFileVersionInfoW(reinterpret_cast<const wchar_t*>(native.utf16()),
                             handle, size, buffer.data())) {
        return QString();
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    }* translate = nullptr;
    UINT transLen = 0;
    if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translate), &transLen) || !transLen) {
        return QString();
    }

    const QString langHex = QString::number(translate->wLanguage, 16).rightJustified(4, '0');
    const QString pageHex = QString::number(translate->wCodePage, 16).rightJustified(4, '0');
    const QString subBlock =
        QString("\\StringFileInfo\\%1%2\\FileDescription").arg(langHex, pageHex);

    wchar_t* value = nullptr;
    UINT valueLen = 0;
    if (VerQueryValueW(buffer.data(), reinterpret_cast<const wchar_t*>(subBlock.utf16()),
                       reinterpret_cast<void**>(&value), &valueLen) && value) {
        return QString::fromWCharArray(value);
    }
    return QString();
#else
    Q_UNUSED(path);
    return QString();
#endif
}

QString BrowserLauncher::ensureScheme(const QString& url) {
    QString trimmed = url.trimmed();
    // 本地文件路径直接返回
    if (trimmed.contains("://")) {
        return trimmed;
    }
    return "https://" + trimmed;
}

QString BrowserLauncher::appPathFromRegistry(const QString& exeName) {
    // 依次查询当前用户、64 位本机、32 位本机的 App Paths
    const QStringList roots = {
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName,
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName,
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName,
    };
    for (const QString& root : roots) {
        QSettings s(root, QSettings::NativeFormat);
        const QString path = s.value("Default").toString().trimmed();
        if (!path.isEmpty() && path != "(value not set)" && QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

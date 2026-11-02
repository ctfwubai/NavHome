#include "BrowserLauncher.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QStandardPaths>
#include <QUrl>
#include <QImage>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winver.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <objbase.h>
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

#ifdef Q_OS_WIN
// 把 Win32 HICON 转换为 QPixmap（带 alpha，颜色来自图标彩色位图）
QPixmap pixmapFromHICON(HICON icon) {
    if (!icon) {
        return QPixmap();
    }
    ICONINFO ii{};
    if (!GetIconInfo(icon, &ii)) {
        DestroyIcon(icon);
        return QPixmap();
    }
    BITMAP bm{};
    if (!ii.hbmColor || !GetObjectW(ii.hbmColor, sizeof(BITMAP), &bm)) {
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        DestroyIcon(icon);
        return QPixmap();
    }
    const int w = bm.bmWidth;
    const int h = bm.bmHeight;

    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, ii.hbmColor));

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // 自顶向下，QImage 直接可用
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    QImage img(w, h, QImage::Format_ARGB32);
    if (!img.isNull()) {
        GetDIBits(memDC, ii.hbmColor, 0, static_cast<UINT>(h), img.bits(), &bmi, DIB_RGB_COLORS);
    }

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    DestroyIcon(icon);

    return img.isNull() ? QPixmap() : QPixmap::fromImage(img);
}
#endif

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

    // FTP / FTPS：交给系统默认处理器（资源管理器或浏览器）
    if (trimmed.startsWith("ftp://", Qt::CaseInsensitive) ||
        trimmed.startsWith("ftps://", Qt::CaseInsensitive)) {
        return QDesktopServices::openUrl(QUrl(trimmed));
    }

    // 目录（本地文件夹）→ 用资源管理器打开
    const QFileInfo fi(trimmed);
    if (fi.exists() && fi.isDir()) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
    }

    // UNC / SMB 共享路径（\\10.8.249.35 或 \\server\share）→ 资源管理器打开；
    // 目标暂不可达 / 需认证时 QFileInfo 判定不了，仍交给系统 Shell 尝试打开
    if (trimmed.startsWith("\\\\")) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(trimmed));
    }

    // 快捷方式（.lnk）：直接交给系统 Shell 打开，保留原始参数/工作目录
    if (trimmed.endsWith(".lnk", Qt::CaseInsensitive)) {
        return QDesktopServices::openUrl(QUrl::fromLocalFile(trimmed));
    }

    // 纯文件路径（无参数，含空格）：
    // 可执行文件 / 批处理作为进程启动，其它文档（图片/文本等）用系统关联程序打开
    if (QFileInfo::exists(trimmed)) {
        const QString suffix = QFileInfo(trimmed).suffix().toLower();
        if (suffix == "exe" || suffix == "com" || suffix == "bat" || suffix == "cmd") {
            return QProcess::startDetached(trimmed, QStringList(),
                                           QFileInfo(trimmed).absolutePath());
        }
        return QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(trimmed).absoluteFilePath()));
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

QString BrowserLauncher::shortcutTarget(const QString& lnkPath) {
#ifdef Q_OS_WIN
    const HRESULT init = CoInitialize(nullptr);
    QString result;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(ShellLink), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IShellLinkW), reinterpret_cast<void**>(&link)))) {
        IPersistFile* file = nullptr;
        if (SUCCEEDED(link->QueryInterface(__uuidof(IPersistFile),
                                           reinterpret_cast<void**>(&file)))) {
            if (SUCCEEDED(file->Load(reinterpret_cast<const wchar_t*>(lnkPath.utf16()), STGM_READ))) {
                wchar_t target[MAX_PATH] = {};
                wchar_t args[MAX_PATH * 4] = {};
                if (SUCCEEDED(link->GetPath(target, MAX_PATH, nullptr, 0))) {
                    QString cmd = QString::fromWCharArray(target);
                    if (SUCCEEDED(link->GetArguments(args, MAX_PATH * 4)) && args[0] != L'\0') {
                        cmd += " " + QString::fromWCharArray(args);
                    }
                    result = cmd;
                }
            }
            file->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(init)) {
        CoUninitialize();
    }
    return result;
#else
    Q_UNUSED(lnkPath);
    return QString();
#endif
}

QString BrowserLauncher::findShortcutName(const QString& exePath) {
    const QString target = QFileInfo(exePath).canonicalFilePath().toLower();
    if (target.isEmpty()) {
        return QString();
    }

    QStringList roots;
    roots << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    roots << QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    roots << "C:/Users/Public/Desktop";
    roots << "C:/ProgramData/Microsoft/Windows/Start Menu/Programs";

    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        QDirIterator it(root, QStringList{"*.lnk"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString lnk = it.next();
            const QString tgt = shortcutTarget(lnk);
            if (tgt.isEmpty()) {
                continue;
            }
            const QString exe = exePathFromCommand(tgt);
            if (exe.isEmpty()) {
                continue;
            }
            if (QFileInfo(exe).canonicalFilePath().toLower() == target) {
                return QFileInfo(lnk).completeBaseName();
            }
        }
    }
    return QString();
}

QString BrowserLauncher::friendlyNameForExe(const QString& exePath) {
    if (exePath.isEmpty()) {
        return QString();
    }
    // 优先桌面/开始菜单快捷方式的中文显示名（如“微信”）
    const QString lnkName = findShortcutName(exePath);
    if (!lnkName.isEmpty()) {
        return lnkName;
    }
    // 其次 exe 版本信息 FileDescription（如 “BarTender Designer”）
    const QString desc = fileDescription(exePath);
    if (!desc.isEmpty()) {
        return desc;
    }
    // 最后文件名兜底
    return QFileInfo(exePath).completeBaseName();
}

QPixmap BrowserLauncher::appIconPixmap(const QString& exePath, int size) {
#ifdef Q_OS_WIN
    if (!exePath.isEmpty() && QFileInfo::exists(exePath)) {
        const QString native = QDir::toNativeSeparators(exePath);
        SHFILEINFOW sfi{};
        if (SHGetFileInfoW(reinterpret_cast<const wchar_t*>(native.utf16()), 0, &sfi, sizeof(sfi),
                           SHGFI_ICON | SHGFI_LARGEICON)) {
            const QPixmap pm = pixmapFromHICON(sfi.hIcon);
            if (!pm.isNull()) {
                return pm;
            }
        }
    }
#endif
    // 回退：QFileIconProvider（Win32 提取失败时）
    QFileIconProvider provider;
    const QIcon icon = provider.icon(QFileInfo(exePath));
    return icon.pixmap(size, size);
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

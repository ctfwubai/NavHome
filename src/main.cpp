#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <QDateTime>
#include <QMutex>
#include <QDebug>
#include <cstdio>
#include <cstdlib>

#include "app/AppConfig.h"
#include "app/Database.h"
#include "app/PasswordManager.h"
#include "models/BookmarkRepository.h"
#include "ui/MainWindow.h"
#include "ui/LoginDialog.h"

namespace {

// —— 日志 ——
// 捕获 Qt 全部调试/警告/错误消息，追加写入数据目录下的 navhome.log，
// 便于在程序闪退后定位原因（每次写入立即 flush，确保闪退时不丢最后一行）。
QFile g_logFile;
QMutex g_logMutex;

void navHomeMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO"; break;
    case QtWarningMsg:  level = "WARN"; break;
    case QtCriticalMsg: level = "CRITICAL"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }

    const QString line = QString("[%1] [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"), level, msg);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile.isOpen()) {
            g_logFile.write(line.toUtf8());
            g_logFile.flush();
        }
    }
    // 同时输出到调试输出，便于开发期观察
    fputs(line.toUtf8().constData(), stderr);
    fflush(stderr);

    if (type == QtFatalMsg) {
        std::abort();  // 致命错误：写盘后按 Qt 约定终止进程
    }
}

// 首次启动：生成带注释的 config.ini 模板，便于非专业用户手工编辑找回密码
void ensureDefaultConfig(const QString& path) {
    if (QFile::exists(path)) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    f.write(R"(# 导航之家（NavHome）配置文件
# 本文件为 INI 格式，可手工编辑，修改后请重启程序生效。
# 忘记密码时：将 [security] 下 password_enabled 改为 false 即可关闭密码保护。

[app]
# 默认浏览器可执行文件路径；留空表示使用系统默认浏览器
default_browser=
# 界面主题（默认 light）
theme=light
# 界面语言（默认 zh_CN）
language=zh_CN
# 是否已完成首次浏览器选择引导（0/1）
browser_configured=false

[security]
# 是否启用密码保护（0/1）
password_enabled=false
# 访问密码（明文或哈希值；password_hash=1 时为 SHA-256 哈希）
password=
# 是否使用哈希存储密码（0/1）
password_hash=false

[ui]
# 用户选中的应用图标序号（0-19）
icon_index=0
# 标题栏透明度（0-100，100 为不透明）
titlebar_opacity=100

[backup]
# 是否启用自动备份（0/1）
backup_enabled=false
# 备份周期（daily/weekly/monthly）
backup_period=daily
# 自动备份时间点（如 23:00）
backup_time=23:00
# 保留备份份数
backup_keep_count=7
# 是否加密备份文件（0/1）
backup_encrypt=false
# 备份文件保存目录（留空使用程序目录）
backup_path=
)");
    f.close();
}

// 选择数据目录：程序目录可写则用便携模式（数据随程序），
// 否则回退到用户数据目录（支持安装在公共目录时各用户独立可写）
QString chooseDataDir() {
    const QString exeDir = QCoreApplication::applicationDirPath();

    // 探测程序目录是否可写
    const QString probe = QDir(exeDir).filePath(".navhome_write_test");
    QFile f(probe);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.close();
        f.remove();
        return exeDir;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData;
}

// 首次运行（数据库为空）时预置默认分组与常用收藏；
// 老用户重装后数据从 %APPDATA% 恢复，不会重复插入
void seedDefaultBookmarks() {
    BookmarkRepository& repo = BookmarkRepository::instance();
    if (!repo.allBookmarks().isEmpty() || !repo.groups().isEmpty()) {
        return;
    }
    const int groupId = repo.addGroup("常用");
    if (groupId < 0) {
        return;
    }
    const QStringList urls = {
        "https://cn.bing.com/",
        "https://www.deepseek.com/",
        "https://www.doubao.com/",
        "https://chatgpt.com/",
        "https://github.com/"
    };
    const QStringList names = {
        "必应", "DeepSeek", "豆包", "ChatGPT", "GitHub"
    };
    for (int i = 0; i < urls.size(); ++i) {
        Bookmark b;
        b.groupId = groupId;
        b.name = names.at(i);
        b.url = urls.at(i);
        repo.addBookmark(b);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("NavHome");
    QApplication::setApplicationDisplayName("导航之家");
    QApplication::setApplicationVersion("2.2.4");
    QApplication::setOrganizationName("NavHome");

    // 数据目录：程序目录可写则便携（数据随程序），否则回退到用户数据目录
    // （安装在 D 盘公共目录时，AD 域普通用户仍可读写自己的数据）
    const QString dataDir = chooseDataDir();
    AppConfig::instance().setDataDir(dataDir);

    // 打开日志文件（追加模式），并接管 Qt 消息输出
    g_logFile.setFileName(QDir(dataDir).filePath("navhome.log"));
    (void)g_logFile.open(QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(navHomeMessageHandler);
    qInfo() << "==== NavHome 启动 v" << QApplication::applicationVersion() << "====";

    // 初始化配置（config.ini 位于数据目录，可手工编辑）
    const QString configPath = QDir(dataDir).filePath("config.ini");
    ensureDefaultConfig(configPath);
    AppConfig::instance().setConfigPath(configPath);

    // 提前应用全局样式，保证登录对话框等主窗口之前的界面风格统一
    MainWindow::applyGlobalStyle();

    // 初始化数据库并绑定数据访问层
    const QString dbPath = QDir(dataDir).filePath("navhome.db");
    if (!Database::instance().open(dbPath)) {
        QMessageBox::critical(nullptr, "导航之家",
                              "数据库初始化失败，程序无法启动。\n请检查程序目录是否可写。");
        return 1;
    }
    BookmarkRepository::instance().setDatabase(Database::instance().database());

    // 首次运行预置默认收藏
    seedDefaultBookmarks();

    // 密码保护：启动时校验（对应需求 3.7.1）
    if (PasswordManager::isEnabled()) {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted || !login.verified()) {
            return 0;
        }
    }

    MainWindow w;
    w.show();

    const int rc = app.exec();
    qInfo() << "==== NavHome 退出，返回码" << rc << "====";
    return rc;
}

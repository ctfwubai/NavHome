#pragma once

#include <QString>

class QSettings;

// 应用配置：读写安装路径下的 config.ini（INI 格式，可手工编辑）
// 对应需求 5.1 的各个配置段与字段
class AppConfig {
public:
    static AppConfig& instance();

    // 设置配置文件路径（须在首次读写前调用一次）
    void setConfigPath(const QString& filePath);

    // 数据目录（navhome.db、config.ini、icons/ 所在目录）
    QString dataDir() const;
    void setDataDir(const QString& dir);

    // —— [app] 段 ——
    QString defaultBrowser() const;
    void setDefaultBrowser(const QString& path);

    QString theme() const;
    void setTheme(const QString& theme);

    QString language() const;
    void setLanguage(const QString& lang);

    // 是否已完成首次浏览器选择引导（避免重复弹出）
    bool browserConfigured() const;
    void setBrowserConfigured(bool configured);

    // —— [security] 段 ——
    bool passwordEnabled() const;
    void setPasswordEnabled(bool enabled);

    QString password() const;
    void setPassword(const QString& pwd);

    bool passwordHash() const;
    void setPasswordHash(bool useHash);

    // —— [ui] 段 ——
    int iconIndex() const;
    void setIconIndex(int index);

    // 背景：空=默认；preset:xxx=内置渐变；image:path=自定义图片
    QString background() const;
    void setBackground(const QString& bg);

    // 标题栏透明度（0-100，100 为不透明）
    int titleBarOpacity() const;
    void setTitleBarOpacity(int opacity);

    // —— [backup] 段 ——
    bool backupEnabled() const;
    void setBackupEnabled(bool enabled);

    QString backupPeriod() const;   // daily / weekly / monthly
    void setBackupPeriod(const QString& period);

    QString backupTime() const;     // 如 23:00
    void setBackupTime(const QString& time);

    int backupKeepCount() const;
    void setBackupKeepCount(int count);

    bool backupEncrypt() const;
    void setBackupEncrypt(bool enabled);

    QString backupPath() const;
    void setBackupPath(const QString& path);

    // 立即同步写盘
    void sync();

private:
    AppConfig() = default;
    ~AppConfig();

    // 惰性创建 QSettings
    QSettings* settings() const;

    mutable QSettings* m_settings = nullptr;
    mutable QString m_path;
    QString m_dataDir;
};

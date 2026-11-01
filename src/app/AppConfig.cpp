#include "AppConfig.h"

#include <QSettings>

AppConfig& AppConfig::instance() {
    static AppConfig inst;
    return inst;
}

AppConfig::~AppConfig() {
    delete m_settings;
    m_settings = nullptr;
}

void AppConfig::setConfigPath(const QString& filePath) {
    m_path = filePath;
    // 路径变更后重建设置对象
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
}

QString AppConfig::dataDir() const {
    return m_dataDir;
}

void AppConfig::setDataDir(const QString& dir) {
    m_dataDir = dir;
}

QSettings* AppConfig::settings() const {
    if (!m_settings) {
        // Qt 6 的 INI 默认按 UTF-8 读写，无需额外设置编码
        m_settings = new QSettings(m_path, QSettings::IniFormat);
    }
    return m_settings;
}

void AppConfig::sync() {
    if (m_settings) {
        m_settings->sync();
    }
}

// —— [app] 段 ——

QString AppConfig::defaultBrowser() const {
    return settings()->value("app/default_browser", "").toString();
}

void AppConfig::setDefaultBrowser(const QString& path) {
    settings()->setValue("app/default_browser", path);
}

QString AppConfig::theme() const {
    return settings()->value("app/theme", "light").toString();
}

void AppConfig::setTheme(const QString& theme) {
    settings()->setValue("app/theme", theme);
}

QString AppConfig::language() const {
    return settings()->value("app/language", "zh_CN").toString();
}

void AppConfig::setLanguage(const QString& lang) {
    settings()->setValue("app/language", lang);
}

bool AppConfig::browserConfigured() const {
    return settings()->value("app/browser_configured", false).toBool();
}

void AppConfig::setBrowserConfigured(bool configured) {
    settings()->setValue("app/browser_configured", configured);
}

// —— [security] 段 ——

bool AppConfig::passwordEnabled() const {
    return settings()->value("security/password_enabled", false).toBool();
}

void AppConfig::setPasswordEnabled(bool enabled) {
    settings()->setValue("security/password_enabled", enabled);
}

QString AppConfig::password() const {
    return settings()->value("security/password", "").toString();
}

void AppConfig::setPassword(const QString& pwd) {
    settings()->setValue("security/password", pwd);
}

bool AppConfig::passwordHash() const {
    return settings()->value("security/password_hash", false).toBool();
}

void AppConfig::setPasswordHash(bool useHash) {
    settings()->setValue("security/password_hash", useHash);
}

// —— [ui] 段 ——

int AppConfig::iconIndex() const {
    return settings()->value("ui/icon_index", 0).toInt();
}

void AppConfig::setIconIndex(int index) {
    settings()->setValue("ui/icon_index", index);
}

QString AppConfig::background() const {
    return settings()->value("ui/background", "").toString();
}

void AppConfig::setBackground(const QString& bg) {
    settings()->setValue("ui/background", bg);
}

int AppConfig::titleBarOpacity() const {
    return settings()->value("ui/titlebar_opacity", 100).toInt();
}

void AppConfig::setTitleBarOpacity(int opacity) {
    settings()->setValue("ui/titlebar_opacity", opacity);
}

// —— [backup] 段 ——

bool AppConfig::backupEnabled() const {
    return settings()->value("backup/backup_enabled", false).toBool();
}

void AppConfig::setBackupEnabled(bool enabled) {
    settings()->setValue("backup/backup_enabled", enabled);
}

QString AppConfig::backupPeriod() const {
    return settings()->value("backup/backup_period", "daily").toString();
}

void AppConfig::setBackupPeriod(const QString& period) {
    settings()->setValue("backup/backup_period", period);
}

QString AppConfig::backupTime() const {
    return settings()->value("backup/backup_time", "23:00").toString();
}

void AppConfig::setBackupTime(const QString& time) {
    settings()->setValue("backup/backup_time", time);
}

int AppConfig::backupKeepCount() const {
    return settings()->value("backup/backup_keep_count", 7).toInt();
}

void AppConfig::setBackupKeepCount(int count) {
    settings()->setValue("backup/backup_keep_count", count);
}

bool AppConfig::backupEncrypt() const {
    return settings()->value("backup/backup_encrypt", false).toBool();
}

void AppConfig::setBackupEncrypt(bool enabled) {
    settings()->setValue("backup/backup_encrypt", enabled);
}

QString AppConfig::backupPath() const {
    return settings()->value("backup/backup_path", "").toString();
}

void AppConfig::setBackupPath(const QString& path) {
    settings()->setValue("backup/backup_path", path);
}

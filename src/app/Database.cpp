#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace {
// 数据库连接名（本程序全局唯一）
const char kConnectionName[] = "navhome";
}

Database& Database::instance() {
    static Database inst;
    return inst;
}

Database::~Database() {
    close();
}

bool Database::open(const QString& filePath) {
    if (m_ready) {
        return true;
    }

    // 复用已有连接，避免重复 addDatabase 告警
    if (QSqlDatabase::contains(kConnectionName)) {
        m_db = QSqlDatabase::database(kConnectionName, false);
    } else {
        // QSQLITE 驱动由 Qt 内置提供（MSYS2 Qt6 已包含 qsqlite 插件）
        m_db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    }
    m_db.setDatabaseName(filePath);

    if (!m_db.open()) {
        qWarning() << "数据库打开失败:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) {
        return false;
    }

    m_ready = true;
    return true;
}

void Database::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_ready = false;
}

bool Database::isReady() const {
    return m_ready;
}

QSqlDatabase Database::database() const {
    return m_db;
}

bool Database::createTables() {
    QSqlQuery q(m_db);

    // 分组表（对应需求 5.2）
    const QString sqlGroups = R"(
        CREATE TABLE IF NOT EXISTS groups (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL DEFAULT '',
            sort_order  INTEGER NOT NULL DEFAULT 0
        );
    )";

    // 收藏表（对应需求 5.2）
    const QString sqlBookmarks = R"(
        CREATE TABLE IF NOT EXISTS bookmarks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id    INTEGER NOT NULL DEFAULT 0,
            name        TEXT NOT NULL DEFAULT '',
            url         TEXT NOT NULL DEFAULT '',
            icon        TEXT NOT NULL DEFAULT '',
            sort_order  INTEGER NOT NULL DEFAULT 0,
            created_at  TEXT NOT NULL DEFAULT '',
            updated_at  TEXT NOT NULL DEFAULT ''
        );
    )";

    // 键值设置表（可选）
    const QString sqlSettings = R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL DEFAULT ''
        );
    )";

    const QStringList statements = { sqlGroups, sqlBookmarks, sqlSettings };
    for (const QString& sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "创建表失败:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QList>

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
            username    TEXT NOT NULL DEFAULT '',
            password    TEXT NOT NULL DEFAULT '',
            type        TEXT NOT NULL DEFAULT 'url',
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

    // 文件夹成员表：文件夹（type=folder 的收藏）与其收纳的收藏/应用
    const QString sqlFolderItems = R"(
        CREATE TABLE IF NOT EXISTS folder_items (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            folder_id   INTEGER NOT NULL,
            bookmark_id INTEGER NOT NULL,
            sort_order  INTEGER NOT NULL DEFAULT 0
        );
    )";

    const QStringList statements = { sqlGroups, sqlBookmarks, sqlSettings, sqlFolderItems };
    for (const QString& sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "创建表失败:" << q.lastError().text();
            return false;
        }
    }

    // 老版本数据库迁移：为 bookmarks 表补充新增列（type 用于区分网址/应用）
    migrate();
    return true;
}

// 增量迁移：老库已有数据时，CREATE TABLE IF NOT EXISTS 不会新增列，需逐列补齐
void Database::migrate() {
    auto hasColumn = [this](const QString& table, const QString& column) {
        QSqlQuery q(m_db);
        q.prepare(QString("PRAGMA table_info(%1)").arg(table));
        if (!q.exec()) {
            return false;
        }
        while (q.next()) {
            if (q.value(1).toString() == column) {
                return true;
            }
        }
        return false;
    };

    if (!hasColumn("bookmarks", "type")) {
        QSqlQuery q(m_db);
        if (!q.exec("ALTER TABLE bookmarks ADD COLUMN type TEXT NOT NULL DEFAULT 'url'")) {
            qWarning() << "迁移 bookmarks.type 失败:" << q.lastError().text();
        }
    }

    if (!hasColumn("bookmarks", "username")) {
        QSqlQuery q(m_db);
        if (!q.exec("ALTER TABLE bookmarks ADD COLUMN username TEXT NOT NULL DEFAULT ''")) {
            qWarning() << "迁移 bookmarks.username 失败:" << q.lastError().text();
        }
    }

    if (!hasColumn("bookmarks", "password")) {
        QSqlQuery q(m_db);
        if (!q.exec("ALTER TABLE bookmarks ADD COLUMN password TEXT NOT NULL DEFAULT ''")) {
            qWarning() << "迁移 bookmarks.password 失败:" << q.lastError().text();
        }
    }

    // 全局化 sort_order：老版本 sort_order 为分组内独立值（跨分组重复），
    // 导致“全部”视图排序混乱；改为全局唯一后“全部”视图拖拽排序才正确。
    {
        QSqlQuery dup(m_db);
        dup.exec("SELECT COUNT(*) FROM (SELECT sort_order, COUNT(*) c FROM bookmarks "
                 "GROUP BY sort_order HAVING c > 1)");
        if (dup.next() && dup.value(0).toInt() > 0) {
            QSqlQuery sel(m_db);
            sel.exec("SELECT id FROM bookmarks ORDER BY group_id ASC, sort_order ASC, id ASC");
            QList<int> ids;
            while (sel.next()) {
                ids.append(sel.value(0).toInt());
            }
            QSqlQuery upd(m_db);
            upd.prepare("UPDATE bookmarks SET sort_order = :o WHERE id = :id");
            for (int i = 0; i < ids.size(); ++i) {
                upd.bindValue(":o", i + 1);
                upd.bindValue(":id", ids[i]);
                upd.exec();
            }
        }
    }
}

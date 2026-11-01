#include "BookmarkRepository.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>

namespace {

// 将查询结果当前行解析为 Group
Group groupFromQuery(const QSqlQuery& q) {
    Group g;
    g.id = q.value("id").toInt();
    g.name = q.value("name").toString();
    g.sortOrder = q.value("sort_order").toInt();
    return g;
}

// 将查询结果当前行解析为 Bookmark
Bookmark bookmarkFromQuery(const QSqlQuery& q) {
    Bookmark b;
    b.id = q.value("id").toInt();
    b.groupId = q.value("group_id").toInt();
    b.name = q.value("name").toString();
    b.url = q.value("url").toString();
    b.icon = q.value("icon").toString();
    b.sortOrder = q.value("sort_order").toInt();
    b.createdAt = q.value("created_at").toString();
    b.updatedAt = q.value("updated_at").toString();
    return b;
}

// QSQLITE 驱动会把 null QString 绑定为 SQL NULL，这里统一转为空字符串
inline QString nz(const QString& v) {
    return v.isNull() ? QString("") : v;
}

} // namespace

BookmarkRepository& BookmarkRepository::instance() {
    static BookmarkRepository inst;
    return inst;
}

void BookmarkRepository::setDatabase(QSqlDatabase db) {
    m_db = db;
}

// —— 分组 ——

QList<Group> BookmarkRepository::groups() {
    QList<Group> result;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, name, sort_order FROM groups ORDER BY sort_order ASC, id ASC")) {
        qWarning() << "查询分组失败:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(groupFromQuery(q));
    }
    return result;
}

int BookmarkRepository::addGroup(const QString& name) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO groups (name, sort_order) VALUES (:name, "
              "(SELECT IFNULL(MAX(sort_order), 0) + 1 FROM groups))");
    q.bindValue(":name", nz(name));
    if (!q.exec()) {
        qWarning() << "新增分组失败:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool BookmarkRepository::renameGroup(int id, const QString& name) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE groups SET name = :name WHERE id = :id");
    q.bindValue(":name", nz(name));
    q.bindValue(":id", id);
    return q.exec();
}

bool BookmarkRepository::deleteGroup(int id, bool moveToUngrouped) {
    QSqlQuery q(m_db);
    if (moveToUngrouped) {
        q.prepare("UPDATE bookmarks SET group_id = 0 WHERE group_id = :id");
        q.bindValue(":id", id);
        if (!q.exec()) {
            return false;
        }
    } else {
        q.prepare("DELETE FROM bookmarks WHERE group_id = :id");
        q.bindValue(":id", id);
        if (!q.exec()) {
            return false;
        }
    }
    q.prepare("DELETE FROM groups WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool BookmarkRepository::updateGroupOrder(int id, int order) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE groups SET sort_order = :order WHERE id = :id");
    q.bindValue(":order", order);
    q.bindValue(":id", id);
    return q.exec();
}

// —— 收藏 ——

QList<Bookmark> BookmarkRepository::bookmarks(int groupId) {
    QList<Bookmark> result;
    QSqlQuery q(m_db);
    if (groupId < 0) {
        q.exec("SELECT * FROM bookmarks ORDER BY sort_order ASC, id ASC");
    } else {
        q.prepare("SELECT * FROM bookmarks WHERE group_id = :gid ORDER BY sort_order ASC, id ASC");
        q.bindValue(":gid", groupId);
        if (!q.exec()) {
            qWarning() << "查询收藏失败:" << q.lastError().text();
            return result;
        }
    }
    while (q.next()) {
        result.append(bookmarkFromQuery(q));
    }
    return result;
}

QList<Bookmark> BookmarkRepository::allBookmarks() {
    return bookmarks(-1);
}

QList<Bookmark> BookmarkRepository::search(const QString& keyword) {
    QList<Bookmark> result;
    if (keyword.trimmed().isEmpty()) {
        return allBookmarks();
    }
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM bookmarks WHERE name LIKE :kw OR url LIKE :kw ORDER BY sort_order ASC, id ASC");
    q.bindValue(":kw", "%" + keyword + "%");
    if (!q.exec()) {
        qWarning() << "搜索失败:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(bookmarkFromQuery(q));
    }
    return result;
}

int BookmarkRepository::addBookmark(const Bookmark& bookmark) {
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 计算新收藏的排序值（当前分组内最大 sort_order + 1）
    int nextOrder = 1;
    {
        QSqlQuery maxQ(m_db);
        maxQ.prepare("SELECT IFNULL(MAX(sort_order), 0) + 1 FROM bookmarks WHERE group_id = :gid");
        maxQ.bindValue(":gid", bookmark.groupId);
        if (maxQ.exec() && maxQ.next()) {
            nextOrder = maxQ.value(0).toInt();
        }
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO bookmarks (group_id, name, url, icon, sort_order, created_at, updated_at) "
              "VALUES (:gid, :name, :url, :icon, :sort, :created, :updated)");
    q.bindValue(":gid", bookmark.groupId);
    q.bindValue(":name", nz(bookmark.name));
    q.bindValue(":url", nz(bookmark.url));
    q.bindValue(":icon", nz(bookmark.icon));
    q.bindValue(":sort", nextOrder);
    q.bindValue(":created", now);
    q.bindValue(":updated", now);
    if (!q.exec()) {
        qWarning() << "新增收藏失败:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool BookmarkRepository::updateBookmark(const Bookmark& bookmark) {
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET group_id = :gid, name = :name, url = :url, icon = :icon, "
              "updated_at = :updated WHERE id = :id");
    q.bindValue(":gid", bookmark.groupId);
    q.bindValue(":name", nz(bookmark.name));
    q.bindValue(":url", nz(bookmark.url));
    q.bindValue(":icon", nz(bookmark.icon));
    q.bindValue(":updated", now);
    q.bindValue(":id", bookmark.id);
    return q.exec();
}

bool BookmarkRepository::deleteBookmark(int id) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM bookmarks WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool BookmarkRepository::deleteBookmarks(const QList<int>& ids) {
    if (ids.isEmpty()) {
        return true;
    }
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << ":id" + QString::number(i);
    }
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM bookmarks WHERE id IN (" + placeholders.join(", ") + ")");
    for (int i = 0; i < ids.size(); ++i) {
        q.bindValue(placeholders[i], ids[i]);
    }
    return q.exec();
}

bool BookmarkRepository::moveBookmarks(const QList<int>& ids, int targetGroupId) {
    if (ids.isEmpty()) {
        return true;
    }
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i) {
        placeholders << ":id" + QString::number(i);
    }
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET group_id = :gid WHERE id IN (" + placeholders.join(", ") + ")");
    q.bindValue(":gid", targetGroupId);
    for (int i = 0; i < ids.size(); ++i) {
        q.bindValue(placeholders[i], ids[i]);
    }
    return q.exec();
}

bool BookmarkRepository::updateBookmarkOrder(int id, int order) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET sort_order = :order WHERE id = :id");
    q.bindValue(":order", order);
    q.bindValue(":id", id);
    return q.exec();
}

bool BookmarkRepository::updateIcon(int id, const QString& icon) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET icon = :icon WHERE id = :id");
    q.bindValue(":icon", icon);
    q.bindValue(":id", id);
    return q.exec();
}

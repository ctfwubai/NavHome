#include "BookmarkRepository.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>

#include "app/Crypto.h"

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
    b.username = q.value("username").toString();
    b.password = Crypto::decrypt(q.value("password").toString());
    b.type = q.value("type").toString().isEmpty() ? "url" : q.value("type").toString();
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
        // 一并删除组内收藏前，先清理其文件夹成员关系，避免残留孤儿记录
        QSqlQuery fq(m_db);
        fq.prepare("DELETE FROM folder_items WHERE bookmark_id IN "
                   "(SELECT id FROM bookmarks WHERE group_id = :id)");
        fq.bindValue(":id", id);
        fq.exec();
        fq.prepare("DELETE FROM folder_items WHERE folder_id IN "
                   "(SELECT id FROM bookmarks WHERE group_id = :id AND type = 'folder')");
        fq.bindValue(":id", id);
        fq.exec();

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

    // 计算新收藏的排序值（全局最大 sort_order + 1，保证“全部”视图排序稳定）
    int nextOrder = 1;
    {
        QSqlQuery maxQ(m_db);
        maxQ.prepare("SELECT IFNULL(MAX(sort_order), 0) + 1 FROM bookmarks");
        if (maxQ.exec() && maxQ.next()) {
            nextOrder = maxQ.value(0).toInt();
        }
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO bookmarks (group_id, name, url, icon, username, password, type, sort_order, created_at, updated_at) "
              "VALUES (:gid, :name, :url, :icon, :username, :password, :type, :sort, :created, :updated)");
    q.bindValue(":gid", bookmark.groupId);
    q.bindValue(":name", nz(bookmark.name));
    q.bindValue(":url", nz(bookmark.url));
    q.bindValue(":icon", nz(bookmark.icon));
    q.bindValue(":username", nz(bookmark.username));
    q.bindValue(":password", nz(Crypto::encrypt(bookmark.password)));
    q.bindValue(":type", nz(bookmark.type).isEmpty() ? "url" : nz(bookmark.type));
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
              "username = :username, password = :password, type = :type, updated_at = :updated WHERE id = :id");
    q.bindValue(":gid", bookmark.groupId);
    q.bindValue(":name", nz(bookmark.name));
    q.bindValue(":url", nz(bookmark.url));
    q.bindValue(":icon", nz(bookmark.icon));
    q.bindValue(":username", nz(bookmark.username));
    q.bindValue(":password", nz(Crypto::encrypt(bookmark.password)));
    q.bindValue(":type", nz(bookmark.type).isEmpty() ? "url" : nz(bookmark.type));
    q.bindValue(":updated", now);
    q.bindValue(":id", bookmark.id);
    return q.exec();
}

bool BookmarkRepository::deleteBookmark(int id) {
    QSqlQuery q(m_db);
    // 清理文件夹成员关系（作为成员或作为文件夹）
    q.prepare("DELETE FROM folder_items WHERE bookmark_id = :id OR folder_id = :id");
    q.bindValue(":id", id);
    q.exec();

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
    const QString inClause = placeholders.join(", ");

    QSqlQuery q(m_db);
    q.prepare("DELETE FROM folder_items WHERE bookmark_id IN (" + inClause + ") "
              "OR folder_id IN (" + inClause + ")");
    for (int i = 0; i < ids.size(); ++i) {
        q.bindValue(placeholders[i], ids[i]);
    }
    q.exec();

    q.prepare("DELETE FROM bookmarks WHERE id IN (" + inClause + ")");
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

// —— 文件夹 ——

int BookmarkRepository::addFolder(int groupId, const QString& name) {
    Bookmark b;
    b.groupId = groupId;
    b.name = name.isEmpty() ? "文件夹" : name;
    b.type = "folder";
    b.url = QString();
    return addBookmark(b);
}

bool BookmarkRepository::renameFolder(int folderId, const QString& name) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE bookmarks SET name = :name, updated_at = :updated WHERE id = :id");
    q.bindValue(":name", name);
    q.bindValue(":updated", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":id", folderId);
    return q.exec();
}

bool BookmarkRepository::deleteFolder(int folderId) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM folder_items WHERE folder_id = :fid");
    q.bindValue(":fid", folderId);
    if (!q.exec()) {
        return false;
    }
    return deleteBookmark(folderId);
}

bool BookmarkRepository::addItemToFolder(int folderId, int bookmarkId) {
    // 已存在则视为成功（幂等）
    QSqlQuery dup(m_db);
    dup.prepare("SELECT id FROM folder_items WHERE folder_id = :fid AND bookmark_id = :bid");
    dup.bindValue(":fid", folderId);
    dup.bindValue(":bid", bookmarkId);
    if (dup.exec() && dup.next()) {
        return true;
    }

    int nextOrder = 1;
    QSqlQuery maxQ(m_db);
    maxQ.prepare("SELECT IFNULL(MAX(sort_order), 0) + 1 FROM folder_items WHERE folder_id = :fid");
    maxQ.bindValue(":fid", folderId);
    if (maxQ.exec() && maxQ.next()) {
        nextOrder = maxQ.value(0).toInt();
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO folder_items (folder_id, bookmark_id, sort_order) "
              "VALUES (:fid, :bid, :sort)");
    q.bindValue(":fid", folderId);
    q.bindValue(":bid", bookmarkId);
    q.bindValue(":sort", nextOrder);
    return q.exec();
}

bool BookmarkRepository::removeItemFromFolder(int folderId, int bookmarkId) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM folder_items WHERE folder_id = :fid AND bookmark_id = :bid");
    q.bindValue(":fid", folderId);
    q.bindValue(":bid", bookmarkId);
    return q.exec();
}

QList<Bookmark> BookmarkRepository::folderItems(int folderId) {
    QList<Bookmark> result;
    QSqlQuery q(m_db);
    q.prepare("SELECT b.* FROM bookmarks b JOIN folder_items fi ON fi.bookmark_id = b.id "
              "WHERE fi.folder_id = :fid ORDER BY fi.sort_order ASC, fi.id ASC");
    q.bindValue(":fid", folderId);
    if (!q.exec()) {
        qWarning() << "查询文件夹成员失败:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(bookmarkFromQuery(q));
    }
    return result;
}

QSet<int> BookmarkRepository::folderMemberIds() {
    QSet<int> result;
    QSqlQuery q(m_db);
    if (q.exec("SELECT bookmark_id FROM folder_items")) {
        while (q.next()) {
            result.insert(q.value(0).toInt());
        }
    }
    return result;
}

bool BookmarkRepository::isFolderMember(int bookmarkId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM folder_items WHERE bookmark_id = :bid LIMIT 1");
    q.bindValue(":bid", bookmarkId);
    return q.exec() && q.next();
}

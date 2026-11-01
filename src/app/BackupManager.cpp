#include "BackupManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>
#include <QHash>

#include "app/Database.h"

namespace {

// 执行查询并把结果集序列化为 JSON 数组
QJsonArray rowsToJson(QSqlDatabase db, const QString& sql) {
    QJsonArray arr;
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        return arr;
    }
    while (q.next()) {
        QJsonObject obj;
        const QSqlRecord rec = q.record();
        for (int i = 0; i < rec.count(); ++i) {
            obj.insert(rec.fieldName(i), QJsonValue::fromVariant(rec.value(i)));
        }
        arr.append(obj);
    }
    return arr;
}

} // namespace

bool BackupManager::exportData(const QString& filePath, QString* error) {
    const QSqlDatabase db = Database::instance().database();

    QJsonObject root;
    root.insert("app", "NavHome");
    root.insert("version", "1.0");
    root.insert("exported_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert("groups", rowsToJson(db, "SELECT * FROM groups ORDER BY sort_order ASC, id ASC"));
    root.insert("bookmarks",
                rowsToJson(db, "SELECT * FROM bookmarks ORDER BY sort_order ASC, id ASC"));
    root.insert("folder_items",
                rowsToJson(db, "SELECT * FROM folder_items ORDER BY folder_id ASC, sort_order ASC, id ASC"));

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = "无法写入文件：" + filePath;
        }
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool BackupManager::importData(const QString& filePath, ImportMode mode, QString* error) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "无法读取文件：" + filePath;
        }
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = "备份文件格式错误：" + parseErr.errorString();
        }
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray groupsArr = root.value("groups").toArray();
    const QJsonArray bookmarksArr = root.value("bookmarks").toArray();
    const QJsonArray folderItemsArr = root.value("folder_items").toArray();

    QSqlDatabase db = Database::instance().database();
    if (!db.transaction()) {
        if (error) {
            *error = "无法开始事务：" + db.lastError().text();
        }
        return false;
    }

    // 覆盖模式：先清空现有数据（folder_items 先删，避免残留孤儿记录）
    if (mode == Overwrite) {
        QSqlQuery q(db);
        if (!q.exec("DELETE FROM folder_items") || !q.exec("DELETE FROM bookmarks") ||
            !q.exec("DELETE FROM groups")) {
            if (error) {
                *error = q.lastError().text();
            }
            db.rollback();
            return false;
        }
    }

    // 导入分组（重新分配 id 以避免主键冲突），记录旧 id -> 新 id 映射
    QHash<int, int> groupIdMap;
    for (const QJsonValue& v : groupsArr) {
        const QJsonObject g = v.toObject();
        const int oldId = g.value("id").toInt();
        QSqlQuery q(db);
        q.prepare("INSERT INTO groups (name, sort_order) VALUES (:name, :sort)");
        q.bindValue(":name", g.value("name").toString());
        q.bindValue(":sort", g.value("sort_order").toInt());
        if (!q.exec()) {
            if (error) {
                *error = "导入分组失败：" + q.lastError().text();
            }
            db.rollback();
            return false;
        }
        groupIdMap.insert(oldId, q.lastInsertId().toInt());
    }

    // 导入收藏（group_id 依据映射重写；type 区分网址/应用/文件夹），记录旧 id -> 新 id 映射
    QHash<int, int> bookmarkIdMap;
    for (const QJsonValue& v : bookmarksArr) {
        const QJsonObject b = v.toObject();
        const int oldId = b.value("id").toInt();
        const int oldGroupId = b.value("group_id").toInt();
        const int newGroupId = groupIdMap.value(oldGroupId, 0);
        const QString type = b.value("type").toString().isEmpty() ? "url" : b.value("type").toString();

        QSqlQuery q(db);
        q.prepare("INSERT INTO bookmarks (group_id, name, url, icon, username, password, type, sort_order, created_at, updated_at) "
                  "VALUES (:gid, :name, :url, :icon, :username, :password, :type, :sort, :created, :updated)");
        q.bindValue(":gid", newGroupId);
        q.bindValue(":name", b.value("name").toString());
        q.bindValue(":url", b.value("url").toString());
        q.bindValue(":icon", b.value("icon").toString());
        // 账号明文原样导入；密码为 DPAPI 密文，原样导入（换机器后不可解密，用户重新填写即可）
        q.bindValue(":username", b.value("username").toString());
        q.bindValue(":password", b.value("password").toString());
        q.bindValue(":type", type);
        q.bindValue(":sort", b.value("sort_order").toInt());
        q.bindValue(":created", b.value("created_at").toString());
        q.bindValue(":updated", b.value("updated_at").toString());
        if (!q.exec()) {
            if (error) {
                *error = "导入收藏失败：" + q.lastError().text();
            }
            db.rollback();
            return false;
        }
        bookmarkIdMap.insert(oldId, q.lastInsertId().toInt());
    }

    // 导入文件夹成员关系（folder_id / bookmark_id 均依据映射重写）
    for (const QJsonValue& v : folderItemsArr) {
        const QJsonObject fi = v.toObject();
        const int oldFolderId = fi.value("folder_id").toInt();
        const int oldBookmarkId = fi.value("bookmark_id").toInt();
        // 映射缺失说明备份数据不完整（如旧版本无文件夹功能），跳过该成员
        if (!bookmarkIdMap.contains(oldFolderId) || !bookmarkIdMap.contains(oldBookmarkId)) {
            continue;
        }

        QSqlQuery q(db);
        q.prepare("INSERT INTO folder_items (folder_id, bookmark_id, sort_order) "
                  "VALUES (:fid, :bid, :sort)");
        q.bindValue(":fid", bookmarkIdMap.value(oldFolderId));
        q.bindValue(":bid", bookmarkIdMap.value(oldBookmarkId));
        q.bindValue(":sort", fi.value("sort_order").toInt());
        if (!q.exec()) {
            if (error) {
                *error = "导入文件夹成员失败：" + q.lastError().text();
            }
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        if (error) {
            *error = "提交事务失败：" + db.lastError().text();
        }
        db.rollback();
        return false;
    }
    return true;
}

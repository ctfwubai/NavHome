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

    QSqlDatabase db = Database::instance().database();
    if (!db.transaction()) {
        if (error) {
            *error = "无法开始事务：" + db.lastError().text();
        }
        return false;
    }

    // 覆盖模式：先清空现有数据
    if (mode == Overwrite) {
        QSqlQuery q(db);
        if (!q.exec("DELETE FROM bookmarks") || !q.exec("DELETE FROM groups")) {
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

    // 导入收藏（group_id 依据映射重写）
    for (const QJsonValue& v : bookmarksArr) {
        const QJsonObject b = v.toObject();
        const int oldGroupId = b.value("group_id").toInt();
        const int newGroupId = groupIdMap.value(oldGroupId, 0);

        QSqlQuery q(db);
        q.prepare("INSERT INTO bookmarks (group_id, name, url, icon, sort_order, created_at, updated_at) "
                  "VALUES (:gid, :name, :url, :icon, :sort, :created, :updated)");
        q.bindValue(":gid", newGroupId);
        q.bindValue(":name", b.value("name").toString());
        q.bindValue(":url", b.value("url").toString());
        q.bindValue(":icon", b.value("icon").toString());
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

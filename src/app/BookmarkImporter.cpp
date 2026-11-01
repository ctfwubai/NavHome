#include "BookmarkImporter.h"

#include <QFile>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>
#include <QHash>
#include <QStringList>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>

#include <algorithm>

#include "app/Database.h"

namespace {

struct ParsedBookmark {
    QString name;
    QString url;
    QString groupName;  // 空表示未分组
};

// 解析常见的 HTML 实体
QString decodeEntities(const QString& s) {
    QString r = s;
    r.replace("&amp;", "&");
    r.replace("&lt;", "<");
    r.replace("&gt;", ">");
    r.replace("&quot;", "\"");
    r.replace("&#39;", "'");
    r.replace("&apos;", "'");
    r.replace("&nbsp;", " ");
    return r;
}

struct Event {
    int pos = 0;
    enum Type { Folder, Bookmark, FolderEnd } type = FolderEnd;
    QString name;
    QString url;
};

// 解析 Netscape 书签 HTML：按标签出现位置排序，用栈跟踪分组嵌套
bool parseHtml(const QString& html, QStringList* groupNames, QList<ParsedBookmark>* bookmarks) {
    static const QRegularExpression h3Re(
        "<H3[^>]*>(.*?)</H3>",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression aRe(
        "<A[^>]*HREF=[\"']([^\"']*)[\"'][^>]*>(.*?)</A>",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression dlEndRe("</DL>", QRegularExpression::CaseInsensitiveOption);

    QList<Event> events;

    auto h3It = h3Re.globalMatch(html);
    while (h3It.hasNext()) {
        const QRegularExpressionMatch m = h3It.next();
        Event e;
        e.pos = m.capturedStart();
        e.type = Event::Folder;
        e.name = decodeEntities(m.captured(1).trimmed());
        events.append(e);
    }

    auto aIt = aRe.globalMatch(html);
    while (aIt.hasNext()) {
        const QRegularExpressionMatch m = aIt.next();
        Event e;
        e.pos = m.capturedStart();
        e.type = Event::Bookmark;
        e.url = decodeEntities(m.captured(1).trimmed());
        e.name = decodeEntities(m.captured(2).trimmed());
        events.append(e);
    }

    auto dlIt = dlEndRe.globalMatch(html);
    while (dlIt.hasNext()) {
        const QRegularExpressionMatch m = dlIt.next();
        Event e;
        e.pos = m.capturedStart();
        e.type = Event::FolderEnd;
        events.append(e);
    }

    std::sort(events.begin(), events.end(),
              [](const Event& a, const Event& b) { return a.pos < b.pos; });

    QStringList stack;        // 当前分组层级栈
    QStringList seenGroups;   // 按出现顺序去重的分组名

    for (const Event& e : events) {
        if (e.type == Event::Folder) {
            if (!e.name.isEmpty()) {
                stack.append(e.name);
                if (!seenGroups.contains(e.name)) {
                    seenGroups.append(e.name);
                }
            }
        } else if (e.type == Event::Bookmark) {
            if (!e.url.isEmpty()) {
                ParsedBookmark b;
                b.name = e.name.isEmpty() ? e.url : e.name;
                b.url = e.url;
                b.groupName = stack.isEmpty() ? QString() : stack.last();
                bookmarks->append(b);
            }
        } else {  // FolderEnd
            if (!stack.isEmpty()) {
                stack.removeLast();
            }
        }
    }

    *groupNames = seenGroups;
    return true;
}

// 递归解析 Chrome/Edge 书签 JSON 的 folder 节点；groupPath 为当前文件夹完整路径（如“书签栏/子文件夹”）
void parseChromeFolder(const QJsonObject& folder, const QString& groupPath,
                       QStringList* groupNames, QList<ParsedBookmark>* bookmarks) {
    const QJsonArray children = folder.value("children").toArray();
    for (const QJsonValue& v : children) {
        const QJsonObject node = v.toObject();
        const QString type = node.value("type").toString();
        const QString name = node.value("name").toString();
        if (type == "url") {
            const QString url = node.value("url").toString();
            if (!url.isEmpty()) {
                ParsedBookmark b;
                b.name = name.isEmpty() ? url : name;
                b.url = url;
                b.groupName = groupPath;
                bookmarks->append(b);
            }
        } else if (type == "folder") {
            const QString childPath = groupPath.isEmpty() ? name : groupPath + "/" + name;
            if (!childPath.isEmpty() && !groupNames->contains(childPath)) {
                groupNames->append(childPath);
            }
            parseChromeFolder(node, childPath, groupNames, bookmarks);
        }
    }
}

// 解析 Chrome/Edge 书签 JSON（roots -> folder 树）
bool parseChromeJson(const QString& json, QStringList* groupNames, QList<ParsedBookmark>* bookmarks) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject roots = doc.object().value("roots").toObject();
    for (const QString& key : roots.keys()) {
        const QJsonObject root = roots.value(key).toObject();
        const QString rootName = root.value("name").toString();
        if (!rootName.isEmpty() && !groupNames->contains(rootName)) {
            groupNames->append(rootName);
        }
        parseChromeFolder(root, rootName, groupNames, bookmarks);
    }
    return true;
}

// 将解析结果（分组名 + 书签列表）写入数据库，整体在一个事务内完成
bool importParsed(const QStringList& groupNames, const QList<ParsedBookmark>& bookmarks,
                  BookmarkImporter::Result* result, QString* error) {
    QSqlDatabase db = Database::instance().database();
    if (!db.transaction()) {
        if (error) {
            *error = "无法开始事务：" + db.lastError().text();
        }
        return false;
    }

    // 已有分组名 -> id 映射，同名分组复用
    QHash<QString, int> nameToId;
    {
        QSqlQuery q(db);
        if (q.exec("SELECT id, name FROM groups")) {
            while (q.next()) {
                nameToId.insert(q.value(1).toString(), q.value(0).toInt());
            }
        }
    }

    // 创建缺失分组
    int groupsCreated = 0;
    for (const QString& name : groupNames) {
        if (nameToId.contains(name)) {
            continue;
        }
        QSqlQuery q(db);
        q.prepare("INSERT INTO groups (name, sort_order) VALUES (:name, "
                  "(SELECT IFNULL(MAX(sort_order), 0) + 1 FROM groups))");
        q.bindValue(":name", name);
        if (!q.exec()) {
            if (error) {
                *error = "创建分组失败：" + q.lastError().text();
            }
            db.rollback();
            return false;
        }
        nameToId.insert(name, q.lastInsertId().toInt());
        ++groupsCreated;
    }

    // 导入收藏
    int imported = 0;
    for (const ParsedBookmark& pb : bookmarks) {
        const int gid = pb.groupName.isEmpty() ? 0 : nameToId.value(pb.groupName, 0);

        int nextOrder = 1;
        {
            QSqlQuery maxQ(db);
            maxQ.prepare("SELECT IFNULL(MAX(sort_order), 0) + 1 FROM bookmarks WHERE group_id = :gid");
            maxQ.bindValue(":gid", gid);
            if (maxQ.exec() && maxQ.next()) {
                nextOrder = maxQ.value(0).toInt();
            }
        }

        const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        QSqlQuery q(db);
        q.prepare("INSERT INTO bookmarks (group_id, name, url, icon, sort_order, created_at, updated_at) "
                  "VALUES (:gid, :name, :url, :icon, :sort, :created, :updated)");
        q.bindValue(":gid", gid);
        q.bindValue(":name", pb.name);
        q.bindValue(":url", pb.url);
        q.bindValue(":icon", QStringLiteral(""));  // 空字符串，避免 NULL 触发 NOT NULL 约束
        q.bindValue(":sort", nextOrder);
        q.bindValue(":created", now);
        q.bindValue(":updated", now);
        if (!q.exec()) {
            if (error) {
                *error = "导入收藏失败：" + q.lastError().text();
            }
            db.rollback();
            return false;
        }
        ++imported;
    }

    if (!db.commit()) {
        if (error) {
            *error = "提交事务失败：" + db.lastError().text();
        }
        db.rollback();
        return false;
    }

    if (result) {
        result->groupsCreated = groupsCreated;
        result->bookmarksImported = imported;
    }
    return true;
}

} // namespace

bool BookmarkImporter::import(const QString& filePath, Result* result, QString* error) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = "无法打开文件：" + filePath;
        }
        return false;
    }
    QByteArray raw = f.readAll();
    f.close();
    if (raw.startsWith("\xEF\xBB\xBF")) {
        raw.remove(0, 3);  // 去除 UTF-8 BOM（Edge/Chrome 导出的 HTML 常带）
    }
    const QString html = QString::fromUtf8(raw);

    QStringList groupNames;
    QList<ParsedBookmark> bookmarks;
    if (!parseHtml(html, &groupNames, &bookmarks)) {
        if (error) {
            *error = "解析书签文件失败";
        }
        return false;
    }
    return importParsed(groupNames, bookmarks, result, error);
}

bool BookmarkImporter::importChromeBookmarks(const QString& bookmarksJsonPath, Result* result, QString* error) {
    QFile f(bookmarksJsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "无法打开文件：" + bookmarksJsonPath;
        }
        return false;
    }
    const QString json = QString::fromUtf8(f.readAll());
    f.close();

    QStringList groupNames;
    QList<ParsedBookmark> bookmarks;
    if (!parseChromeJson(json, &groupNames, &bookmarks)) {
        if (error) {
            *error = "解析浏览器书签数据失败";
        }
        return false;
    }
    return importParsed(groupNames, bookmarks, result, error);
}

bool BookmarkImporter::importFolder(const QString& dirPath, Result* result, QString* error) {
    const QDir dir(dirPath);
    if (!dir.exists()) {
        if (error) {
            *error = "文件夹不存在：" + dirPath;
        }
        return false;
    }

    QStringList groupNames;
    QList<ParsedBookmark> bookmarks;

    // 递归扫描文件夹内所有 .html/.htm 书签文件，逐个解析后合并
    QDirIterator it(dirPath, {QStringLiteral("*.html"), QStringLiteral("*.htm")},
                    QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    int filesParsed = 0;
    while (it.hasNext()) {
        const QString filePath = it.next();
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        QByteArray raw = f.readAll();
        f.close();
        if (raw.startsWith("\xEF\xBB\xBF")) {
            raw.remove(0, 3);  // 去除 UTF-8 BOM
        }
        const QString html = QString::fromUtf8(raw);

        QStringList g;
        QList<ParsedBookmark> b;
        if (parseHtml(html, &g, &b)) {
            for (const QString& name : g) {
                if (!groupNames.contains(name)) {
                    groupNames.append(name);
                }
            }
            bookmarks.append(b);
            ++filesParsed;
        }
    }

    if (filesParsed == 0) {
        if (error) {
            *error = "文件夹内未找到可导入的 HTML 书签文件";
        }
        return false;
    }
    return importParsed(groupNames, bookmarks, result, error);
}

QString BookmarkImporter::chromeBookmarksPath() {
    const QString local = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString base = local + "/Google/Chrome/User Data";
    const QString def = base + "/Default/Bookmarks";
    if (QFile::exists(def)) {
        return def;
    }
    for (int i = 1; i <= 20; ++i) {
        const QString p = base + QString("/Profile %1/Bookmarks").arg(i);
        if (QFile::exists(p)) {
            return p;
        }
    }
    return QString();
}

QString BookmarkImporter::edgeBookmarksPath() {
    const QString local = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString base = local + "/Microsoft/Edge/User Data";
    const QString def = base + "/Default/Bookmarks";
    if (QFile::exists(def)) {
        return def;
    }
    for (int i = 1; i <= 20; ++i) {
        const QString p = base + QString("/Profile %1/Bookmarks").arg(i);
        if (QFile::exists(p)) {
            return p;
        }
    }
    return QString();
}

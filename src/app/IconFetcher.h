#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// 网站图标（favicon）抓取器（对应需求 3.4.1）
class IconFetcher : public QObject {
    Q_OBJECT
public:
    explicit IconFetcher(QObject* parent = nullptr);

    // 为指定收藏异步抓取 favicon，成功后发出 iconFetched 信号
    void fetchForBookmark(int bookmarkId, const QString& url);

    // 由网址构造 favicon 候选地址（域名/favicon.ico）
    static QString faviconUrl(const QString& url);

signals:
    // 抓取成功：bookmarkId 与保存到本地的图标路径
    void iconFetched(int bookmarkId, const QString& localPath);

private:
    QNetworkAccessManager* m_nam = nullptr;
    QHash<QNetworkReply*, int> m_pending;   // 请求 -> 收藏 id
};

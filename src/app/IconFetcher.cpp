#include "IconFetcher.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDir>
#include <QPixmap>
#include <QCoreApplication>

#include "app/AppConfig.h"

IconFetcher::IconFetcher(QObject* parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void IconFetcher::fetchForBookmark(int bookmarkId, const QString& url) {
    const QString fav = faviconUrl(url);
    if (fav.isEmpty()) {
        return;
    }

    QNetworkRequest req{QUrl(fav)};
    req.setTransferTimeout(5000);   // 5 秒超时，避免长时间阻塞
    QNetworkReply* reply = m_nam->get(req);
    m_pending.insert(reply, bookmarkId);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int id = m_pending.take(reply);
        if (reply->error() != QNetworkReply::NoError) {
            return;   // 抓取失败，保持占位图标
        }
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) {
            return;
        }
        // 保存到数据目录下的 icons 子目录，统一转为 PNG
        const QString dirPath = QDir(AppConfig::instance().dataDir()).filePath("icons");
        QDir().mkpath(dirPath);
        const QString localPath = QDir(dirPath).filePath(QString("bookmark_%1.png").arg(id));
        if (pm.save(localPath, "PNG")) {
            emit iconFetched(id, localPath);
        }
    });
}

QString IconFetcher::faviconUrl(const QString& url) {
    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty()) {
        return QString();
    }
    QString scheme = u.scheme();
    if (!scheme.startsWith("http", Qt::CaseInsensitive)) {
        return QString();
    }
    return scheme + "://" + u.host() + "/favicon.ico";
}

#pragma once

#include <QSqlDatabase>
#include <QString>

// SQLite 数据库封装：负责连接、建表与生命周期管理
class Database {
public:
    // 获取全局唯一实例
    static Database& instance();

    // 打开（不存在则创建）指定路径的数据库，并初始化表结构
    bool open(const QString& filePath);

    // 关闭数据库连接
    void close();

    // 当前是否已就绪
    bool isReady() const;

    // 获取底层 QSqlDatabase 引用，供查询使用
    QSqlDatabase database() const;

private:
    Database() = default;
    ~Database();

    // 初始化表结构
    bool createTables();

    QSqlDatabase m_db;
    bool m_ready = false;
};

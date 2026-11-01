#pragma once

#include <QString>

// 本地敏感数据加密工具：Windows 下用 DPAPI（绑定当前用户/机器），密文以 Base64 文本存储
class Crypto {
public:
    // 加密明文为 Base64 密文；空串或失败返回空串
    static QString encrypt(const QString& plain);

    // 解密 Base64 密文为明文；空串或失败返回空串
    static QString decrypt(const QString& encoded);
};

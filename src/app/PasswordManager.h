#pragma once

#include <QString>

// 密码存储与校验（对应需求 3.7）
class PasswordManager {
public:
    // 计算 SHA-256 哈希（十六进制字符串）
    static QString hash(const QString& password);

    // 是否已启用密码保护
    static bool isEnabled();

    // 校验输入密码是否正确
    static bool verify(const QString& input);

    // 启用密码保护；useHash=true 时以哈希存储，否则明文
    static void enable(const QString& password, bool useHash);

    // 关闭密码保护
    static void disable();
};

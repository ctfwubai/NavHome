#include "Crypto.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>
#endif

QString Crypto::encrypt(const QString& plain) {
#ifdef Q_OS_WIN
    if (plain.isEmpty()) {
        return QString("");  // 空串（非 null），避免绑定为 SQL NULL 触发 NOT NULL 约束
    }
    const QByteArray utf8 = plain.toUtf8();
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.constData()));
    in.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return QString();
    }
    const QByteArray enc(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return QString::fromLatin1(enc.toBase64());
#else
    return plain;
#endif
}

QString Crypto::decrypt(const QString& encoded) {
#ifdef Q_OS_WIN
    if (encoded.isEmpty()) {
        return QString("");
    }
    const QByteArray raw = QByteArray::fromBase64(encoded.toLatin1());
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(raw.constData()));
    in.cbData = static_cast<DWORD>(raw.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return QString();
    }
    const QString plain = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData),
                                            static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return plain;
#else
    return encoded;
#endif
}

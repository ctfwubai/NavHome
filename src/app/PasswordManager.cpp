#include "PasswordManager.h"

#include <QCryptographicHash>

#include "app/AppConfig.h"

QString PasswordManager::hash(const QString& password) {
    return QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool PasswordManager::isEnabled() {
    return AppConfig::instance().passwordEnabled();
}

bool PasswordManager::verify(const QString& input) {
    const QString stored = AppConfig::instance().password();
    const bool useHash = AppConfig::instance().passwordHash();

    if (useHash) {
        return hash(input) == stored;
    }
    return input == stored;
}

void PasswordManager::enable(const QString& password, bool useHash) {
    AppConfig::instance().setPasswordEnabled(true);
    AppConfig::instance().setPasswordHash(useHash);
    AppConfig::instance().setPassword(useHash ? hash(password) : password);
    AppConfig::instance().sync();
}

void PasswordManager::disable() {
    AppConfig::instance().setPasswordEnabled(false);
    AppConfig::instance().setPassword("");
    AppConfig::instance().sync();
}

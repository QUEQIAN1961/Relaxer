#include "credential-store.h"

#include "ai-model.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace relaxer::ai {
namespace {
bool isKnownProvider(const QString &providerId) {
    return providerId == QString::fromLatin1(DeepSeekProviderId);
}

QString targetName(const QString &providerId) {
    return QStringLiteral("Relaxer/AI/%1/api-key").arg(providerId);
}

void setError(QString *error) {
    if (error)
        *error = QStringLiteral("无法访问 Windows 凭据存储");
}
} // namespace

QString normalizedApiKey(const QString &secret, QString *error) {
    qsizetype begin = 0, end = secret.size();
    while (begin < end && secret.at(begin) == QLatin1Char(' ')) ++begin;
    while (end > begin && secret.at(end - 1) == QLatin1Char(' ')) --end;
    bool valid = begin < end;
    for (auto i = begin; valid && i < end; ++i)
        valid = secret.at(i).unicode() >= 0x21 && secret.at(i).unicode() <= 0x7e;
    if (error)
        *error = valid ? QString() : QStringLiteral("API Key 为空或包含空白、控制字符或非 ASCII 字符");
    return valid ? secret.mid(begin, end - begin) : QString();
}

bool WindowsCredentialStore::store(const QString &providerId, const QString &secret, QString *error) {
#ifdef Q_OS_WIN
    if (!isKnownProvider(providerId) || secret.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("供应商或 API Key 无效");
        return false;
    }
    auto bytes = secret.toUtf8();
    if (bytes.isEmpty() || bytes.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        SecureZeroMemory(bytes.data(), size_t(bytes.size()));
        if (error)
            *error = QStringLiteral("API Key 长度无效");
        return false;
    }
    const auto target = targetName(providerId).toStdWString();
    const auto username = providerId.toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.UserName = const_cast<LPWSTR>(username.c_str());
    credential.CredentialBlobSize = DWORD(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(bytes.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    const bool ok = CredWriteW(&credential, 0) != FALSE;
    SecureZeroMemory(bytes.data(), size_t(bytes.size()));
    if (!ok)
        setError(error);
    else if (error)
        error->clear();
    return ok;
#else
    Q_UNUSED(providerId)
    Q_UNUSED(secret)
    setError(error);
    return false;
#endif
}

CredentialReadResult WindowsCredentialStore::read(const QString &providerId) {
    CredentialReadResult result;
#ifdef Q_OS_WIN
    if (!isKnownProvider(providerId)) {
        result.error = QStringLiteral("供应商无效");
        return result;
    }
    PCREDENTIALW credential = nullptr;
    const auto target = targetName(providerId).toStdWString();
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        result.ok = GetLastError() == ERROR_NOT_FOUND;
        if (!result.ok)
            result.error = QStringLiteral("无法读取 Windows 凭据");
        return result;
    }
    result.ok = true;
    result.found = credential && credential->CredentialBlob && credential->CredentialBlobSize > 0;
    if (result.found) {
        QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                         int(credential->CredentialBlobSize));
        result.secret = QString::fromUtf8(bytes.constData(), bytes.size());
        SecureZeroMemory(bytes.data(), size_t(bytes.size()));
        SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
    }
    if (credential)
        CredFree(credential);
#else
    Q_UNUSED(providerId)
    result.error = QStringLiteral("此平台不支持 Windows 凭据存储");
#endif
    return result;
}

bool WindowsCredentialStore::remove(const QString &providerId, QString *error) {
#ifdef Q_OS_WIN
    if (!isKnownProvider(providerId)) {
        if (error)
            *error = QStringLiteral("供应商无效");
        return false;
    }
    const auto target = targetName(providerId).toStdWString();
    const bool ok = CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != FALSE;
    if (ok || GetLastError() == ERROR_NOT_FOUND) {
        if (error)
            error->clear();
        return true;
    }
    setError(error);
    return false;
#else
    Q_UNUSED(providerId)
    setError(error);
    return false;
#endif
}

bool WindowsCredentialStore::supported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
} // namespace relaxer::ai

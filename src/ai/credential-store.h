#pragma once

#include <QString>

namespace relaxer::ai {

struct CredentialReadResult {
    bool ok = false;
    bool found = false;
    QString secret;
    QString error;
};

class CredentialStore {
  public:
    virtual ~CredentialStore() = default;
    virtual bool store(const QString &providerId, const QString &secret, QString *error = nullptr) = 0;
    virtual CredentialReadResult read(const QString &providerId) = 0;
    virtual bool remove(const QString &providerId, QString *error = nullptr) = 0;
    virtual bool supported() const = 0;
};

class WindowsCredentialStore final : public CredentialStore {
  public:
    bool store(const QString &providerId, const QString &secret, QString *error = nullptr) override;
    CredentialReadResult read(const QString &providerId) override;
    bool remove(const QString &providerId, QString *error = nullptr) override;
    bool supported() const override;
};

} // namespace relaxer::ai

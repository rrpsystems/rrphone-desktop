#pragma once

#include <QString>

// The SIP password never goes into QSettings (registry) or any config file
// in plain text — it lives in the Windows Credential Manager, which is the
// non-functional requirement in Section 6 of the PRD.
//
// Everything is stored under a single generic credential target, since the
// MVP is a single-account softphone.
class CredentialStore {
public:
    static constexpr auto kTarget = "RRPSoftphone";

    static bool savePassword(const QString &target, const QString &user, const QString &password);
    // Returns false when there's no stored credential (a fresh install).
    static bool loadPassword(const QString &target, QString *passwordOut);
    static bool removePassword(const QString &target);
};

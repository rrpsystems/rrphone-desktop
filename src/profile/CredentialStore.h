#pragma once

#include <QString>

// The SIP password never goes into QSettings (registry) or any config file
// in plain text — it lives in the Windows Credential Manager, which is the
// non-functional requirement in Section 6 of the PRD.
//
// One generic credential per account ("RRPSoftphone:<ramal>@<servidor>").
// The Credential Manager is shared by every copy of the app on the machine —
// the classic installer and the Microsoft Store (MSIX) package, whose
// settings are otherwise kept apart — so a single fixed name let one copy
// overwrite the other's password and the next REGISTER got 403.
class CredentialStore {
public:
    // Pre-1.0.6 name, shared by all accounts: still read (and migrated) when
    // its user matches, never written.
    static constexpr auto kTarget = "RRPSoftphone";

    static QString targetFor(const QString &user, const QString &domain);

    static bool savePassword(const QString &target, const QString &user, const QString &password);
    // Returns false when there's no stored credential (a fresh install).
    // userOut, when given, receives the credential's user name.
    static bool loadPassword(const QString &target, QString *passwordOut, QString *userOut = nullptr);
    static bool removePassword(const QString &target);
};

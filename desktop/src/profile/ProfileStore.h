#pragma once

#include <QString>
#include <QList>

#include "core/CodecInfo.h"

// D-13 — import/export of a single account "backup" profile.
//
// SECURITY NOTE (see README.md "Known gaps"): the passphrase-based
// obfuscation used here (see ProfileStore.cpp) is a PLACEHOLDER, not
// production-grade encryption. It exists so the import/export UI flow can be
// built and exercised end-to-end now. It MUST be replaced with a real AEAD
// cipher (e.g. AES-256-GCM via OpenSSL/mbedTLS — both already linked
// transitively through liblinphone) before this app is used with real
// customer SIP credentials.
struct AccountProfile {
    QString displayName;
    QString username;
    QString password; // plaintext in memory; only ever encrypted at rest in the file
    QString domain;
    QString transport;       // "udp" | "tcp" | "tls"
    QString dtmfMethod;      // "rfc2833" | "info" | "inband" — mirrors SipCoreManager::DtmfMethod
    QString contactsUrl;     // D-16, optional
    QList<CodecInfo> codecs; // D-14, in priority order; empty = "use engine defaults"
};

class ProfileStore {
public:
    // Writes `profile` to `filePath` as a `.rrpprofile` (JSON) file, with
    // the password field protected by `passphrase`.
    // Returns true on success; on failure, `errorOut` (if non-null) is set.
    static bool exportProfile(const AccountProfile &profile,
                               const QString &filePath,
                               const QString &passphrase,
                               QString *errorOut = nullptr);

    // Reads and decrypts a `.rrpprofile` file. `passphrase` must match the
    // one used at export time, or decryption yields garbage (there is no
    // separate integrity check in the placeholder cipher — see header note
    // above; a real AEAD cipher would reject a wrong passphrase outright).
    static bool importProfile(const QString &filePath,
                               const QString &passphrase,
                               AccountProfile *profileOut,
                               QString *errorOut = nullptr);
};

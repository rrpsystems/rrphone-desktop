#pragma once

#include <QString>
#include <QList>

#include "core/CodecInfo.h"

// D-13 — import/export of a single account "backup" profile.
//
// The file is AES-256-GCM with a PBKDF2-derived key; the whole account goes
// inside the encrypted payload, including the contacts URL, which routinely
// carries HTTP credentials of its own. See ProfileCipher.h for the key model
// — in particular, why a profile exported without a passphrase is obfuscated
// rather than confidential.
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

    // Whether importing this file needs a passphrase from the user. Lets the
    // UI skip the prompt entirely for the common case (app-key protection).
    static bool requiresPassphrase(const QString &filePath);
};

#pragma once

#include <QByteArray>
#include <QString>

// Authenticated encryption for the .rrpprofile provisioning file
// (AES-256-GCM, key derived with PBKDF2-HMAC-SHA256, all from mbedTLS, which
// already ships with the liblinphone SDK).
//
// KEY MODEL — read this before trusting the file with anything.
//
// By default the key comes from a constant compiled into the app, so a profile
// can be exported and imported without anyone typing a password. That constant
// lives in this repository, which is public: it is not a secret, and anyone who
// has the app or the source can decrypt any profile produced this way.
// Treat the default as *obfuscation with integrity checking* — it keeps SIP
// passwords and the provisioning URL's credentials out of plain sight in an
// emailed attachment, and it detects tampering. It does not make the file
// confidential against someone who wants in.
//
// When a passphrase is supplied it is mixed into the key derivation, and the
// file then really is confidential — as confidential as that passphrase, which
// has to travel by some channel other than the email carrying the file.
//
// Either way the encryption is authenticated: a wrong key, a truncated file or
// a single flipped byte is rejected outright. The obfuscation this replaced
// would happily "decrypt" garbage into an account and register it.
namespace ProfileCipher {

struct SealedBox {
    QByteArray salt;       // 16 bytes, fresh per file
    QByteArray nonce;      // 12 bytes, fresh per file — never reused with a key
    QByteArray tag;        // 16-byte GCM authentication tag
    QByteArray ciphertext;
    bool passphraseUsed = false; // tells import whether to ask the user for one
};

// Encrypts `plaintext`. An empty `passphrase` means "app key only".
// Returns false only if the platform RNG fails.
bool seal(const QByteArray &plaintext, const QString &passphrase, SealedBox *out, QString *errorOut = nullptr);

// Decrypts and verifies. Returns false if the tag doesn't match, which means
// the wrong passphrase, a corrupted file, or a tampered one — the caller
// cannot tell which, and shouldn't pretend to.
bool open(const SealedBox &box, const QString &passphrase, QByteArray *plaintextOut, QString *errorOut = nullptr);

} // namespace ProfileCipher

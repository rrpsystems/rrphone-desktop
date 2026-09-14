#include "CredentialStore.h"

#include <QByteArray>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>

namespace {
// Qt strings are UTF-16 already, which is what the Credential Manager wants
// for the user/target fields; the blob is stored as raw UTF-16 bytes.
LPWSTR asWide(const QString &value) {
    return const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(value.utf16()));
}
} // namespace

bool CredentialStore::savePassword(const QString &target, const QString &user, const QString &password) {
    const QByteArray blob(reinterpret_cast<const char *>(password.utf16()),
                           static_cast<int>(password.size() * sizeof(ushort)));

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = asWide(target);
    cred.UserName = asWide(user);
    cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(blob.constData()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW(&cred, 0) != FALSE;
}

bool CredentialStore::loadPassword(const QString &target, QString *passwordOut) {
    PCREDENTIALW cred = nullptr;
    if (CredReadW(asWide(target), CRED_TYPE_GENERIC, 0, &cred) == FALSE) {
        return false;
    }

    if (passwordOut != nullptr && cred->CredentialBlob != nullptr && cred->CredentialBlobSize > 0) {
        *passwordOut = QString::fromUtf16(reinterpret_cast<const char16_t *>(cred->CredentialBlob),
                                           static_cast<qsizetype>(cred->CredentialBlobSize / sizeof(ushort)));
    }

    CredFree(cred);
    return true;
}

bool CredentialStore::removePassword(const QString &target) {
    return CredDeleteW(asWide(target), CRED_TYPE_GENERIC, 0) != FALSE;
}

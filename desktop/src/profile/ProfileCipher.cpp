#include "ProfileCipher.h"

#include <QObject>
#include <QRandomGenerator>

#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>

namespace {

// Not a secret — see the key-model note in ProfileCipher.h. Its job is to make
// the file unreadable to whoever happens to receive or forward it, not to
// withstand someone who reads this file.
const unsigned char kAppKey[32] = {
    0x8f, 0x2b, 0xd1, 0x74, 0x0a, 0x63, 0xe5, 0x19, 0xc4, 0x7d, 0x36, 0xa8, 0x51, 0xbe, 0x92, 0x0f,
    0x27, 0xdc, 0x68, 0xb3, 0x4e, 0x15, 0xf7, 0x8a, 0x39, 0xc0, 0x6d, 0x24, 0xab, 0x50, 0xe3, 0x96,
};

constexpr int kSaltBytes = 16;
constexpr int kNonceBytes = 12; // the size GCM is designed around
constexpr int kTagBytes = 16;
constexpr int kKeyBytes = 32;   // AES-256
// Only matters when a user passphrase is in play; against the app key alone the
// iteration count is irrelevant, since that key is public anyway.
constexpr unsigned int kPbkdf2Iterations = 200000;

// Bound into the GCM tag so a file cannot be replayed under a different format
// version by editing the JSON around it.
const char kAssociatedData[] = "rrpprofile-v3";

// key = PBKDF2(app key || passphrase, salt). Concatenating rather than choosing
// between them means a passphrase strictly adds entropy: the passphrase-less
// case is exactly "app key only", and no file is ever weaker than that.
bool deriveKey(const QString &passphrase, const QByteArray &salt, unsigned char *keyOut) {
    QByteArray secret(reinterpret_cast<const char *>(kAppKey), sizeof(kAppKey));
    secret.append(passphrase.toUtf8());

    const int rc = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char *>(secret.constData()),
        static_cast<size_t>(secret.size()), reinterpret_cast<const unsigned char *>(salt.constData()),
        static_cast<size_t>(salt.size()), kPbkdf2Iterations, kKeyBytes, keyOut);
    return rc == 0;
}

// Qt's system generator rather than mbedtls_ctr_drbg: the mbedTLS shipped in
// the liblinphone SDK is built without a platform entropy source, so seeding
// its DRBG fails outright here and every export would abort. QRandomGenerator::system()
// goes straight to the OS CSPRNG (BCryptGenRandom on Windows), which is what
// the DRBG would have been seeded from anyway.
bool randomBytes(QByteArray *out, int count) {
    out->resize(count);
    QRandomGenerator::system()->generate(out->begin(), out->end());
    return true;
}

} // namespace

bool ProfileCipher::seal(const QByteArray &plaintext, const QString &passphrase, SealedBox *out,
                          QString *errorOut) {
    SealedBox box;
    box.passphraseUsed = !passphrase.isEmpty();

    // A repeated nonce under the same key breaks GCM outright, so a failure to
    // get real randomness has to abort the export, never fall back to anything.
    if (!randomBytes(&box.salt, kSaltBytes) || !randomBytes(&box.nonce, kNonceBytes)) {
        if (errorOut) {
            *errorOut = QObject::tr("Não foi possível gerar números aleatórios seguros.");
        }
        return false;
    }

    unsigned char key[kKeyBytes];
    if (!deriveKey(passphrase, box.salt, key)) {
        if (errorOut) {
            *errorOut = QObject::tr("Falha ao derivar a chave de criptografia.");
        }
        return false;
    }

    box.ciphertext.resize(plaintext.size());
    box.tag.resize(kTagBytes);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, kKeyBytes * 8);
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(
            &gcm, MBEDTLS_GCM_ENCRYPT, static_cast<size_t>(plaintext.size()),
            reinterpret_cast<const unsigned char *>(box.nonce.constData()), kNonceBytes,
            reinterpret_cast<const unsigned char *>(kAssociatedData), sizeof(kAssociatedData) - 1,
            reinterpret_cast<const unsigned char *>(plaintext.constData()),
            reinterpret_cast<unsigned char *>(box.ciphertext.data()), kTagBytes,
            reinterpret_cast<unsigned char *>(box.tag.data()));
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (rc != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Falha ao criptografar o perfil.");
        }
        return false;
    }
    *out = box;
    return true;
}

bool ProfileCipher::open(const SealedBox &box, const QString &passphrase, QByteArray *plaintextOut,
                          QString *errorOut) {
    if (box.salt.size() != kSaltBytes || box.nonce.size() != kNonceBytes || box.tag.size() != kTagBytes) {
        if (errorOut) {
            *errorOut = QObject::tr("Arquivo de perfil malformado.");
        }
        return false;
    }

    unsigned char key[kKeyBytes];
    if (!deriveKey(passphrase, box.salt, key)) {
        if (errorOut) {
            *errorOut = QObject::tr("Falha ao derivar a chave de criptografia.");
        }
        return false;
    }

    QByteArray plaintext(box.ciphertext.size(), Qt::Uninitialized);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, kKeyBytes * 8);
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(
            &gcm, static_cast<size_t>(box.ciphertext.size()),
            reinterpret_cast<const unsigned char *>(box.nonce.constData()), kNonceBytes,
            reinterpret_cast<const unsigned char *>(kAssociatedData), sizeof(kAssociatedData) - 1,
            reinterpret_cast<const unsigned char *>(box.tag.constData()), kTagBytes,
            reinterpret_cast<const unsigned char *>(box.ciphertext.constData()),
            reinterpret_cast<unsigned char *>(plaintext.data()));
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (rc != 0) {
        // Deliberately one message for every failure: a wrong passphrase and a
        // tampered file are indistinguishable here, and guessing between them
        // for the user would be a lie.
        if (errorOut) {
            *errorOut = box.passphraseUsed
                             ? QObject::tr("Senha incorreta, ou o arquivo está corrompido/alterado.")
                             : QObject::tr("O arquivo está corrompido ou foi alterado.");
        }
        return false;
    }
    *plaintextOut = plaintext;
    return true;
}

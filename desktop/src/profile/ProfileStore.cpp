#include "ProfileStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QObject>

#include "ProfileCipher.h"

namespace {

// --- LEGACY, READ-ONLY: how formats 1 and 2 protected the password ---------
// A SHA-256 keystream XOR: no authentication, and a wrong passphrase yields
// garbage instead of an error. New files are written by ProfileCipher
// (AES-256-GCM); this stays only so profiles exported by earlier builds can
// still be imported, and must never be used to write anything.
QByteArray keystream(const QString &passphrase, int lengthNeeded) {
    QByteArray stream;
    QByteArray block = QCryptographicHash::hash(passphrase.toUtf8(), QCryptographicHash::Sha256);
    while (stream.size() < lengthNeeded) {
        stream.append(block);
        block = QCryptographicHash::hash(block, QCryptographicHash::Sha256);
    }
    stream.truncate(lengthNeeded);
    return stream;
}

QByteArray xorObfuscate(const QByteArray &data, const QString &passphrase) {
    const QByteArray key = keystream(passphrase, data.size());
    QByteArray out(data.size(), Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i) {
        out[i] = data[i] ^ key[i];
    }
    return out;
}

// Format 2 stores just the names of the enabled codecs, in priority order:
//
//     "codecs": ["PCMA", "PCMU", "G729"]
//
// Format 1 wrote all twelve payload types the engine knows as objects carrying
// mimeType/clockRate/channels/enabled, which made a provisioning file that a
// person has to read (and often hand-edit) mostly noise about codecs that were
// switched off. The clock rate and channel layout are redundant anyway: each
// codec this app supports has exactly one, and Codecs::findByName() recovers
// it. Anything absent from the list is simply disabled.
QJsonArray codecsToJson(const QList<CodecInfo> &codecs) {
    QJsonArray arr;
    for (const CodecInfo &c : codecs) {
        if (c.enabled) {
            arr.append(c.mimeType);
        }
    }
    return arr;
}

QList<CodecInfo> codecsFromJson(const QJsonArray &arr) {
    QList<CodecInfo> codecs;
    for (const QJsonValue &v : arr) {
        if (v.isString()) { // format 2: just the name
            CodecInfo c;
            if (Codecs::findByName(v.toString(), &c)) {
                c.enabled = true;
                codecs.append(c);
            }
            continue;
        }
        // Format 1: full objects, including disabled ones. Still read so that
        // profiles exported by earlier builds keep importing.
        const QJsonObject o = v.toObject();
        CodecInfo c;
        c.mimeType = o.value("mimeType").toString();
        c.clockRate = o.value("clockRate").toInt();
        c.channels = o.value("channels").toInt(1);
        c.enabled = o.value("enabled").toBool();
        if (Codecs::isSupported(c)) {
            codecs.append(c);
        }
    }
    return codecs;
}

} // namespace

bool ProfileStore::exportProfile(const AccountProfile &profile,
                                  const QString &filePath,
                                  const QString &passphrase,
                                  QString *errorOut) {
    // Everything goes inside the encrypted payload, not just the SIP password.
    // The contacts URL routinely carries HTTP credentials of its own
    // (http://user:pass@host/...), and formats 1 and 2 wrote it in the clear —
    // so the file leaked the phonebook to anyone the attachment reached.
    QJsonObject account;
    account["displayName"] = profile.displayName;
    account["username"] = profile.username;
    account["password"] = profile.password;
    account["domain"] = profile.domain;
    account["transport"] = profile.transport;
    account["dtmfMethod"] = profile.dtmfMethod;
    account["contactsUrl"] = profile.contactsUrl;
    account["codecs"] = codecsToJson(profile.codecs);

    ProfileCipher::SealedBox box;
    if (!ProfileCipher::seal(QJsonDocument(account).toJson(QJsonDocument::Compact), passphrase, &box, errorOut)) {
        return false;
    }

    QJsonObject root;
    root["version"] = 3;
    root["cipher"] = "AES-256-GCM";
    root["kdf"] = "PBKDF2-HMAC-SHA256";
    // So import knows whether to ask for one, instead of making the user find
    // out by way of a decryption failure.
    root["passphrase"] = box.passphraseUsed;
    root["salt"] = QString::fromLatin1(box.salt.toBase64());
    root["nonce"] = QString::fromLatin1(box.nonce.toBase64());
    root["tag"] = QString::fromLatin1(box.tag.toBase64());
    root["payload"] = QString::fromLatin1(box.ciphertext.toBase64());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool ProfileStore::importProfile(const QString &filePath,
                                  const QString &passphrase,
                                  AccountProfile *profileOut,
                                  QString *errorOut) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = parseError.errorString();
        }
        return false;
    }

    const QJsonObject root = doc.object();

    // Format 3 keeps the account inside an authenticated envelope; formats 1
    // and 2 kept it in the clear next to an XOR-obfuscated password. Older
    // files are still accepted so nobody's existing provisioning file breaks.
    QJsonObject account = root;
    if (root.value("version").toInt() >= 3) {
        ProfileCipher::SealedBox box;
        box.salt = QByteArray::fromBase64(root.value("salt").toString().toLatin1());
        box.nonce = QByteArray::fromBase64(root.value("nonce").toString().toLatin1());
        box.tag = QByteArray::fromBase64(root.value("tag").toString().toLatin1());
        box.ciphertext = QByteArray::fromBase64(root.value("payload").toString().toLatin1());
        box.passphraseUsed = root.value("passphrase").toBool();

        QByteArray plaintext;
        if (!ProfileCipher::open(box, passphrase, &plaintext, errorOut)) {
            return false;
        }
        const QJsonDocument inner = QJsonDocument::fromJson(plaintext);
        if (!inner.isObject()) {
            if (errorOut) {
                *errorOut = QObject::tr("Conteúdo do perfil inválido após a descriptografia.");
            }
            return false;
        }
        account = inner.object();
    }

    AccountProfile profile;
    profile.displayName = account.value("displayName").toString();
    profile.username = account.value("username").toString();
    profile.domain = account.value("domain").toString();
    profile.transport = account.value("transport").toString();
    profile.dtmfMethod = account.value("dtmfMethod").toString();
    profile.contactsUrl = account.value("contactsUrl").toString();
    profile.codecs = codecsFromJson(account.value("codecs").toArray());

    if (root.value("version").toInt() >= 3) {
        profile.password = account.value("password").toString();
    } else {
        profile.password = QString::fromUtf8(
            xorObfuscate(QByteArray::fromBase64(root.value("password_enc").toString().toLatin1()), passphrase));
    }

    if (profileOut) {
        *profileOut = profile;
    }
    return true;
}

bool ProfileStore::requiresPassphrase(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    // Formats 1 and 2 always needed one; format 3 says so explicitly.
    return root.value("version").toInt() < 3 || root.value("passphrase").toBool();
}

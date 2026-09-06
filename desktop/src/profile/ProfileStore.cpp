#include "ProfileStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>

namespace {

// --- PLACEHOLDER CIPHER — see ProfileStore.h for the security disclaimer ---
// Keystream XOR: key material is the (possibly repeated) SHA-256 hash chain
// of the passphrase. This gives no authentication and is trivially breakable
// by anyone who can guess the passphrase length pattern; it is here ONLY to
// avoid storing the SIP password in plain text by accident during MVP
// development. Replace before shipping.
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

QJsonArray codecsToJson(const QList<CodecInfo> &codecs) {
    QJsonArray arr;
    for (const CodecInfo &c : codecs) {
        QJsonObject o;
        o["mimeType"] = c.mimeType;
        o["clockRate"] = c.clockRate;
        o["channels"] = c.channels;
        o["enabled"] = c.enabled;
        arr.append(o);
    }
    return arr;
}

QList<CodecInfo> codecsFromJson(const QJsonArray &arr) {
    QList<CodecInfo> codecs;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        CodecInfo c;
        c.mimeType = o.value("mimeType").toString();
        c.clockRate = o.value("clockRate").toInt();
        c.channels = o.value("channels").toInt(1);
        c.enabled = o.value("enabled").toBool();
        codecs.append(c);
    }
    return codecs;
}

} // namespace

bool ProfileStore::exportProfile(const AccountProfile &profile,
                                  const QString &filePath,
                                  const QString &passphrase,
                                  QString *errorOut) {
    QJsonObject root;
    root["version"] = 1;
    root["displayName"] = profile.displayName;
    root["username"] = profile.username;
    root["domain"] = profile.domain;
    root["transport"] = profile.transport;
    root["dtmfMethod"] = profile.dtmfMethod;
    root["contactsUrl"] = profile.contactsUrl;
    root["codecs"] = codecsToJson(profile.codecs);

    const QByteArray encryptedPassword = xorObfuscate(profile.password.toUtf8(), passphrase);
    root["password_enc"] = QString::fromLatin1(encryptedPassword.toBase64());

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
    AccountProfile profile;
    profile.displayName = root.value("displayName").toString();
    profile.username = root.value("username").toString();
    profile.domain = root.value("domain").toString();
    profile.transport = root.value("transport").toString();
    profile.dtmfMethod = root.value("dtmfMethod").toString();
    profile.contactsUrl = root.value("contactsUrl").toString();
    profile.codecs = codecsFromJson(root.value("codecs").toArray());

    const QByteArray encryptedPassword = QByteArray::fromBase64(
        root.value("password_enc").toString().toLatin1());
    profile.password = QString::fromUtf8(xorObfuscate(encryptedPassword, passphrase));

    if (profileOut) {
        *profileOut = profile;
    }
    return true;
}

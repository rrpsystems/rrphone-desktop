#include "LocalContactsStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString LocalContactsStore::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/contacts_local.json");
}

QList<Contact> LocalContactsStore::load() {
    QList<Contact> contacts;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return contacts;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        Contact contact;
        contact.name = object.value("name").toString();
        contact.number = object.value("number").toString();
        contact.info = object.value("info").toString();
        contact.phone = object.value("phone").toString();
        contact.mobile = object.value("mobile").toString();
        contact.email = object.value("email").toString();
        contacts.append(contact);
    }
    return contacts;
}

void LocalContactsStore::save(const QList<Contact> &contacts) {
    QJsonArray array;
    for (const Contact &contact : contacts) {
        QJsonObject object;
        object["name"] = contact.name;
        object["number"] = contact.number;
        object["info"] = contact.info;
        object["phone"] = contact.phone;
        object["mobile"] = contact.mobile;
        object["email"] = contact.email;
        array.append(object);
    }

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void LocalContactsStore::clear() {
    QFile::remove(filePath());
}

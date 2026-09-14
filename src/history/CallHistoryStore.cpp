#include "CallHistoryStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
QJsonObject toJson(const CallRecord &record) {
    QJsonObject object;
    object["peer"] = record.peer;
    object["displayName"] = record.displayName;
    object["startedAt"] = record.startedAt.toString(Qt::ISODate);
    object["durationSeconds"] = record.durationSeconds;
    object["incoming"] = record.incoming;
    object["answered"] = record.answered;
    object["note"] = record.note;
    return object;
}

CallRecord fromJson(const QJsonObject &object) {
    CallRecord record;
    record.peer = object.value("peer").toString();
    record.displayName = object.value("displayName").toString();
    record.startedAt = QDateTime::fromString(object.value("startedAt").toString(), Qt::ISODate);
    record.durationSeconds = object.value("durationSeconds").toInt();
    record.incoming = object.value("incoming").toBool();
    record.answered = object.value("answered").toBool();
    record.note = object.value("note").toString();
    return record;
}
} // namespace

QString CallHistoryStore::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/call_history.json");
}

QList<CallRecord> CallHistoryStore::load() {
    QList<CallRecord> records;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return records;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : document.array()) {
        records.append(fromJson(value.toObject()));
    }
    return records;
}

void CallHistoryStore::append(const CallRecord &record) {
    QList<CallRecord> records = load();
    records.prepend(record); // newest first
    while (records.size() > kMaxRecords) {
        records.removeLast();
    }

    QJsonArray array;
    for (const CallRecord &item : records) {
        array.append(toJson(item));
    }

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void CallHistoryStore::clear() {
    QFile::remove(filePath());
}

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// D-08 — one entry in the call history.
struct CallRecord {
    QString peer;        // extension/number, used when redialing
    QString displayName; // optional, when the PBX sent one
    QDateTime startedAt;
    int durationSeconds = 0;
    bool incoming = false;
    bool answered = false;
    // Why the call ended the way it did, when it wasn't simply answered:
    // "recusada (não perturbe)", "encaminhada para 2130", "não atendida".
    QString note;
};

// Keeps the call history in a JSON file next to the app's log, because
// liblinphone's own call logs live only in memory unless the core is given a
// database, and we deliberately create it without config files.
class CallHistoryStore {
public:
    // Newest first. Reads from disk on every call — the list is small and
    // this avoids a stale cache between the panel and the writer.
    static QList<CallRecord> load();
    static void append(const CallRecord &record);
    static void clear();

    static QString filePath();

private:
    // Old entries are dropped past this; a softphone can run for years.
    static constexpr int kMaxRecords = 300;
};

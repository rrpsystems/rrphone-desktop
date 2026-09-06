#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace {
constexpr qint64 kMaxLogBytes = 2 * 1024 * 1024; // 2 MB, then start over

QMutex g_mutex;
QString g_logPath;

QString resolveLogPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/rrpsoftphone.log");
}

const char *levelName(QtMsgType type) {
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO ";
    case QtWarningMsg: return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?????";
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message) {
    QMutexLocker locker(&g_mutex);

    QFile file(g_logPath);
    // Roll over instead of growing forever; a softphone can run for weeks.
    // The previous generation is kept as .1: deleting outright used to discard
    // the history of the very session being investigated, right when a problem
    // was being reproduced.
    if (file.exists() && file.size() > kMaxLogBytes) {
        const QString previous = g_logPath + QStringLiteral(".1");
        QFile::remove(previous);
        QFile::rename(g_logPath, previous);
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' '
        << levelName(type) << ' ' << message << '\n';
}
} // namespace

void Logger::install() {
    g_logPath = resolveLogPath();
    qInstallMessageHandler(messageHandler);
}

QString Logger::logFilePath() {
    return g_logPath.isEmpty() ? resolveLogPath() : g_logPath;
}

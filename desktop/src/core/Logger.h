#pragma once

#include <QString>

// Writes the app's diagnostic messages (qInfo/qWarning/qCritical) to a file,
// because the app is a GUI binary with no console: without this, anything
// logged is invisible unless you launch it from a terminal with
// QT_ASSUME_STDERR_HAS_CONSOLE=1, which no end user will ever do.
//
// This is what makes "it didn't register on my machine" answerable.
namespace Logger {
// Installs the Qt message handler. Call once, early in main().
void install();

// Full path of the log file, e.g.
// C:/Users/<user>/AppData/Local/RRP Systems/RRPSoftphone/rrpsoftphone.log
QString logFilePath();
}

#include <QApplication>

#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "core/Logger.h"

#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false); // keep running in the tray (D-10)
    QApplication::setOrganizationName("RRP Systems");
    QApplication::setApplicationName("RRPSoftphone");
    QApplication::setApplicationVersion(RRP_APP_VERSION);

    // Must come after the app/organization names: the log path is derived
    // from them.
    Logger::install();
    qInfo().noquote() << "=== RRP Softphone" << RRP_APP_VERSION << "iniciando ===";
    app.setStyleSheet(Theme::styleSheet());

    MainWindow window;
    window.show();

    return QApplication::exec();
}

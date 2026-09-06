// Standalone registration smoke test for SipCoreManager, driven purely from
// the command line so real SIP credentials never have to be typed into (or
// committed as part of) the GUI/source tree.
//
// Usage: sip_register_test.exe <username> <password> <domain[:port]> <transport udp|tcp|tls>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#include "core/SipCoreManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 5) {
        qWarning() << "Usage: sip_register_test <username> <password> <domain[:port]> <transport udp|tcp|tls>";
        return 1;
    }

    SipCoreManager sipCore;
    sipCore.start();

    SipCoreManager::AccountConfig cfg;
    cfg.username = QString::fromLocal8Bit(argv[1]);
    cfg.password = QString::fromLocal8Bit(argv[2]);
    cfg.domain = QString::fromLocal8Bit(argv[3]);
    cfg.transport = QString::fromLocal8Bit(argv[4]);
    cfg.displayName = cfg.username;

    QObject::connect(&sipCore, &SipCoreManager::registrationStateChanged,
                      [&app](bool registered, const QString &message) {
                          qInfo().noquote() << "[registrationStateChanged]" << (registered ? "REGISTERED" : "NOT REGISTERED")
                                             << "-" << message;
                          if (registered) {
                              QTimer::singleShot(500, &app, &QCoreApplication::quit);
                          }
                      });
    QObject::connect(&sipCore, &SipCoreManager::errorOccurred, [](const QString &message) {
        qWarning().noquote() << "[errorOccurred]" << message;
    });

    qInfo().noquote() << "Configuring account" << cfg.username << "@" << cfg.domain << "via" << cfg.transport << "...";
    sipCore.configureAccount(cfg);

    QTimer::singleShot(15000, &app, [&app]() {
        qWarning() << "Timed out waiting for a registration state change.";
        app.quit();
    });

    return app.exec();
}

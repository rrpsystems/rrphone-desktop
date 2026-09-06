#pragma once

#include <QObject>
#include <QList>
#include <QUrl>
#include <QTimer>

#include "Contact.h"

class QNetworkAccessManager;
class QNetworkReply;

// D-16 — downloads and parses a MicroSip-format contacts XML from a
// configurable URL:
//
//   <?xml version="1.0" encoding="UTF-8"?>
//   <contacts refresh="0">
//     <contact name="" number="" .../>
//   </contacts>
//
// `refresh` is in minutes; 0 means "no automatic refresh" (fetch() must be
// called manually, e.g. on startup or via a toolbar button).
//
// This class only depends on Qt (Network + Xml) — it has no relationship to
// liblinphone/SipCoreManager, so it can be developed/tested independently.
class ContactsXmlFetcher : public QObject {
    Q_OBJECT

public:
    explicit ContactsXmlFetcher(QObject *parent = nullptr);

    void setUrl(const QUrl &url);
    QUrl url() const { return m_url; }

    // Triggers an immediate download + parse.
    void fetch();

signals:
    void contactsUpdated(const QList<Contact> &contacts);
    void fetchFailed(const QString &reason);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    static QList<Contact> parseXml(const QByteArray &xmlData, int *refreshMinutesOut);
    void scheduleAutoRefresh(int minutes);

    QNetworkAccessManager *m_networkManager;
    QUrl m_url;
    QTimer m_autoRefreshTimer;
};

#include "ContactsXmlFetcher.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>

ContactsXmlFetcher::ContactsXmlFetcher(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)) {
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &ContactsXmlFetcher::onReplyFinished);

    m_autoRefreshTimer.setSingleShot(false);
    connect(&m_autoRefreshTimer, &QTimer::timeout, this, &ContactsXmlFetcher::fetch);
}

void ContactsXmlFetcher::setUrl(const QUrl &url) {
    m_url = url;
}

void ContactsXmlFetcher::fetch() {
    if (!m_url.isValid()) {
        emit fetchFailed(tr("URL da lista de contatos não configurada"));
        return;
    }
    m_networkManager->get(QNetworkRequest(m_url));
}

void ContactsXmlFetcher::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit fetchFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    int refreshMinutes = 0;
    const QList<Contact> contacts = parseXml(data, &refreshMinutes);

    scheduleAutoRefresh(refreshMinutes);
    emit contactsUpdated(contacts);
}

void ContactsXmlFetcher::scheduleAutoRefresh(int minutes) {
    m_autoRefreshTimer.stop();
    if (minutes > 0) {
        m_autoRefreshTimer.start(minutes * 60 * 1000);
    }
    // minutes <= 0: no auto refresh, matches MicroSip's refresh="0" semantics.
}

QList<Contact> ContactsXmlFetcher::parseXml(const QByteArray &xmlData, int *refreshMinutesOut) {
    QList<Contact> contacts;
    if (refreshMinutesOut) {
        *refreshMinutesOut = 0;
    }

    QXmlStreamReader xml(xmlData);
    while (!xml.atEnd() && !xml.hasError()) {
        const auto token = xml.readNext();
        if (token != QXmlStreamReader::StartElement) {
            continue;
        }

        if (xml.name().compare(QLatin1String("contacts"), Qt::CaseInsensitive) == 0) {
            if (refreshMinutesOut) {
                *refreshMinutesOut = xml.attributes().value(QLatin1String("refresh")).toInt();
            }
        } else if (xml.name().compare(QLatin1String("contact"), Qt::CaseInsensitive) == 0) {
            const auto attrs = xml.attributes();
            auto attr = [&attrs](const char *name) {
                return attrs.value(QLatin1String(name)).toString();
            };

            Contact contact;
            contact.name = attr("name");
            contact.number = attr("number");
            contact.firstName = attr("firstname");
            contact.lastName = attr("lastname");
            contact.phone = attr("phone");
            contact.mobile = attr("mobile");
            contact.email = attr("email");
            contact.address = attr("address");
            contact.city = attr("city");
            contact.state = attr("state");
            contact.zip = attr("zip");
            contact.comment = attr("comment");
            contact.info = attr("info");
            contact.presence = attr("presence") == QLatin1String("1");

            // Tolerant parsing (PRD Section 8): a contact missing `name` and
            // `number` entirely is not usable, skip it rather than showing a
            // blank row.
            if (!contact.name.isEmpty() || !contact.number.isEmpty()) {
                contacts.append(contact);
            }
        }
    }

    return contacts;
}

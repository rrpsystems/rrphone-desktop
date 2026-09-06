#pragma once

#include <QWidget>
#include <QList>

#include "contacts/Contact.h"

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;

// D-16 — contacts page. Shows the remote XML list and the locally created
// contacts together; local ones are marked and can be edited or removed.
// Double-clicking any of them dials it.
class ContactsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ContactsPanel(QWidget *parent = nullptr);

public slots:
    // Contacts coming from the provisioning XML.
    void setRemoteContacts(const QList<Contact> &contacts);
    void showMessage(const QString &message);
    // Re-reads the locally stored contacts from disk.
    void reloadLocalContacts();

signals:
    void callRequested(const QString &number);
    void refreshRequested();

private:
    void rebuildList();
    void addContact();
    void editSelected();
    void removeSelected();
    // Index into m_localContacts for the selected row, or -1 when the
    // selection is a remote contact (which can't be edited here).
    int selectedLocalIndex() const;

    QListWidget *m_listWidget;
    QLineEdit *m_searchEdit;
    QLabel *m_messageLabel;
    QPushButton *m_editButton;
    QPushButton *m_removeButton;

    QList<Contact> m_remoteContacts;
    QList<Contact> m_localContacts;
};

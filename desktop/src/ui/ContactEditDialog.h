#pragma once

#include <QDialog>

#include "contacts/Contact.h"

class QLineEdit;

// Add/edit a locally stored contact. Only the fields that make sense to type
// by hand — the remote XML carries more, but nobody fills a CEP to call a
// colleague.
class ContactEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit ContactEditDialog(QWidget *parent = nullptr);

    void setContact(const Contact &contact);
    Contact contact() const;

private:
    QLineEdit *m_nameEdit;
    QLineEdit *m_numberEdit;
    QLineEdit *m_infoEdit;
};

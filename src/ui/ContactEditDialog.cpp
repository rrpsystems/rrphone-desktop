#include "ContactEditDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ContactEditDialog::ContactEditDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Contato"));
    resize(320, 170);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setAccessibleName(tr("Nome"));
    m_numberEdit = new QLineEdit(this);
    m_numberEdit->setPlaceholderText(tr("ex.: 2130"));
    m_numberEdit->setAccessibleName(tr("Ramal ou número"));
    m_infoEdit = new QLineEdit(this);
    m_infoEdit->setPlaceholderText(tr("ex.: Financeiro"));
    m_infoEdit->setAccessibleName(tr("Observação"));

    auto *form = new QFormLayout();
    form->addRow(tr("Nome"), m_nameEdit);
    form->addRow(tr("Ramal ou número"), m_numberEdit);
    form->addRow(tr("Observação"), m_infoEdit);

    auto *saveButton = new QPushButton(tr("Salvar"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        // A contact without a number can't be dialed, which is the whole
        // point of the list.
        if (m_numberEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Contato"), tr("Informe o ramal ou número."));
            m_numberEdit->setFocus();
            return;
        }
        accept();
    });
    auto *cancelButton = new QPushButton(tr("Cancelar"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(saveButton);
    buttons->addWidget(cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);

    m_nameEdit->setFocus();
}

void ContactEditDialog::setContact(const Contact &contact) {
    m_nameEdit->setText(contact.name);
    m_numberEdit->setText(contact.number);
    m_infoEdit->setText(contact.info);
}

Contact ContactEditDialog::contact() const {
    Contact contact;
    contact.name = m_nameEdit->text().trimmed();
    contact.number = m_numberEdit->text().trimmed();
    contact.info = m_infoEdit->text().trimmed();
    if (contact.name.isEmpty()) {
        contact.name = contact.number; // list must always show something
    }
    return contact;
}

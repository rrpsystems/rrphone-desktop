#include "ContactsPanel.h"
#include "ContactEditDialog.h"
#include "Theme.h"
#include "contacts/LocalContactsStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int RoleNumber = Qt::UserRole;
constexpr int RoleLocalIndex = Qt::UserRole + 1; // -1 for remote contacts
} // namespace

ContactsPanel::ContactsPanel(QWidget *parent) : QWidget(parent) {
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Buscar contato"));
    m_searchEdit->setAccessibleName(tr("Buscar contato"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { rebuildList(); });

    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const QString number = item->data(RoleNumber).toString();
        if (!number.isEmpty()) {
            emit callRequested(number);
        }
    });
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        // Only locally created contacts can be edited or deleted here; the
        // remote ones belong to the server.
        const bool isLocal = selectedLocalIndex() >= 0;
        m_editButton->setEnabled(isLocal);
        m_removeButton->setEnabled(isLocal);
    });

    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Theme::kTextSecondary));

    auto *addButton = new QPushButton(tr("Novo"), this);
    connect(addButton, &QPushButton::clicked, this, &ContactsPanel::addContact);
    m_editButton = new QPushButton(tr("Editar"), this);
    m_editButton->setEnabled(false);
    connect(m_editButton, &QPushButton::clicked, this, &ContactsPanel::editSelected);
    m_removeButton = new QPushButton(tr("Excluir"), this);
    m_removeButton->setEnabled(false);
    connect(m_removeButton, &QPushButton::clicked, this, &ContactsPanel::removeSelected);
    auto *refreshButton = new QPushButton(tr("Atualizar"), this);
    refreshButton->setToolTip(tr("Baixa novamente a lista do servidor"));
    connect(refreshButton, &QPushButton::clicked, this, &ContactsPanel::refreshRequested);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(4);
    buttons->addWidget(addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addWidget(refreshButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(m_messageLabel);
    layout->addLayout(buttons);

    reloadLocalContacts();
}

void ContactsPanel::setRemoteContacts(const QList<Contact> &contacts) {
    m_remoteContacts = contacts;
    rebuildList();
    showMessage(contacts.isEmpty() ? tr("O servidor não retornou contatos.")
                                    : tr("%1 contatos do servidor.").arg(contacts.size()));
}

void ContactsPanel::reloadLocalContacts() {
    m_localContacts = LocalContactsStore::load();
    rebuildList();
}

void ContactsPanel::showMessage(const QString &message) {
    m_messageLabel->setText(message);
}

void ContactsPanel::rebuildList() {
    const QString needle = m_searchEdit->text();
    m_listWidget->clear();

    const auto matches = [&needle](const Contact &contact) {
        return needle.isEmpty() || contact.name.contains(needle, Qt::CaseInsensitive) ||
               contact.number.contains(needle, Qt::CaseInsensitive) ||
               contact.info.contains(needle, Qt::CaseInsensitive);
    };

    const auto addRow = [this](const Contact &contact, int localIndex) {
        const QString suffix = contact.info.isEmpty() ? QString() : QStringLiteral("\n%1").arg(contact.info);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2%3").arg(contact.name, contact.number, suffix), m_listWidget);
        item->setData(RoleNumber, contact.number);
        item->setData(RoleLocalIndex, localIndex);
        if (localIndex >= 0) {
            // Tinted so it is obvious which entries the user owns and can
            // change, versus the ones the server provisioned.
            item->setForeground(QColor(Theme::kAccentTeal));
            item->setToolTip(tr("Contato local — duplo clique para ligar"));
        } else {
            item->setToolTip(tr("Contato do servidor — duplo clique para ligar"));
        }
    };

    for (int i = 0; i < m_localContacts.size(); ++i) {
        if (matches(m_localContacts[i])) {
            addRow(m_localContacts[i], i);
        }
    }
    for (const Contact &contact : m_remoteContacts) {
        if (matches(contact)) {
            addRow(contact, -1);
        }
    }
}

int ContactsPanel::selectedLocalIndex() const {
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item == nullptr || !item->isSelected()) {
        return -1;
    }
    return item->data(RoleLocalIndex).toInt();
}

void ContactsPanel::addContact() {
    ContactEditDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_localContacts.append(dialog.contact());
    LocalContactsStore::save(m_localContacts);
    rebuildList();
}

void ContactsPanel::editSelected() {
    const int index = selectedLocalIndex();
    if (index < 0 || index >= m_localContacts.size()) {
        return;
    }
    ContactEditDialog dialog(this);
    dialog.setContact(m_localContacts[index]);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_localContacts[index] = dialog.contact();
    LocalContactsStore::save(m_localContacts);
    rebuildList();
}

void ContactsPanel::removeSelected() {
    const int index = selectedLocalIndex();
    if (index < 0 || index >= m_localContacts.size()) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Excluir contato"),
        tr("Excluir \"%1\"?").arg(m_localContacts[index].name),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    m_localContacts.removeAt(index);
    LocalContactsStore::save(m_localContacts);
    rebuildList();
}

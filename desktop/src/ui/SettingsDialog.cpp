#include "SettingsDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

namespace {
constexpr int RoleMimeType = Qt::UserRole;
constexpr int RoleClockRate = Qt::UserRole + 1;
constexpr int RoleChannels = Qt::UserRole + 2;
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Configurações"));
    resize(420, 480);

    // --- Account tab (D-01) -------------------------------------------------
    m_displayNameEdit = new QLineEdit(this);
    m_usernameEdit = new QLineEdit(this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_domainEdit = new QLineEdit(this);
    m_domainEdit->setPlaceholderText(tr("sip.exemplo.com[:porta]"));
    m_transportCombo = new QComboBox(this);
    m_transportCombo->addItems({"UDP", "TCP", "TLS"});

    // Accessible names: the form labels are separate widgets, so without
    // these the fields are anonymous to screen readers and UI automation.
    m_displayNameEdit->setAccessibleName(tr("Nome de exibição"));
    m_usernameEdit->setAccessibleName(tr("Usuário/Ramal"));
    m_passwordEdit->setAccessibleName(tr("Senha"));
    m_domainEdit->setAccessibleName(tr("Servidor SIP"));
    m_transportCombo->setAccessibleName(tr("Transporte"));

    auto *accountForm = new QFormLayout();
    accountForm->addRow(tr("Nome de exibição"), m_displayNameEdit);
    accountForm->addRow(tr("Usuário/Ramal"), m_usernameEdit);
    accountForm->addRow(tr("Senha"), m_passwordEdit);
    accountForm->addRow(tr("Servidor SIP"), m_domainEdit);
    accountForm->addRow(tr("Transporte"), m_transportCombo);
    auto *accountTab = new QWidget(this);
    accountTab->setLayout(accountForm);

    // --- Audio tab (D-14 codecs, D-15 DTMF) ---------------------------------
    m_codecList = new QListWidget(this);
    m_codecList->setDragDropMode(QAbstractItemView::InternalMove);
    m_codecList->setToolTip(tr("Arraste para reordenar a prioridade. Marque/desmarque para habilitar."));

    m_dtmfCombo = new QComboBox(this);
    m_dtmfCombo->addItem(tr("RFC2833 (out-of-band, recomendado)"), "rfc2833");
    m_dtmfCombo->addItem(tr("SIP INFO"), "info");
    m_dtmfCombo->addItem(tr("In-band (tons no áudio)"), "inband");

    // Device routing. The ringer is separate on purpose: with a USB headset
    // the call belongs on the headset, but the ring has to be audible on the
    // PC speakers or the user misses calls while not wearing it.
    m_captureDeviceCombo = new QComboBox(this);
    m_captureDeviceCombo->setAccessibleName(tr("Microfone"));
    m_playbackDeviceCombo = new QComboBox(this);
    m_playbackDeviceCombo->setAccessibleName(tr("Alto-falante"));
    m_ringerDeviceCombo = new QComboBox(this);
    m_ringerDeviceCombo->setAccessibleName(tr("Toque"));

    // Lets the user confirm the chosen output actually produces sound without
    // having to place a call — the only way to tell a dead output device apart
    // from a call/media problem.
    auto *testSoundButton = new QPushButton(tr("Testar som"), this);
    m_audioTestLabel = new QLabel(this);
    connect(testSoundButton, &QPushButton::clicked, this, [this]() {
        m_audioTestLabel->setText(tr("Tocando..."));
        emit audioTestRequested();
    });

    auto *testRow = new QHBoxLayout();
    testRow->addWidget(testSoundButton);
    testRow->addWidget(m_audioTestLabel, 1);

    auto *deviceForm = new QFormLayout();
    deviceForm->addRow(tr("Microfone"), m_captureDeviceCombo);
    deviceForm->addRow(tr("Alto-falante (chamada)"), m_playbackDeviceCombo);
    deviceForm->addRow(tr("Toque (chamada recebida)"), m_ringerDeviceCombo);
    deviceForm->addRow(QString(), testRow);

    auto *audioLayout = new QVBoxLayout();
    audioLayout->addLayout(deviceForm);
    audioLayout->addWidget(new QLabel(tr("Codecs de áudio (prioridade e habilitação):"), this));
    audioLayout->addWidget(m_codecList);
    audioLayout->addWidget(new QLabel(tr("Método de DTMF:"), this));
    audioLayout->addWidget(m_dtmfCombo);
    auto *audioTab = new QWidget(this);
    audioTab->setLayout(audioLayout);

    // --- Calls tab: unconditional forwarding ("siga-me") --------------------
    m_forwardTargetEdit = new QLineEdit(this);
    m_forwardTargetEdit->setPlaceholderText(tr("ex.: 2130"));
    m_forwardTargetEdit->setAccessibleName(tr("Encaminhar chamadas para"));

    auto *callsForm = new QFormLayout();
    callsForm->addRow(tr("Encaminhar chamadas para"), m_forwardTargetEdit);
    callsForm->addRow(new QLabel(tr("Toda chamada recebida vai direto para esse ramal,\n"
                                     "sem tocar aqui. Deixe em branco para desativar."), this));
    auto *callsTab = new QWidget(this);
    callsTab->setLayout(callsForm);

    // --- Contacts tab (D-16) -------------------------------------------------
    m_contactsUrlEdit = new QLineEdit(this);
    m_contactsUrlEdit->setPlaceholderText(tr("https://.../contacts.xml"));
    m_contactsUrlEdit->setAccessibleName(tr("URL da lista de contatos"));
    m_replaceLocalContactsCheck = new QCheckBox(
        tr("Apagar os contatos locais quando a lista do servidor chegar"), this);
    m_replaceLocalContactsCheck->setToolTip(
        tr("Use quando a agenda do servidor deve ser a única fonte.\n"
           "Desmarcado, os contatos criados aqui convivem com os do servidor."));

    auto *contactsForm = new QFormLayout();
    contactsForm->addRow(tr("URL da lista de contatos"), m_contactsUrlEdit);
    contactsForm->addRow(new QLabel(tr("Formato MicroSip (<contacts>/<contact>). Deixe em branco para não usar.")));
    contactsForm->addRow(m_replaceLocalContactsCheck);
    auto *contactsTab = new QWidget(this);
    contactsTab->setLayout(contactsForm);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(accountTab, tr("Conta"));
    tabs->addTab(audioTab, tr("Áudio"));
    tabs->addTab(callsTab, tr("Chamadas"));
    tabs->addTab(contactsTab, tr("Contatos"));

    // Put the cursor in the field as soon as a tab with a single obvious
    // input is opened — otherwise the user clicks the tab, types, and
    // nothing happens because focus is still on the tab bar.
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        if (tabs->tabText(index) == tr("Chamadas")) {
            m_forwardTargetEdit->setFocus();
        } else if (tabs->tabText(index) == tr("Contatos")) {
            m_contactsUrlEdit->setFocus();
        }
    });

    // --- Profile import/export (D-13) ---------------------------------------
    auto *importButton = new QPushButton(tr("Importar configuração..."), this);
    auto *exportButton = new QPushButton(tr("Exportar configuração..."), this);
    connect(importButton, &QPushButton::clicked, this, &SettingsDialog::onImportClicked);
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::onExportClicked);
    auto *profileRow = new QHBoxLayout();
    profileRow->addWidget(importButton);
    profileRow->addWidget(exportButton);

    auto *saveButton = new QPushButton(tr("Salvar"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);
    auto *closeButton = new QPushButton(tr("Fechar"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    auto *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(saveButton);
    bottomRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addLayout(profileRow);
    layout->addLayout(bottomRow);
}

void SettingsDialog::setAvailableCodecs(const QList<CodecInfo> &codecs) {
    m_codecList->clear();
    for (const CodecInfo &c : codecs) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 / %2 Hz / %3 ch").arg(c.mimeType).arg(c.clockRate).arg(c.channels));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(c.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(RoleMimeType, c.mimeType);
        item->setData(RoleClockRate, c.clockRate);
        item->setData(RoleChannels, c.channels);
        m_codecList->addItem(item);
    }
}

void SettingsDialog::setProfile(const AccountProfile &profile) {
    m_displayNameEdit->setText(profile.displayName);
    m_usernameEdit->setText(profile.username);
    m_passwordEdit->setText(profile.password);
    m_domainEdit->setText(profile.domain);
    const int transportIndex = m_transportCombo->findText(profile.transport.toUpper());
    m_transportCombo->setCurrentIndex(transportIndex >= 0 ? transportIndex : 0);
    const int dtmfIndex = m_dtmfCombo->findData(profile.dtmfMethod);
    m_dtmfCombo->setCurrentIndex(dtmfIndex >= 0 ? dtmfIndex : 0);
    m_contactsUrlEdit->setText(profile.contactsUrl);

    if (!profile.codecs.isEmpty()) {
        setAvailableCodecs(profile.codecs);
    }
}

AccountProfile SettingsDialog::currentProfile() const {
    AccountProfile profile;
    profile.displayName = m_displayNameEdit->text();
    profile.username = m_usernameEdit->text();
    profile.password = m_passwordEdit->text();
    profile.domain = m_domainEdit->text();
    profile.transport = m_transportCombo->currentText();
    profile.dtmfMethod = m_dtmfCombo->currentData().toString();
    profile.contactsUrl = m_contactsUrlEdit->text();

    for (int i = 0; i < m_codecList->count(); ++i) {
        const QListWidgetItem *item = m_codecList->item(i);
        CodecInfo c;
        c.mimeType = item->data(RoleMimeType).toString();
        c.clockRate = item->data(RoleClockRate).toInt();
        c.channels = item->data(RoleChannels).toInt();
        c.enabled = item->checkState() == Qt::Checked;
        profile.codecs.append(c);
    }
    return profile;
}

void SettingsDialog::setAudioDevices(const DeviceList &captureDevices, const DeviceList &playbackDevices,
                                      const SettingsStore::AudioRouting &current) {
    const auto fill = [](QComboBox *combo, const DeviceList &devices, const QString &selectedId) {
        combo->clear();
        // First entry = let the engine decide, which is the right default for
        // anyone who never touches this screen.
        combo->addItem(tr("Padrão do sistema"), QString());
        for (const auto &device : devices) {
            combo->addItem(device.second, device.first);
        }
        const int index = combo->findData(selectedId);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    };
    fill(m_captureDeviceCombo, captureDevices, current.captureId);
    fill(m_playbackDeviceCombo, playbackDevices, current.playbackId);
    fill(m_ringerDeviceCombo, playbackDevices, current.ringerId);
}

void SettingsDialog::setAudioTestResult(const QString &message) {
    m_audioTestLabel->setText(message);
}

SettingsStore::AudioRouting SettingsDialog::audioRouting() const {
    SettingsStore::AudioRouting routing;
    routing.captureId = m_captureDeviceCombo->currentData().toString();
    routing.playbackId = m_playbackDeviceCombo->currentData().toString();
    routing.ringerId = m_ringerDeviceCombo->currentData().toString();
    return routing;
}

void SettingsDialog::setCallForwardTarget(const QString &target) {
    m_forwardTargetEdit->setText(target);
}

QString SettingsDialog::callForwardTarget() const {
    return m_forwardTargetEdit->text().trimmed();
}

void SettingsDialog::setReplaceLocalContacts(bool enabled) {
    m_replaceLocalContactsCheck->setChecked(enabled);
}

bool SettingsDialog::replaceLocalContacts() const {
    return m_replaceLocalContactsCheck->isChecked();
}

void SettingsDialog::onSave() {
    emit settingsApplied(currentProfile());
    accept();
}

void SettingsDialog::onImportClicked() {
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Importar configuração"), QString(), tr("Perfil RRP (*.rrpprofile)"));
    if (filePath.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString passphrase = QInputDialog::getText(
        this, tr("Senha do arquivo"), tr("Senha usada ao exportar este arquivo:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok) {
        return;
    }

    AccountProfile profile;
    QString error;
    if (!ProfileStore::importProfile(filePath, passphrase, &profile, &error)) {
        QMessageBox::warning(this, tr("Falha ao importar"), error);
        return;
    }

    setProfile(profile);
    emit profileImported(profile);
    QMessageBox::information(this, tr("Importado"), tr("Configuração importada e aplicada."));
}

void SettingsDialog::onExportClicked() {
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Exportar configuração"), "rrp.rrpprofile", tr("Perfil RRP (*.rrpprofile)"));
    if (filePath.isEmpty()) {
        return;
    }
    bool ok = false;
    const QString passphrase = QInputDialog::getText(
        this, tr("Proteger arquivo com senha"),
        tr("Defina uma senha para proteger a senha SIP dentro do arquivo:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok || passphrase.isEmpty()) {
        QMessageBox::warning(this, tr("Exportação cancelada"), tr("Uma senha é obrigatória para exportar."));
        return;
    }

    QString error;
    if (!ProfileStore::exportProfile(currentProfile(), filePath, passphrase, &error)) {
        QMessageBox::warning(this, tr("Falha ao exportar"), error);
        return;
    }
    QMessageBox::information(this, tr("Exportado"), tr("Configuração exportada com sucesso."));
}

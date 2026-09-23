#include "SettingsDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
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
#include <QFileInfo>

namespace {
constexpr int RoleMimeType = Qt::UserRole;
constexpr int RoleClockRate = Qt::UserRole + 1;
constexpr int RoleChannels = Qt::UserRole + 2;

// One titled block of the settings page — the same "card" the Android app
// uses in Ajustes, styled by QFrame#card / QLabel#cardTitle in Theme.cpp.
QFrame *makeCard(const QString &title, QLayout *content, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("cardTitle"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);
    layout->addWidget(heading);
    layout->addLayout(content);
    return card;
}

QLabel *makeHint(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("hint"));
    label->setWordWrap(true);
    return label;
}
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Configurações"));
    resize(460, 640);

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

    // Only needed behind a push gateway (Flexisip). Left empty on the desktop:
    // a PC stays connected and has nothing to be woken up for.
    m_outboundProxyEdit = new QLineEdit(this);
    m_outboundProxyEdit->setPlaceholderText(tr("vazio = direto no servidor"));
    m_outboundProxyEdit->setAccessibleName(tr("Proxy de saída"));
    m_outboundProxyEdit->setToolTip(
        tr("Servidor por onde passam o registro e as chamadas antes de chegar ao PBX,\n"
           "por exemplo o gateway de push. Sem porta/transporte, usa TLS (5061).\n"
           "Deixe em branco para registrar direto no servidor SIP."));

    auto *accountForm = new QFormLayout();
    accountForm->addRow(tr("Nome de exibição"), m_displayNameEdit);
    accountForm->addRow(tr("Usuário/Ramal"), m_usernameEdit);
    accountForm->addRow(tr("Senha"), m_passwordEdit);
    accountForm->addRow(tr("Servidor SIP"), m_domainEdit);
    accountForm->addRow(tr("Transporte"), m_transportCombo);
    accountForm->addRow(tr("Proxy de saída"), m_outboundProxyEdit);

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

    // Ringtone file. Taste in ring sounds is personal enough that shipping one
    // default and calling it done is a losing bet.
    m_ringtoneLabel = new QLabel(tr("Padrão do aplicativo"), this);
    m_ringtoneLabel->setWordWrap(true);
    auto *ringtoneBrowse = new QPushButton(tr("Escolher..."), this);
    auto *ringtoneReset = new QPushButton(tr("Padrão"), this);
    auto *ringtonePlay = new QPushButton(tr("Ouvir"), this);
    connect(ringtoneBrowse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Escolher toque"), QString(),
                                                           tr("Áudio WAV (*.wav)"));
        if (!path.isEmpty()) {
            m_ringtonePath = path;
            updateRingtoneLabel();
        }
    });
    connect(ringtoneReset, &QPushButton::clicked, this, [this]() {
        m_ringtonePath.clear();
        updateRingtoneLabel();
    });
    connect(ringtonePlay, &QPushButton::clicked, this, [this]() { emit ringtonePreviewRequested(); });

    auto *ringtoneRow = new QHBoxLayout();
    ringtoneRow->addWidget(ringtoneBrowse);
    ringtoneRow->addWidget(ringtoneReset);
    ringtoneRow->addWidget(ringtonePlay);
    ringtoneRow->addStretch();

    auto *deviceForm = new QFormLayout();
    deviceForm->addRow(tr("Microfone"), m_captureDeviceCombo);
    deviceForm->addRow(tr("Alto-falante (chamada)"), m_playbackDeviceCombo);
    deviceForm->addRow(tr("Toque (chamada recebida)"), m_ringerDeviceCombo);
    deviceForm->addRow(QString(), testRow);
    deviceForm->addRow(tr("Som do toque"), m_ringtoneLabel);
    deviceForm->addRow(QString(), ringtoneRow);

    // Processamento do microfone. Vale valer explicitamente porque cada um tem
    // um custo: o supressor come um pouco da naturalidade da voz, e o AGC
    // levanta o ruído de fundo nas pausas.
    m_noiseSuppressionCheck = new QCheckBox(tr("Supressão de ruído"), this);
    m_noiseSuppressionCheck->setToolTip(
        tr("Reduz teclado, ar-condicionado e conversa de fundo no que você envia.\n\n"
           "Vem desligada: o filtro remove o que julga não ser voz e, nisso, tira um\n"
           "pouco da naturalidade do timbre. Em sala silenciosa só custa qualidade;\n"
           "em ambiente barulhento compensa. Ligue se o seu caso for o segundo."));
    m_echoCancellationCheck = new QCheckBox(tr("Cancelamento de eco"), this);
    m_echoCancellationCheck->setToolTip(
        tr("Evita que o outro lado ouça a própria voz de volta.\n"
           "Essencial para quem usa a caixa de som do PC; com headset, pouco muda."));
    m_agcCheck = new QCheckBox(tr("Controle automático de ganho (AGC)"), this);
    m_agcCheck->setToolTip(
        tr("Nivela o volume da sua voz, útil quando as pessoas sentam a distâncias\n"
           "diferentes do microfone. Em compensação, levanta o ruído de fundo nas\n"
           "pausas — por isso vem desligado."));

    auto *audioLayout = new QVBoxLayout();
    audioLayout->addLayout(deviceForm);
    audioLayout->addSpacing(4);
    audioLayout->addWidget(makeHint(tr("Processamento do microfone — vale a partir da próxima chamada."), this));
    audioLayout->addWidget(m_noiseSuppressionCheck);
    audioLayout->addWidget(m_echoCancellationCheck);
    audioLayout->addWidget(m_agcCheck);

    // Compact rows, and exactly as tall as the four codecs need.
    m_codecList->setObjectName(QStringLiteral("codecList"));
    m_codecList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codecList->setFixedHeight(4 * 22 + 6);
    auto *codecLayout = new QVBoxLayout();
    codecLayout->addWidget(makeHint(tr("Arraste para mudar a prioridade; desmarque para desabilitar."), this));
    codecLayout->addWidget(m_codecList);
    auto *dtmfForm = new QFormLayout();
    dtmfForm->addRow(tr("Método de DTMF"), m_dtmfCombo);
    codecLayout->addLayout(dtmfForm);

    // --- Calls tab: unconditional forwarding ("siga-me") --------------------
    m_forwardTargetEdit = new QLineEdit(this);
    m_forwardTargetEdit->setPlaceholderText(tr("ex.: 2130"));
    m_forwardTargetEdit->setAccessibleName(tr("Encaminhar chamadas para"));

    m_incomingBehaviorCombo = new QComboBox(this);
    m_incomingBehaviorCombo->addItem(tr("Notificar sem interromper"), "notify");
    m_incomingBehaviorCombo->addItem(tr("Trazer a janela para frente"), "front");
    m_incomingBehaviorCombo->setToolTip(
        tr("Notificando, o app avisa pelo toque, por uma notificação do Windows e\n"
           "piscando na barra de tarefas, sem tirar o foco do que você está fazendo.\n\n"
           "Trazendo para frente, ele tenta assumir a tela. O Windows pode recusar:\n"
           "o sistema bloqueia que um programa em segundo plano roube o foco."));

    auto *callsForm = new QFormLayout();
    callsForm->addRow(tr("Ao receber chamada"), m_incomingBehaviorCombo);
    callsForm->addRow(tr("Encaminhar chamadas para"), m_forwardTargetEdit);
    callsForm->addRow(makeHint(tr("Toda chamada recebida vai direto para esse ramal, sem tocar aqui. "
                                  "Deixe em branco para desativar."), this));

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
    contactsForm->addRow(makeHint(tr("Formato <contacts>/<contact>. Deixe em branco para não usar."), this));
    contactsForm->addRow(m_replaceLocalContactsCheck);

    // --- Profile import/export (D-13) ---------------------------------------
    auto *importButton = new QPushButton(tr("Importar configuração..."), this);
    auto *exportButton = new QPushButton(tr("Exportar configuração..."), this);
    connect(importButton, &QPushButton::clicked, this, &SettingsDialog::onImportClicked);
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::onExportClicked);
    // Off by default: the point of the app-key protection is that provisioning
    // a new machine takes no password at all.
    m_exportPassphraseCheck = new QCheckBox(tr("Proteger a exportação com uma senha"), this);
    m_exportPassphraseCheck->setToolTip(
        tr("Sem marcar, o arquivo é criptografado com a chave do próprio aplicativo:\n"
           "qualquer RRP Softphone consegue importá-lo, sem senha nenhuma.\n\n"
           "Marcando, é preciso definir uma senha — que deve ser enviada por um\n"
           "canal diferente do arquivo, ou a proteção não serve para nada."));

    auto *profileRow = new QHBoxLayout();
    profileRow->addWidget(importButton);
    profileRow->addWidget(exportButton);

    auto *profileLayout = new QVBoxLayout();
    profileLayout->addWidget(makeHint(tr("Um arquivo .rrpprofile leva a conta, os codecs, o DTMF e a agenda para "
                                         "outro computador ou para o app do celular."), this));
    profileLayout->addLayout(profileRow);
    profileLayout->addWidget(m_exportPassphraseCheck);

    // One scrolling page of titled cards instead of tabs — the same
    // arrangement as Ajustes on Android, and nothing hides behind a tab.
    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("scrollContent"));
    auto *cards = new QVBoxLayout(content);
    cards->setContentsMargins(12, 12, 12, 12);
    cards->setSpacing(12);
    cards->addWidget(makeCard(tr("Conta"), accountForm, content));
    cards->addWidget(makeCard(tr("Chamadas"), callsForm, content));
    cards->addWidget(makeCard(tr("Áudio"), audioLayout, content));
    cards->addWidget(makeCard(tr("Codecs e DTMF"), codecLayout, content));
    cards->addWidget(makeCard(tr("Agenda"), contactsForm, content));
    cards->addWidget(makeCard(tr("Configuração"), profileLayout, content));
    cards->addStretch();

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *saveButton = new QPushButton(tr("Salvar"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);
    auto *closeButton = new QPushButton(tr("Fechar"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    auto *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(saveButton);
    bottomRow->addWidget(closeButton);

    bottomRow->setContentsMargins(12, 0, 12, 12);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(scroll, 1);
    layout->addLayout(bottomRow);
}

void SettingsDialog::setAvailableCodecs(const QList<CodecInfo> &codecs) {
    // Always exactly the codecs this app offers: the given ones first, in
    // their priority order, then any missing one unchecked. A profile only
    // lists the enabled codecs, so without this a codec switched off once
    // would vanish from the screen and could never be switched back on.
    QList<CodecInfo> shown;
    for (const CodecInfo &c : codecs) {
        if (Codecs::isSupported(c)) {
            shown.append(c);
        }
    }
    for (CodecInfo known : Codecs::supported()) {
        const bool present = std::any_of(shown.cbegin(), shown.cend(), [&known](const CodecInfo &c) {
            return c.mimeType.compare(known.mimeType, Qt::CaseInsensitive) == 0;
        });
        if (!present) {
            known.enabled = false;
            shown.append(known);
        }
    }

    m_codecList->clear();
    for (const CodecInfo &c : shown) {
        // Just the name: each codec this app offers has a single clock rate,
        // so "8000 Hz / 1 ch" was noise.
        auto *item = new QListWidgetItem(c.mimeType.toUpper());
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
    m_outboundProxyEdit->setText(profile.outboundProxy);
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
    profile.outboundProxy = m_outboundProxyEdit->text().trimmed();
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
    m_ringtonePath = current.ringtonePath;
    updateRingtoneLabel();
}

void SettingsDialog::updateRingtoneLabel() {
    m_ringtoneLabel->setText(m_ringtonePath.isEmpty() ? tr("Padrão do aplicativo")
                                                       : QFileInfo(m_ringtonePath).fileName());
    m_ringtoneLabel->setToolTip(m_ringtonePath);
}

void SettingsDialog::setAudioTestResult(const QString &message) {
    m_audioTestLabel->setText(message);
}

SettingsStore::AudioRouting SettingsDialog::audioRouting() const {
    SettingsStore::AudioRouting routing;
    routing.captureId = m_captureDeviceCombo->currentData().toString();
    routing.playbackId = m_playbackDeviceCombo->currentData().toString();
    routing.ringerId = m_ringerDeviceCombo->currentData().toString();
    routing.ringtonePath = m_ringtonePath;
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
    // Only ask when the file itself says it was exported with a passphrase.
    QString passphrase;
    if (ProfileStore::requiresPassphrase(filePath)) {
        bool ok = false;
        passphrase = QInputDialog::getText(this, tr("Senha do arquivo"),
                                            tr("Este arquivo foi protegido com senha na exportação:"),
                                            QLineEdit::Password, QString(), &ok);
        if (!ok) {
            return;
        }
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
    // No prompt in the normal case: the file is encrypted with the app's own
    // key, so any RRP Softphone can import it and nobody has to carry a
    // password around. A passphrase is opt-in, via the checkbox next to the
    // button, for when the file is going somewhere sensitive.
    QString passphrase;
    if (m_exportPassphraseCheck->isChecked()) {
        bool ok = false;
        passphrase = QInputDialog::getText(this, tr("Senha do arquivo"),
                                            tr("Senha para proteger este arquivo.\n"
                                               "Ela terá que ser informada na importação, e precisa\n"
                                               "ser enviada por um canal diferente do arquivo."),
                                            QLineEdit::Password, QString(), &ok);
        if (!ok || passphrase.isEmpty()) {
            QMessageBox::warning(this, tr("Exportação cancelada"),
                                  tr("Nenhuma senha informada. Desmarque a opção para exportar "
                                     "com a proteção padrão do aplicativo."));
            return;
        }
    }

    QString error;
    if (!ProfileStore::exportProfile(currentProfile(), filePath, passphrase, &error)) {
        QMessageBox::warning(this, tr("Falha ao exportar"), error);
        return;
    }
    QMessageBox::information(this, tr("Exportado"), tr("Configuração exportada com sucesso."));
}

void SettingsDialog::setIncomingCallBehavior(SettingsStore::IncomingCallBehavior behavior) {
    const QString key = behavior == SettingsStore::IncomingCallBehavior::BringToFront
                             ? QStringLiteral("front")
                             : QStringLiteral("notify");
    const int index = m_incomingBehaviorCombo->findData(key);
    m_incomingBehaviorCombo->setCurrentIndex(index >= 0 ? index : 0);
}

SettingsStore::IncomingCallBehavior SettingsDialog::incomingCallBehavior() const {
    return m_incomingBehaviorCombo->currentData().toString() == QLatin1String("front")
               ? SettingsStore::IncomingCallBehavior::BringToFront
               : SettingsStore::IncomingCallBehavior::Notify;
}

void SettingsDialog::setAudioProcessing(const SettingsStore::AudioProcessing &processing) {
    m_noiseSuppressionCheck->setChecked(processing.noiseSuppression);
    m_echoCancellationCheck->setChecked(processing.echoCancellation);
    m_agcCheck->setChecked(processing.automaticGainControl);
}

SettingsStore::AudioProcessing SettingsDialog::audioProcessing() const {
    SettingsStore::AudioProcessing processing;
    processing.noiseSuppression = m_noiseSuppressionCheck->isChecked();
    processing.echoCancellation = m_echoCancellationCheck->isChecked();
    processing.automaticGainControl = m_agcCheck->isChecked();
    return processing;
}

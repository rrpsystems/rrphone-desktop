#include "MainWindow.h"

#include <QCloseEvent>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSlider>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QTime>
#include <QShortcut>
#include <QKeySequence>
#include <QKeyEvent>

#include "DialPadWidget.h"
#include "ContactsPanel.h"
#include "HistoryPanel.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "profile/SettingsStore.h"
#include "contacts/LocalContactsStore.h"
#include "history/CallHistoryStore.h"

#ifdef Q_OS_WIN
// Depois dos headers do Qt de propósito: windows.h define macros (min/max e
// afins) que colidem com o Qt quando incluído antes.
#include <windows.h>
#endif

namespace {
// D-11 autostart: Qt's NativeFormat QSettings maps straight onto the
// Windows registry, no <windows.h> needed.
QSettings autoStartSettings() {
    return QSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
}
constexpr auto kAutoStartKey = "RRPSoftphone";

QToolButton *makeIconButton(const QString &iconPath, const QString &tooltip, QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    // Icon-only buttons have no text, so spell the name out for screen
    // readers (and for UI automation).
    button->setAccessibleName(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    return button;
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("RRP Softphone"));
    setWindowIcon(QIcon(":/rrp_logo.png"));
    resize(286, 610);
    setMinimumWidth(268);

    m_sipCore = new SipCoreManager(this);
    m_contactsFetcher = new ContactsXmlFetcher(this);

    m_callDurationTimer = new QTimer(this);
    m_callDurationTimer->setInterval(1000);
    connect(m_callDurationTimer, &QTimer::timeout, this, &MainWindow::updateCallDuration);

    m_contactsPanel = new ContactsPanel(this);
    m_historyPanel = new HistoryPanel(this);

    m_mainStack = new QStackedWidget(this);
    m_mainStack->addWidget(buildPhonePage()); // index 0
    m_mainStack->addWidget(m_contactsPanel);  // index 1
    m_mainStack->addWidget(m_historyPanel);   // index 2

    auto *rootLayout = new QVBoxLayout();
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildStatusRow());
    rootLayout->addWidget(m_mainStack, 1);
    rootLayout->addWidget(buildBottomBar());

    auto *root = new QWidget(this);
    root->setLayout(rootLayout);
    setCentralWidget(root);

    setupTrayIcon();

    // Unregister cleanly when the app quits, so we don't leave a dangling
    // contact binding on the PBX.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { m_sipCore->shutdown(); });

    // Keyboard shortcut for settings, so it doesn't depend on hunting for
    // the gear in the bottom bar.
    auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
    connect(settingsShortcut, &QShortcut::activated, this, &MainWindow::onSettingsRequested);

    // --- SipCoreManager wiring ---
    connect(m_sipCore, &SipCoreManager::registrationStateChanged, this, &MainWindow::onRegistrationStateChanged);
    connect(m_sipCore, &SipCoreManager::incomingCall, this, &MainWindow::onIncomingCall);
    connect(m_sipCore, &SipCoreManager::callStateChanged, this, &MainWindow::onCallStateChangedLabel);
    connect(m_sipCore, &SipCoreManager::callConnected, this, &MainWindow::onCallConnected);
    connect(m_sipCore, &SipCoreManager::remotePartyChanged, this, &MainWindow::onRemotePartyChanged);
    connect(m_sipCore, &SipCoreManager::callEnded, this, &MainWindow::onCallEnded);
    connect(m_sipCore, &SipCoreManager::callWaiting, this, &MainWindow::onCallWaiting);
    connect(m_sipCore, &SipCoreManager::waitingCallEnded, this, &MainWindow::onWaitingCallEnded);
    connect(m_sipCore, &SipCoreManager::heldCallEnded, this, &MainWindow::onHeldCallEnded);
    connect(m_sipCore, &SipCoreManager::heldCallPromoted, this, &MainWindow::onHeldCallPromoted);
    connect(m_sipCore, &SipCoreManager::callAutoHandled, this, &MainWindow::onCallAutoHandled);
    connect(m_sipCore, &SipCoreManager::consultationCallConnected, this, &MainWindow::onConsultationConnected);
    connect(m_sipCore, &SipCoreManager::errorOccurred, this, &MainWindow::onErrorOccurred);

    // --- Contacts wiring (D-16) ---
    connect(m_contactsFetcher, &ContactsXmlFetcher::contactsUpdated, this, &MainWindow::onContactsUpdated);
    connect(m_contactsFetcher, &ContactsXmlFetcher::fetchFailed, this, &MainWindow::onContactsFetchFailed);
    connect(m_contactsPanel, &ContactsPanel::refreshRequested, m_contactsFetcher, &ContactsXmlFetcher::fetch);
    // Dialing from the contacts or history lists: same behaviour, so both go
    // through one place.
    connect(m_contactsPanel, &ContactsPanel::callRequested, this, &MainWindow::startCallTo);
    connect(m_historyPanel, &HistoryPanel::callRequested, this, &MainWindow::startCallTo);

    applyUiCallState(UiCallState::Idle);
    m_sipCore->start();

    // Restore the saved configuration so the softphone comes back registered
    // instead of asking for the account on every launch.
    int speakerVolume = SipCoreManager::kUnityVolumePercent;
    int micVolume = SipCoreManager::kUnityVolumePercent;
    SettingsStore::loadVolumes(&speakerVolume, &micVolume);
    m_speakerSlider->setValue(speakerVolume);
    m_micSlider->setValue(micVolume);

    const SettingsStore::AudioRouting routing = SettingsStore::loadAudioRouting();
    m_sipCore->setCaptureDevice(routing.captureId);
    m_sipCore->setPlaybackDevice(routing.playbackId);
    m_sipCore->setRingerDevice(routing.ringerId);
    m_sipCore->setRingtoneFile(routing.ringtonePath);

    const SettingsStore::AudioProcessing savedProcessing = SettingsStore::loadAudioProcessing();
    m_sipCore->setAudioProcessing({savedProcessing.noiseSuppression, savedProcessing.echoCancellation,
                                   savedProcessing.automaticGainControl});

    bool savedDnd = false;
    QString savedForward;
    SettingsStore::loadCallHandling(&savedDnd, &savedForward);
    m_sipCore->setDoNotDisturb(savedDnd);
    {
        QSignalBlocker blocker(m_dndButton); // don't re-persist while restoring
        m_dndButton->setChecked(savedDnd);
    }
    applyForwardTarget(savedForward);

    AccountProfile savedProfile;
    if (SettingsStore::loadProfile(&savedProfile)) {
        applyProfile(savedProfile);
    } else {
        // Distinct from "registration failed": there is simply nothing saved
        // yet. Saying so (and where to fix it) beats a vague "Sem conta".
        qInfo().noquote() << "[settings] nenhuma conta salva — aguardando configuração do usuário";
        m_accountLabel->setText(tr("Configure em ⚙"));
        m_accountLabel->setToolTip(tr("Nenhuma conta salva. Abra Configurações (Ctrl+,) para cadastrar o ramal."));
    }
}

// --- Status row: presence dot + account, mirroring the reference's
// "Available / Custom Status" strip. ---------------------------------------
QWidget *MainWindow::buildStatusRow() {
    m_presenceLabel = new QLabel(tr("● Não registrado"), this);
    m_presenceLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Theme::kTextSecondary));

    m_accountLabel = new QLabel(tr("Sem conta"), this);
    m_accountLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Theme::kTextSecondary));

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(10, 6, 10, 6);
    layout->addWidget(m_presenceLabel);
    layout->addStretch();
    layout->addWidget(m_accountLabel);

    auto *row = new QWidget(this);
    row->setLayout(layout);
    row->setStyleSheet(QStringLiteral("background-color: %1; border-bottom: 1px solid %2;")
                            .arg(Theme::kPanel, Theme::kBorder));
    return row;
}

// --- Display area: the branded panel that doubles as the number display
// while dialing and as the call info panel during a call. ------------------
QWidget *MainWindow::buildDisplayArea() {
    // Page 0 — idle/dialing: logo, replaced by the typed number as you dial.
    m_logoLabel = new QLabel(this);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setPixmap(QPixmap(":/rrp_logo.png").scaled(76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_numberEdit = new QLineEdit(this);
    m_numberEdit->setAlignment(Qt::AlignCenter);
    m_numberEdit->setFrame(false);
    m_numberEdit->setPlaceholderText(tr("Ramal ou número"));
    m_numberEdit->setFixedHeight(30);
    m_numberEdit->setStyleSheet(QStringLiteral("background: transparent; border: none; font-size: 20px; color: %1;")
                                     .arg(Theme::kTextPrimary));
    // Not QLineEdit::returnPressed: that signal fires and the widget then
    // *ignores* the key event, so it keeps bubbling up to keyPressEvent and the
    // action runs twice — Enter would dial and immediately hang up. Filtering
    // the key here lets us consume it instead.
    m_numberEdit->installEventFilter(this);
    m_numberEdit->hide();
    connect(m_numberEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        // The logo gives way to the number as soon as there's something to
        // show, like the display on the reference softphone. The field only
        // exists while there's a number, so an idle display shows the brand
        // panel cleanly instead of a stray text caret.
        const bool composing = (m_uiCallState == UiCallState::TransferDialing ||
                                 m_uiCallState == UiCallState::ForwardDialing);
        if (m_uiCallState != UiCallState::Idle && !composing) {
            return;
        }
        const bool empty = text.isEmpty();
        m_logoLabel->setVisible(empty && !composing);
        m_numberEdit->setVisible(!empty || composing);
        m_backspaceButton->setVisible(!empty);
        // Nothing to dial, nothing to press — except when there is a number to
        // redial, which is what the key does on an empty display.
        m_actionButton->setEnabled(!empty || !lastDialedNumber().isEmpty());
        if (empty && !composing) {
            setFocus();
        } else if (!m_numberEdit->hasFocus()) {
            m_numberEdit->setFocus();
            m_numberEdit->setCursorPosition(text.size());
        }
    });

    // Erase key. Without it, fixing a typo made with the on-screen keypad
    // forces the user over to the physical keyboard — which defeats the point
    // of having a keypad on screen at all. A long press clears the whole
    // number, like a desk phone.
    m_backspaceButton = new QToolButton(this);
    m_backspaceButton->setText(QStringLiteral("⌫"));
    m_backspaceButton->setAccessibleName(tr("Apagar"));
    m_backspaceButton->setToolTip(tr("Apagar (segure para limpar)"));
    m_backspaceButton->setCursor(Qt::PointingHandCursor);
    m_backspaceButton->setFixedSize(30, 30);
    m_backspaceButton->setAutoRepeat(true);
    m_backspaceButton->setAutoRepeatDelay(500);
    m_backspaceButton->setAutoRepeatInterval(90);
    m_backspaceButton->setStyleSheet(
        QStringLiteral("QToolButton { background: transparent; border: none; font-size: 17px; color: %1; }"
                        "QToolButton:hover { color: %2; }")
            .arg(Theme::kTextSecondary, Theme::kAccentTeal));
    m_backspaceButton->hide();
    connect(m_backspaceButton, &QToolButton::clicked, this, [this]() {
        QString current = m_numberEdit->text();
        current.chop(1);
        m_numberEdit->setText(current);
    });

    auto *numberRow = new QHBoxLayout();
    numberRow->setContentsMargins(0, 0, 0, 0);
    numberRow->setSpacing(2);
    // The spacer mirrors the button's width so the number stays optically
    // centred in the display instead of being pushed left by the key.
    numberRow->addSpacing(m_backspaceButton->width());
    numberRow->addWidget(m_numberEdit, 1);
    numberRow->addWidget(m_backspaceButton);

    auto *dialLayout = new QVBoxLayout();
    dialLayout->setContentsMargins(10, 8, 10, 8);
    dialLayout->addStretch();
    dialLayout->addWidget(m_logoLabel);
    dialLayout->addLayout(numberRow);
    dialLayout->addStretch();
    auto *dialPage = new QWidget(this);
    dialPage->setLayout(dialLayout);

    // Page 1 — in call: who and for how long.
    m_callPeerLabel = new QLabel(this);
    m_callPeerLabel->setAlignment(Qt::AlignCenter);
    m_callPeerLabel->setWordWrap(true);
    m_callPeerLabel->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600; color: %1;")
                                        .arg(Theme::kTextPrimary));

    m_callDurationLabel = new QLabel(QStringLiteral("00:00"), this);
    m_callDurationLabel->setAlignment(Qt::AlignCenter);
    m_callDurationLabel->setStyleSheet(QStringLiteral("font-size: 22px; color: %1;").arg(Theme::kAccentTeal));

    auto *callLayout = new QVBoxLayout();
    callLayout->setContentsMargins(10, 8, 10, 8);
    callLayout->addStretch();
    callLayout->addWidget(m_callPeerLabel);
    callLayout->addWidget(m_callDurationLabel);
    callLayout->addStretch();
    auto *callPage = new QWidget(this);
    callPage->setLayout(callLayout);

    m_displayStack = new QStackedWidget(this);
    m_displayStack->addWidget(dialPage);
    m_displayStack->addWidget(callPage);
    m_displayStack->setFixedHeight(132);
    return m_displayStack;
}

// --- Hook line: call state on the left, call actions on the right, like the
// "On Hook  [icons]" row in the reference. ---------------------------------
QWidget *MainWindow::buildHookRow() {
    m_hookLabel = new QLabel(this);
    m_hookLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Theme::kTextSecondary));

    m_muteButton = makeIconButton(QStringLiteral(":/icons/mute.svg"), tr("Mudo"), this);
    m_muteButton->setCheckable(true);
    connect(m_muteButton, &QToolButton::toggled, this, [this](bool muted) {
        m_sipCore->setMuted(muted);
        refreshHookLabel();
    });

    m_dndButton = makeIconButton(QStringLiteral(":/icons/dnd.svg"),
                                  tr("Não perturbe — recusa chamadas recebidas"), this);
    m_dndButton->setCheckable(true);
    connect(m_dndButton, &QToolButton::toggled, this, &MainWindow::onDoNotDisturbToggled);

    m_forwardButton = makeIconButton(QStringLiteral(":/icons/forward.svg"),
                                      tr("Siga-me — encaminhar chamadas para outro ramal"), this);
    m_forwardButton->setCheckable(true);
    // `clicked` rather than `toggled`: turning it on takes a destination
    // first, so the checked state is set by us, not by the click.
    connect(m_forwardButton, &QToolButton::clicked, this, &MainWindow::onForwardButtonClicked);

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(10, 2, 8, 2);
    layout->setSpacing(2);
    layout->addWidget(m_hookLabel);
    layout->addStretch();
    layout->addWidget(m_muteButton);
    layout->addWidget(m_dndButton);
    layout->addWidget(m_forwardButton);

    auto *row = new QWidget(this);
    row->setLayout(layout);
    // Set through setHookText so the base text is recorded — the label is
    // composed from it plus the active mode badges.
    setHookText(tr("No gancho"));
    return row;
}

// --- Action row: hold and transfer flank the main call button. ------------
QWidget *MainWindow::buildActionRow() {
    m_holdButton = new QPushButton(this);
    m_holdButton->setIcon(QIcon(QStringLiteral(":/icons/hold.svg")));
    m_holdButton->setIconSize(QSize(16, 16));
    m_holdButton->setToolTip(tr("Espera"));
    m_holdButton->setAccessibleName(tr("Espera"));
    m_holdButton->setCheckable(true);
    m_holdButton->setFixedWidth(52);
    m_holdButton->setMinimumHeight(34);
    connect(m_holdButton, &QPushButton::toggled, this, [this](bool held) { m_sipCore->setHeld(held); });

    m_transferButton = new QPushButton(this);
    m_transferButton->setIcon(QIcon(QStringLiteral(":/icons/transfer.svg")));
    m_transferButton->setIconSize(QSize(16, 16));
    m_transferButton->setToolTip(tr("Transferir"));
    m_transferButton->setAccessibleName(tr("Transferir"));
    m_transferButton->setFixedWidth(52);
    m_transferButton->setMinimumHeight(34);
    connect(m_transferButton, &QPushButton::clicked, this, &MainWindow::onTransferClicked);

    m_actionButton = new QPushButton(tr("Ligar"), this);
    m_actionButton->setMinimumHeight(34);
    connect(m_actionButton, &QPushButton::clicked, this, &MainWindow::onActionButtonClicked);

    m_secondaryButton = new QPushButton(tr("Recusar"), this);
    m_secondaryButton->setMinimumHeight(34);
    m_secondaryButton->setVisible(false);
    connect(m_secondaryButton, &QPushButton::clicked, this, &MainWindow::onSecondaryButtonClicked);

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(5);
    layout->addWidget(m_holdButton);
    layout->addWidget(m_actionButton, 1);
    layout->addWidget(m_secondaryButton);
    layout->addWidget(m_transferButton);

    auto *row = new QWidget(this);
    row->setLayout(layout);
    return row;
}

// --- Speaker and microphone level sliders, always visible. ----------------
QWidget *MainWindow::buildVolumeControls() {
    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(8, 2, 8, 4);
    layout->setSpacing(2);

    const QString stepButtonStyle =
        QStringLiteral("QToolButton { color: %1; font-size: 15px; font-weight: 600; padding: 0px;"
                        " min-width: 16px; background: transparent; border: none; }"
                        "QToolButton:hover { color: %2; }")
            .arg(Theme::kTextSecondary, Theme::kTextPrimary);

    auto addVolumeRow = [this, layout, stepButtonStyle](const QString &iconPath, const QString &name,
                                                         QSlider *&sliderOut, auto &&onChanged) {
        // A plain label, not a button — this icon just names the row.
        auto *icon = new QLabel(this);
        icon->setPixmap(QIcon(iconPath).pixmap(17, 17));
        icon->setAccessibleName(name);

        auto *slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 100);
        slider->setValue(SipCoreManager::kUnityVolumePercent);
        slider->setAccessibleName(name);
        connect(slider, &QSlider::valueChanged, this, onChanged);
        // Persist once the user settles on a level, not on every tick.
        connect(slider, &QSlider::sliderReleased, this, [this]() {
            SettingsStore::saveVolumes(m_speakerSlider->value(), m_micSlider->value());
        });

        // setValue() doesn't emit sliderReleased, so the step buttons persist
        // the new level themselves.
        auto persistVolumes = [this]() {
            SettingsStore::saveVolumes(m_speakerSlider->value(), m_micSlider->value());
        };

        auto *minus = new QToolButton(this);
        minus->setText(QStringLiteral("-"));
        minus->setStyleSheet(stepButtonStyle);
        minus->setAccessibleName(tr("Diminuir %1").arg(name));
        connect(minus, &QToolButton::clicked, slider, [slider, persistVolumes]() {
            slider->setValue(slider->value() - 5);
            persistVolumes();
        });

        auto *plus = new QToolButton(this);
        plus->setText(QStringLiteral("+"));
        plus->setStyleSheet(stepButtonStyle);
        plus->setAccessibleName(tr("Aumentar %1").arg(name));
        connect(plus, &QToolButton::clicked, slider, [slider, persistVolumes]() {
            slider->setValue(slider->value() + 5);
            persistVolumes();
        });

        auto *row = new QHBoxLayout();
        row->setSpacing(4);
        row->addWidget(icon);
        row->addWidget(minus);
        row->addWidget(slider, 1);
        row->addWidget(plus);
        layout->addLayout(row);

        sliderOut = slider;
    };

    addVolumeRow(QStringLiteral(":/icons/speaker.svg"), tr("Volume do alto-falante"), m_speakerSlider,
                  [this](int value) { m_sipCore->setSpeakerVolume(value); });
    addVolumeRow(QStringLiteral(":/icons/mic.svg"), tr("Volume do microfone"), m_micSlider,
                  [this](int value) { m_sipCore->setMicVolume(value); });

    auto *panel = new QWidget(this);
    panel->setLayout(layout);
    return panel;
}

// --- Bottom icon nav, like the icon strip at the foot of the reference. ---
QWidget *MainWindow::buildBottomBar() {
    m_navDialer = makeIconButton(QStringLiteral(":/icons/keypad.svg"), tr("Discador"), this);
    m_navDialer->setCheckable(true);
    m_navDialer->setChecked(true);

    m_navContacts = makeIconButton(QStringLiteral(":/icons/contacts.svg"), tr("Contatos"), this);
    m_navContacts->setCheckable(true);

    m_navHistory = makeIconButton(QStringLiteral(":/icons/history.svg"), tr("Histórico de chamadas"), this);
    m_navHistory->setCheckable(true);

    m_navSettings = makeIconButton(QStringLiteral(":/icons/settings.svg"), tr("Configurações"), this);

    // One helper keeps the three page buttons mutually exclusive; Qt's
    // auto-exclusive only works for buttons sharing a parent layout group.
    const auto showPage = [this](int index) {
        m_mainStack->setCurrentIndex(index);
        m_navDialer->setChecked(index == 0);
        m_navContacts->setChecked(index == 1);
        m_navHistory->setChecked(index == 2);
        if (index == 2) {
            m_historyPanel->reload(); // never show a stale list
        }
    };
    connect(m_navDialer, &QToolButton::clicked, this, [showPage]() { showPage(0); });
    connect(m_navContacts, &QToolButton::clicked, this, [showPage]() { showPage(1); });
    connect(m_navHistory, &QToolButton::clicked, this, [showPage]() { showPage(2); });

    // The gear carries what used to live in the menu bar, so the window can
    // stay chrome-free like the reference.
    auto *settingsMenu = new QMenu(this);
    auto *settingsAction = settingsMenu->addAction(tr("Configurações..."));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsRequested);

    m_autoStartAction = settingsMenu->addAction(tr("Iniciar com o Windows"));
    m_autoStartAction->setCheckable(true);
    m_autoStartAction->setChecked(isAutoStartEnabled());
    connect(m_autoStartAction, &QAction::toggled, this, &MainWindow::setAutoStartEnabled);

    settingsMenu->addSeparator();
    auto *quitAction = settingsMenu->addAction(tr("Sair"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_navSettings->setMenu(settingsMenu);
    m_navSettings->setPopupMode(QToolButton::InstantPopup);

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(8, 4, 8, 4);
    layout->addWidget(m_navDialer);
    layout->addStretch();
    layout->addWidget(m_navContacts);
    layout->addStretch();
    layout->addWidget(m_navHistory);
    layout->addStretch();
    layout->addWidget(m_navSettings);

    auto *bar = new QWidget(this);
    bar->setLayout(layout);
    bar->setStyleSheet(QStringLiteral("background-color: %1; border-top: 1px solid %2;")
                            .arg(Theme::kPanel, Theme::kBorder));
    return bar;
}

QWidget *MainWindow::buildPhonePage() {
    m_dialPad = new DialPadWidget(this);
    connect(m_dialPad, &DialPadWidget::digitPressed, this, &MainWindow::onKeyPressed);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildDisplayArea());
    layout->addWidget(buildHookRow());
    layout->addWidget(m_dialPad);
    layout->addWidget(buildActionRow());
    layout->addWidget(buildVolumeControls());

    auto *page = new QWidget(this);
    page->setLayout(layout);
    return page;
}

void MainWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/rrp_logo.png"));
    m_trayIcon->setToolTip(tr("RRP Softphone"));

    auto *trayMenu = new QMenu(this);
    auto *showAction = trayMenu->addAction(tr("Mostrar"));
    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    auto *aboutAction = trayMenu->addAction(tr("Sobre"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    trayMenu->addSeparator();
    auto *quitAction = trayMenu->addAction(tr("Sair"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            setVisible(!isVisible());
            if (isVisible()) {
                raise();
                activateWindow();
            }
        }
    });
    // Clicking the "chamada recebida" notification is the user consenting to
    // switch — and a click *is* a foreground activation Windows honours, so
    // this is the reliable way to reach the call from another app.
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });

    m_trayIcon->show();
}

QString MainWindow::lastDialedNumber() const {
    for (const CallRecord &record : CallHistoryStore::load()) { // newest first
        if (!record.incoming && !record.peer.isEmpty()) {
            return record.peer;
        }
    }
    return {};
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_numberEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            onActionButtonClicked();
            return true; // consumed: must not reach keyPressEvent as well
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // With the number field hidden on an empty display, the window itself
    // has to accept typing (and DTMF keys during a call).
    const QString text = event->text();
    if (!text.isEmpty()) {
        const QChar ch = text.at(0);
        if (ch.isDigit() || ch == QLatin1Char('*') || ch == QLatin1Char('#') || ch == QLatin1Char('+')) {
            onKeyPressed(ch);
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_Backspace:
        if (m_uiCallState == UiCallState::Idle) {
            QString current = m_numberEdit->text();
            current.chop(1);
            m_numberEdit->setText(current);
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // Only reached when the number field doesn't have focus (it is hidden
        // on an empty display and during a call); the focused case is consumed
        // by eventFilter().
        onActionButtonClicked();
        return;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_trayIcon->isVisible()) {
        hide();
        m_trayIcon->showMessage(tr("RRP Softphone"), tr("Continua em execução na bandeja do sistema."),
                                 QSystemTrayIcon::Information, 3000);
        event->ignore();
    } else {
        event->accept();
    }
}

// --- UI state -------------------------------------------------------------

void MainWindow::applyUiCallState(UiCallState state) {
    m_uiCallState = state;

    const bool waiting = (state == UiCallState::CallWaiting);
    const bool twoCalls = (state == UiCallState::TwoCalls);
    // A conversation is in progress in all three: plain Active, while a second
    // call rings for a decision, and while one of two answered calls is parked.
    // Mute, hold and transfer stay meaningful throughout.
    const bool inCall = (state == UiCallState::Active || waiting || twoCalls);
    const bool incoming = (state == UiCallState::Incoming);
    const bool dialingTransfer = (state == UiCallState::TransferDialing);
    const bool consulting = (state == UiCallState::TransferConsulting);
    const bool dialingForward = (state == UiCallState::ForwardDialing);
    // Whenever the user is composing a number — dialing, choosing a transfer
    // target or setting the forward target — the dialing page is reused.
    const bool composing = dialingTransfer || dialingForward;
    const bool showDialPage = (state == UiCallState::Idle || composing);

    m_displayStack->setCurrentIndex(showDialPage ? 0 : 1);
    if (showDialPage) {
        const bool empty = m_numberEdit->text().isEmpty();
        // No logo while composing — the display is a field.
        m_logoLabel->setVisible(empty && state == UiCallState::Idle);
        m_numberEdit->setVisible(!empty || composing);
        m_backspaceButton->setVisible(!empty);
        if (composing) {
            m_numberEdit->setFocus();
        }
    }

    m_muteButton->setEnabled(inCall);
    m_holdButton->setEnabled(inCall);
    // No nested transfers: the button only starts a transfer from a call.
    m_transferButton->setEnabled(inCall);
    // The whole transfer flow counts as "still on a call" here. Leaving
    // dialingTransfer out used to un-hold the call the instant the transfer
    // screen opened: onTransferClicked() parks the call, then this ran and
    // unchecked the button, whose toggled() resumed it — so the other party
    // heard the user picking a destination, which is precisely what the hold
    // is for.
    //
    // The signal blockers matter independently: these buttons are being
    // synchronised to state here, not operated by the user, so they must not
    // command the engine on the way.
    if (!inCall && !consulting && !dialingTransfer) {
        QSignalBlocker muteBlocker(m_muteButton);
        QSignalBlocker holdBlocker(m_holdButton);
        m_muteButton->setChecked(false);
        m_holdButton->setChecked(false);
        m_sipCore->setMuted(false);
    }

    // A dimmed-out version of the same colour for the disabled state, so the
    // button reads as "not available right now" rather than as a different
    // control.
    const auto actionStyle = [](const char *color, const char *disabledColor) {
        return QStringLiteral("QPushButton { background-color: %1; color: white; font-weight: 600; }"
                               "QPushButton:disabled { background-color: %2; color: %3; }")
            .arg(QLatin1String(color), QLatin1String(disabledColor), QLatin1String(Theme::kTextSecondary));
    };
    const QString neutralSecondary =
        QStringLiteral("QPushButton { background-color: %1; color: %2; }")
            .arg(Theme::kButton, Theme::kTextPrimary);
    const QString dangerSecondary =
        QStringLiteral("QPushButton { background-color: %1; color: white; font-weight: 600; }")
            .arg(Theme::kDangerRed);

    m_secondaryButton->setVisible(incoming || waiting || twoCalls || dialingTransfer || consulting ||
                                   dialingForward);
    if (incoming) {
        m_secondaryButton->setText(tr("Recusar"));
        m_secondaryButton->setStyleSheet(dangerSecondary);
    } else if (waiting) {
        // Refuses only the newcomer; the conversation in progress is untouched.
        m_secondaryButton->setText(tr("Recusar"));
        m_secondaryButton->setToolTip(tr("Recusa a chamada em espera e mantém a atual"));
        m_secondaryButton->setStyleSheet(dangerSecondary);
    } else if (twoCalls) {
        m_secondaryButton->setText(tr("Alternar"));
        m_secondaryButton->setToolTip(tr("Troca qual das duas chamadas está no ar"));
        m_secondaryButton->setStyleSheet(neutralSecondary);
    } else if (dialingTransfer || consulting) {
        m_secondaryButton->setText(tr("Cancelar"));
        m_secondaryButton->setToolTip(tr("Volta para a chamada original"));
        m_secondaryButton->setStyleSheet(neutralSecondary);
    } else if (dialingForward) {
        m_secondaryButton->setText(tr("Cancelar"));
        m_secondaryButton->setToolTip(tr("Não ativa o encaminhamento"));
        m_secondaryButton->setStyleSheet(neutralSecondary);
    }

    if (incoming) {
        m_actionButton->setText(tr("Atender"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kSuccessGreen, Theme::kSuccessGreenMuted));
        m_actionButton->setEnabled(true);
    } else if (waiting) {
        // Green "Atender" for the second call: the current one is parked
        // automatically, so this is the safe, expected action.
        m_actionButton->setText(tr("Atender"));
        m_actionButton->setToolTip(tr("Coloca a chamada atual em espera e atende a nova"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kSuccessGreen, Theme::kSuccessGreenMuted));
        m_actionButton->setEnabled(true);
    } else if (inCall) {
        m_actionButton->setText(tr("Desligar"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kDangerRed, Theme::kDangerRed));
        m_actionButton->setEnabled(true);
    } else if (dialingTransfer) {
        m_actionButton->setText(tr("Chamar"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kSuccessGreen, Theme::kSuccessGreenMuted));
        m_actionButton->setEnabled(!m_numberEdit->text().isEmpty());
    } else if (consulting) {
        m_actionButton->setText(tr("Transferir"));
        m_actionButton->setToolTip(tr("Conecta as duas pessoas e sai da linha"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kSuccessGreen, Theme::kSuccessGreenMuted));
        m_actionButton->setEnabled(true);
    } else if (dialingForward) {
        m_actionButton->setText(tr("Ativar"));
        m_actionButton->setToolTip(tr("Passa a encaminhar todas as chamadas para esse ramal"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kAccentBlue, Theme::kSuccessGreenMuted));
        m_actionButton->setEnabled(!m_numberEdit->text().isEmpty());
    } else {
        m_actionButton->setText(tr("Ligar"));
        m_actionButton->setStyleSheet(actionStyle(Theme::kSuccessGreen, Theme::kSuccessGreenMuted));
        // Idle: dialable with a number on the display, and also on an empty
        // display when there is something to redial.
        const bool canRedial = !lastDialedNumber().isEmpty();
        m_actionButton->setEnabled(!m_numberEdit->text().isEmpty() || canRedial);
        m_actionButton->setToolTip(m_numberEdit->text().isEmpty() && canRedial
                                        ? tr("Rediscar %1").arg(lastDialedNumber())
                                        : QString());
    }

    if (inCall || consulting) {
        // The clock is NOT started here: while the phone is still ringing on
        // the other end there is no call duration yet. It starts when
        // SipCoreManager reports the call answered (onCallConnected), so a
        // call that is never picked up never shows a duration.
        if (!m_callDurationTimer->isActive()) {
            m_callSeconds = 0;
            m_callDurationLabel->clear();
        }
    } else {
        m_callDurationTimer->stop();
        m_callSeconds = 0;
        m_callDurationLabel->clear();
    }

    // The hook line carries "quem está aguardando"/"quem está em espera",
    // which only this function knows has just changed.
    refreshHookLabel();
}

void MainWindow::onRemotePartyChanged(const QString &displayName, const QString &number) {
    // Show who actually picked up. The dialed number stays visible when it
    // differs, so it's clear the call landed somewhere else.
    QString shown = displayName.isEmpty() ? number : QStringLiteral("%1 (%2)").arg(displayName, number);
    if (!m_currentCallPeer.isEmpty() && !m_currentCallIncoming && number != m_currentCallPeer) {
        shown += tr("\ndiscado: %1").arg(m_currentCallPeer);
    }
    m_callPeerLabel->setText(shown);

    // History should record who was really reached, not just what was typed.
    if (!displayName.isEmpty()) {
        m_currentCallDisplayName = displayName;
    }
    if (!number.isEmpty()) {
        m_currentCallConnectedPeer = number;
    }
}

void MainWindow::onCallConnected() {
    m_currentCallAnswered = true;
    setHookText(tr("Em chamada"));
    // Fires again when resuming from hold, so don't restart a running clock.
    if (m_callDurationTimer->isActive()) {
        return;
    }
    m_callSeconds = 0;
    m_callDurationLabel->setText(QStringLiteral("00:00"));
    m_callDurationTimer->start();
}

void MainWindow::setHookText(const QString &text) {
    m_hookBaseText = text;
    refreshHookLabel();
}

void MainWindow::refreshHookLabel() {
    // Modes that silence the phone are painted in the alert colour so they
    // stand out from the call situation next to them.
    const auto badge = [](const QString &text, const char *color) {
        return QStringLiteral("<span style=\"color:%1;\">%2</span>")
            .arg(QLatin1String(color), text.toHtmlEscaped());
    };

    QStringList parts;
    parts << m_hookBaseText.toHtmlEscaped();
    // Who else is on the line matters more than any mode badge, so it comes
    // first: with two calls the user has to know which one they are talking to
    // and who is parked.
    if (m_uiCallState == UiCallState::CallWaiting && !m_waitingPeer.isEmpty()) {
        parts << badge(tr("Aguardando: %1").arg(m_waitingDisplayName.isEmpty() ? m_waitingPeer
                                                                                : m_waitingDisplayName),
                       Theme::kAccentTeal);
    } else if (m_uiCallState == UiCallState::TwoCalls && !m_heldPeer.isEmpty()) {
        parts << badge(tr("%1 em espera").arg(m_heldPeer), Theme::kAccentTeal);
    }
    if (m_muteButton->isChecked()) {
        parts << badge(tr("Mudo"), Theme::kDangerRed);
    }
    if (m_dndButton->isChecked()) {
        parts << badge(tr("Não perturbe"), Theme::kDangerRed);
    }
    m_hookLabel->setText(parts.join(QStringLiteral(" · ")));
}

void MainWindow::updateCallDuration() {
    ++m_callSeconds;
    const int minutes = m_callSeconds / 60;
    const int seconds = m_callSeconds % 60;
    m_callDurationLabel->setText(QStringLiteral("%1:%2")
                                      .arg(minutes, 2, 10, QLatin1Char('0'))
                                      .arg(seconds, 2, 10, QLatin1Char('0')));
}

// --- SIP events -----------------------------------------------------------

void MainWindow::onRegistrationStateChanged(bool registered, const QString &message) {
    m_registered = registered;
    m_registrationMessage = message;
    updatePresenceLabel();
}

void MainWindow::updatePresenceLabel() {
    const auto paint = [this](const QString &text, const char *color) {
        m_presenceLabel->setText(text);
        m_presenceLabel->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px;").arg(QLatin1String(color)));
    };

    if (!m_registered) {
        paint(tr("● Não registrado"), Theme::kTextSecondary);
        m_accountLabel->setText(m_registrationMessage.isEmpty() ? m_accountLabel->text()
                                                                 : m_registrationMessage);
        setWindowTitle(tr("RRP Softphone"));
        return;
    }

    m_accountLabel->setText(m_extension.isEmpty() ? tr("Registrado") : tr("Ramal %1").arg(m_extension));
    setWindowTitle(m_extension.isEmpty() ? tr("RRP Softphone") : tr("RRP Softphone - %1").arg(m_extension));

    // Anything that stops this phone from ringing must be loud in the status
    // line — a user who forgets DND is on just sees calls silently vanish.
    if (!m_forwardTarget.isEmpty()) {
        paint(tr("● Siga-me → %1").arg(m_forwardTarget), Theme::kAccentBlue);
    } else if (m_dndButton->isChecked()) {
        paint(tr("● Não perturbe"), Theme::kDangerRed);
    } else {
        paint(tr("● Disponível"), Theme::kAccentTeal);
    }
}

void MainWindow::onDoNotDisturbToggled(bool enabled) {
    m_sipCore->setDoNotDisturb(enabled);
    SettingsStore::saveCallHandling(enabled, m_forwardTarget);
    updatePresenceLabel();
    refreshHookLabel();
}

void MainWindow::applyForwardTarget(const QString &target) {
    m_forwardTarget = target.trimmed();
    m_sipCore->setCallForwardTarget(m_forwardTarget);
    SettingsStore::saveCallHandling(m_dndButton->isChecked(), m_forwardTarget);

    QSignalBlocker blocker(m_forwardButton);
    m_forwardButton->setChecked(!m_forwardTarget.isEmpty());
    updatePresenceLabel();
}

void MainWindow::onForwardButtonClicked() {
    // Already forwarding: one click turns it off, no questions asked.
    if (!m_forwardTarget.isEmpty()) {
        applyForwardTarget(QString());
        setHookText(tr("Siga-me desligado"));
        return;
    }

    // Otherwise ask where to. Uses the display and keypad, same as dialing,
    // instead of sending the user into Configurações.
    QSignalBlocker blocker(m_forwardButton);
    m_forwardButton->setChecked(false);

    if (m_uiCallState != UiCallState::Idle) {
        return; // don't hijack the display in the middle of a call
    }
    m_numberEdit->clear();
    setHookText(tr("Encaminhar para qual ramal?"));
    applyUiCallState(UiCallState::ForwardDialing);
}

void MainWindow::onIncomingCall(const QString &displayName, const QString &address) {
    const QString who = displayName.isEmpty() ? address : QStringLiteral("%1 (%2)").arg(displayName, address);
    m_callPeerLabel->setText(who);

    m_currentCallPeer = address;
    m_currentCallDisplayName = displayName;
    m_currentCallStart = QDateTime::currentDateTime();
    m_currentCallIncoming = true;
    m_currentCallAnswered = false;

    setHookText(tr("Chamada recebida"));
    applyUiCallState(UiCallState::Incoming);
    announceIncomingCall(who);
}

// The GPL requires that whoever receives the binary be told their rights and
// where the source is. A user who got only the installer never opens the
// repository, so the notice has to be reachable from inside the app — this is
// that place, and it is also where support reads the version from.
void MainWindow::showAbout() {
    const QString licensesDir = QDir(QCoreApplication::applicationDirPath() + "/licenses").absolutePath();
    const QString text =
        tr("<b>%1</b> versão %2<br>"
           "Copyright © 2026 %3<br><br>"
           "Este programa é <b>software livre</b>, distribuído sob a "
           "<a href=\"file:///%4/gpl-3.0.txt\">GNU General Public License v3</a>. "
           "Você pode redistribuí-lo e modificá-lo sob os termos dessa licença.<br><br>"
           "Ele vem <b>sem nenhuma garantia</b>, nem mesmo a garantia implícita de "
           "comercialização ou adequação a um fim específico.<br><br>"
           "Código-fonte e o procedimento de compilação:<br>"
           "<a href=\"https://github.com/RRPSystems/rrphone-desktop\">github.com/RRPSystems/rrphone-desktop</a><br><br>"
           "Componentes de terceiros e suas licenças: "
           "<a href=\"file:///%4/THIRD-PARTY-NOTICES.md\">avisos de terceiros</a>.")
            .arg(QLatin1String(RRP_APP_NAME), QLatin1String(RRP_APP_VERSION),
                  QLatin1String(RRP_COPYRIGHT_HOLDER), licensesDir);

    QMessageBox about(this);
    about.setWindowTitle(tr("Sobre o %1").arg(QLatin1String(RRP_APP_NAME)));
    about.setIconPixmap(QPixmap(":/rrp_logo.png").scaled(64, 64, Qt::KeepAspectRatio,
                                                          Qt::SmoothTransformation));
    about.setTextFormat(Qt::RichText);
    about.setText(text);
    // Opens the license files and the repository in the user's own browser or
    // editor, instead of leaving the links as decoration.
    about.setTextInteractionFlags(Qt::TextBrowserInteraction);
    about.exec();
}

// How a ringing call asks for attention. Windows refuses to let a background
// process steal the foreground, so activateWindow() alone silently does
// nothing when another app is in front — which is why the window used to pop
// up only when it was minimised. Rather than fight that (the workarounds all
// involve faking input focus), the default leans into it: notify clearly and
// let the user decide when to switch.
void MainWindow::announceIncomingCall(const QString &who) {
    // Un-minimising is always allowed, and is what the user expects when the
    // app is out of the way. It doesn't take focus from anything.
    if (isMinimized() || !isVisible()) {
        showNormal();
    }

    if (SettingsStore::loadIncomingCallBehavior() == SettingsStore::IncomingCallBehavior::BringToFront) {
        raise();
        activateWindow();
    }

    if (m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(tr("Chamada recebida"), who, QSystemTrayIcon::Information, 20000);
    }

#ifdef Q_OS_WIN
    // Flashes the taskbar button until the window is activated. This is the
    // piece that makes "don't steal focus" workable: without it a call ringing
    // behind another window is easy to miss, since only the audio announces it.
    FLASHWINFO flash = {};
    flash.cbSize = sizeof(flash);
    flash.hwnd = reinterpret_cast<HWND>(winId());
    flash.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
    flash.uCount = 0;
    flash.dwTimeout = 0;
    FlashWindowEx(&flash);
#endif
}

void MainWindow::onCallStateChangedLabel(const QString &stateLabel) {
    // While the user is composing a transfer or forward target, the hook line
    // is holding an instruction ("Transferir para qual ramal?"). The engine
    // keeps reporting its own progress through the same line — parking and
    // resuming the call both produce labels — and letting those through
    // replaces the instruction with "Em chamada", leaving the user staring at
    // an empty field with no idea what it wants.
    if (m_uiCallState == UiCallState::TransferDialing || m_uiCallState == UiCallState::ForwardDialing) {
        return;
    }
    setHookText(stateLabel);
}

void MainWindow::onCallEnded() {
    recordCallInHistory();
    m_numberEdit->clear();
    setHookText(tr("No gancho"));
    applyUiCallState(UiCallState::Idle);

    // If the call died with a reason ("Busy", "Temporarily Unavailable"),
    // keep it on screen briefly instead of snapping straight back to
    // "No gancho" — otherwise the user never sees why the call failed.
    if (!m_pendingErrorText.isEmpty()) {
        const QString reason = m_pendingErrorText;
        m_pendingErrorText.clear();
        setHookText(reason);
        QTimer::singleShot(6000, this, [this, reason]() {
            // Compare against the base text, not the label: the label may
            // carry mode badges ("· Mudo") appended to it.
            if (m_uiCallState == UiCallState::Idle && m_hookBaseText == reason) {
                setHookText(tr("No gancho"));
            }
        });
    }
}

void MainWindow::onConsultationConnected() {
    // The hook line is what tells the user the destination picked up and
    // that completing the transfer will now hand over a live conversation.
    setHookText(tr("Falando com o destino — toque em Transferir para concluir"));
}

void MainWindow::startCallTo(const QString &number) {
    if (number.isEmpty() || m_uiCallState != UiCallState::Idle) {
        return;
    }
    m_numberEdit->setText(number);
    m_mainStack->setCurrentIndex(0);
    m_navDialer->setChecked(true);
    m_navContacts->setChecked(false);
    m_navHistory->setChecked(false);
    onActionButtonClicked();
}

void MainWindow::recordCallInHistory() {
    if (m_currentCallPeer.isEmpty()) {
        return; // nothing was dialed or received
    }

    CallRecord record;
    record.peer = m_currentCallPeer;
    record.displayName = m_currentCallDisplayName;
    record.startedAt = m_currentCallStart.isValid() ? m_currentCallStart : QDateTime::currentDateTime();
    record.incoming = m_currentCallIncoming;
    record.answered = m_currentCallAnswered;
    record.durationSeconds = m_currentCallAnswered ? m_callSeconds : 0;
    if (!m_currentCallAnswered) {
        record.note = m_currentCallIncoming ? tr("não atendida") : tr("sem resposta");
    } else if (!m_currentCallConnectedPeer.isEmpty() && m_currentCallConnectedPeer != m_currentCallPeer) {
        // Landed somewhere other than what was dialed. `peer` stays as the
        // dialed number so redialing from the history repeats the original
        // intent (the hunt group, not whoever happened to pick up).
        record.note = tr("atendida por %1").arg(m_currentCallConnectedPeer);
    }
    CallHistoryStore::append(record);

    m_currentCallPeer.clear();
    m_currentCallConnectedPeer.clear();
    m_currentCallDisplayName.clear();
    m_currentCallAnswered = false;

    if (m_mainStack->currentIndex() == 2) {
        m_historyPanel->reload();
    }
    // The number just used becomes the redial target, so the call key has to
    // come back enabled even with the display empty.
    m_actionButton->setEnabled(!m_numberEdit->text().trimmed().isEmpty() || !lastDialedNumber().isEmpty());
}

void MainWindow::onCallWaiting(const QString &displayName, const QString &number) {
    m_waitingPeer = number;
    m_waitingDisplayName = displayName;

    // A discreet double beep instead of the full ringtone: the user is in the
    // middle of a conversation and the ring would talk over it. The local DTMF
    // player is the right tool here — it is designed to be audible to this
    // user only, so the person on the line hears nothing.
    m_sipCore->playLocalDtmf('1');
    QTimer::singleShot(220, this, [this]() {
        if (m_uiCallState == UiCallState::CallWaiting) {
            m_sipCore->playLocalDtmf('1');
        }
    });

    applyUiCallState(UiCallState::CallWaiting);
}

void MainWindow::onWaitingCallEnded() {
    // Caller gave up, or we declined: log it so the call isn't invisible, and
    // go back to the conversation that never stopped.
    if (!m_waitingPeer.isEmpty()) {
        m_waitingPeer.clear();
        m_waitingDisplayName.clear();
    }
    if (m_uiCallState == UiCallState::CallWaiting) {
        applyUiCallState(m_sipCore->hasHeldCall() ? UiCallState::TwoCalls : UiCallState::Active);
    }
}

void MainWindow::onHeldCallEnded() {
    m_heldPeer.clear();
    if (m_uiCallState == UiCallState::TwoCalls) {
        setHookText(tr("O outro lado desligou"));
        applyUiCallState(UiCallState::Active);
    }
}

void MainWindow::onHeldCallPromoted() {
    // The call we were on ended; the parked one is back in the foreground.
    m_currentCallPeer = m_heldPeer;
    m_currentCallConnectedPeer.clear();
    m_heldPeer.clear();
    m_callPeerLabel->setText(m_currentCallPeer);
    setHookText(tr("Em chamada"));
    applyUiCallState(UiCallState::Active);
}

void MainWindow::onCallAutoHandled(const QString &fromAddress, const QString &note) {
    // DND/forwarding never reach the normal call flow, so they are recorded
    // here — otherwise the user has no idea who called.
    CallRecord record;
    record.peer = fromAddress;
    record.startedAt = QDateTime::currentDateTime();
    record.incoming = true;
    record.answered = false;
    record.note = note;
    CallHistoryStore::append(record);

    if (m_mainStack->currentIndex() == 2) {
        m_historyPanel->reload();
    }
}

void MainWindow::onErrorOccurred(const QString &message) {
    // Deliberately NOT a modal dialog: this runs inside a liblinphone
    // callback, and a modal box spins a nested event loop right in the
    // middle of the SIP state machine — the "call ended" notification that
    // follows never arrives and the UI stays stuck on "Chamando...".
    // A failed call also doesn't deserve a dialog to dismiss every time;
    // the hook line is where the user is already looking.
    qWarning().noquote() << "[sip] falha na chamada:" << message;
    m_pendingErrorText = message;
    setHookText(message);
}

// --- User actions ---------------------------------------------------------

void MainWindow::onKeyPressed(QChar digit) {
    if (m_uiCallState == UiCallState::Active || m_uiCallState == UiCallState::TransferConsulting) {
        // D-05: during a call the keypad sends DTMF instead of composing a
        // new number.
        m_sipCore->sendDtmf(digit.toLatin1());
        return;
    }
    // Idle or picking a transfer target: compose a number, with the same
    // audible key feedback a desk phone gives.
    m_sipCore->playLocalDtmf(digit.toLatin1());
    m_numberEdit->setText(m_numberEdit->text() + digit);
}

void MainWindow::onActionButtonClicked() {
    switch (m_uiCallState) {
    case UiCallState::Incoming:
        m_sipCore->answer();
        setHookText(tr("Em chamada"));
        applyUiCallState(UiCallState::Active);
        break;

    case UiCallState::Active:
    case UiCallState::TwoCalls:
        // Ends only the call in the foreground. With one parked, the engine
        // brings it back and onHeldCallPromoted() puts the UI back in a call.
        m_sipCore->hangup();
        break;

    case UiCallState::CallWaiting: {
        // The conversation in progress is parked, not dropped.
        m_heldPeer = m_currentCallConnectedPeer.isEmpty() ? m_currentCallPeer : m_currentCallConnectedPeer;
        m_currentCallPeer = m_waitingPeer;
        m_currentCallDisplayName = m_waitingDisplayName;
        m_currentCallConnectedPeer.clear();
        m_currentCallIncoming = true;
        m_currentCallAnswered = true;
        m_waitingPeer.clear();
        m_waitingDisplayName.clear();

        m_sipCore->answerWaitingCall();
        m_callPeerLabel->setText(m_currentCallDisplayName.isEmpty() ? m_currentCallPeer
                                                                     : m_currentCallDisplayName);
        applyUiCallState(UiCallState::TwoCalls);
        break;
    }

    case UiCallState::Idle: {
        QString number = m_numberEdit->text().trimmed();
        if (number.isEmpty()) {
            // Redial: an empty display plus the call key brings back the last
            // number dialed, but does not place the call — the user still has
            // to press again. Recalling a number by accident is harmless;
            // dialing one is not.
            const QString last = lastDialedNumber();
            if (!last.isEmpty()) {
                m_numberEdit->setText(last);
            }
            return;
        }
        m_sipCore->call(number);
        m_callPeerLabel->setText(number);

        m_currentCallPeer = number;
        m_currentCallDisplayName.clear();
        m_currentCallStart = QDateTime::currentDateTime();
        m_currentCallIncoming = false;
        m_currentCallAnswered = false;

        setHookText(tr("Chamando..."));
        applyUiCallState(UiCallState::Active);
        break;
    }

    case UiCallState::TransferDialing: {
        const QString target = m_numberEdit->text().trimmed();
        if (target.isEmpty()) {
            return;
        }
        m_sipCore->beginAttendedTransfer(target);
        m_callPeerLabel->setText(tr("Transferir para %1").arg(target));
        setHookText(tr("Chamando o destino..."));
        m_numberEdit->clear();
        applyUiCallState(UiCallState::TransferConsulting);
        break;
    }

    case UiCallState::TransferConsulting:
        // Completes the handover. Consultative if the destination already
        // answered, blind if it is still ringing — same gesture either way.
        m_sipCore->completeAttendedTransfer();
        setHookText(tr("Transferindo..."));
        break;

    case UiCallState::ForwardDialing: {
        const QString target = m_numberEdit->text().trimmed();
        if (target.isEmpty()) {
            return;
        }
        applyForwardTarget(target);
        m_numberEdit->clear();
        setHookText(tr("Chamadas seguem para %1").arg(target));
        applyUiCallState(UiCallState::Idle);
        break;
    }
    }
}

void MainWindow::onSecondaryButtonClicked() {
    switch (m_uiCallState) {
    case UiCallState::Incoming:
        m_sipCore->hangup();
        setHookText(tr("No gancho"));
        applyUiCallState(UiCallState::Idle);
        break;

    case UiCallState::CallWaiting:
        // Only the newcomer is refused; the conversation carries on.
        m_sipCore->declineWaitingCall();
        break;

    case UiCallState::TwoCalls: {
        m_sipCore->swapCalls();
        const QString wasInForeground =
            m_currentCallConnectedPeer.isEmpty() ? m_currentCallPeer : m_currentCallConnectedPeer;
        m_currentCallPeer = m_heldPeer;
        m_currentCallConnectedPeer.clear();
        m_currentCallDisplayName.clear();
        m_heldPeer = wasInForeground;
        m_callPeerLabel->setText(m_currentCallPeer);
        refreshHookLabel();
        break;
    }

    case UiCallState::TransferDialing:
        // Gave up before dialing: just take the call off hold.
        m_sipCore->setHeld(false);
        m_numberEdit->clear();
        setHookText(tr("Em chamada"));
        applyUiCallState(UiCallState::Active);
        break;

    case UiCallState::TransferConsulting:
        m_sipCore->cancelAttendedTransfer();
        setHookText(tr("Em chamada"));
        applyUiCallState(UiCallState::Active);
        break;

    case UiCallState::ForwardDialing:
        m_numberEdit->clear();
        setHookText(tr("No gancho"));
        applyUiCallState(UiCallState::Idle);
        break;

    default:
        break;
    }
}

void MainWindow::onTransferClicked() {
    if (!m_sipCore->hasActiveCall()) {
        return;
    }
    // Park the current call before dialing so the other party doesn't hear
    // the keypad, then let the main display double as the target field.
    m_sipCore->setHeld(true);
    {
        // Reflect the hold in the button without letting it re-command the
        // engine — a second pause makes liblinphone log "already in the
        // process of being paused".
        QSignalBlocker blocker(m_holdButton);
        m_holdButton->setChecked(true);
    }
    m_numberEdit->clear();
    setHookText(tr("Transferir para qual ramal?"));
    applyUiCallState(UiCallState::TransferDialing);
}

void MainWindow::onSettingsRequested() {
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::onSettingsApplied);
        connect(m_settingsDialog, &SettingsDialog::profileImported, this, &MainWindow::onSettingsApplied);
        // "Testar som": route to whatever is selected right now — without
        // saving — so the user can try devices until one is audible.
        // "Ouvir": apply the pending ringtone choice and play it, so the user
        // hears the actual file before committing to it.
        connect(m_settingsDialog, &SettingsDialog::ringtonePreviewRequested, this, [this]() {
            const SettingsStore::AudioRouting routing = m_settingsDialog->audioRouting();
            m_sipCore->setRingerDevice(routing.ringerId);
            if (!m_sipCore->setRingtoneFile(routing.ringtonePath)) {
                QMessageBox::warning(m_settingsDialog, tr("Toque inválido"),
                                      tr("Não foi possível usar esse arquivo. Escolha um WAV."));
                return;
            }
            m_sipCore->playRingtonePreview();
        });
        connect(m_settingsDialog, &SettingsDialog::audioTestRequested, this, [this]() {
            const SettingsStore::AudioRouting routing = m_settingsDialog->audioRouting();
            m_sipCore->setPlaybackDevice(routing.playbackId);
            const bool started = m_sipCore->playTestSound();
            m_settingsDialog->setAudioTestResult(
                started ? tr("Tocando em: %1").arg(m_sipCore->playbackDeviceName())
                        : tr("Falhou — o dispositivo de saída não pôde ser aberto."));
        });
    }
    // Engine codec list first, then the saved profile on top of it, so the
    // user's saved order/enablement wins.
    m_settingsDialog->setAvailableCodecs(m_sipCore->audioCodecs());
    if (!m_currentProfile.username.isEmpty()) {
        m_settingsDialog->setProfile(m_currentProfile);
    }

    SettingsDialog::DeviceList captureDevices;
    SettingsDialog::DeviceList playbackDevices;
    for (const SipCoreManager::AudioDeviceInfo &device : m_sipCore->audioDevices()) {
        if (device.canCapture) {
            captureDevices.append({device.id, device.name});
        }
        if (device.canPlay) {
            playbackDevices.append({device.id, device.name});
        }
    }
    m_settingsDialog->setAudioDevices(captureDevices, playbackDevices, SettingsStore::loadAudioRouting());
    m_settingsDialog->setCallForwardTarget(m_forwardTarget);
    m_settingsDialog->setIncomingCallBehavior(SettingsStore::loadIncomingCallBehavior());
    m_settingsDialog->setAudioProcessing(SettingsStore::loadAudioProcessing());
    m_settingsDialog->setReplaceLocalContacts(SettingsStore::loadReplaceLocalContacts());

    m_settingsDialog->exec();
}

void MainWindow::onSettingsApplied(const AccountProfile &profile) {
    applyProfile(profile);
    SettingsStore::saveProfile(profile);

    if (m_settingsDialog == nullptr) {
        return;
    }

    // Audio routing and forwarding live outside AccountProfile on purpose:
    // they are machine/user preferences, not part of the account that gets
    // shipped in a .rrpprofile.
    SettingsStore::AudioRouting routing = m_settingsDialog->audioRouting();
    m_sipCore->setCaptureDevice(routing.captureId);
    m_sipCore->setPlaybackDevice(routing.playbackId);
    m_sipCore->setRingerDevice(routing.ringerId);
    if (!m_sipCore->setRingtoneFile(routing.ringtonePath)) {
        // Don't persist a file we just failed to load: it would come back on
        // every launch and leave incoming calls silent with no explanation.
        QMessageBox::warning(this, tr("Toque inválido"),
                              tr("O arquivo de toque escolhido não pôde ser lido; o padrão foi mantido."));
        routing.ringtonePath.clear();
    }
    SettingsStore::saveAudioRouting(routing);

    applyForwardTarget(m_settingsDialog->callForwardTarget());
    SettingsStore::saveIncomingCallBehavior(m_settingsDialog->incomingCallBehavior());

    const SettingsStore::AudioProcessing processing = m_settingsDialog->audioProcessing();
    SettingsStore::saveAudioProcessing(processing);
    m_sipCore->setAudioProcessing({processing.noiseSuppression, processing.echoCancellation,
                                   processing.automaticGainControl});
    SettingsStore::saveReplaceLocalContacts(m_settingsDialog->replaceLocalContacts());
}

void MainWindow::applyProfile(const AccountProfile &profile) {
    m_currentProfile = profile;

    SipCoreManager::AccountConfig accountConfig;
    accountConfig.displayName = profile.displayName;
    accountConfig.username = profile.username;
    accountConfig.password = profile.password;
    accountConfig.domain = profile.domain;
    accountConfig.transport = profile.transport;

    m_extension = profile.username;
    m_sipCore->configureAccount(accountConfig);

    if (!profile.codecs.isEmpty()) {
        m_sipCore->setAudioCodecsOrder(profile.codecs);
    }

    SipCoreManager::DtmfMethod dtmfMethod = SipCoreManager::DtmfMethod::Rfc2833;
    if (profile.dtmfMethod == QStringLiteral("info")) {
        dtmfMethod = SipCoreManager::DtmfMethod::SipInfo;
    } else if (profile.dtmfMethod == QStringLiteral("inband")) {
        dtmfMethod = SipCoreManager::DtmfMethod::InBand;
    }
    m_sipCore->setDtmfMethod(dtmfMethod);

    if (!profile.contactsUrl.isEmpty()) {
        m_contactsFetcher->setUrl(QUrl(profile.contactsUrl));
        m_contactsFetcher->fetch();
    }
}

void MainWindow::onContactsUpdated(const QList<Contact> &contacts) {
    // Optional "server is the single source of truth" mode: a successful
    // download clears whatever the user had created locally. Only on
    // success — a failed fetch must never destroy the user's own contacts.
    if (SettingsStore::loadReplaceLocalContacts()) {
        qInfo().noquote() << "[contatos] lista do servidor recebida; apagando contatos locais";
        LocalContactsStore::clear();
        m_contactsPanel->reloadLocalContacts();
    }
    m_contactsPanel->setRemoteContacts(contacts);
}

void MainWindow::onContactsFetchFailed(const QString &reason) {
    m_contactsPanel->showMessage(tr("Falha ao carregar contatos: %1").arg(reason));
}

// --- Autostart (D-11) -----------------------------------------------------

bool MainWindow::isAutoStartEnabled() const {
    return autoStartSettings().contains(kAutoStartKey);
}

void MainWindow::setAutoStartEnabled(bool enabled) {
    QSettings settings = autoStartSettings();
    if (enabled) {
        settings.setValue(kAutoStartKey, QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        settings.remove(kAutoStartKey);
    }
}

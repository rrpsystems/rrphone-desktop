#pragma once

#include <QMainWindow>
#include <QList>
#include <QDateTime>

#include "core/SipCoreManager.h"
#include "contacts/ContactsXmlFetcher.h"
#include "profile/ProfileStore.h"

class QCloseEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QSystemTrayIcon;
class QTimer;
class QToolButton;
class QAction;

class DialPadWidget;
class ContactsPanel;
class HistoryPanel;
class SettingsDialog;

// Single-window softphone UI, laid out like the compact desk-phone softphones
// the PRD references (3CXPhone/MicroSip): status row, display area, hook line
// with call actions, line tabs, keypad, one big action button, and an icon
// nav bar at the bottom. Everything lives in one window — no menu bar, no
// side dock — so it stays as close to that reference layout as possible.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // Consumes Enter on the number field so it is not handled twice.
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRegistrationStateChanged(bool registered, const QString &message);
    void onIncomingCall(const QString &displayName, const QString &address);
    void onCallStateChangedLabel(const QString &stateLabel);
    void onCallConnected();
    void onRemotePartyChanged(const QString &displayName, const QString &number);
    void onCallEnded();
    void onCallWaiting(const QString &displayName, const QString &number);
    void onWaitingCallEnded();
    void onHeldCallEnded();
    void onHeldCallPromoted();
    void onCallAutoHandled(const QString &fromAddress, const QString &note);
    // Dial a number picked from the contacts or history lists.
    void startCallTo(const QString &number);
    void onConsultationConnected();
    void onErrorOccurred(const QString &message);

    void onKeyPressed(QChar digit);
    void onActionButtonClicked();
    void onSecondaryButtonClicked();
    void onTransferClicked();
    void onDoNotDisturbToggled(bool enabled);
    void onForwardButtonClicked();

    void onSettingsRequested();
    // Applies a profile to the running engine (no persistence).
    void applyProfile(const AccountProfile &profile);
    // Applies *and* persists — this is what the settings dialog triggers.
    void onSettingsApplied(const AccountProfile &profile);

    void onContactsUpdated(const QList<Contact> &contacts);
    void onContactsFetchFailed(const QString &reason);

    void updateCallDuration();

private:
    // How the UI should present itself; drives the action buttons, the
    // display area and the hook line all at once.
    //
    // Transferring is deliberately a state of the main window rather than a
    // dialog with blind/attended options: the user picks the destination and
    // then either completes or cancels, and whether it ends up consultative
    // or blind depends only on whether the target picked up.
    // CallWaiting: talking to one person while a second call rings for a
    // decision. TwoCalls: both answered, one parked, swappable.
    enum class UiCallState { Idle, Incoming, Active, CallWaiting, TwoCalls,
                              TransferDialing, TransferConsulting, ForwardDialing };

    QWidget *buildStatusRow();
    QWidget *buildDisplayArea();
    QWidget *buildHookRow();
    QWidget *buildActionRow();
    QWidget *buildVolumeControls();
    QWidget *buildBottomBar();
    QWidget *buildPhonePage();

    void setupTrayIcon();
    void applyUiCallState(UiCallState state);
    // The hook line shows the call situation plus whatever modes are active
    // (mudo, não perturbe), so it is composed from state in one place rather
    // than written directly.
    void setHookText(const QString &text);
    void refreshHookLabel();
    // The status line has to convey four things at once (registered, DND,
    // forwarding, extension), so it is rendered from state in one place.
    void updatePresenceLabel();
    // Single place that turns forwarding on/off: engine, persistence, button
    // state and status line always move together.
    void applyForwardTarget(const QString &target);
    void recordCallInHistory();
    // Ringing-call attention: notification, taskbar flash and, if the user
    // chose it, bringing the window forward.
    void announceIncomingCall(const QString &who);
    // Most recent outgoing number, for the redial gesture. Empty if none.
    QString lastDialedNumber() const;
    void setAutoStartEnabled(bool enabled);
    bool isAutoStartEnabled() const;

    SipCoreManager *m_sipCore;
    ContactsXmlFetcher *m_contactsFetcher;

    // Status row
    QLabel *m_presenceLabel;
    QLabel *m_accountLabel;

    // Display area
    QStackedWidget *m_displayStack;
    QLabel *m_logoLabel;
    QLineEdit *m_numberEdit;
    QToolButton *m_backspaceButton;
    QLabel *m_callPeerLabel;
    QLabel *m_callDurationLabel;

    // Hook row + call actions
    QLabel *m_hookLabel;
    QToolButton *m_muteButton;
    QToolButton *m_dndButton;
    QToolButton *m_forwardButton;

    // Action row: hold and transfer flank the main call button. The
    // secondary button is context-dependent (Recusar / Cancelar).
    QPushButton *m_holdButton;
    QPushButton *m_transferButton;
    QPushButton *m_actionButton;
    QPushButton *m_secondaryButton;

    // Volume
    QSlider *m_speakerSlider;
    QSlider *m_micSlider;

    QStackedWidget *m_mainStack;
    DialPadWidget *m_dialPad;
    ContactsPanel *m_contactsPanel;
    HistoryPanel *m_historyPanel;
    SettingsDialog *m_settingsDialog = nullptr;

    QToolButton *m_navDialer;
    QToolButton *m_navContacts;
    QToolButton *m_navHistory;
    QToolButton *m_navSettings;

    // Context of the call in progress, used to write the history entry when
    // it ends.
    QString m_currentCallPeer;          // what was dialed / who called
    QString m_currentCallConnectedPeer; // who actually answered, when different
    QString m_currentCallDisplayName;
    // The second caller, while call waiting is on screen.
    QString m_waitingPeer;
    QString m_waitingDisplayName;
    // Who is parked while talking to the other party.
    QString m_heldPeer;
    QDateTime m_currentCallStart;
    bool m_currentCallIncoming = false;
    bool m_currentCallAnswered = false;

    QSystemTrayIcon *m_trayIcon;
    QAction *m_autoStartAction;

    QTimer *m_callDurationTimer;
    int m_callSeconds = 0;
    QString m_extension;
    bool m_registered = false;
    QString m_forwardTarget;
    QString m_registrationMessage;
    // Call situation without the mode suffixes ("No gancho", "Em chamada").
    QString m_hookBaseText;
    // Reason the last call failed, shown on the hook line after it ends.
    QString m_pendingErrorText;
    AccountProfile m_currentProfile;
    UiCallState m_uiCallState = UiCallState::Idle;
};

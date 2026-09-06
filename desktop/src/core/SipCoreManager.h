#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>

#include <linphone/core.h>

#include "CodecInfo.h"

// Thin Qt wrapper around a single liblinphone LinphoneCore instance.
// One instance = one registered SIP account (D-01, single line per the
// PRD's MVP decision — see Docs/RRP_Softphone_Desktop_PRD_Lite_v1.0.md).
//
// liblinphone's C core has no event loop of its own on desktop: it must be
// pumped periodically via linphone_core_iterate(). We do that with a QTimer
// so every callback below always fires on the Qt/GUI thread.
class SipCoreManager : public QObject {
    Q_OBJECT

public:
    struct AccountConfig {
        QString displayName;
        QString username;
        QString password;
        QString domain;     // SIP server, e.g. "sip.example.com" or "sip.example.com:5061"
        QString transport;  // "udp" | "tcp" | "tls"
    };

    enum class DtmfMethod { Rfc2833, SipInfo, InBand };

    // One selectable sound device. `id` is what gets persisted — device names
    // can repeat, ids don't.
    struct AudioDeviceInfo {
        QString id;
        QString name;
        bool canCapture = false;
        bool canPlay = false;
    };

    explicit SipCoreManager(QObject *parent = nullptr);
    ~SipCoreManager() override;

    // Boots the LinphoneCore and starts the iterate timer. Call once at
    // startup, before configureAccount().
    void start();

    // Hangs up, unregisters from the SIP server and stops the core. Must run
    // before the process exits: killing the app without unregistering leaves
    // a stale contact binding on the PBX, and enough of those make the
    // endpoint start refusing new registrations with 403 Forbidden.
    void shutdown();

    // D-01 — configure/replace the single SIP account and (re)register.
    // Safe to call at any time: if the core is still starting up (it only
    // finishes once linphone_core_iterate() starts running, i.e. after the
    // Qt event loop is spinning), the account is queued and applied as soon
    // as the core reports itself ready.
    void configureAccount(const AccountConfig &config);
    void clearAccount();

    // D-02 / D-03 — call control.
    void call(const QString &target);
    void answer();
    void hangup();

    // D-04 — mute is global-mic based (liblinphone has no true per-call mute).
    void setMuted(bool muted);
    void setHeld(bool held);

    // D-05. Also plays the tone locally so the keypad gives audible feedback:
    // DTMF travels out-of-band, so nothing is heard otherwise.
    void sendDtmf(char digit);
    void playLocalDtmf(char digit);

    // Plays a short sound through the current output device without needing a
    // call — the "Testar som" button. Returns false if the engine refused to
    // start playback, which is the signal that the output device is unusable.
    bool playTestSound();

    // Volume controls, exposed as 0..100 for the UI sliders. liblinphone
    // works in dB, so the percentage is mapped onto -30 dB .. +10 dB, which
    // puts unity gain (0 dB, i.e. "untouched audio") at 75%.
    void setSpeakerVolume(int percent);
    void setMicVolume(int percent);
    int speakerVolume() const;
    int micVolume() const;
    static constexpr int kUnityVolumePercent = 75;

    // D-06/D-07 — one transfer flow, no blind/attended choice for the user.
    // 1) beginAttendedTransfer(): holds the active call and dials the target.
    // 2) completeAttendedTransfer(): hands the call over. If the target has
    //    already answered this is a consultative transfer (REFER+Replaces);
    //    if it is still ringing it falls back to a blind transfer, so the
    //    same gesture works whether or not the user waited to talk first.
    // cancelAttendedTransfer(): drops the consultation leg and resumes the
    //    original call instead.
    void beginAttendedTransfer(const QString &target);
    void completeAttendedTransfer();
    void cancelAttendedTransfer();

    // D-14 — audio codec list, in current priority order.
    QList<CodecInfo> audioCodecs() const;
    // Re-applies enable/disable + priority order in one shot (index order
    // in `orderedCodecs` becomes the new negotiation priority).
    void setAudioCodecsOrder(const QList<CodecInfo> &orderedCodecs);

    // D-15
    void setDtmfMethod(DtmfMethod method);
    DtmfMethod dtmfMethod() const;

    // Audio routing. Headset users typically want the call on the headset but
    // the ringer on the PC speakers, so the ringer is chosen separately.
    // An empty id means "leave whatever the engine picked by default".
    QList<AudioDeviceInfo> audioDevices() const;
    void setCaptureDevice(const QString &deviceId);
    void setPlaybackDevice(const QString &deviceId);
    void setRingerDevice(const QString &deviceId);
    QString captureDeviceId() const;
    QString playbackDeviceId() const;
    QString playbackDeviceName() const;
    QString ringerDeviceId() const;

    // Incoming-call handling. Forwarding wins over DND: the call goes to the
    // chosen extension either way, and this phone stays quiet.
    void setDoNotDisturb(bool enabled);
    bool doNotDisturb() const { return m_doNotDisturb; }
    // Empty target turns forwarding off. Redirects with a SIP 302, so the
    // phone never rings and the PBX routes the call immediately.
    void setCallForwardTarget(const QString &target);
    QString callForwardTarget() const { return m_callForwardTarget; }

    bool hasActiveCall() const { return m_activeCall != nullptr; }
    bool inAttendedTransferFlow() const { return m_consultationCall != nullptr; }

signals:
    void registrationStateChanged(bool registered, const QString &message);
    void incomingCall(const QString &fromDisplay, const QString &fromAddress);
    void callStateChanged(const QString &stateLabel);
    // The main call was actually answered by the other side — this, not the
    // moment we start dialing, is when a call duration should start counting.
    // May fire more than once for the same call (e.g. after resuming from
    // hold), so treat it as "is connected", not "just connected".
    void callConnected();
    // Who we are actually talking to, which is not always who was dialed:
    // hunt groups, pickups, forwards and transfers all change it. Emitted
    // whenever the PBX tells us it changed.
    void remotePartyChanged(const QString &displayName, const QString &number);
    void callEnded();
    // An incoming call that never reached the user because DND or forwarding
    // handled it. Emitted so it still shows up in the history — otherwise
    // those calls vanish without a trace.
    void callAutoHandled(const QString &fromAddress, const QString &note);
    // Consultation leg (D-06 step 1) reached StreamsRunning — safe to offer
    // "complete transfer" in the UI now.
    void consultationCallConnected();
    void errorOccurred(const QString &message);

private:
    void iterate();
    void logAudioDevices();
    void configureG729();
    void configureSounds();
    void applyAccountConfig(const AccountConfig &config);
    // Reads the connected party from the SIP signalling and emits
    // remotePartyChanged when it differs from what was last reported.
    void publishRemoteParty(LinphoneCall *call);
    void handleGlobalStateChanged(LinphoneGlobalState state, const char *message);
    void handleRegistrationStateChanged(LinphoneProxyConfig *cfg, LinphoneRegistrationState state, const char *message);
    void handleCallStateChanged(LinphoneCall *call, LinphoneCallState state, const char *message);

    // liblinphone callback trampolines. liblinphone has no per-callback
    // user_data param on these two signatures, so the instance is recovered
    // via linphone_core_cbs_get_user_data(linphone_core_get_current_callbacks(lc)).
    static void onGlobalStateChanged(LinphoneCore *lc, LinphoneGlobalState state, const char *message);
    static void onRegistrationStateChanged(LinphoneCore *lc, LinphoneProxyConfig *cfg, LinphoneRegistrationState state, const char *message);
    static void onCallStateChanged(LinphoneCore *lc, LinphoneCall *call, LinphoneCallState state, const char *message);
    // Forwards liblinphone/mediastreamer log lines into the app log file.
    static void onEngineLogMessage(LinphoneLoggingService *service, const char *domain, LinphoneLogLevel level,
                                   const char *message);

    LinphoneCore *m_core = nullptr;
    LinphoneAccount *m_account = nullptr;
    LinphoneCall *m_activeCall = nullptr;       // call A
    LinphoneCall *m_consultationCall = nullptr; // call C, only during a transfer (D-06)
    QString m_transferTarget;                   // kept for the blind-transfer fallback
    bool m_consultationAnswered = false;
    bool m_doNotDisturb = false;
    QString m_callForwardTarget;
    QString m_ringerDeviceId;
    QString m_ringPath;
    QString m_testSoundPath;
    // The engine's own device choice at startup, so "Padrão do sistema" is restorable.
    QString m_engineDefaultPlaybackId;
    QString m_engineDefaultCaptureId;
    QString m_lastRemoteParty; // avoids re-emitting the same identity
    QTimer *m_iterateTimer = nullptr;
    QElapsedTimer m_statsTick; // throttles the periodic media report
    DtmfMethod m_dtmfMethod = DtmfMethod::Rfc2833;

    // The core isn't usable for registration until it reports LinphoneGlobalOn.
    bool m_coreReady = false;
    bool m_shutdownDone = false;
    bool m_hasPendingAccount = false;
    AccountConfig m_pendingAccount;
};

#include "SipCoreManager.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QRegularExpression>

SipCoreManager::SipCoreManager(QObject *parent) : QObject(parent) {}

SipCoreManager::~SipCoreManager() {
    shutdown();
    if (m_core) {
        linphone_core_unref(m_core);
        m_core = nullptr;
    }
}

void SipCoreManager::shutdown() {
    if (m_shutdownDone) {
        return;
    }
    m_shutdownDone = true;

    if (m_iterateTimer) {
        m_iterateTimer->stop();
    }

    if (m_core != nullptr) {
        // Say goodbye to whoever is on the line before dropping the core.
        if (m_consultationCall) {
            linphone_call_terminate(m_consultationCall);
            m_consultationCall = nullptr;
        }
        if (m_activeCall) {
            linphone_call_terminate(m_activeCall);
            m_activeCall = nullptr;
        }

        if (m_coreReady) {
            // Blocking on purpose: it performs the SIP unregistration, which
            // is the whole point of a clean shutdown.
            qInfo().noquote() << "[sip] encerrando core e removendo registro do servidor...";
            linphone_core_stop(m_core);
            m_coreReady = false;
        }
    }

    if (m_account) {
        linphone_account_unref(m_account);
        m_account = nullptr;
    }
}

void SipCoreManager::start() {
    LinphoneFactory *factory = linphone_factory_get();

    // Field diagnostics: RRP_SIP_DEBUG=1 turns on liblinphone's full SIP
    // trace (REGISTER/INVITE exchanges included). Off by default because it
    // is extremely chatty.
    // Route the engine's own logs into our log file. Without this the only
    // thing on record is what this class prints, so any failure inside
    // mediastreamer (the audio driver above all) is invisible — the app looks
    // healthy while producing no sound.
    LinphoneLoggingService *logService = linphone_logging_service_get();
    LinphoneLoggingServiceCbs *logCbs = linphone_factory_create_logging_service_cbs(factory);
    linphone_logging_service_cbs_set_log_message_written(logCbs, &SipCoreManager::onEngineLogMessage);
    linphone_logging_service_add_callbacks(logService, logCbs);
    linphone_logging_service_cbs_unref(logCbs);

    // RRP_SIP_DEBUG=1 turns on the full debug trace (REGISTER/INVITE exchanges
    // included). Off by default because it is extremely chatty; warnings and
    // errors are always kept, since those are what diagnose a broken install.
    linphone_logging_service_set_log_level(
        logService, qEnvironmentVariableIsSet("RRP_SIP_DEBUG") ? LinphoneLogLevelDebug : LinphoneLogLevelWarning);

    // liblinphone needs to find its bundled grammars (SIP/SDP/vCard parsing),
    // sound files, and root CA bundle under a "top resources dir" — without
    // this, core creation aborts the process outright (bctbx-fatal) the
    // first time it needs the vCard grammar. CMakeLists.txt copies the SDK's
    // share/ folder next to the executable at build time to match this.
    const QString resourcesDir = QDir(QCoreApplication::applicationDirPath() + "/share").absolutePath();
    linphone_factory_set_top_resources_dir(factory, resourcesDir.toUtf8().constData());

    // The Windows audio driver (libmswasapi) and the echo canceller
    // (libmswebrtc) ship as mediastreamer *plugins*, in a separate folder
    // from the main DLLs. Without pointing liblinphone at them there is no
    // sound card driver at all — the core logs "Could not find a suitable
    // soundcard" and calls can't build a media stream. CMakeLists.txt copies
    // that folder to <exe dir>/plugins at build time.
    const QString pluginsDir = QDir(QCoreApplication::applicationDirPath() + "/plugins").absolutePath();
    linphone_factory_set_msplugins_dir(factory, pluginsDir.toUtf8().constData());

    LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(factory);
    linphone_core_cbs_set_user_data(cbs, this);
    linphone_core_cbs_set_global_state_changed(cbs, &SipCoreManager::onGlobalStateChanged);
    linphone_core_cbs_set_registration_state_changed(cbs, &SipCoreManager::onRegistrationStateChanged);
    linphone_core_cbs_set_call_state_changed(cbs, &SipCoreManager::onCallStateChanged);

    // No config files: the app owns account state itself (Section 5 of the
    // PRD — .rrpprofile import/export instead of a linphonerc file).
    m_core = linphone_factory_create_core_3(factory, nullptr, nullptr, nullptr);
    linphone_core_add_callbacks(m_core, cbs);
    linphone_core_cbs_unref(cbs); // core now owns a ref via add_callbacks

    // Identifies us in the SIP User-Agent header — otherwise the PBX shows
    // the extension as an unknown device.
    linphone_core_set_user_agent(m_core, RRP_APP_NAME, RRP_APP_VERSION);

    // This is a softphone, not a messenger: chat, presence subscriptions and
    // conference event packages are all unused here. Left on, they make the
    // core subscribe to services the PBX doesn't offer, which logs an error on
    // every startup ("Unable to subscribe to the conference event package") and
    // puts pointless SUBSCRIBE traffic on the wire.
    linphone_core_disable_chat(m_core, LinphoneReasonNotAcceptable);
    linphone_core_enable_friend_list_subscription(m_core, FALSE);

    // D-15 default DTMF method.
    setDtmfMethod(m_dtmfMethod);

    linphone_core_start(m_core);

    m_iterateTimer = new QTimer(this);
    connect(m_iterateTimer, &QTimer::timeout, this, &SipCoreManager::iterate);
    m_iterateTimer->start(20); // liblinphone-recommended desktop iterate interval
    m_statsTick.start();

    configureG729();
    configureSounds();
    logAudioDevices();
}

// The SDK's default ringtone is a Matroska (.mkv) file, but the Windows SDK
// package ships no matroska mediastreamer plugin — so the engine picks a
// ringtone it physically cannot decode and incoming calls arrive in complete
// silence. Every startup logs it as a mere warning ("No ringtone has been
// defined in sound config, using default one"), which is easy to read past.
// Pointing ring/ringback at the plain WAV files in the same folder fixes it
// with no extra dependency.
void SipCoreManager::configureSounds() {
    if (!m_core) {
        return;
    }
    const QString soundsDir = QDir(QCoreApplication::applicationDirPath() + "/share/sounds/linphone").absolutePath();
    m_ringPath = soundsDir + QStringLiteral("/rings/oldphone-mono.wav");
    const QString ringbackPath = soundsDir + QStringLiteral("/ringback.wav");
    m_testSoundPath = soundsDir + QStringLiteral("/hello8000.wav");

    // Never hand the ringing off to Windows: we want it on the device the
    // user chose for the ringer, which is the whole point of the separate
    // ringer setting (headset users keep the ring on the PC speakers).
    linphone_core_set_native_ringing_enabled(m_core, FALSE);

    if (QFile::exists(m_ringPath)) {
        linphone_core_set_ring(m_core, m_ringPath.toUtf8().constData());
    } else {
        qWarning().noquote() << "[audio] toque não encontrado:" << m_ringPath;
        m_ringPath.clear();
    }
    if (QFile::exists(ringbackPath)) {
        linphone_core_set_ringback(m_core, ringbackPath.toUtf8().constData());
    }
    if (!QFile::exists(m_testSoundPath)) {
        m_testSoundPath = m_ringPath;
    }

    // The default echo canceller (MSWebRTCAEC) refuses to run below 16 kHz and
    // simply disables itself on every narrowband call — which here is every
    // call, since the carrier speaks G.711/G.729. The result is no echo
    // cancellation at all for anyone not on a headset. MSSpeexEC is built into
    // mediastreamer2 and does handle 8 kHz.
    linphone_core_set_echo_canceller_filter_name(m_core, "MSSpeexEC");
    linphone_core_enable_echo_cancellation(m_core, TRUE);

    const char *ring = linphone_core_get_ring(m_core);
    qInfo().noquote() << "[audio] toque:" << QString::fromUtf8(ring != nullptr ? ring : "(nenhum)")
                      << "| cancelador de eco:"
                      << QString::fromUtf8(linphone_core_get_echo_canceller_filter_name(m_core));
}

// Plays a short sound through the current output device, without needing a
// call. This is the "Testar som" button in the audio settings: it is the only
// way for a user to tell an unusable output device apart from a SIP/media
// problem, and it is also the fastest diagnostic we can ask them to run.
bool SipCoreManager::playTestSound() {
    if (!m_core || m_testSoundPath.isEmpty()) {
        return false;
    }
    const LinphoneStatus status = linphone_core_play_local(m_core, m_testSoundPath.toUtf8().constData());
    qInfo().noquote() << "[audio] teste de som:" << m_testSoundPath
                      << (status == 0 ? QStringLiteral("iniciado") : QStringLiteral("FALHOU"))
                      << "| saída:" << playbackDeviceName();
    return status == 0;
}

void SipCoreManager::onEngineLogMessage(LinphoneLoggingService *, const char *domain, LinphoneLogLevel level,
                                         const char *message) {
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QString::fromUtf8(domain != nullptr ? domain : "linphone"),
                                  QString::fromUtf8(message != nullptr ? message : ""));
    switch (level) {
    case LinphoneLogLevelFatal:
    case LinphoneLogLevelError:
        qCritical().noquote() << line;
        break;
    case LinphoneLogLevelWarning:
        qWarning().noquote() << line;
        break;
    default:
        qInfo().noquote() << line;
        break;
    }
}

// G.729 Annex B (silence suppression / comfort noise) is a classic interop
// trap: liblinphone offers "annexb=yes" by default, most PBX-side G.729
// implementations don't do Annex B, and the mismatch produces a call that
// negotiates fine and carries RTP but plays silence or choppy audio.
// Announcing annexb=no is the interoperable choice.
void SipCoreManager::configureG729() {
    if (!m_core) {
        return;
    }
    bctbx_list_t *codecs = linphone_core_get_audio_payload_types(m_core);
    for (const bctbx_list_t *node = codecs; node != nullptr; node = node->next) {
        auto *payload = static_cast<LinphonePayloadType *>(node->data);
        const char *mime = linphone_payload_type_get_mime_type(payload);
        if (mime != nullptr && QString::fromUtf8(mime).compare(QLatin1String("G729"), Qt::CaseInsensitive) == 0) {
            // Only the *recv* fmtp belongs to us: it is what goes out in our
            // SDP, describing what we accept. The send fmtp is filled in by
            // negotiation from what the peer answered, so setting it by hand
            // just gets our value concatenated onto theirs and the offer goes
            // out as "annexb=no;annexb=no".
            linphone_payload_type_set_recv_fmtp(payload, "annexb=no");
            qInfo().noquote() << "[audio] G.729 configurado com annexb=no";
        }
    }
    bctbx_list_free_with_data(codecs, reinterpret_cast<bctbx_list_free_func>(linphone_payload_type_unref));
}

// Diagnostics: "no audio" is one of the most common softphone complaints, and
// the first question is always which devices the engine actually found.
void SipCoreManager::logAudioDevices() {
    if (!m_core) {
        return;
    }
    // linphone_core_get_sound_devices_list() is deprecated and its ownership
    // contract is self-contradictory ("unmodifiable" but "@tobefreed") — the
    // LinphoneAudioDevice list is refcounted, so ownership is unambiguous.
    bctbx_list_t *devices = linphone_core_get_extended_audio_devices(m_core);
    QStringList names;
    for (const bctbx_list_t *node = devices; node != nullptr; node = node->next) {
        auto *device = static_cast<LinphoneAudioDevice *>(node->data);
        const char *name = linphone_audio_device_get_device_name(device);
        names.append(QString::fromUtf8(name != nullptr ? name : "?"));
    }
    bctbx_list_free_with_data(devices, reinterpret_cast<bctbx_list_free_func>(linphone_audio_device_unref));

    const char *playback = linphone_core_get_playback_device(m_core);
    const char *capture = linphone_core_get_capture_device(m_core);
    qInfo().noquote() << "[audio] dispositivos encontrados:" << (names.isEmpty() ? QStringLiteral("NENHUM") : names.join(QStringLiteral(" | ")));
    // Remembered so that picking "Padrão do sistema" again after forcing a
    // device actually goes back to the engine's own choice, instead of being
    // a no-op that leaves the forced device in place until the next restart.
    if (m_engineDefaultPlaybackId.isEmpty()) {
        m_engineDefaultPlaybackId = QString::fromUtf8(playback != nullptr ? playback : "");
        m_engineDefaultCaptureId = QString::fromUtf8(capture != nullptr ? capture : "");
    }

    const char *ringer = linphone_core_get_ringer_device(m_core);
    qInfo().noquote() << "[audio] saída:" << QString::fromUtf8(playback ? playback : "(nenhuma)")
                       << "| entrada:" << QString::fromUtf8(capture ? capture : "(nenhuma)")
                       << "| toque:" << QString::fromUtf8(ringer ? ringer : "(nenhum)");

    QStringList codecs;
    for (const CodecInfo &codec : audioCodecs()) {
        codecs.append(QStringLiteral("%1/%2%3")
                           .arg(codec.mimeType)
                           .arg(codec.clockRate)
                           .arg(codec.enabled ? QString() : QStringLiteral(" (off)")));
    }
    qInfo().noquote() << "[audio] codecs:" << codecs.join(QStringLiteral(" | "));
}

void SipCoreManager::iterate() {
    if (!m_core) {
        return;
    }
    linphone_core_iterate(m_core);

    // Periodic media report during a call. "No audio" has too many possible
    // causes to guess at, and these numbers split them apart: no download
    // bandwidth means nothing is reaching our socket (firewall/NAT), while
    // healthy bandwidth with silence points at the local playback chain.
    if (m_activeCall != nullptr && m_statsTick.elapsed() >= 5000) {
        m_statsTick.restart();
        if (LinphoneCallStats *stats = linphone_call_get_audio_stats(m_activeCall)) {
            qInfo().noquote() << QStringLiteral("[audio] rtp | recebendo: %1 kbit/s | enviando: %2 kbit/s | perda rx: %3% "
                                                "| perda tx: %4% | jitter: %5 ms")
                                     .arg(linphone_call_stats_get_download_bandwidth(stats), 0, 'f', 1)
                                     .arg(linphone_call_stats_get_upload_bandwidth(stats), 0, 'f', 1)
                                     .arg(linphone_call_stats_get_receiver_loss_rate(stats), 0, 'f', 1)
                                     .arg(linphone_call_stats_get_sender_loss_rate(stats), 0, 'f', 1)
                                     .arg(linphone_call_stats_get_jitter_buffer_size_ms(stats), 0, 'f', 0);
            linphone_call_stats_unref(stats);
        }
    }
}

void SipCoreManager::configureAccount(const AccountConfig &config) {
    if (!m_core) {
        qWarning() << "SipCoreManager::configureAccount called before start()";
        return;
    }

    // linphone_core_start() only completes once the core is iterated, which
    // can't happen before the Qt event loop runs. Registering an account
    // against a core still in Startup silently does nothing — so queue it.
    if (!m_coreReady) {
        m_pendingAccount = config;
        m_hasPendingAccount = true;
        qInfo().noquote() << "[sip] core ainda inicializando; conta enfileirada para" << config.username;
        return;
    }

    applyAccountConfig(config);
}

void SipCoreManager::applyAccountConfig(const AccountConfig &config) {
    clearAccount();

    LinphoneFactory *factory = linphone_factory_get();

    const QString identityUri = QStringLiteral("sip:%1@%2").arg(config.username, config.domain);
    LinphoneAddress *identity = linphone_factory_create_address(factory, identityUri.toUtf8().constData());
    if (!identity) {
        emit errorOccurred(tr("Endereço SIP inválido: %1").arg(identityUri));
        return;
    }

    const QString serverUri = QStringLiteral("sip:%1;transport=%2")
                                   .arg(config.domain, config.transport.toLower());
    LinphoneAddress *serverAddr = linphone_factory_create_address(factory, serverUri.toUtf8().constData());

    LinphoneAccountParams *params = linphone_core_create_account_params(m_core);
    linphone_account_params_set_identity_address(params, identity);
    linphone_account_params_set_server_address(params, serverAddr);
    linphone_account_params_enable_register(params, TRUE);
    // Short registration lifetime on purpose. Every run of the app takes a
    // new source port, so it shows up as a new contact on the PBX; if the
    // process is killed (crash, Task Manager) no unREGISTER is sent and that
    // contact lingers until it expires. With the default (~1h) those orphans
    // pile up and can trip max_contacts, which the server answers with a
    // confusing "403 Forbidden". Five minutes also keeps the NAT binding warm.
    linphone_account_params_set_expires(params, 300);

    m_account = linphone_core_create_account(m_core, params);
    const LinphoneStatus addStatus = linphone_core_add_account(m_core, m_account);
    linphone_core_set_default_account(m_core, m_account);
    qInfo().noquote() << "[sip] conta configurada:" << identityUri << "via" << serverUri
                       << "| add_account status:" << addStatus
                       << "| senha:" << (config.password.isEmpty() ? "VAZIA" : "presente");

    LinphoneAuthInfo *authInfo = linphone_factory_create_auth_info(
        factory,
        config.username.toUtf8().constData(),
        nullptr,
        config.password.toUtf8().constData(),
        nullptr,
        nullptr,
        config.domain.toUtf8().constData());
    linphone_core_add_auth_info(m_core, authInfo);

    linphone_account_params_unref(params);
    linphone_address_unref(identity);
    linphone_address_unref(serverAddr);
    // m_account keeps the ref handed back by linphone_core_create_account;
    // released in clearAccount()/destructor.
}

void SipCoreManager::clearAccount() {
    if (m_account && m_core) {
        linphone_core_remove_account(m_core, m_account);
        linphone_account_unref(m_account);
        m_account = nullptr;
    }
}

void SipCoreManager::call(const QString &target) {
    if (!m_core || target.isEmpty()) {
        return;
    }
    // linphone_core_interpret_url resolves a bare extension ("1042") against
    // the default account's domain, or accepts a full sip:/tel: URI as-is.
    LinphoneAddress *addr = linphone_core_interpret_url(m_core, target.toUtf8().constData());
    if (!addr) {
        emit errorOccurred(tr("Não foi possível interpretar o destino: %1").arg(target));
        return;
    }
    m_activeCall = linphone_core_invite_address(m_core, addr);
    linphone_address_unref(addr);
    qInfo().noquote() << "[sip] discando para" << target
                      << (m_activeCall != nullptr ? "| INVITE enviado" : "| FALHOU ao criar a chamada");
}

void SipCoreManager::answer() {
    if (m_activeCall) {
        linphone_call_accept(m_activeCall);
    }
}

void SipCoreManager::hangup() {
    qInfo().noquote() << "[sip] desligar solicitado | principal:" << (m_activeCall != nullptr ? "sim" : "nao")
                      << "| consulta:" << (m_consultationCall != nullptr ? "sim" : "nao");
    if (m_consultationCall) {
        linphone_call_terminate(m_consultationCall);
    }
    if (m_activeCall) {
        linphone_call_terminate(m_activeCall);
    }
}

void SipCoreManager::setMuted(bool muted) {
    if (m_core) {
        // liblinphone mutes at the core/mic level, not per LinphoneCall.
        linphone_core_enable_mic(m_core, muted ? FALSE : TRUE);
    }
}

void SipCoreManager::setHeld(bool held) {
    if (!m_activeCall) {
        return;
    }
    if (held) {
        linphone_call_pause(m_activeCall);
    } else {
        linphone_call_resume(m_activeCall);
    }
}

void SipCoreManager::sendDtmf(char digit) {
    // The audible feedback is local-only: DTMF is carried to the other side
    // out-of-band (RFC 2833 / SIP INFO), so without this the keypad is
    // completely silent and the user gets no confirmation a digit registered.
    playLocalDtmf(digit);
    if (m_activeCall) {
        linphone_call_send_dtmf(m_activeCall, digit);
    }
}

void SipCoreManager::playLocalDtmf(char digit) {
    if (m_core) {
        linphone_core_play_dtmf(m_core, digit, 120);
    }
}

QString SipCoreManager::playbackDeviceName() const {
    if (!m_core) {
        return {};
    }
    // get_output_audio_device() is call-scoped and returns NULL when idle, so
    // the default device is what actually answers "where will sound come out".
    const LinphoneAudioDevice *device = linphone_core_get_default_output_audio_device(m_core);
    if (device == nullptr) {
        device = linphone_core_get_output_audio_device(m_core);
    }
    if (device != nullptr) {
        const char *name = linphone_audio_device_get_device_name(device);
        if (name != nullptr) {
            return QString::fromUtf8(name);
        }
    }
    const char *legacy = linphone_core_get_playback_device(m_core);
    return QString::fromUtf8(legacy != nullptr ? legacy : "(desconhecido)");
}

namespace {
// Slider percent <-> liblinphone gain in dB. Linear in dB (which is how
// volume actually "feels"), spanning -30 dB (near silent) to +10 dB (boost),
// with 0 dB — untouched audio — landing at 75%.
constexpr float kMinGainDb = -30.0f;
constexpr float kMaxGainDb = 10.0f;

float percentToGainDb(int percent) {
    const float clamped = qBound(0, percent, 100) / 100.0f;
    return kMinGainDb + clamped * (kMaxGainDb - kMinGainDb);
}

int gainDbToPercent(float gainDb) {
    const float ratio = (gainDb - kMinGainDb) / (kMaxGainDb - kMinGainDb);
    return qBound(0, qRound(ratio * 100.0f), 100);
}
} // namespace

void SipCoreManager::setSpeakerVolume(int percent) {
    if (m_core) {
        linphone_core_set_playback_gain_db(m_core, percentToGainDb(percent));
    }
}

void SipCoreManager::setMicVolume(int percent) {
    if (m_core) {
        linphone_core_set_mic_gain_db(m_core, percentToGainDb(percent));
    }
}

int SipCoreManager::speakerVolume() const {
    if (!m_core) {
        return kUnityVolumePercent;
    }
    return gainDbToPercent(linphone_core_get_playback_gain_db(m_core));
}

int SipCoreManager::micVolume() const {
    if (!m_core) {
        return kUnityVolumePercent;
    }
    return gainDbToPercent(linphone_core_get_mic_gain_db(m_core));
}

void SipCoreManager::beginAttendedTransfer(const QString &target) {
    if (!m_core || !m_activeCall || target.isEmpty()) {
        return;
    }
    m_transferTarget = target;
    m_consultationAnswered = false;
    linphone_call_pause(m_activeCall);

    LinphoneAddress *addr = linphone_core_interpret_url(m_core, target.toUtf8().constData());
    if (!addr) {
        emit errorOccurred(tr("Não foi possível interpretar o destino: %1").arg(target));
        linphone_call_resume(m_activeCall);
        m_transferTarget.clear();
        return;
    }
    m_consultationCall = linphone_core_invite_address(m_core, addr);
    linphone_address_unref(addr);
}

void SipCoreManager::completeAttendedTransfer() {
    if (!m_activeCall) {
        return;
    }

    if (m_consultationCall != nullptr && m_consultationAnswered) {
        // Target already picked up: REFER with Replaces, so they keep the
        // conversation they are already having. Both legs end on our side
        // once accepted; handleCallStateChanged clears the pointers.
        linphone_call_transfer_to_another(m_activeCall, m_consultationCall);
    } else if (!m_transferTarget.isEmpty()) {
        // Target is still ringing (or we never got that far): hand the call
        // over blind, so "complete the transfer" does the expected thing
        // regardless of whether the user waited to announce the call.
        if (m_consultationCall != nullptr) {
            linphone_call_terminate(m_consultationCall);
            m_consultationCall = nullptr;
        }
        linphone_call_transfer(m_activeCall, m_transferTarget.toUtf8().constData());
    }
    m_transferTarget.clear();
    m_consultationAnswered = false;
}

void SipCoreManager::cancelAttendedTransfer() {
    m_transferTarget.clear();
    m_consultationAnswered = false;
    if (m_consultationCall) {
        linphone_call_terminate(m_consultationCall);
        m_consultationCall = nullptr;
    }
    if (m_activeCall) {
        linphone_call_resume(m_activeCall);
    }
}

namespace {
// LinphonePayloadType is the modern (non-deprecated) codec handle; the old
// mediastreamer2 PayloadType* + payload_type_get_*() accessors used by
// liblinphone's now-deprecated linphone_core_get_audio_codecs() aren't even
// exposed in this SDK's public headers anymore.
bool matchesCodecInfo(LinphonePayloadType *pt, const CodecInfo &info) {
    const QString mime = QString::fromUtf8(linphone_payload_type_get_mime_type(pt));
    return mime.compare(info.mimeType, Qt::CaseInsensitive) == 0 &&
           linphone_payload_type_get_clock_rate(pt) == info.clockRate &&
           linphone_payload_type_get_channels(pt) == info.channels;
}
} // namespace

QList<CodecInfo> SipCoreManager::audioCodecs() const {
    QList<CodecInfo> result;
    if (!m_core) {
        return result;
    }
    // "Freshly allocated ... @tobefreed": we own one ref per element plus
    // the list spine itself.
    bctbx_list_t *codecs = linphone_core_get_audio_payload_types(m_core);
    for (const bctbx_list_t *node = codecs; node != nullptr; node = node->next) {
        auto *payload = static_cast<LinphonePayloadType *>(node->data);
        CodecInfo info;
        info.mimeType = QString::fromUtf8(linphone_payload_type_get_mime_type(payload));
        info.clockRate = linphone_payload_type_get_clock_rate(payload);
        info.channels = linphone_payload_type_get_channels(payload);
        info.enabled = linphone_payload_type_enabled(payload);
        result.append(info);
    }
    bctbx_list_free_with_data(codecs, reinterpret_cast<bctbx_list_free_func>(linphone_payload_type_unref));
    return result;
}

void SipCoreManager::setAudioCodecsOrder(const QList<CodecInfo> &orderedCodecs) {
    if (!m_core) {
        return;
    }
    // Per linphone_core_set_audio_payload_types() docs: the objects handed
    // back must be the very same ones returned by get_audio_payload_types(),
    // just reordered/enabled — not looked up separately.
    bctbx_list_t *currentCodecs = linphone_core_get_audio_payload_types(m_core);

    bctbx_list_t *newOrder = nullptr;
    for (const CodecInfo &info : orderedCodecs) {
        for (const bctbx_list_t *node = currentCodecs; node != nullptr; node = node->next) {
            auto *payload = static_cast<LinphonePayloadType *>(node->data);
            if (matchesCodecInfo(payload, info)) {
                linphone_payload_type_enable(payload, info.enabled ? TRUE : FALSE);
                newOrder = bctbx_list_append(newOrder, payload);
                break;
            }
        }
    }
    linphone_core_set_audio_payload_types(m_core, newOrder);

    bctbx_list_free(newOrder); // spine only: elements are owned via currentCodecs below
    bctbx_list_free_with_data(currentCodecs, reinterpret_cast<bctbx_list_free_func>(linphone_payload_type_unref));
}

void SipCoreManager::setDtmfMethod(DtmfMethod method) {
    m_dtmfMethod = method;
    if (!m_core) {
        return;
    }
    switch (method) {
    case DtmfMethod::Rfc2833:
        linphone_core_set_use_rfc2833_for_dtmf(m_core, TRUE);
        linphone_core_set_use_info_for_dtmf(m_core, FALSE);
        break;
    case DtmfMethod::SipInfo:
        linphone_core_set_use_rfc2833_for_dtmf(m_core, FALSE);
        linphone_core_set_use_info_for_dtmf(m_core, TRUE);
        break;
    case DtmfMethod::InBand:
        // Neither out-of-band method enabled -> liblinphone falls back to
        // sending DTMF as audible tones mixed into the RTP stream.
        linphone_core_set_use_rfc2833_for_dtmf(m_core, FALSE);
        linphone_core_set_use_info_for_dtmf(m_core, FALSE);
        break;
    }
}

SipCoreManager::DtmfMethod SipCoreManager::dtmfMethod() const {
    return m_dtmfMethod;
}

// --- Audio routing --------------------------------------------------------

namespace {
// Finds a device by id in the engine's list. Caller must unref the returned
// list; the returned device points into it, so use it before freeing.
LinphoneAudioDevice *findDevice(bctbx_list_t *devices, const QString &deviceId) {
    for (const bctbx_list_t *node = devices; node != nullptr; node = node->next) {
        auto *device = static_cast<LinphoneAudioDevice *>(node->data);
        const char *id = linphone_audio_device_get_id(device);
        if (id != nullptr && deviceId == QString::fromUtf8(id)) {
            return device;
        }
    }
    return nullptr;
}
} // namespace

QList<SipCoreManager::AudioDeviceInfo> SipCoreManager::audioDevices() const {
    QList<AudioDeviceInfo> result;
    if (!m_core) {
        return result;
    }
    bctbx_list_t *devices = linphone_core_get_extended_audio_devices(m_core);
    for (const bctbx_list_t *node = devices; node != nullptr; node = node->next) {
        auto *device = static_cast<LinphoneAudioDevice *>(node->data);
        AudioDeviceInfo info;
        const char *id = linphone_audio_device_get_id(device);
        const char *name = linphone_audio_device_get_device_name(device);
        info.id = QString::fromUtf8(id != nullptr ? id : "");
        info.name = QString::fromUtf8(name != nullptr ? name : "?");
        info.canCapture = linphone_audio_device_has_capability(device, LinphoneAudioDeviceCapabilityRecord);
        info.canPlay = linphone_audio_device_has_capability(device, LinphoneAudioDeviceCapabilityPlay);
        result.append(info);
    }
    bctbx_list_free_with_data(devices, reinterpret_cast<bctbx_list_free_func>(linphone_audio_device_unref));
    return result;
}

// There are two distinct device settings in liblinphone and picking the wrong
// one is silent: linphone_core_set_{input,output}_audio_device() only retargets
// calls that are *already running*, so calling it from the settings screen —
// where by definition no call is up — does nothing at all. What the next call
// uses is the "default" device, and the file/ring players use the older
// string-keyed sound card. All three are set together here so that whatever the
// user picks applies to calls, ringing and the sound test alike.
void SipCoreManager::setCaptureDevice(const QString &deviceId) {
    if (!m_core) {
        return;
    }
    const QString wanted = deviceId.isEmpty() ? m_engineDefaultCaptureId : deviceId;
    if (wanted.isEmpty()) {
        return;
    }
    bctbx_list_t *devices = linphone_core_get_extended_audio_devices(m_core);
    if (LinphoneAudioDevice *device = findDevice(devices, wanted)) {
        linphone_core_set_default_input_audio_device(m_core, device);
        linphone_core_set_input_audio_device(m_core, device); // retarget a live call too
        linphone_core_set_capture_device(m_core, wanted.toUtf8().constData());
    }
    bctbx_list_free_with_data(devices, reinterpret_cast<bctbx_list_free_func>(linphone_audio_device_unref));
}

void SipCoreManager::setPlaybackDevice(const QString &deviceId) {
    if (!m_core) {
        return;
    }
    const QString wanted = deviceId.isEmpty() ? m_engineDefaultPlaybackId : deviceId;
    if (wanted.isEmpty()) {
        return;
    }
    bctbx_list_t *devices = linphone_core_get_extended_audio_devices(m_core);
    if (LinphoneAudioDevice *device = findDevice(devices, wanted)) {
        linphone_core_set_default_output_audio_device(m_core, device);
        linphone_core_set_output_audio_device(m_core, device); // retarget a live call too
        linphone_core_set_playback_device(m_core, wanted.toUtf8().constData());
    }
    bctbx_list_free_with_data(devices, reinterpret_cast<bctbx_list_free_func>(linphone_audio_device_unref));
}

void SipCoreManager::setRingerDevice(const QString &deviceId) {
    if (!m_core || deviceId.isEmpty()) {
        return;
    }
    // No modern per-device API for the ringer, only this (deprecated but
    // working) core-level setter that takes the device id.
    m_ringerDeviceId = deviceId;
    linphone_core_set_ringer_device(m_core, deviceId.toUtf8().constData());
}

QString SipCoreManager::captureDeviceId() const {
    if (!m_core) {
        return {};
    }
    const LinphoneAudioDevice *device = linphone_core_get_input_audio_device(m_core);
    const char *id = device != nullptr ? linphone_audio_device_get_id(device) : nullptr;
    return QString::fromUtf8(id != nullptr ? id : "");
}

QString SipCoreManager::playbackDeviceId() const {
    if (!m_core) {
        return {};
    }
    const LinphoneAudioDevice *device = linphone_core_get_output_audio_device(m_core);
    const char *id = device != nullptr ? linphone_audio_device_get_id(device) : nullptr;
    return QString::fromUtf8(id != nullptr ? id : "");
}

QString SipCoreManager::ringerDeviceId() const {
    return m_ringerDeviceId;
}

// --- Incoming call handling -----------------------------------------------

void SipCoreManager::setDoNotDisturb(bool enabled) {
    m_doNotDisturb = enabled;
    qInfo().noquote() << "[sip] não perturbe:" << (enabled ? "ligado" : "desligado");
}

void SipCoreManager::setCallForwardTarget(const QString &target) {
    m_callForwardTarget = target.trimmed();
    qInfo().noquote() << "[sip] siga-me:"
                       << (m_callForwardTarget.isEmpty() ? QStringLiteral("desligado") : m_callForwardTarget);
}

namespace {
// Pulls "Nome" <sip:2130@pbx>;party=called out of a Remote-Party-ID or
// P-Asserted-Identity header. Written by hand instead of reusing
// linphone_factory_create_address() because those headers carry trailing
// header parameters that the address parser rejects.
bool parseIdentityHeader(const QString &value, QString *displayNameOut, QString *numberOut) {
    if (value.trimmed().isEmpty()) {
        return false;
    }

    static const QRegularExpression userRe(QStringLiteral(R"([sS][iI][pP][sS]?:([^@>;\s]+))"));
    const QRegularExpressionMatch userMatch = userRe.match(value);
    if (!userMatch.hasMatch()) {
        return false;
    }
    *numberOut = userMatch.captured(1);

    static const QRegularExpression quotedNameRe(QStringLiteral("\"([^\"]*)\""));
    const QRegularExpressionMatch nameMatch = quotedNameRe.match(value);
    if (nameMatch.hasMatch()) {
        *displayNameOut = nameMatch.captured(1).trimmed();
    } else {
        // Unquoted display name, e.g.  Fulano <sip:2130@pbx>
        const int angle = value.indexOf(QLatin1Char('<'));
        if (angle > 0) {
            *displayNameOut = value.left(angle).trimmed();
        }
    }
    return true;
}
} // namespace

void SipCoreManager::publishRemoteParty(LinphoneCall *call) {
    if (call == nullptr) {
        return;
    }

    QString displayName;
    QString number;

    // Asterisk can be told to advertise the connected party with either
    // header; P-Asserted-Identity (RFC 3325) is the modern one, but
    // Remote-Party-ID is still what many dialplans send.
    if (const LinphoneCallParams *params = linphone_call_get_remote_params(call)) {
        for (const char *header : {"P-Asserted-Identity", "Remote-Party-ID"}) {
            const char *raw = linphone_call_params_get_custom_header(params, header);
            if (raw != nullptr && parseIdentityHeader(QString::fromUtf8(raw), &displayName, &number)) {
                break;
            }
        }
    }

    // No identity headers: fall back to the dialog's remote address, which
    // liblinphone already keeps up to date across transfers.
    if (number.isEmpty()) {
        if (const LinphoneAddress *remote = linphone_call_get_remote_address(call)) {
            const char *user = linphone_address_get_username(remote);
            const char *name = linphone_address_get_display_name(remote);
            number = QString::fromUtf8(user != nullptr ? user : "");
            if (displayName.isEmpty() && name != nullptr) {
                displayName = QString::fromUtf8(name);
            }
        }
    }

    if (number.isEmpty()) {
        return;
    }

    const QString identity = displayName + QLatin1Char('|') + number;
    if (identity == m_lastRemoteParty) {
        return; // nothing changed, don't churn the UI
    }
    m_lastRemoteParty = identity;
    qInfo().noquote() << "[sip] interlocutor:" << (displayName.isEmpty() ? number
                                                                          : displayName + " <" + number + ">");
    emit remotePartyChanged(displayName, number);
}

void SipCoreManager::handleGlobalStateChanged(LinphoneGlobalState state, const char *message) {
    qInfo().noquote() << "[sip] estado global do core:" << QString::fromUtf8(linphone_global_state_to_string(state))
                       << QString::fromUtf8(message ? message : "");
    if (state != LinphoneGlobalOn) {
        return;
    }

    m_coreReady = true;
    if (m_hasPendingAccount) {
        m_hasPendingAccount = false;
        applyAccountConfig(m_pendingAccount);
    }
}

void SipCoreManager::handleRegistrationStateChanged(LinphoneProxyConfig *, LinphoneRegistrationState state, const char *message) {
    const bool registered = (state == LinphoneRegistrationOk);
    emit registrationStateChanged(registered, QString::fromUtf8(message ? message : ""));
}

void SipCoreManager::handleCallStateChanged(LinphoneCall *call, LinphoneCallState state, const char *message) {
    // Every transition, not just the unhandled ones. A call that ends early leaves
    // no other trace, and without the full sequence there is no way to tell
    // "the INVITE never went out" from "something hung it up immediately".
    qInfo().noquote() << "[sip] chamada" << (call == m_consultationCall && call != nullptr ? "(consulta)" : "(principal)")
                      << "estado:" << QString::fromUtf8(linphone_call_state_to_string(state))
                      << (message != nullptr && *message != '\0' ? QStringLiteral("| %1").arg(QString::fromUtf8(message))
                                                                 : QString());

    // Any progress on the main call can carry a new connected identity — the
    // PBX updates it when the call lands somewhere other than what was
    // dialed (grupo de captura, desvio, transferência).
    if (call == m_activeCall && call != nullptr) {
        switch (state) {
        case LinphoneCallStateOutgoingRinging:
        case LinphoneCallStateOutgoingEarlyMedia:
        case LinphoneCallStateConnected:
        case LinphoneCallStateStreamsRunning:
        case LinphoneCallStateUpdatedByRemote:
        case LinphoneCallStateReferred:
            publishRemoteParty(call);
            break;
        default:
            break;
        }
    }

    switch (state) {
    case LinphoneCallIncomingReceived: {
        const LinphoneAddress *caller = linphone_call_get_remote_address(call);
        const char *callerName = caller != nullptr ? linphone_address_get_username(caller) : nullptr;
        const QString callerId = QString::fromUtf8(callerName != nullptr ? callerName : "");

        // Forwarding first: the call is handed straight to the other
        // extension with a 302, so this phone never rings at all — no delay,
        // no "try here first" step.
        if (!m_callForwardTarget.isEmpty()) {
            LinphoneAddress *target =
                linphone_core_interpret_url(m_core, m_callForwardTarget.toUtf8().constData());
            if (target != nullptr) {
                qInfo().noquote() << "[sip] siga-me: redirecionando chamada para" << m_callForwardTarget;
                linphone_call_redirect_to(call, target);
                linphone_address_unref(target);
                emit callAutoHandled(callerId, tr("encaminhada para %1").arg(m_callForwardTarget));
                break;
            }
            qWarning().noquote() << "[sip] siga-me: destino inválido, ignorando:" << m_callForwardTarget;
        }

        if (m_doNotDisturb) {
            qInfo().noquote() << "[sip] não perturbe: recusando chamada recebida";
            linphone_call_decline(call, LinphoneReasonBusy);
            emit callAutoHandled(callerId, tr("recusada (não perturbe)"));
            break;
        }

        m_activeCall = call;
        const char *displayName = caller != nullptr ? linphone_address_get_display_name(caller) : nullptr;
        emit incomingCall(QString::fromUtf8(displayName != nullptr ? displayName : ""), callerId);
        break;
    }
    case LinphoneCallConnected:
        // 200 OK from the other side: the call was answered.
        if (call == m_activeCall) {
            emit callConnected();
        } else if (call == m_consultationCall) {
            m_consultationAnswered = true;
        }
        emit callStateChanged(tr("Em chamada"));
        break;
    case LinphoneCallStreamsRunning:
        // Everything needed to explain "não sai áudio" in one line: what was
        // actually negotiated and which devices the engine is using.
        if (const LinphoneCallParams *current = linphone_call_get_current_params(call)) {
            const LinphonePayloadType *used = linphone_call_params_get_used_audio_payload_type(current);
            const char *mime = used != nullptr ? linphone_payload_type_get_mime_type(used) : nullptr;
            const char *fmtp = used != nullptr ? linphone_payload_type_get_send_fmtp(used) : nullptr;
            const LinphoneAudioDevice *out = linphone_core_get_output_audio_device(m_core);
            const LinphoneAudioDevice *in = linphone_core_get_input_audio_device(m_core);
            qInfo().noquote() << "[audio] chamada ativa | codec:"
                               << QString::fromUtf8(mime != nullptr ? mime : "?")
                               << QString::fromUtf8(fmtp != nullptr ? fmtp : "")
                               << "| saída:"
                               << QString::fromUtf8(out != nullptr ? linphone_audio_device_get_device_name(out) : "?")
                               << "| entrada:"
                               << QString::fromUtf8(in != nullptr ? linphone_audio_device_get_device_name(in) : "?");
        }
        if (call == m_consultationCall) {
            m_consultationAnswered = true;
            emit consultationCallConnected();
        } else if (call == m_activeCall) {
            // Safety net: media is flowing, so the call is up even if we
            // somehow missed the Connected transition.
            emit callConnected();
        }
        emit callStateChanged(tr("Em chamada"));
        break;
    case LinphoneCallOutgoingInit:
    case LinphoneCallOutgoingProgress:
        emit callStateChanged(tr("Chamando..."));
        break;
    case LinphoneCallOutgoingRinging:
    case LinphoneCallOutgoingEarlyMedia:
        emit callStateChanged(tr("Tocando..."));
        break;
    case LinphoneCallResuming:
        emit callStateChanged(tr("Retomando..."));
        break;
    case LinphoneCallStateReferred:
        emit callStateChanged(tr("Transferindo..."));
        break;
    case LinphoneCallPaused:
        emit callStateChanged(tr("Em espera"));
        break;
    case LinphoneCallPausedByRemote:
        emit callStateChanged(tr("Em espera pelo outro lado"));
        break;
    case LinphoneCallError:
        emit errorOccurred(QString::fromUtf8(message ? message : "Erro na chamada"));
        [[fallthrough]];
    case LinphoneCallEnd:
    case LinphoneCallReleased:
        if (call == m_activeCall) {
            m_activeCall = nullptr;
            m_lastRemoteParty.clear(); // next call must report its own identity
            emit callEnded();
        }
        if (call == m_consultationCall) {
            m_consultationCall = nullptr;
        }
        break;
    default:
        // Remaining states are internal transitions (UpdatedByRemote,
        // EarlyUpdating, ...) with no useful meaning for the user. Leaving
        // the current label alone beats showing raw enum names like
        // "LinphoneCallOutgoingRinging" in the UI.
        qInfo().noquote() << "[sip] estado de chamada:"
                           << QString::fromUtf8(linphone_call_state_to_string(state));
        break;
    }
}

void SipCoreManager::onGlobalStateChanged(LinphoneCore *lc, LinphoneGlobalState state, const char *message) {
    auto *cbs = linphone_core_get_current_callbacks(lc);
    auto *self = static_cast<SipCoreManager *>(linphone_core_cbs_get_user_data(cbs));
    if (self) {
        self->handleGlobalStateChanged(state, message);
    }
}

void SipCoreManager::onRegistrationStateChanged(LinphoneCore *lc, LinphoneProxyConfig *cfg, LinphoneRegistrationState state, const char *message) {
    auto *cbs = linphone_core_get_current_callbacks(lc);
    auto *self = static_cast<SipCoreManager *>(linphone_core_cbs_get_user_data(cbs));
    if (self) {
        self->handleRegistrationStateChanged(cfg, state, message);
    }
}

void SipCoreManager::onCallStateChanged(LinphoneCore *lc, LinphoneCall *call, LinphoneCallState state, const char *message) {
    auto *cbs = linphone_core_get_current_callbacks(lc);
    auto *self = static_cast<SipCoreManager *>(linphone_core_cbs_get_user_data(cbs));
    if (self) {
        self->handleCallStateChanged(call, state, message);
    }
}

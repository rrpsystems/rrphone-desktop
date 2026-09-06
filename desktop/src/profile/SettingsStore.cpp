#include "SettingsStore.h"
#include "CredentialStore.h"

#include <QSettings>
#include <QDebug>

namespace {
constexpr auto kKeyDisplayName = "account/displayName";
constexpr auto kKeyUsername = "account/username";
constexpr auto kKeyDomain = "account/domain";
constexpr auto kKeyTransport = "account/transport";
constexpr auto kKeyDtmfMethod = "account/dtmfMethod";
constexpr auto kKeyContactsUrl = "contacts/url";
constexpr auto kKeySpeakerVolume = "audio/speakerVolume";
constexpr auto kKeyMicVolume = "audio/micVolume";
constexpr auto kArrayCodecs = "codecs";
} // namespace

void SettingsStore::saveProfile(const AccountProfile &profile) {
    QSettings settings;
    settings.setValue(kKeyDisplayName, profile.displayName);
    settings.setValue(kKeyUsername, profile.username);
    settings.setValue(kKeyDomain, profile.domain);
    settings.setValue(kKeyTransport, profile.transport);
    settings.setValue(kKeyDtmfMethod, profile.dtmfMethod);
    settings.setValue(kKeyContactsUrl, profile.contactsUrl);

    settings.beginWriteArray(kArrayCodecs, profile.codecs.size());
    for (int i = 0; i < profile.codecs.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("mimeType", profile.codecs[i].mimeType);
        settings.setValue("clockRate", profile.codecs[i].clockRate);
        settings.setValue("channels", profile.codecs[i].channels);
        settings.setValue("enabled", profile.codecs[i].enabled);
    }
    settings.endArray();
    settings.sync();

    // Password goes to the Credential Manager, never here.
    CredentialStore::savePassword(QString::fromLatin1(CredentialStore::kTarget),
                                   profile.username, profile.password);
}

bool SettingsStore::loadProfile(AccountProfile *profileOut) {
    QSettings settings;
    const QString username = settings.value(kKeyUsername).toString();
    const QString domain = settings.value(kKeyDomain).toString();
    qInfo().noquote() << "[settings] lendo de" << settings.fileName()
                       << "| usuario:" << (username.isEmpty() ? QStringLiteral("(vazio)") : username)
                       << "| servidor:" << (domain.isEmpty() ? QStringLiteral("(vazio)") : domain);
    if (username.isEmpty() || domain.isEmpty()) {
        return false; // nothing usable saved yet
    }

    AccountProfile profile;
    profile.displayName = settings.value(kKeyDisplayName).toString();
    profile.username = username;
    profile.domain = domain;
    profile.transport = settings.value(kKeyTransport, QStringLiteral("UDP")).toString();
    profile.dtmfMethod = settings.value(kKeyDtmfMethod, QStringLiteral("rfc2833")).toString();
    profile.contactsUrl = settings.value(kKeyContactsUrl).toString();

    const int codecCount = settings.beginReadArray(kArrayCodecs);
    for (int i = 0; i < codecCount; ++i) {
        settings.setArrayIndex(i);
        CodecInfo codec;
        codec.mimeType = settings.value("mimeType").toString();
        codec.clockRate = settings.value("clockRate").toInt();
        codec.channels = settings.value("channels", 1).toInt();
        codec.enabled = settings.value("enabled").toBool();
        profile.codecs.append(codec);
    }
    settings.endArray();

    CredentialStore::loadPassword(QString::fromLatin1(CredentialStore::kTarget), &profile.password);

    if (profileOut != nullptr) {
        *profileOut = profile;
    }
    return true;
}

void SettingsStore::clearProfile() {
    QSettings settings;
    settings.clear();
    settings.sync();
    CredentialStore::removePassword(QString::fromLatin1(CredentialStore::kTarget));
}

void SettingsStore::saveVolumes(int speakerPercent, int micPercent) {
    QSettings settings;
    settings.setValue(kKeySpeakerVolume, speakerPercent);
    settings.setValue(kKeyMicVolume, micPercent);
}

void SettingsStore::saveAudioRouting(const AudioRouting &routing) {
    QSettings settings;
    settings.setValue("audio/captureDevice", routing.captureId);
    settings.setValue("audio/playbackDevice", routing.playbackId);
    settings.setValue("audio/ringerDevice", routing.ringerId);
    settings.setValue("audio/ringtonePath", routing.ringtonePath);
    settings.sync();
    // Logged because an audio device that silently fails to persist looks
    // exactly like one that was never chosen — and the difference matters when
    // the user has to re-pick it on every launch.
    qInfo().noquote() << "[settings] roteamento de áudio salvo em" << settings.fileName()
                      << "| status:" << (settings.status() == QSettings::NoError ? "ok" : "ERRO")
                      << "| entrada:" << (routing.captureId.isEmpty() ? QStringLiteral("(padrão)") : routing.captureId)
                      << "| saída:" << (routing.playbackId.isEmpty() ? QStringLiteral("(padrão)") : routing.playbackId)
                      << "| toque:" << (routing.ringerId.isEmpty() ? QStringLiteral("(padrão)") : routing.ringerId);
}

SettingsStore::AudioRouting SettingsStore::loadAudioRouting() {
    QSettings settings;
    AudioRouting routing;
    routing.captureId = settings.value("audio/captureDevice").toString();
    routing.playbackId = settings.value("audio/playbackDevice").toString();
    routing.ringerId = settings.value("audio/ringerDevice").toString();
    routing.ringtonePath = settings.value("audio/ringtonePath").toString();
    return routing;
}

void SettingsStore::saveCallHandling(bool doNotDisturb, const QString &forwardTarget) {
    QSettings settings;
    settings.setValue("calls/doNotDisturb", doNotDisturb);
    settings.setValue("calls/forwardTarget", forwardTarget);
}

void SettingsStore::loadCallHandling(bool *doNotDisturb, QString *forwardTarget) {
    QSettings settings;
    if (doNotDisturb != nullptr) {
        *doNotDisturb = settings.value("calls/doNotDisturb", false).toBool();
    }
    if (forwardTarget != nullptr) {
        *forwardTarget = settings.value("calls/forwardTarget").toString();
    }
}

void SettingsStore::saveReplaceLocalContacts(bool enabled) {
    QSettings settings;
    settings.setValue("contacts/replaceLocal", enabled);
}

bool SettingsStore::loadReplaceLocalContacts() {
    QSettings settings;
    return settings.value("contacts/replaceLocal", false).toBool();
}

void SettingsStore::loadVolumes(int *speakerPercent, int *micPercent) {
    QSettings settings;
    if (speakerPercent != nullptr) {
        *speakerPercent = settings.value(kKeySpeakerVolume, *speakerPercent).toInt();
    }
    if (micPercent != nullptr) {
        *micPercent = settings.value(kKeyMicVolume, *micPercent).toInt();
    }
}

void SettingsStore::saveIncomingCallBehavior(IncomingCallBehavior behavior) {
    QSettings settings;
    settings.setValue("calls/incomingBehavior",
                       behavior == IncomingCallBehavior::BringToFront ? "front" : "notify");
}

SettingsStore::IncomingCallBehavior SettingsStore::loadIncomingCallBehavior() {
    QSettings settings;
    return settings.value("calls/incomingBehavior").toString() == QLatin1String("front")
               ? IncomingCallBehavior::BringToFront
               : IncomingCallBehavior::Notify;
}

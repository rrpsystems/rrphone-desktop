#pragma once

#include <QDialog>
#include <QList>

#include "profile/ProfileStore.h"
#include "profile/SettingsStore.h"
#include "core/CodecInfo.h"

#include <QPair>

class QLineEdit;
class QComboBox;
class QLabel;
class QListWidget;
class QCheckBox;

// D-01 (account), D-14 (codecs), D-15 (DTMF method), D-16 (contacts URL),
// D-13 (profile import/export) — one settings screen for all of the basic
// configuration the PRD calls for.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Called once at startup (and after import) with the engine's current
    // codec list, so the dialog has something to show/reorder.
    void setAvailableCodecs(const QList<CodecInfo> &codecs);

    void setProfile(const AccountProfile &profile);
    AccountProfile currentProfile() const;

    // Device lists are (id, name) pairs already filtered by capability; the
    // ringer picks from the playback list.
    using DeviceList = QList<QPair<QString, QString>>;
    void setAudioDevices(const DeviceList &captureDevices, const DeviceList &playbackDevices,
                          const SettingsStore::AudioRouting &current);
    SettingsStore::AudioRouting audioRouting() const;

    void setAudioProcessing(const SettingsStore::AudioProcessing &processing);
    SettingsStore::AudioProcessing audioProcessing() const;

    void setCallForwardTarget(const QString &target);
    QString callForwardTarget() const;

    void setIncomingCallBehavior(SettingsStore::IncomingCallBehavior behavior);
    SettingsStore::IncomingCallBehavior incomingCallBehavior() const;

    void setReplaceLocalContacts(bool enabled);
    bool replaceLocalContacts() const;

    // Feedback for the "Testar som" button.
    void setAudioTestResult(const QString &message);

signals:
    // Emitted when the user clicks "Salvar" — MainWindow applies all of it
    // (account, codecs, DTMF method, contacts URL) to the running SipCoreManager.
    void settingsApplied(const AccountProfile &profile);
    // Emitted after a successful import — same handling as settingsApplied,
    // MainWindow should apply it and this dialog's fields are already updated.
    void profileImported(const AccountProfile &profile);
    // "Testar som" was clicked — MainWindow applies the currently selected
    // devices to the engine and plays a sample through them.
    void audioTestRequested();
    // "Ouvir" — MainWindow applies the pending ringtone choice and plays it.
    void ringtonePreviewRequested();

private slots:
    void onSave();
    void onImportClicked();
    void onExportClicked();

private:
    void updateRingtoneLabel();

private:
    QListWidget *m_codecList;

    QLineEdit *m_displayNameEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_domainEdit;
    QComboBox *m_transportCombo;
    QComboBox *m_dtmfCombo;
    QLineEdit *m_contactsUrlEdit;

    QComboBox *m_captureDeviceCombo;
    QComboBox *m_playbackDeviceCombo;
    QComboBox *m_ringerDeviceCombo;
    QComboBox *m_incomingBehaviorCombo;
    QCheckBox *m_noiseSuppressionCheck;
    QCheckBox *m_echoCancellationCheck;
    QCheckBox *m_agcCheck;
    QLabel *m_audioTestLabel;
    QLabel *m_ringtoneLabel;
    QString m_ringtonePath; // empty = the ringtone shipped with the app
    QCheckBox *m_exportPassphraseCheck;
    QLineEdit *m_forwardTargetEdit;
    QCheckBox *m_replaceLocalContactsCheck;
};

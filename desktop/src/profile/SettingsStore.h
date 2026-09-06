#pragma once

#include "ProfileStore.h" // AccountProfile

// Persists the app configuration between runs: everything non-sensitive goes
// into QSettings (HKCU\Software\RRP Systems\RRPSoftphone on Windows), while
// the SIP password goes to the Credential Manager via CredentialStore.
//
// This is what makes the softphone usable day to day — without it the
// account has to be retyped on every launch.
class SettingsStore {
public:
    static void saveProfile(const AccountProfile &profile);
    // Returns false when nothing has been saved yet (fresh install), in
    // which case *profileOut is left untouched.
    static bool loadProfile(AccountProfile *profileOut);
    static void clearProfile();

    static void saveVolumes(int speakerPercent, int micPercent);
    static void loadVolumes(int *speakerPercent, int *micPercent);

    // Audio device ids are per-machine (a headset id means nothing on another
    // PC), so they are stored here and deliberately kept out of the
    // .rrpprofile that gets emailed to users.
    struct AudioRouting {
        QString captureId;
        QString playbackId;
        QString ringerId;
    };
    static void saveAudioRouting(const AudioRouting &routing);
    static AudioRouting loadAudioRouting();

    // Do-not-disturb and call forwarding survive restarts on purpose: a user
    // who forwarded their extension expects it to stay forwarded.
    static void saveCallHandling(bool doNotDisturb, const QString &forwardTarget);
    static void loadCallHandling(bool *doNotDisturb, QString *forwardTarget);

    // When true, a successful download of the provisioning XML wipes the
    // locally created contacts — for deployments where the server list is
    // meant to be the single source of truth.
    static void saveReplaceLocalContacts(bool enabled);
    static bool loadReplaceLocalContacts();
};

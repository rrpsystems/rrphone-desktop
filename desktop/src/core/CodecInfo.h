#pragma once

#include <QList>
#include <QString>

// D-14: one row in the codec configuration list (Settings > Audio).
// Identified by mimeType+clockRate+channels because that's what liblinphone
// uses to look the matching PayloadType back up (linphone_core_find_payload_type).
struct CodecInfo {
    QString mimeType;   // e.g. "opus", "PCMU", "PCMA", "G729"
    int clockRate = 0;
    int channels = 1;
    bool enabled = false;
};

namespace Codecs {

// The only codecs this softphone offers. liblinphone enables a dozen more
// (speex at three rates, GSM, BV16, two flavours of L16...) that no PBX in
// this deployment speaks; listing them just makes the settings screen and the
// SDP offer harder to read. Everything outside this list is hidden from the
// UI and removed from the payload types the engine negotiates.
//
// Order here is the default negotiation priority: narrowband first, because
// the carrier is G.711/G.729 and OPUS would only force transcoding on the
// PBX.
const QList<CodecInfo> &supported();

// Whether a codec reported by the engine is one we expose.
bool isSupported(const CodecInfo &codec);

// Resolves an SDP name ("PCMA", "G729", "opus") back to a full CodecInfo.
// Used on import, where the profile stores only the name — unambiguous,
// since each supported codec has exactly one clock rate/channel layout.
bool findByName(const QString &mimeType, CodecInfo *out);

} // namespace Codecs

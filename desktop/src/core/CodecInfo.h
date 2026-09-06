#pragma once

#include <QString>

// D-14: one row in the codec configuration list (Settings > Audio).
// Identified by mimeType+clockRate+channels because that's what liblinphone
// uses to look the matching PayloadType back up (linphone_core_find_payload_type).
struct CodecInfo {
    QString mimeType;   // e.g. "opus", "PCMU", "PCMA", "G722", "GSM"
    int clockRate = 0;
    int channels = 1;
    bool enabled = false;
};

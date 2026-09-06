#include "CodecInfo.h"

namespace {
CodecInfo makeCodec(const char *mime, int clockRate, int channels) {
    CodecInfo codec;
    codec.mimeType = QString::fromLatin1(mime);
    codec.clockRate = clockRate;
    codec.channels = channels;
    codec.enabled = true;
    return codec;
}
} // namespace

const QList<CodecInfo> &Codecs::supported() {
    static const QList<CodecInfo> kSupported = {
        makeCodec("PCMA", 8000, 1),  // G.711 a-law — the carrier's default
        makeCodec("PCMU", 8000, 1),  // G.711 µ-law
        makeCodec("G729", 8000, 1),  // saves bandwidth without transcoding
        makeCodec("opus", 48000, 2), // wideband, for internal extension-to-extension calls
    };
    return kSupported;
}

bool Codecs::isSupported(const CodecInfo &codec) {
    for (const CodecInfo &known : supported()) {
        if (known.mimeType.compare(codec.mimeType, Qt::CaseInsensitive) == 0 &&
            known.clockRate == codec.clockRate && known.channels == codec.channels) {
            return true;
        }
    }
    return false;
}

bool Codecs::findByName(const QString &mimeType, CodecInfo *out) {
    for (const CodecInfo &known : supported()) {
        if (known.mimeType.compare(mimeType, Qt::CaseInsensitive) == 0) {
            if (out != nullptr) {
                *out = known;
            }
            return true;
        }
    }
    return false;
}

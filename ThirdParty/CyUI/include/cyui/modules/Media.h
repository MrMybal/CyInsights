#pragma once
#include "cyui/modules/Images.h"
namespace cyui {
struct MediaSnapshot {
    ImageData frame;
    double position = 0, duration = 0;
    float volume = 1;
    bool playing = false, muted = false, seekable = false, ready = false;
    std::string title, error;
};
// Optional host engine. UI-thread calls only; snapshot() must not block.
// No browser, codec, file access or audio device is created by this interface.
class MediaBackend {
  public:
    virtual ~MediaBackend() = default;
    virtual MediaSnapshot snapshot() const = 0;
    virtual void play(bool) = 0;
    virtual void seek(double seconds) = 0;
    virtual void volume(float) = 0;
    virtual void mute(bool) = 0;
};
struct MediaPlayerState {
    float seek = 0;
    bool seeking = false;
};
// Video frame and controls are native. A backend without a frame supplies an audio player.
CYUI_API void mediaPlayer(const char *id, MediaPlayerState &, MediaBackend &, ImVec2 size = {});
} // namespace cyui

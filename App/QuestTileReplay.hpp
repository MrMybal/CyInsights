// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "../Core/OvrRecording.hpp"
#include <d3d11.h>
#include <future>
#include <imgui.h>
#include <memory>
namespace cyinsights {
namespace fs = std::filesystem;
void SetQuestImageDevice(ID3D11Device *device);
class QuestTileReplay {
  public:
    ~QuestTileReplay();
    void Open(const std::filesystem::path &directory);
    void Clear();
    void Clock(const std::filesystem::path &cache);
    bool Empty() const { return !recording_ || recording_->samples.empty(); }
    bool Present() const { return recording_ || !error_.empty(); }
    bool Track(float width, double start, double end);
    void Panel(double selection_start, double selection_end);
    const std::string &Error() const { return error_; }

  private:
    struct Detail {
        std::shared_ptr<cyovr::Capture> capture;
        std::vector<unsigned char> pixels;
        unsigned width = 0, height = 0;
        bool black_image = false;
        std::string error;
        size_t index = 0;
    };
    void Select(size_t index);
    void Poll();
    std::shared_ptr<cyovr::Recording> recording_;
    std::future<Detail> pending_;
    std::shared_ptr<cyovr::Capture> capture_;
    size_t requested_ = 0, selected_ = 0, loaded_ = SIZE_MAX;
    double clock_retry_ = 0;
    ID3D11ShaderResourceView *image_ = nullptr;
    unsigned image_width_ = 0, image_height_ = 0;
    std::string error_;
    int surface_ = 0, tile_ = -1, stage_ = 0;
    float threshold_ = 500, factor_ = 2.5f, opacity_ = .35f;
    bool overlay_ = false, image_black_ = false, show_black_ = false;
};
} // namespace cyinsights

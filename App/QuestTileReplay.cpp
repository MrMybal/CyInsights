// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "QuestTileReplay.hpp"
#include "OvrHeatmapView.hpp"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <wincodec.h>
#include <wrl/client.h>
namespace cyinsights {
namespace {
ID3D11Device *imageDevice = nullptr;
using Microsoft::WRL::ComPtr;
void Decode(const fs::path &file, std::vector<unsigned char> &pixels, unsigned &width, unsigned &height) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct Cleanup {
        bool own;
        ~Cleanup() {
            if (own)
                CoUninitialize();
        }
    } cleanup{SUCCEEDED(init)};
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr))
        hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || !width || !height || width > 4096 || height > 4096)
        throw std::runtime_error("Cannot decode recorded Quest view");
    pixels.resize(size_t(width) * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, UINT(pixels.size()), pixels.data())))
        throw std::runtime_error("Cannot read recorded image pixels");
}
std::string CostLabel(double value) {
    if (!std::isfinite(value))
        return "unavailable";
    char text[48];
    std::snprintf(text, sizeof(text), "%.1f us", value);
    return text;
}
double Time(const cyovr::Recording &r, const cyovr::Observation &s) {
    return r.aligned
               ? r.native_seconds + (s.mono >= r.anchor ? double(s.mono - r.anchor) : -double(r.anchor - s.mono)) * 1e-9
               : s.relative;
}
} // namespace
void SetQuestImageDevice(ID3D11Device *device) { imageDevice = device; }
QuestTileReplay::~QuestTileReplay() {
    if (image_)
        image_->Release();
}
void QuestTileReplay::Clear() {
    if (pending_.valid()) {
        pending_.wait();
        pending_ = std::future<Detail>();
    }
    recording_.reset();
    capture_.reset();
    if (image_) {
        image_->Release();
        image_ = nullptr;
    }
    loaded_ = SIZE_MAX;
    clock_retry_ = 0;
    error_.clear();
}
void QuestTileReplay::Open(const fs::path &directory) {
    Clear();
    try {
        recording_ = std::make_shared<cyovr::Recording>(cyovr::ReadRecording(directory));
        if (!Empty())
            Select(0);
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void QuestTileReplay::Clock(const fs::path &cache) {
    if (recording_ && !recording_->aligned && ImGui::GetTime() >= clock_retry_) {
        clock_retry_ = ImGui::GetTime() + .5;
        cyovr::AlignRecording(*recording_, cache / "quest-tiles-clock.json");
    }
}
void QuestTileReplay::Select(size_t index) {
    if (Empty())
        return;
    selected_ = std::min(index, recording_->samples.size() - 1);
    requested_ = selected_;
    if (pending_.valid())
        return;
    auto r = recording_;
    const auto selected = selected_;
    pending_ = std::async(std::launch::async, [r, selected] {
        Detail out;
        out.index = selected;
        try {
            const auto &observation = r->samples.at(selected);
            auto value = cyovr::JsonFile(r->directory / fs::u8path(observation.path));
            out.capture = std::make_shared<cyovr::Capture>(cyovr::ParseCapture(value));
            if (out.capture->package != r->package || out.capture->boot_id != r->boot ||
                cyovr::Tick(value["observed_mono_ns"]) != observation.mono)
                throw std::runtime_error("Tile snapshot identity does not match its recording");
            if (!observation.image.empty()) {
                Decode(r->directory / fs::u8path(observation.image), out.pixels, out.width, out.height);
                out.black_image=!out.pixels.empty();
                for(size_t i=0;i+3<out.pixels.size();i+=4)if(out.pixels[i]||out.pixels[i+1]||out.pixels[i+2]){out.black_image=false;break;}
            }
        } catch (const std::exception &e) {
            out.error = e.what();
        }
        return out;
    });
}
void QuestTileReplay::Poll() {
    if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    auto detail = pending_.get();
    if (detail.index != requested_) {
        Select(requested_);
        return;
    }
    capture_ = std::move(detail.capture);
    image_black_ = detail.black_image;
    error_ = detail.error;
    loaded_ = detail.index;
    surface_ = capture_ ? cyovr::LargestSurface(*capture_) : 0;
    tile_ = -1;
    if (image_) {
        image_->Release();
        image_ = nullptr;
    }
    if (imageDevice && !detail.pixels.empty()) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = detail.width;
        d.Height = detail.height;
        d.MipLevels = d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_IMMUTABLE;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{detail.pixels.data(), detail.width * 4, 0};
        ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(imageDevice->CreateTexture2D(&d, &pixels, &texture)) &&
            SUCCEEDED(imageDevice->CreateShaderResourceView(texture.Get(), nullptr, &image_))) {
            image_width_ = detail.width;
            image_height_ = detail.height;
        } else
            error_ = "Cannot upload recorded image";
    }
}
bool QuestTileReplay::Track(float width, double start, double end) {
    if (Empty() || !recording_->aligned)
        return false;
    Poll();
    const auto &r = *recording_;
    const auto &p = cyui::palette();
    ImGui::TextUnformatted("Quest tile snapshots / observation time");
    const auto origin = ImGui::GetCursorScreenPos();
    float h = cyui::dp(22);
    ImGui::InvisibleButton("##quest_tile_track", ImVec2(width, h));
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + h), p.bg1);
    std::vector<int> buckets(size_t(std::max(1.f, width)), -1);
    auto first = std::lower_bound(r.samples.begin(), r.samples.end(), start,
                                  [&](const auto &s, double t) { return Time(r, s) < t; });
    for (auto i = first; i != r.samples.end() && Time(r, *i) <= end; ++i) {
        int x =
            std::clamp(int((Time(r, *i) - start) / std::max(1e-9, end - start) * width), 0, int(buckets.size() - 1));
        int n = int(i - r.samples.begin());
        if (buckets[x] < 0 ||
            (std::isfinite(i->max_render_us) && (!std::isfinite(r.samples[buckets[x]].max_render_us) ||
                                                 i->max_render_us > r.samples[buckets[x]].max_render_us)))
            buckets[x] = n;
    }
    int hit = -1;
    for (size_t x = 0; x < buckets.size(); ++x)
        if (buckets[x] >= 0) {
            int i = buckets[x];
            auto color = !std::isfinite(r.samples[i].max_render_us) ? p.bg3
                         : r.samples[i].max_render_us >= threshold_ ? p.error
                                                                    : p.accent;
            draw->AddRectFilled(ImVec2(origin.x + float(x), origin.y + 2),
                                ImVec2(origin.x + float(x) + cyui::dp(3), origin.y + h - 2), color);
            if (ImGui::IsItemHovered() && std::abs(ImGui::GetIO().MousePos.x - origin.x - float(x)) < cyui::dp(5))
                hit = i;
        }
    if (hit >= 0) {
        ImGui::SetTooltip("Snapshot %d | max Render %s | %s", hit + 1, CostLabel(r.samples[hit].max_render_us).c_str(),
                          r.samples[hit].image.empty() ? "no associated image" : "shared image available");
        if (ImGui::IsMouseClicked(0)) {
            Select(size_t(hit));
            return true;
        }
    }
    return false;
}
void QuestTileReplay::Panel(double a, double b) {
    Poll();
    if (Empty()) {
        cyui::emptyState("No Quest tile recording",
                         error_.empty() ? "Enable Quest tile snapshots in capture options." : error_.c_str());
        return;
    }
    auto &r = *recording_;
    cyui::heading("Quest tile replay", 2);
    cyui::textDim("%zu samples | %s", r.samples.size(), r.package.c_str());
    if (!r.aligned)
        cyui::callout(cyui::Tone::warning,
                      "CPU timeline alignment is unavailable. Positions below are relative observation times.");
    int sample = int(selected_);
    ImGui::SetNextItemWidth(cyui::dp(300));
    if (ImGui::SliderInt("Snapshot", &sample, 0, int(r.samples.size() - 1)))
        Select(size_t(sample));
    ImGui::SameLine();
    if (ImGui::Button("Previous") && selected_)
        Select(selected_ - 1);
    ImGui::SameLine();
    if (ImGui::Button("Next") && selected_ + 1 < r.samples.size())
        Select(selected_ + 1);
    size_t count = 0, hot = 0;
    size_t peak = selected_;
    double maximum = -1;
    for (size_t i = 0; i < r.samples.size(); ++i) {
        double t = Time(r, r.samples[i]);
        if (r.aligned && a != b && (t < std::min(a, b) || t > std::max(a, b)))
            continue;
        ++count;
        if (r.samples[i].max_render_us >= threshold_)
            ++hot;
        if (r.samples[i].max_render_us > maximum) {
            maximum = r.samples[i].max_render_us;
            peak = i;
        }
    }
    ImGui::SetNextItemWidth(cyui::dp(130));
    ImGui::DragFloat("Tile alert threshold (us)", &threshold_, 10, 1, 100000, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(cyui::dp(100));
    ImGui::DragFloat("Median factor", &factor_, .1f, 1.1f, 20, "%.1fx");
    cyui::textDim("Timeline range: %zu samples | %zu above threshold | peak %s", count, hot,
                  CostLabel(maximum < 0 ? NAN : maximum).c_str());
    if (maximum >= 0 && ImGui::Button("Inspect range peak"))
        Select(peak);
    if (loaded_ != selected_) {
        cyui::progressBar(-1, ImVec2(-1, cyui::dp(16)), "Loading image and tile data...");
        return;
    }
    if (!error_.empty())
        cyui::callout(cyui::Tone::warning, error_.c_str());
    const auto &observation = r.samples[selected_];
    cyui::textMono("Observation %.6f s | max Render %s", Time(r, observation),
                   CostLabel(observation.max_render_us).c_str());
    bool split=image_&&capture_&&!overlay_&&ImGui::GetContentRegionAvail().x>cyui::dp(850)&&ImGui::BeginTable("##quest_visuals",2,ImGuiTableFlags_SizingStretchSame);
    if(split)ImGui::TableNextColumn();
    if (image_) {
        bool black=observation.black_image||image_black_;
        if(black){cyui::callout(cyui::Tone::warning,"The received image contains only black RGB pixels. Game-scene visibility is unverified.");ImGui::Checkbox("Show black image",&show_black_);}
        ImGui::Checkbox("Approximate grid over image", &overlay_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(cyui::dp(100));
        ImGui::SliderFloat("Grid opacity", &opacity_, .05f, .9f);
        cyui::textDim("Image observation offset: %+.1f ms | composited view; eye/frame correspondence unknown",
                      observation.image_delta_ms);
        if (!overlay_&&(!black||show_black_)) {
            float w = std::min(ImGui::GetContentRegionAvail().x, cyui::dp(650));
            ImGui::Image((ImTextureID)image_, ImVec2(w, w * image_height_ / image_width_));
        }
    } else
        cyui::textDim("No associated image was received for this snapshot.");
    if(split)ImGui::TableNextColumn();
    if (capture_) {
        cyovr::Heatmap(*capture_, surface_, tile_, stage_, overlay_ ? (ImTextureID)image_ : 0, opacity_);
        if (!capture_->surfaces.empty()) {
            const auto &s = capture_->surfaces[size_t(surface_)];
            auto stats = cyovr::AnalyzeTiles(s, "Render", threshold_, factor_);
            cyui::textDim("Render: median %.1f us | p95 %.1f us | max %.1f us | %zu alert tiles / %zu measured",
                          stats.median, stats.p95, stats.maximum, stats.hot, stats.measured);
            if (ImGui::BeginTable("##hot_tiles", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (const char *label : {"Tile", "Render us", "vs median", "Area px"})
                    ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for (size_t row = 0; row < std::min(size_t(20), stats.descending.size()); ++row) {
                    size_t i = stats.descending[row];
                    const auto &t = s.tiles[i];
                    double cost = t.Cost("Render");
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    std::string label = std::to_string(t.id) + "##" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), tile_ == int(i), ImGuiSelectableFlags_SpanAllColumns))
                        tile_ = int(i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", cost);
                    ImGui::TableNextColumn();
                    if (stats.median > 0)
                        ImGui::Text("%.2fx", cost / stats.median);
                    else
                        ImGui::TextUnformatted("--");
                    ImGui::TableNextColumn();
                    ImGui::Text("%u x %u", t.width, t.height);
                }
                ImGui::EndTable();
            }
        }
    }
    if(split)ImGui::EndTable();
    cyui::textFaint("Alerts compare measured tile Render costs against your threshold or the surface median. They do "
                    "not identify a shader or mesh automatically.");
}
} // namespace cyinsights

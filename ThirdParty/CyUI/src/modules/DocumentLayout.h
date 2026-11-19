#pragma once
#include "cyui/modules/DocumentViews.h"
#include <cctype>
#include <cfloat>

namespace cyui {
namespace {
bool hitDocument(Rect r, ImVec2 p) {
    return p.x >= r.min.x && p.y >= r.min.y && p.x < r.max.x && p.y < r.max.y;
}
std::size_t nextDocumentChar(const std::string &s, std::size_t at) {
    if (at < s.size())
        ++at;
    while (at < s.size() && (static_cast<unsigned char>(s[at]) & 0xc0) == 0x80)
        ++at;
    return at;
}
struct DocumentRenderer {
    DocumentState &s;
    const Document &doc;
    const DocumentOptions &o;
    DocumentResult result;
    bool hovered() const { return ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows); }
    void media(const DocumentInline &r) {
        const auto *services = o.services;
        const auto kind = documentMediaKind(r.target);
        if (kind == DocumentMediaKind::image) {
            const auto image = services && services->image ? services->image(r.target) : ImageData{};
            if (!image.texture || image.width <= 0 || image.height <= 0) {
                callout(Tone::info, tr("Image unavailable").c_str());
                textDim("%s", r.text.empty() ? r.target.c_str() : r.text.c_str());
                return;
            }
            const ItemId imageId = ImGui::GetID(r.target.c_str());
            float w = std::min(ImGui::GetContentRegionAvail().x, s.resizingImage == imageId ? s.resizingWidth
                                                                 : r.width > 0              ? dp(r.width)
                                                                               : float(image.width)),
                  h = w * float(image.height) / float(image.width);
            const float limit = r.height > 0 ? dp(r.height) : dp(o.imageHeight);
            if (h > limit) {
                w *= limit / h;
                h = limit;
            }
            w = std::max(dp(1), w);
            h = std::max(dp(1), h);
            const auto p = ImGui::GetCursorScreenPos();
            const bool activated = ImGui::InvisibleButton("##image", {w, h}, ImGuiButtonFlags_EnableNav);
            ImGui::GetWindowDrawList()->AddImage(image.texture, p, {p.x + w, p.y + h}, image.uvMin,
                                                 image.uvMax);
            if (o.imageZoom && activated) {
                s.enlargedImage = r.target;
                s.enlargedCaption = r.text;
                s.imageView = {};
            }
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (o.resizeImages) {
                const auto cursor = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos({p.x + w - dp(24), p.y + h});
                ImGui::InvisibleButton("##resizeImage", dp(24, 8));
                const auto grip = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddLine({grip.x + dp(8), grip.y + dp(3)},
                                                    {grip.x + dp(22), grip.y + dp(3)}, palette().textFaint,
                                                    dp(2));
                if (ImGui::IsItemActivated()) {
                    s.resizingImage = imageId;
                    s.resizingWidth = w;
                }
                if (ImGui::IsItemActive()) {
                    s.resizingWidth = std::clamp(s.resizingWidth + ImGui::GetIO().MouseDelta.x, dp(24),
                                                 std::max(dp(24), ImGui::GetWindowWidth() - dp(32)));
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
                if (s.resizingImage == imageId && ImGui::IsKeyPressed(ImGuiKey_Escape))
                    s.resizingImage = 0;
                if (ImGui::IsItemDeactivatedAfterEdit() ||
                    (s.resizingImage == imageId && !ImGui::IsMouseDown(0))) {
                    result.resizedImage = r.target;
                    result.resizedWidth = s.resizingWidth / dp(1);
                    s.resizingImage = 0;
                }
                ImGui::SetCursorScreenPos({cursor.x, std::max(cursor.y, p.y + h + dp(10))});
            }
            if (!r.text.empty())
                textDim("%s", r.text.c_str());
            return;
        }
        const ItemId id = ImGui::GetID(r.target.c_str());
        if (s.activeEmbeds.count(id) && services && services->embed) {
            if (ghostButton(trLabel("Close content").c_str()))
                s.activeEmbeds.erase(id);
            else if (services->embed({id, kind, r.target, r.text, r.width, r.height},
                                     {ImGui::GetContentRegionAvail().x, dp(o.embedHeight)}))
                return;
            else
                s.activeEmbeds.erase(id);
        }
        ImGui::BeginGroup();
        heading(r.text.empty() ? r.target.c_str() : r.text.c_str(), 3);
        textDim("%s", tr(kind == DocumentMediaKind::video ? "Video" : "Audio").c_str());
        textFaint("%s", r.target.c_str());
        if (ghostButton(trLabel("Open content").c_str())) {
            result.media = r.target;
            if (services && services->embed)
                s.activeEmbeds.insert(id);
        }
        ImGui::EndGroup();
    }
    void text(const std::vector<DocumentInline> &runs, float scale = 1, int align = 0) {
        auto origin = ImGui::GetCursorScreenPos();
        const float right = origin.x + std::max(dp(1), ImGui::GetContentRegionAvail().x);
        float x = origin.x, y = origin.y;
        const float size = ImGui::GetFontSize() * scale, line = size + dp(4);
        bool any = false;
        auto *draw = ImGui::GetWindowDrawList();
        for (std::size_t n = 0; n < runs.size(); ++n) {
            const auto &r = runs[n];
            static const std::string space = " ";
            const auto &shownText = r.kind == DocumentInlineKind::softBreak && !o.breaks ? space : r.text;
            ScopedID rid{int(n)};
            if (r.kind == DocumentInlineKind::image) {
                if (any)
                    ImGui::Dummy({1, y - origin.y + line});
                media(r);
                origin = ImGui::GetCursorScreenPos();
                x = origin.x;
                y = origin.y;
                any = false;
                continue;
            }
            unsigned style = r.style;
            if (r.kind == DocumentInlineKind::code)
                style |= documentMono;
            if (r.kind == DocumentInlineKind::strong)
                style |= documentBold;
            if (r.kind == DocumentInlineKind::emphasis)
                style |= documentItalic;
            ImFont *font = style & (documentMono | documentKeyboard) ? monoFont()
                           : style & documentBold                    ? boldFont()
                                                                     : ImGui::GetFont();
            if (!font)
                font = ImGui::GetFont();
            const bool hidden = (style & documentSpoiler) && !s.spoilers.count(r.spoiler);
            if (r.spaced && x > origin.x)
                x += font->CalcTextSizeA(size, FLT_MAX, 0, " ").x;
            std::size_t at = 0;
            while (at < shownText.size()) {
                if (shownText[at] == '\n') {
                    x = origin.x;
                    y += line;
                    ++at;
                    any = true;
                    continue;
                }
                auto end = at;
                const bool white = std::isspace(static_cast<unsigned char>(shownText[at])) != 0;
                while (end < shownText.size() && shownText[end] != '\n' &&
                       (std::isspace(static_cast<unsigned char>(shownText[end])) != 0) == white)
                    end = nextDocumentChar(shownText, end);
                float width =
                    font->CalcTextSizeA(size, FLT_MAX, 0, shownText.data() + at, shownText.data() + end).x;
                if (x > origin.x && x + width > right) {
                    x = origin.x;
                    y += line;
                    if (white) {
                        at = end;
                        continue;
                    }
                }
                if (width > right - x) {
                    end = at;
                    width = 0;
                    while (end < shownText.size() && shownText[end] != '\n') {
                        const auto next = nextDocumentChar(shownText, end);
                        const float w = font->CalcTextSizeA(size, FLT_MAX, 0, shownText.data() + end,
                                                            shownText.data() + next)
                                            .x;
                        if (end > at && width + w > right - x)
                            break;
                        width += w;
                        end = next;
                    }
                }
                float sx = x;
                if (runs.size() == 1 && at == 0 && end == shownText.size()) {
                    if (align == 2)
                        sx += (right - x - width) / 2;
                    else if (align == 3)
                        sx = right - width;
                }
                const Rect bounds{{sx, y}, {sx + width, y + line}};
                const bool hit = hovered() && hitDocument(bounds, ImGui::GetIO().MousePos);
                if (style & documentHighlight)
                    draw->AddRectFilled(bounds.min, {bounds.max.x, y + size + dp(2)},
                                        alpha(palette().warning, .25f), dp(2));
                if (style & (documentMono | documentKeyboard))
                    draw->AddRectFilled(bounds.min, {bounds.max.x, y + size + dp(2)}, palette().bg3, dp(2));
                if (hidden) {
                    draw->AddRectFilled(bounds.min, {bounds.max.x, y + size + dp(2)}, palette().bg3, dp(3));
                    if (hit) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                } else {
                    const auto color = !r.target.empty()      ? palette().accent
                                       : style & documentMono ? palette().codeString
                                                              : palette().text;
                    const int first = draw->VtxBuffer.Size;
                    draw->AddText(font, size, {sx, y}, color, shownText.data() + at, shownText.data() + end);
                    if (style & documentItalic)
                        for (int v = first; v < draw->VtxBuffer.Size; ++v)
                            draw->VtxBuffer[v].pos.x += (y + size - draw->VtxBuffer[v].pos.y) * .14f;
                    if (style & documentUnderline || !r.target.empty())
                        draw->AddLine({sx, y + size}, {sx + width, y + size}, color, dp(1));
                    if (style & documentStrike)
                        draw->AddLine({sx, y + size * .55f}, {sx + width, y + size * .55f}, color, dp(1));
                    s.hits.push_back({bounds, shownText.substr(at, end - at),
                                      r.sourceBegin + std::min(at, r.sourceEnd - r.sourceBegin),
                                      r.sourceBegin + std::min(end, r.sourceEnd - r.sourceBegin)});
                }
                if ((hidden || !r.target.empty()) && !white) {
                    ScopedID wordId{int(at)};
                    const auto saved = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(bounds.min);
                    const bool activate = ImGui::InvisibleButton(
                        "##documentAction", {std::max(dp(1), width), line}, ImGuiButtonFlags_EnableNav);
                    if (ImGui::IsItemHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    const auto drag = ImGui::GetMouseDragDelta(0);
                    if (activate && drag.x * drag.x + drag.y * drag.y < dp(4) * dp(4)) {
                        if (hidden)
                            s.spoilers.insert(r.spoiler);
                        else if (r.target[0] == '#')
                            s.jumpTo = r.target.substr(1);
                        else
                            result.link = r.target;
                    }
                    ImGui::SetCursorScreenPos(saved);
                }
                x += width;
                at = end;
                any = true;
                if (at < shownText.size() && x >= right) {
                    x = origin.x;
                    y += line;
                }
            }
        }
        if (any)
            ImGui::Dummy({std::max(1.f, x - origin.x), y - origin.y + line});
    }
    void content() {
        s.codeViews.resize(doc.blocks().size());
        s.hits.clear();
        s.anchors.clear();
        for (std::size_t n = 0; n < doc.blocks().size(); ++n) {
            const auto &b = doc.blocks()[n];
            ScopedID block{int(n)};
            if (!b.anchor.empty())
                s.anchors[b.anchor] = ImGui::GetCursorPosY();
            if (o.scrollToSource != std::string::npos && o.scrollToSource >= b.sourceBegin &&
                o.scrollToSource <= b.sourceEnd)
                ImGui::SetScrollHereY(0);
            const float indent = dp(float(std::max(0, b.indent - 1)) * 18 + float(b.quoteDepth) * 12);
            if (indent > 0)
                ImGui::Indent(indent);
            const auto start = ImGui::GetCursorScreenPos();
            switch (b.kind) {
            case DocumentBlockKind::heading:
                text(b.inlines, b.level == 1 ? 1.5f : b.level == 2 ? 1.25f : 1.1f);
                break;
            case DocumentBlockKind::rule:
                divider();
                break;
            case DocumentBlockKind::list:
                if (b.task) {
                    bool value = b.checked;
                    ImGui::BeginDisabled(!o.tasks);
                    if (ImGui::Checkbox("##task", &value)) {
                        result.taskOffset = b.taskOffset;
                        result.taskChecked = value;
                    }
                    ImGui::EndDisabled();
                } else if (b.number)
                    ImGui::Text("%u.", b.number);
                else
                    ImGui::Bullet();
                ImGui::SameLine();
                ImGui::BeginGroup();
                text(b.inlines);
                ImGui::EndGroup();
                break;
            case DocumentBlockKind::code:
                textFaint("%s", b.language.c_str());
                ImGui::SameLine();
                if (iconButton(icon::copy, tr("Copy code").c_str())) {
                    ImGui::SetClipboardText(b.text.c_str());
                    result.copiedBlock = int(n);
                }
                s.codeViews[n].searchBar = s.codeViews[n].lineNumbers = false;
                codeViewer("##block", s.codeViews[n], b.code,
                           {0, dp(std::clamp(float(b.code.lines().size()) * 20 + 16, 48.f, 320.f))});
                break;
            case DocumentBlockKind::table: {
                const auto columns = std::min<std::size_t>(32, b.alignments.size());
                if (columns &&
                    ImGui::BeginTable("##table", int(columns),
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame)) {
                    for (std::size_t row = 0; row < b.cells.size(); ++row) {
                        ScopedID ri{int(row)};
                        ImGui::TableNextRow(row ? 0 : ImGuiTableRowFlags_Headers);
                        for (std::size_t col = 0; col < std::min(columns, b.cells[row].size()); ++col) {
                            ScopedID ci{int(col)};
                            ImGui::TableNextColumn();
                            text(b.cells[row][col], 1, b.alignments[col]);
                        }
                    }
                    ImGui::EndTable();
                }
                break;
            }
            default:
                text(b.inlines);
                break;
            }
            if (b.quoteDepth)
                ImGui::GetWindowDrawList()->AddLine({start.x - dp(7), start.y},
                                                    {start.x - dp(7), ImGui::GetCursorScreenPos().y - dp(3)},
                                                    palette().accent, dp(2));
            if (o.linkPreviews && o.services && o.services->preview) {
                int cards = 0;
                for (const auto &r : b.inlines)
                    if (r.kind == DocumentInlineKind::link && r.text == r.target &&
                        (r.target.rfind("http://", 0) == 0 || r.target.rfind("https://", 0) == 0) &&
                        cards < 5 && (!r.sourceBegin || doc.markdown()[r.sourceBegin - 1] != '<')) {
                        const auto p = o.services->preview(r.target);
                        if (p.title.empty())
                            continue;
                        ScopedID card{++cards};
                        ImGui::BeginGroup();
                        textFaint("%s", p.site.c_str());
                        if (linkText(p.title.c_str()))
                            result.link = r.target;
                        if (!p.description.empty())
                            ImGui::TextWrapped("%s", p.description.c_str());
                        if (p.image.texture && p.image.width > 0 && p.image.height > 0) {
                            const float w = std::min(dp(280), ImGui::GetContentRegionAvail().x);
                            ImGui::Image(p.image.texture, {w, w * p.image.height / p.image.width});
                        }
                        if (!p.embedSource.empty()) {
                            const ItemId id = ImGui::GetID(p.embedSource.c_str());
                            if (ghostButton(trLabel("Open content").c_str())) {
                                s.activeEmbeds.insert(id);
                                result.media = p.embedSource;
                            }
                            if (s.activeEmbeds.count(id) && o.services->embed &&
                                !o.services->embed({id, p.kind, p.embedSource, p.title},
                                                   {ImGui::GetContentRegionAvail().x, dp(o.embedHeight)}))
                                s.activeEmbeds.erase(id);
                        }
                        ImGui::EndGroup();
                    }
            }
            if (indent > 0)
                ImGui::Unindent(indent);
            space(5);
        }
    }
    void selection() {
        if (!o.selection)
            return;
        auto &io = ImGui::GetIO();
        const DocumentHit *near = nullptr;
        float best = FLT_MAX;
        for (const auto &h : s.hits) {
            const float dx = std::max({h.bounds.min.x - io.MousePos.x, 0.f, io.MousePos.x - h.bounds.max.x}),
                        dy = std::max({h.bounds.min.y - io.MousePos.y, 0.f, io.MousePos.y - h.bounds.max.y});
            const float d = dx * dx + 16 * dy * dy;
            if (d < best) {
                near = &h;
                best = d;
            }
        }
        std::size_t at = 0;
        if (near) {
            float width = 0;
            std::size_t index = 0;
            while (index < near->text.size()) {
                const auto next = nextDocumentChar(near->text, index);
                const float w = ImGui::CalcTextSize(near->text.data() + index, near->text.data() + next).x;
                if (io.MousePos.x < near->bounds.min.x + width + w * .5f)
                    break;
                width += w;
                index = next;
            }
            at = near->begin + std::min(index, near->end - near->begin);
        }
        if (hovered() && ImGui::IsMouseClicked(0) && near) {
            s.selectionBegin = s.selectionEnd = at;
            s.selecting = true;
        }
        if (s.selecting && ImGui::IsMouseDown(0) && near)
            s.selectionEnd = at;
        if (!ImGui::IsMouseDown(0))
            s.selecting = false;
        const auto low = std::min(s.selectionBegin, s.selectionEnd),
                   high = std::max(s.selectionBegin, s.selectionEnd);
        float previousY = -FLT_MAX;
        for (const auto &h : s.hits)
            if (h.end > low && h.begin < high) {
                const auto a = std::min(h.text.size(), low > h.begin ? low - h.begin : 0),
                           b = std::min(h.text.size(), high - h.begin);
                if (b <= a)
                    continue;
                const float x = ImGui::CalcTextSize(h.text.data(), h.text.data() + a).x,
                            w = ImGui::CalcTextSize(h.text.data() + a, h.text.data() + b).x;
                ImGui::GetWindowDrawList()->AddRectFilled({h.bounds.min.x + x, h.bounds.min.y},
                                                          {h.bounds.min.x + x + w, h.bounds.max.y},
                                                          alpha(palette().selection, .5f));
                if (previousY != h.bounds.min.y && !result.selection.empty())
                    result.selection += '\n';
                result.selection += h.text.substr(a, b - a);
                previousY = h.bounds.min.y;
            }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_C) && !result.selection.empty())
            ImGui::SetClipboardText(result.selection.c_str());
    }
};
void documentLightbox(DocumentState &s, const DocumentOptions &o) {
    if (s.enlargedImage.empty())
        return;
    ImGui::OpenPopup("##documentImage");
    const auto *v = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({v->WorkPos.x + v->WorkSize.x * .5f, v->WorkPos.y + v->WorkSize.y * .5f},
                            ImGuiCond_Always, {.5f, .5f});
    ImGui::SetNextWindowSize({v->WorkSize.x * .9f, v->WorkSize.y * .9f}, ImGuiCond_Always);
    bool open = true;
    if (ImGui::BeginPopupModal("##documentImage", &open, ImGuiWindowFlags_NoSavedSettings)) {
        if (iconButton(icon::close, tr("Close content").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        textDim("%s", s.enlargedCaption.c_str());
        const auto image = o.services && o.services->image ? o.services->image(s.enlargedImage) : ImageData{};
        imageViewport("##zoom", s.imageView, image, ImGui::GetContentRegionAvail());
        ImGui::EndPopup();
    }
    if (!open)
        s.enlargedImage.clear();
}
} // namespace
} // namespace cyui

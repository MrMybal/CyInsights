#include "cyui/modules/DocumentViews.h"
#include "DocumentLayout.h"
#include "ModuleInternal.h"

namespace cyui {
namespace {
// Discord uses double underscores for underline. Normalize that chat-only syntax while
// retaining the host's original Markdown for edit/copy. Code spans/fences stay untouched.
std::string chatMarkup(const std::string &source) {
    std::string out;
    for (std::size_t at = 0; at < source.size();) {
        if (source[at] == '\\' && at + 1 < source.size()) {
            out.append(source, at, 2);
            at += 2;
            continue;
        }
        if (source[at] == '`' ||
            (source[at] == '~' && (at == 0 || source[at - 1] == '\n') && source.compare(at, 3, "~~~") == 0)) {
            auto end = at + 1;
            while (end < source.size() && source[end] == source[at])
                ++end;
            const auto close = source.find(source.substr(at, end - at), end);
            end = close == std::string::npos ? source.size() : close + end - at;
            out.append(source, at, end - at);
            at = end;
            continue;
        }
        if (source.compare(at, 2, "__") == 0 && at + 2 < source.size() &&
            !std::isspace(static_cast<unsigned char>(source[at + 2]))) {
            auto end = source.find("__", at + 2);
            while (end != std::string::npos && end && source[end - 1] == '\\')
                end = source.find("__", end + 2);
            if (end != std::string::npos && end > at + 2 &&
                !std::isspace(static_cast<unsigned char>(source[end - 1]))) {
                out += "<u>";
                out.append(source, at + 2, end - at - 2);
                out += "</u>";
                at = end + 2;
                continue;
            }
        }
        out += source[at++];
    }
    return out;
}
double transcriptPrefix(const TranscriptState &s, std::size_t count) {
    double height = s.estimatedHeight * double(count);
    while (count) {
        const auto node = s.heightIndex.find(count);
        if (node != s.heightIndex.end())
            height += node->second;
        count -= count & (~count + 1);
    }
    return height;
}
void transcriptMeasure(TranscriptState &s, std::size_t row, double height) {
    const auto before = transcriptPrefix(s, row + 1) - transcriptPrefix(s, row);
    const auto delta = height - before;
    if (std::abs(delta) < .01)
        return;
    for (auto node = row + 1; node && node <= s.heightCapacity; node += node & (~node + 1))
        s.heightIndex[node] += delta;
}
std::size_t transcriptRow(const TranscriptState &s, std::size_t count, double position) {
    std::size_t row = 0;
    double height = 0;
    for (auto bit = s.heightCapacity; bit; bit >>= 1) {
        const auto next = row + bit;
        if (next > count)
            continue;
        const auto known = s.heightIndex.find(next);
        const auto step = s.estimatedHeight * double(next & (~next + 1)) +
                          (known == s.heightIndex.end() ? 0 : known->second);
        if (height + step <= position) {
            height += step;
            row = next;
        }
    }
    return count ? std::min(row, count - 1) : 0;
}
void transcriptPrune(TranscriptState &s, std::size_t limit, ItemId editing) {
    while (s.heights.size() > limit) {
        auto oldest = s.heights.end();
        for (auto row = s.heights.begin(); row != s.heights.end(); ++row) {
            const auto view = s.views.find(row->first);
            if (row->first == editing || row->second.used == s.frame ||
                (view != s.views.end() && !view->second.enlargedImage.empty()))
                continue;
            if (oldest == s.heights.end() || row->second.used < oldest->second.used)
                oldest = row;
        }
        if (oldest == s.heights.end())
            break;
        s.documents.erase(oldest->first);
        s.views.erase(oldest->first);
        s.heights.erase(oldest);
    }
}
} // namespace
DocumentResult documentView(const char *id, DocumentState &s, const Document &doc, ImVec2 size) {
    return documentView(id, s, doc, size, {});
}
DocumentResult documentView(const char *id, DocumentState &s, const Document &doc, ImVec2 size,
                            const DocumentOptions &options) {
    ScopedID scope{id};
    ImGui::BeginChild("##document", size);
    if (s.revision != doc.revision()) {
        s.spoilers.clear();
        s.activeEmbeds.clear();
        s.selectionBegin = s.selectionEnd = 0;
        s.selecting = false;
        s.resizingImage = 0;
        s.revision = doc.revision();
    }
    DocumentRenderer renderer{s, doc, options};
    renderer.content();
    renderer.selection();
    if (!s.jumpTo.empty()) {
        auto it = s.anchors.find(s.jumpTo);
        if (it != s.anchors.end())
            ImGui::SetScrollY(it->second);
        s.jumpTo.clear();
    }
    s.scroll = ImGui::GetScrollY();
    s.scrollMax = ImGui::GetScrollMaxY();
    ImGui::EndChild();
    documentLightbox(s, options);
    return renderer.result;
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size) {
    return transcript(id, s, messages, size, DocumentOptions{});
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size, const DocumentOptions &options) {
    TranscriptOptions chat;
    chat.document = options;
    return transcript(id, s, messages, size, chat);
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size, const TranscriptOptions &options) {
    TranscriptSource source;
    source.count = messages.count;
    source.revision = options.revision;
    source.message = [messages](std::size_t index) { return &messages[index]; };
    // ID lookup runs only for an explicit jump. Drawing never scans the backing array.
    source.indexOf = [messages](ItemId target) {
        for (std::size_t n = 0; n < messages.count; ++n)
            if (messages[n].id == target)
                return n;
        return std::string::npos;
    };
    return transcript(id, s, source, size, options);
}
TranscriptResult transcript(const char *id, TranscriptState &s, const TranscriptSource &source, ImVec2 size,
                            const TranscriptOptions &options) {
    ScopedID scope(id);
    TranscriptResult result;
    if (!ImGui::BeginChild("##transcript", size, 0,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::EndChild();
        s.fetched = 0;
        return result;
    }
    auto *window = ImGui::GetCurrentWindow();
    const auto origin = ImGui::GetCursorScreenPos(), available = ImGui::GetContentRegionAvail();
    const float bar = ImGui::GetStyle().ScrollbarSize;
    const float width = std::max(dp(1), available.x - bar), height = std::max(dp(1), available.y);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const bool bottom = s.scroll >= s.scrollMax - dp(8);
    const bool identity = source.id != s.sourceId;
    const bool reset = identity || source.revision != s.sourceRevision || source.count < s.count;
    const auto anchor = transcriptRow(s, s.count, s.scroll);
    const auto anchorOffset = s.scroll - transcriptPrefix(s, anchor);
    const ImVec4 layout{width, ImGui::GetFontSize(), ImGui::GetFrameHeight(), spacing};
    const auto estimated = std::max(dp(32), dp(options.estimatedRowHeight));
    if (reset || estimated != s.estimatedHeight || layout.x != s.layout.x || layout.y != s.layout.y ||
        layout.z != s.layout.z || layout.w != s.layout.w) {
        s.heightIndex.clear();
        s.heightCapacity = 0;
        s.heights.clear();
        s.documents.clear();
        if (identity)
            s.views.clear();
        else
            for (auto view = s.views.begin(); view != s.views.end();)
                if (view->first != options.editing && view->second.enlargedImage.empty())
                    view = s.views.erase(view);
                else
                    ++view;
        s.requestedFirst = std::string::npos;
        s.layout = layout;
        if (!identity && !bottom)
            s.scroll = double(std::min(anchor, source.count)) * estimated +
                       std::clamp(anchorOffset, 0.0, double(estimated));
    }
    s.sourceId = source.id;
    s.sourceRevision = source.revision;
    s.estimatedHeight = estimated;
    while (s.heightCapacity < source.count) {
        const auto capacity = s.heightCapacity ? s.heightCapacity * 2 : 1;
        if (capacity < s.heightCapacity)
            break;
        const auto total =
            transcriptPrefix(s, s.heightCapacity) - s.estimatedHeight * double(s.heightCapacity);
        s.heightCapacity = capacity;
        if (total)
            s.heightIndex[capacity] = total;
    }
    s.scrollMax = std::max(0.0, transcriptPrefix(s, source.count) - height);
    const bool following = s.jumpToBottom || (s.follow && bottom);
    if (following)
        s.scroll = s.scrollMax;
    if (identity && !following)
        s.scroll = 0;
    std::size_t target = s.scrollToIndex;
    if (s.scrollTo && source.indexOf) {
        target = source.indexOf(s.scrollTo);
        s.scrollTo = 0;
    } else if (s.scrollTo) {
        const auto cached = s.heights.find(s.scrollTo);
        if (cached != s.heights.end())
            target = cached->second.index;
        s.scrollTo = 0;
    }
    if (target < source.count) {
        s.scroll = transcriptPrefix(s, target);
        s.scrollTo = 0;
        s.scrollToIndex = std::string::npos;
    }
    const auto scrollId = ImGui::GetID("##scroll");
    const auto *hoveredWindow = GImGui->HoveredWindow;
    const auto wheel = ImGui::GetIO().MouseWheel;
    const bool innerScroll = hoveredWindow && hoveredWindow != window &&
                             ((wheel > 0 && hoveredWindow->Scroll.y > 0) ||
                              (wheel < 0 && hoveredWindow->Scroll.y < hoveredWindow->ScrollMax.y));
    if (!innerScroll && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        s.scroll -= double(wheel * ImGui::GetTextLineHeightWithSpacing() * 5);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        (!GImGui->ActiveId || GImGui->ActiveId == scrollId)) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            s.scroll -= height;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            s.scroll += height;
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
            s.scroll = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_End))
            s.scroll = s.scrollMax;
    }
    s.scroll = std::clamp(s.scroll, 0.0, s.scrollMax);
    const auto initialScroll = s.scroll;
    ImS64 scroll = ImS64(s.scroll);
    ImGui::ScrollbarEx({{origin.x + width, origin.y}, {origin.x + available.x, origin.y + height}}, scrollId,
                       ImGuiAxis_Y, &scroll, ImS64(height),
                       ImS64(std::min(transcriptPrefix(s, source.count), double(INT64_MAX / 2))),
                       ImDrawFlags_RoundCornersAll);
    if (scroll != ImS64(s.scroll))
        s.scroll = double(scroll);
    const bool keepBottom = following && s.scroll >= s.scrollMax - dp(8) && initialScroll == s.scroll;
    const auto first = transcriptRow(s, source.count, s.scroll);
    const auto visibleEnd = source.count ? transcriptRow(s, source.count, s.scroll + height) + 1 : 0;
    const auto requestFirst = first > options.prefetchRows ? first - options.prefetchRows : 0;
    const auto requestEnd = visibleEnd + std::min(options.prefetchRows, source.count - visibleEnd);
    if (source.request &&
        (requestFirst != s.requestedFirst || requestEnd - requestFirst != s.requestedCount)) {
        source.request(requestFirst, requestEnd - requestFirst);
        s.requestedFirst = requestFirst;
        s.requestedCount = requestEnd - requestFirst;
    }
    ++s.frame;
    s.hovered = 0;
    s.fetched = 0;
    s.firstVisible = s.lastVisible = first;
    float y = origin.y + float(transcriptPrefix(s, first) - s.scroll);
    ImGui::PushClipRect(origin, {origin.x + width, origin.y + height}, true);
    const auto savedClip = window->ClipRect;
    window->ClipRect.ClipWith({origin, {origin.x + width, origin.y + height}});
    for (std::size_t n = first; n < source.count && y < origin.y + height; ++n) {
        ImGui::SetCursorScreenPos({origin.x, y});
        ++s.fetched;
        s.lastVisible = n + 1;
        const auto *message = source.message ? source.message(n) : nullptr;
        if (!message) {
            const float rowHeight = float(transcriptPrefix(s, n + 1) - transcriptPrefix(s, n));
            ImGui::Dummy({width, rowHeight});
            window->DrawList->AddRectFilled({origin.x + dp(44), y + dp(8)},
                                            {origin.x + width * .6f, y + dp(20)}, palette().bg3, dp(3));
            y += rowHeight;
            continue;
        }
        const auto &m = *message;
        auto &doc = s.documents[m.id];
        doc.setMarkdown(chatMarkup(m.content));
        ItemScope key(m.id);
        const std::hash<std::string> hash;
        std::size_t signature = hash(m.role) * 31 ^ hash(m.time) ^ std::size_t(m.edited) ^
                                std::size_t(m.replyTo) ^ (std::size_t(m.own) << 3) ^
                                (std::size_t(options.editing == m.id) << 4);
        for (const auto &a : m.attachments)
            signature =
                signature * 31 ^ hash(a.source) ^ hash(a.name) ^ std::size_t(a.bytes) ^ std::size_t(a.image);
        const float top = ImGui::GetCursorPosY();
        const auto start = ImGui::GetCursorScreenPos();
        // Reserve a channel so the hover ground stays behind text and embedded children.
        auto *draw = ImGui::GetWindowDrawList();
        draw->ChannelsSplit(2);
        draw->ChannelsSetCurrent(1);
        const float avatar = dp(32), margin = avatar + dp(12);
        const auto center = ImVec2{start.x + avatar * .5f, start.y + avatar * .5f + dp(4)};
        draw->AddCircleFilled(center, avatar * .5f, m.own ? palette().accentDim : palette().bg4);
        const char *initial = m.role.empty() ? "?" : m.role.c_str();
        const auto initialEnd = initial + (m.role.empty() ? 1 : nextDocumentChar(m.role, 0));
        const auto initialSize = ImGui::CalcTextSize(initial, initialEnd);
        draw->AddText({center.x - initialSize.x * .5f, center.y - initialSize.y * .5f}, palette().text,
                      initial, initialEnd);
        ImGui::Indent(margin);
        ImGui::BeginGroup();
        if (m.replyTo) {
            const auto parent = s.heights.find(m.replyTo);
            const auto label =
                std::string("↳ ") + (parent == s.heights.end()
                                         ? "#" + std::to_string(m.replyTo)
                                         : parent->second.role + "  " + parent->second.preview);
            if (linkText(label.c_str()))
                s.scrollTo = m.replyTo;
        }
        {
            ScopedFont bold(boldFont());
            ImGui::TextUnformatted(m.role.c_str());
        }
        ImGui::SameLine();
        textFaint("%s", m.time.c_str());
        if (m.edited) {
            ImGui::SameLine();
            textFaint("%s", tr("(edited)").c_str());
        }
        if (options.editing == m.id && options.editor)
            options.editor(m, std::max(dp(1), width - margin));
        else if (!m.content.empty()) {
            DocumentRenderer renderer{s.views[m.id], doc, options.document};
            renderer.content();
            renderer.selection();
            if (!renderer.result.link.empty())
                result.link = renderer.result.link;
        }
        for (std::size_t i = 0; i < m.attachments.size(); ++i) {
            const auto &a = m.attachments[i];
            ItemScope attachment(a.id ? a.id : ItemId(i + 1));
            auto &view = s.views[m.id];
            if (a.image) {
                const auto image = options.document.services && options.document.services->image
                                       ? options.document.services->image(a.source)
                                       : ImageData{};
                if (image.texture && image.width > 0 && image.height > 0) {
                    float w = std::min({dp(520), ImGui::GetContentRegionAvail().x, dp(float(image.width))});
                    float h = w * float(image.height) / float(image.width);
                    if (h > dp(320)) {
                        w *= dp(320) / h;
                        h = dp(320);
                    }
                    const auto pos = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("##image", {w, h}, ImGuiButtonFlags_EnableNav)) {
                        view.enlargedImage = a.source;
                        view.enlargedCaption = a.name;
                        view.imageView = {};
                    }
                    draw->AddImageRounded(image.texture, pos, {pos.x + w, pos.y + h}, image.uvMin,
                                          image.uvMax, IM_COL32_WHITE,
                                          dp(6)); // identity tint preserves attachment colors
                    if (ImGui::IsItemHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
            }
            if (!a.image) {
                ScopedColor background(ImGuiCol_ChildBg, palette().bg2);
                ImGui::BeginChild("##attachment",
                                  {std::min(dp(420), ImGui::GetContentRegionAvail().x), dp(66)},
                                  ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextUnformatted(iconsAvailable() ? icon::document : "FILE");
                ImGui::SameLine();
            }
            ImGui::BeginGroup();
            if (linkText(a.name.c_str())) {
                result.attachmentMessage = m.id;
                result.attachmentIndex = i;
            }
            if (a.image)
                ImGui::SameLine();
            textFaint("%.1f %s",
                      a.bytes >= 1024 * 1024 ? double(a.bytes) / (1024 * 1024) : double(a.bytes) / 1024,
                      a.bytes >= 1024 * 1024 ? "MB" : "KB");
            ImGui::EndGroup();
            ImGui::SameLine();
            if (!a.image)
                alignRight(ImGui::GetFrameHeight() + dp(2));
            if (iconButton(icon::download, tr("Save attachment").c_str())) {
                result.attachmentMessage = m.id;
                result.attachmentIndex = i;
                result.download = true;
            }
            if (!a.image)
                ImGui::EndChild();
        }
        ImGui::EndGroup();
        ImGui::Unindent(margin);
        const auto end =
            ImVec2{start.x + width, std::max(start.y + avatar + dp(8), ImGui::GetCursorScreenPos().y)};
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                                    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                             ImGui::IsMouseHoveringRect(start, end);
        if (hovered) {
            s.hovered = m.id;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                ImGui::OpenPopup("##messageMenu");
        }
        if (hovered && options.editing != m.id) {
            const auto cursor = ImGui::GetCursorScreenPos();
            const int count = m.own ? 4 : 2;
            const float actionsWidth = count * (ImGui::GetFrameHeight() + dp(2)) + (count - 1) * dp(2);
            ImGui::SetCursorScreenPos({std::max(start.x + margin, end.x - actionsWidth), start.y});
            ImGui::BeginGroup();
            if (iconButton(icon::back, tr("Reply").c_str()))
                result.reply = m.id;
            ImGui::SameLine(0, dp(2));
            if (iconButton(icon::copy, tr("Copy message").c_str())) {
                ImGui::SetClipboardText(m.content.c_str());
                result.copy = m.id;
            }
            if (m.own) {
                ImGui::SameLine(0, dp(2));
                if (iconButton(icon::edit, tr("Edit message").c_str()))
                    result.edit = m.id;
                ImGui::SameLine(0, dp(2));
                if (iconButton(icon::trash, tr("Delete message").c_str()))
                    result.erase = m.id;
            }
            ImGui::EndGroup();
            ImGui::SetCursorScreenPos(cursor);
        }
        if (ImGui::BeginPopup("##messageMenu")) {
            if (ImGui::MenuItem(trLabel("Reply").c_str()))
                result.reply = m.id;
            if (ImGui::MenuItem(trLabel("Copy message").c_str())) {
                ImGui::SetClipboardText(m.content.c_str());
                result.copy = m.id;
            }
            if (m.own) {
                if (ImGui::MenuItem(trLabel("Edit message").c_str()))
                    result.edit = m.id;
                if (ImGui::MenuItem(trLabel("Delete message").c_str()))
                    result.erase = m.id;
            }
            ImGui::EndPopup();
        }
        draw->ChannelsSetCurrent(0);
        if (hovered)
            draw->AddRectFilled(start, end, palette().bg2, dp(4));
        draw->ChannelsMerge();
        ImGui::SetCursorScreenPos({start.x, end.y + dp(8)});
        ImGui::Dummy({width, 0});
        const auto measured = std::max(dp(32), ImGui::GetCursorPosY() - top);
        transcriptMeasure(s, n, measured);
        s.heights[m.id] = {measured, signature, n, s.frame, m.role, m.content.substr(0, 70)};
        y = start.y + measured;
    }
    window->ClipRect = savedClip;
    ImGui::PopClipRect();
    // Only local viewport coordinates reach ImGui. Logical history offsets stay in doubles/64-bit
    // scrollbar values, avoiding float precision loss in histories tens of millions of pixels tall.
    window->DC.CursorMaxPos.y = origin.y;
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(available);
    s.scrollMax = std::max(0.0, transcriptPrefix(s, source.count) - height);
    if (keepBottom)
        s.scroll = s.scrollMax;
    else
        s.scroll = std::clamp(s.scroll, 0.0, s.scrollMax);
    s.jumpToBottom = false;
    s.count = source.count;
    ImGui::EndChild();
    for (auto &view : s.views) {
        if (view.second.enlargedImage.empty())
            continue;
        ItemScope key(view.first);
        documentLightbox(view.second, options.document);
    }
    transcriptPrune(s, std::max<std::size_t>(1, options.cacheMessages), options.editing);
    return result;
}
namespace {
struct ComposerInput {
    MessageComposerState *state;
};
int composerCallback(ImGuiInputTextCallbackData *data) {
    auto &s = *static_cast<ComposerInput *>(data->UserData)->state;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        s.text.resize(std::size_t(data->BufTextLen));
        data->Buf = s.text.data();
    } else {
        s.first = data->SelectionStart;
        s.last = data->SelectionEnd;
    }
    return 0;
}
void composerReplace(MessageComposerState &s, const std::string &open, const std::string &close) {
    const auto a = std::min(std::size_t(std::max(0, std::min(s.first, s.last))), s.text.size());
    const auto b = std::min(std::size_t(std::max(0, std::max(s.first, s.last))), s.text.size());
    if (a >= open.size() && s.text.compare(a - open.size(), open.size(), open) == 0 &&
        s.text.compare(b, close.size(), close) == 0) {
        s.text.erase(b, close.size());
        s.text.erase(a - open.size(), open.size());
        s.first = int(a - open.size());
        s.last = int(b - open.size());
    } else {
        s.text.insert(b, close);
        s.text.insert(a, open);
        s.first = int(a + open.size());
        s.last = int(b + open.size());
    }
    if (auto *input = ImGui::GetInputTextState(s.input)) {
        input->ReloadUserBufAndKeepSelection();
        input->ReloadSelectionStart = s.first;
        input->ReloadSelectionEnd = s.last;
    }
    s.focus = true;
}
} // namespace
MessageComposerResult messageComposer(const char *id, MessageComposerState &s, ImVec2 size,
                                      const MessageComposerOptions &options) {
    ScopedID scope(id);
    MessageComposerResult result;
    bool submitRequested = false;
    const auto before = s.text;
    if (s.text != s.synchronized)
        if (auto *input = ImGui::GetInputTextState(s.input))
            input->ReloadUserBufAndMoveToEnd();
    const bool active = s.input && GImGui->ActiveId == s.input;
    auto &io = ImGui::GetIO();
    if (active) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && !io.KeyShift) {
            submitRequested = true;
            ImGui::SetKeyOwner(ImGuiKey_Enter, ImGui::GetID("##submit"), ImGuiInputFlags_LockThisFrame);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            result.cancelled = true;
            ImGui::SetKeyOwner(ImGuiKey_Escape, ImGui::GetID("##cancel"), ImGuiInputFlags_LockThisFrame);
        }
        if (io.KeyCtrl || io.KeySuper) {
            if (ImGui::IsKeyPressed(ImGuiKey_B, false))
                composerReplace(s, "**", "**");
            if (ImGui::IsKeyPressed(ImGuiKey_I, false))
                composerReplace(s, "*", "*");
            if (ImGui::IsKeyPressed(ImGuiKey_U, false))
                composerReplace(s, "__", "__");
            if (ImGui::IsKeyPressed(ImGuiKey_E, false))
                composerReplace(s, "`", "`");
            if (!io.KeyShift && options.paste && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
                std::string replacement;
                if (options.paste(replacement)) {
                    if (!replacement.empty()) {
                        const auto a = std::min(std::max(0, std::min(s.first, s.last)), int(s.text.size()));
                        const auto b = std::min(std::max(0, std::max(s.first, s.last)), int(s.text.size()));
                        s.text.replace(std::size_t(a), std::size_t(b - a), replacement);
                        s.first = s.last = a + int(replacement.size());
                        if (auto *input = ImGui::GetInputTextState(s.input))
                            input->ReloadUserBufAndMoveToEnd();
                    }
                    ImGui::SetKeyOwner(ImGuiKey_V, ImGui::GetID("##paste"), ImGuiInputFlags_LockThisFrame);
                }
            }
        }
    }
    ImGui::BeginGroup();
    if (options.attachments) {
        if (iconButton(icon::add, tr("Attach files").c_str()))
            result.attach = true;
        ImGui::SameLine();
    }
    const float width = size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x;
    const float inputWidth =
        std::max(dp(40), width - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
    const auto measured =
        ImGui::CalcTextSize(s.text.c_str(), nullptr, false, std::max(dp(1), inputWidth - dp(24)));
    const float height = size.y > 0 ? size.y
                                    : std::clamp(measured.y + ImGui::GetStyle().FramePadding.y * 2 + dp(8),
                                                 ImGui::GetFrameHeight() + dp(8), dp(options.maxHeight));
    if (s.focus) {
        if (!active)
            ImGui::SetKeyboardFocusHere();
        s.focus = false;
    }
    s.input = ImGui::GetID("##input");
    ComposerInput callback{&s};
    ImGui::InputTextMultiline("##input", s.text.data(), s.text.capacity() + 1, {inputWidth, height},
                              ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_CallbackAlways |
                                  ImGuiInputTextFlags_WordWrap,
                              composerCallback, &callback);
    result.focused = ImGui::IsItemActive();
    if (submitRequested)
        result.submitted = options.canSubmitEmpty || s.text.find_first_not_of(" \t\r\n") != std::string::npos;
    const auto inputMin = ImGui::GetItemRectMin();
    if (s.text.empty() && !result.focused)
        ImGui::GetWindowDrawList()->AddText(
            {inputMin.x + dp(10), inputMin.y + ImGui::GetStyle().FramePadding.y}, palette().textFaint,
            tr("Message…").c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!options.canSubmitEmpty && s.text.find_first_not_of(" \t\r\n") == std::string::npos);
    if (iconButton(options.editing ? icon::check : icon::send,
                   tr(options.editing ? "Save message (Enter)" : "Send message (Enter)").c_str()))
        result.submitted = true;
    ImGui::EndDisabled();
    ImGui::EndGroup();
    const auto toolbarName = "##messageFormat" + std::to_string(ImGui::GetID("##input"));
    const auto *toolbar = ImGui::FindWindowByName(toolbarName.c_str());
    const bool toolbarActive =
        toolbar && toolbar->WasActive &&
        (ImGui::IsMouseHoveringRect(toolbar->Pos,
                                    {toolbar->Pos.x + toolbar->Size.x, toolbar->Pos.y + toolbar->Size.y}) ||
         GImGui->NavWindow == toolbar);
    if (s.first != s.last && (result.focused || toolbarActive)) {
        ImGui::SetNextWindowPos({inputMin.x + dp(32), inputMin.y - ImGui::GetFrameHeight() - dp(10)});
        ImGui::Begin(toolbarName.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav);
        const char *labels[]{"B", "I", "U", "S", "</>", "||"};
        const char *tips[]{"Bold", "Italic", "Underline", "Strikethrough", "Code", "Spoiler"};
        const char *marks[]{"**", "*", "__", "~~", "`", "||"};
        for (int n = 0; n < 6; ++n) {
            if (n)
                ImGui::SameLine(0, dp(2));
            if (ghostButton(labels[n]))
                composerReplace(s, marks[n], marks[n]);
            tooltip(tr(tips[n]).c_str());
        }
        ImGui::End();
    }
    result.changed = before != s.text;
    s.synchronized = s.text;
    return result;
}
} // namespace cyui

#include "cyui/modules/DocumentEditor.h"
#include "cyui/modules/DataGrid.h"
#include "DocumentLayout.h"
#include "cyui/modules/CodeEditor.h"
#include <TextEditor.h>
#include <cstring>
#include <imgui_internal.h>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace cyui {
namespace {
bool boundary(const std::string &s, std::size_t at) {
    return at <= s.size() && (at == s.size() || (static_cast<unsigned char>(s[at]) & 0xc0) != 0x80);
}
std::string escapeMarkdown(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (std::strchr("\\`*_{}[]<>#~|", c))
            out += '\\';
        out += c;
    }
    return out;
}
std::string serialize(const DocumentInline &r) {
    if (r.kind == DocumentInlineKind::image)
        return "![" + escapeMarkdown(r.text) + (r.width > 0 ? "|" + std::to_string(int(r.width)) : "") +
               "](<" + r.target + ">)";
    // A style cannot cross a blank paragraph boundary in CommonMark. Keep its
    // visible characters styled on each line, with the breaks outside delimiters.
    if (r.text.find('\n') != std::string::npos) {
        std::string out;
        for (std::size_t at = 0; at <= r.text.size();) {
            const auto end = r.text.find('\n', at);
            auto line = r;
            line.text = r.text.substr(at, end == std::string::npos ? end : end - at);
            if (!line.text.empty())
                out += serialize(line);
            if (end == std::string::npos)
                break;
            out += '\n';
            at = end + 1;
        }
        return out;
    }
    std::string value = escapeMarkdown(r.text);
    unsigned style = r.style;
    if (style & documentMono) {
        std::string fence = "`";
        while (r.text.find(fence) != std::string::npos)
            fence += '`';
        value = fence + " " + r.text + " " + fence;
    }
    if (style & documentBold)
        value = "**" + value + "**";
    if (style & documentItalic)
        value = "*" + value + "*";
    if (style & documentUnderline)
        value = "<u>" + value + "</u>";
    if (style & documentStrike)
        value = "~~" + value + "~~";
    if (style & documentHighlight)
        value = "<mark>" + value + "</mark>";
    if (style & documentKeyboard)
        value = "<kbd>" + value + "</kbd>";
    if (style & documentSpoiler)
        value = "||" + value + "||";
    if (!r.target.empty())
        value = "[" + value + "](<" + r.target + ">)";
    return value;
}
TextEditor::DocPos position(const std::string &s, std::size_t offset) {
    TextEditor::DocPos p;
    for (std::size_t i = 0; i < std::min(s.size(), offset); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '\n') {
            ++p.line;
            p.index = 0;
        } else if (c != '\r' && (c & 0xc0) != 0x80)
            ++p.index;
    }
    return p;
}
std::pair<std::size_t, std::size_t> blockRange(const DocumentBlock &b, const std::string &source) {
    const auto begin =
        b.sourceBegin ? source.rfind('\n', std::min(b.sourceBegin - 1, source.size())) : std::string::npos;
    auto end = source.find('\n', std::min(b.sourceEnd, source.size()));
    if (end == std::string::npos)
        end = source.size();
    if (end && source[end - 1] == '\r')
        --end;
    return {begin == std::string::npos ? 0 : begin + 1, end};
}
std::string prefix(const DocumentBlock &b) {
    std::string out;
    for (int q = 0; q < b.quoteDepth; ++q)
        out += "> ";
    out += std::string(std::size_t(std::max(0, b.indent - 1)) * 2, ' ');
    if (b.kind == DocumentBlockKind::heading)
        out += std::string(std::size_t(b.level), '#') + " ";
    if (b.kind == DocumentBlockKind::list)
        out += b.task ? (b.checked ? "- [x] " : "- [ ] ") : b.number ? std::to_string(b.number) + ". " : "- ";
    return out;
}
struct LiveField {
    std::vector<char> buffer;
    int first = 0, last = 0, cursor = 0;
    std::string lexicalSource;
    std::vector<CodeToken> tokens;
    std::uint64_t languageRevision = 0;
    RichText rich;
    DocumentBlock block;
    std::string canonical;
    std::vector<std::size_t> projection;
    std::size_t regionFirst = 0, blocks = 1, revision = 0;
    bool cached = false;
    int requestCursor = -1;
};
std::string liveMarkup(const DocumentBlock &block, const RichText &rich) {
    const auto text = rich.markdown();
    std::string output = prefix(block);
    // This is one rich block. Inline breaks must survive parsing at its end, in headings
    // and list items too. Paragraph boundaries are separate source transactions.
    for (const auto c : text) {
        if (c == '\n')
            output += "<br>";
        else if (c != '\r')
            output += c;
    }
    return output;
}
void projectField(LiveField &field) {
    const auto plain = field.rich.text();
    field.projection.assign(plain.size() + 1, field.regionFirst);
    Document projected;
    projected.setMarkdown(field.canonical);
    field.blocks = std::max<std::size_t>(1, projected.blocks().size());
    if (field.block.kind == DocumentBlockKind::code) {
        const auto body = field.canonical.find('\n');
        const auto offset = body == std::string::npos ? 0 : body + 1;
        for (std::size_t at = 0; at <= plain.size(); ++at)
            field.projection[at] = field.regionFirst + std::min(field.canonical.size(), offset + at);
        return;
    }
    std::size_t search = 0, last = 0;
    for (const auto &block : projected.blocks())
        for (const auto &run : block.inlines) {
            if (run.text.empty())
                continue;
            const auto at = plain.find(run.text, search);
            if (at == std::string::npos)
                continue;
            for (std::size_t i = search; i < at; ++i)
                field.projection[i] = field.regionFirst + std::min(field.canonical.size(), last + i - search);
            for (std::size_t i = 0; i < run.text.size(); ++i)
                field.projection[at + i] =
                    field.regionFirst + run.sourceBegin + std::min(i, run.sourceEnd - run.sourceBegin);
            search = at + run.text.size();
            last = run.sourceEnd;
            field.projection[search] = field.regionFirst + last;
        }
    for (std::size_t at = search; at < plain.size(); ++at)
        field.projection[at] = field.regionFirst + std::min(field.canonical.size(), last + at - search);
    field.projection.back() = field.regionFirst + field.canonical.size();
}
int liveCallback(ImGuiInputTextCallbackData *data) {
    auto &s = *static_cast<LiveField *>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        s.buffer.resize(std::size_t(data->BufSize));
        data->Buf = s.buffer.data();
    } else {
        if (s.requestCursor >= 0) {
            data->CursorPos = data->SelectionStart = data->SelectionEnd =
                std::min(s.requestCursor, data->BufTextLen);
            s.requestCursor = -1;
        }
        s.first = data->SelectionStart;
        s.last = data->SelectionEnd;
        s.cursor = data->CursorPos;
    }
    return 0;
}
int liveLines(const std::string &text, float wrap) {
    const auto *at = text.data(), *end = at + text.size();
    int lines = 0;
    while (at < end) {
        ++lines;
        const auto *next = ImFontCalcWordWrapPositionEx(ImGui::GetFont(), ImGui::GetFontSize(), at, end, wrap,
                                                        ImDrawTextFlags_WrapKeepBlanks);
        at = next > at ? next : at + 1;
        if (at < end && *at == '\n')
            ++at;
    }
    if (!lines || (!text.empty() && text.back() == '\n'))
        ++lines;
    return lines;
}
// Size the native field for this frame's input, including a newly inserted empty line.
// Otherwise InputText scrolls the old, shorter field and hides the preceding text.
std::string livePendingText(ImGuiID id, const std::string &plain, bool code, bool readOnly) {
    auto text = plain;
    const auto *input = ImGui::GetInputTextState(id);
    if (readOnly || GImGui->ActiveId != id || !input)
        return text;
    auto first = std::min<std::size_t>(std::max(0, input->GetSelectionStart()), text.size());
    auto last = std::min<std::size_t>(std::max(0, input->GetSelectionEnd()), text.size());
    if (first > last)
        std::swap(first, last);
    auto replace = [&](const std::string &value) {
        text.replace(first, last - first, value);
        first += value.size();
        last = first;
    };
    const auto &io = ImGui::GetIO();
    const bool modifier = io.KeyCtrl || io.KeySuper;
    if (!modifier || io.KeyAlt) {
        std::string typed;
        for (const auto c : io.InputQueueCharacters) {
            if (c < 32 || c == 127)
                continue;
            char utf8[5]{};
            ImTextCharToUtf8(utf8, c);
            typed += utf8;
        }
        if (!typed.empty())
            replace(typed);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && (io.KeyShift || (code && !modifier)))
        replace("\n");
    else if (modifier && ImGui::IsKeyPressed(ImGuiKey_V, false) &&
             (ImGui::GetKeyOwner(ImGuiKey_V) == ImGuiKeyOwner_NoOwner ||
              ImGui::GetKeyOwner(ImGuiKey_V) == id)) {
        if (const auto *clipboard = ImGui::GetClipboardText())
            replace(clipboard);
    } else if (!modifier && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (first == last && first) {
            --first;
            while (first && !boundary(text, first))
                --first;
        }
        replace("");
    } else if (!modifier && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (first == last && last < text.size())
            last = nextDocumentChar(text, last);
        replace("");
    }
    return text;
}
// Rich glyphs share the input field's exact advances and wrap positions, preserving native
// selection, keyboard and IME geometry. Appending to its child draw list keeps clipping/order.
void decorateLive(ImGuiID id, const RichText &rich, const std::vector<CodeToken> *tokens = nullptr,
                  const std::vector<DocumentSpellingIssue> *issues = nullptr, DocumentState *view = nullptr,
                  DocumentResult *result = nullptr, const std::vector<std::size_t> *projection = nullptr,
                  std::pair<std::size_t, std::size_t> selection = {}) {
    auto *parent = ImGui::GetCurrentWindow();
    ImGuiWindow *child = nullptr;
    for (auto *candidate : parent->DC.ChildWindows)
        if (candidate->ChildId == id) {
            child = candidate;
            break;
        }
    if (!child)
        return;
    const auto plain = rich.text();
    auto *input = ImGui::GetInputTextState(id);
    auto *font = ImGui::GetFont();
    const float size = ImGui::GetFontSize(),
                wrap = input ? input->WrapWidth
                             : std::max(dp(1), child->InnerRect.GetWidth() -
                                                   ImGui::GetStyle().FramePadding.x * 2 -
                                                   ImGui::GetStyle().ScrollbarSize);
    const auto origin = child->DC.CursorStartPos;
    ImVec2 p{origin.x + ImGui::GetStyle().FramePadding.x, origin.y + ImGui::GetStyle().FramePadding.y};
    const float startX = p.x;
    auto *draw = child->DrawList;
    draw->PushClipRect(child->InnerClipRect.Min, child->InnerClipRect.Max, true);
    std::size_t at = 0, runIndex = 0, runEnd = rich.runs().empty() ? 0 : rich.runs()[0].text.size();
    const char *lineEnd = ImFontCalcWordWrapPositionEx(font, size, plain.data(), plain.data() + plain.size(),
                                                       wrap, ImDrawTextFlags_WrapKeepBlanks);
    std::size_t tokenIndex = 0;
    while (at <= plain.size()) {
        if (at == plain.size())
            break;
        if (plain.data() + at == lineEnd) {
            p.x = startX;
            p.y += size;
            if (plain[at] == '\n') {
                ++at;
                if (at == plain.size())
                    continue;
            }
            lineEnd = ImFontCalcWordWrapPositionEx(font, size, plain.data() + at, plain.data() + plain.size(),
                                                   wrap, ImDrawTextFlags_WrapKeepBlanks);
        }
        while (runIndex + 1 < rich.runs().size() && at >= runEnd) {
            ++runIndex;
            runEnd += rich.runs()[runIndex].text.size();
        }
        if (rich.runs().empty())
            break;
        const auto &r = rich.runs()[runIndex];
        const auto next = nextDocumentChar(plain, at);
        const float width = font->CalcTextSizeA(size, FLT_MAX, 0, plain.data() + at, plain.data() + next).x;
        if (r.style & documentHighlight)
            draw->AddRectFilled(p, {p.x + width, p.y + size}, alpha(palette().warning, .25f));
        if (r.style & (documentMono | documentKeyboard))
            draw->AddRectFilled(p, {p.x + width, p.y + size}, palette().bg3);
        auto color = !r.target.empty() ? palette().accent : palette().text;
        if (tokens) {
            while (tokenIndex < tokens->size() &&
                   at >= (*tokens)[tokenIndex].start + (*tokens)[tokenIndex].length)
                ++tokenIndex;
            if (tokenIndex < tokens->size() && at >= (*tokens)[tokenIndex].start)
                color = tokenColor((*tokens)[tokenIndex].role, palette().bg0);
        }
        const std::size_t runStart = runEnd - r.text.size();
        const auto sourceAt = projection && at < projection->size()
                                  ? (*projection)[at]
                                  : r.sourceBegin + std::min(at - runStart, r.sourceEnd - r.sourceBegin);
        const auto sourceEnd = projection && next < projection->size()
                                   ? (*projection)[next]
                                   : r.sourceBegin + std::min(next - runStart, r.sourceEnd - r.sourceBegin);
        if (sourceEnd > selection.first && sourceAt < selection.second)
            draw->AddRectFilled(p, {p.x + width, p.y + size}, palette().selection);
        const int vertex = draw->VtxBuffer.Size;
        auto *shown = (r.style & documentBold) && boldFont() ? boldFont() : font;
        const bool hidden = (r.style & documentSpoiler) && !(input && GImGui->ActiveId == id);
        if (hidden)
            draw->AddRectFilled(p, {p.x + width, p.y + size}, palette().bg4, dp(2));
        else
            draw->AddText(shown, size, p, color, plain.data() + at, plain.data() + next);
        if (view)
            view->hits.push_back(
                {{p, {p.x + width, p.y + size}}, plain.substr(at, next - at), sourceAt, sourceEnd});
        if (result && !r.target.empty() && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::IsMouseClicked(0) &&
            hitDocument({p, {p.x + width, p.y + size}}, ImGui::GetIO().MousePos))
            result->link = r.target;
        if (r.style & documentItalic)
            for (int v = vertex; v < draw->VtxBuffer.Size; ++v)
                draw->VtxBuffer[v].pos.x += (p.y + size - draw->VtxBuffer[v].pos.y) * .14f;
        if (r.style & documentUnderline || !r.target.empty())
            draw->AddLine({p.x, p.y + size}, {p.x + width, p.y + size}, color, dp(1));
        if (r.style & documentStrike)
            draw->AddLine({p.x, p.y + size * .55f}, {p.x + width, p.y + size * .55f}, color, dp(1));
        if (issues) {
            const auto source = sourceAt;
            for (const auto &issue : *issues)
                if (source >= issue.begin && source < issue.end) {
                    draw->AddLine({p.x, p.y + size}, {p.x + width, p.y + size}, palette().warning, dp(1));
                    break;
                }
        }
        p.x += width;
        at = next;
    }
    draw->PopClipRect();
}
} // namespace
std::string RichText::text() const {
    std::string out;
    for (const auto &r : runs_)
        out += r.text;
    return out;
}
std::string RichText::markdown() const {
    std::string out;
    DocumentInline merged;
    bool have = false;
    for (const auto &r : runs_) {
        if (have && merged.style == r.style && merged.target == r.target && merged.kind == r.kind &&
            r.kind != DocumentInlineKind::image)
            merged.text += r.text;
        else {
            if (have)
                out += serialize(merged);
            merged = r;
            have = true;
        }
    }
    if (have)
        out += serialize(merged);
    return out;
}
bool RichText::replace(std::size_t first, std::size_t last, const std::string &value) {
    const auto plain = text();
    if (first > last || !boundary(plain, first) || !boundary(plain, last) ||
        value.size() > 32 * 1024 * 1024 || plain.size() - (last - first) + value.size() > 32 * 1024 * 1024)
        return false;
    std::vector<DocumentInline> before, after;
    DocumentInline inserted;
    std::size_t at = 0;
    bool found = false;
    for (const auto &r : runs_) {
        const auto end = at + r.text.size();
        if (!found && first >= at && (first < end || (first == end && first == plain.size()))) {
            inserted = r;
            inserted.kind = r.kind == DocumentInlineKind::image ? DocumentInlineKind::text : r.kind;
            found = true;
        }
        if (at < first) {
            auto copy = r;
            copy.text = r.text.substr(0, std::min(r.text.size(), first - at));
            if (!copy.text.empty())
                before.push_back(std::move(copy));
        }
        if (end > last) {
            auto copy = r;
            copy.text = r.text.substr(last > at ? last - at : 0);
            if (!copy.text.empty())
                after.push_back(std::move(copy));
        }
        at = end;
    }
    inserted.text = value;
    inserted.sourceBegin = inserted.sourceEnd = 0;
    if (!value.empty())
        before.push_back(inserted);
    before.insert(before.end(), after.begin(), after.end());
    runs_ = std::move(before);
    return true;
}
struct DocumentEditor::Impl {
    CodeEditor source;
    Document doc;
    std::size_t parsedRevision = ~std::size_t(0);
    std::vector<std::size_t> lineStarts{0};
    TextEditor::DocSelection sourceSelection;
    std::size_t selectionRevision = ~std::size_t(0), sourceFirst = 0, sourceLast = 0;
    DocumentState view;
    DocumentMode mode = DocumentMode::live;
    std::unordered_map<std::size_t, LiveField> fields;
    struct TableField {
        GridModel grid;
        GridState state;
        std::string source;
    };
    std::unordered_map<std::size_t, TableField> tables;
    std::size_t first = 0, last = 0;
    bool readOnly = false, changed = false, preserveCRLF = false;
    ImGuiID activeInput = 0;
    std::vector<DocumentSpellingIssue> issues;
    std::uint64_t spellingDocument = ~std::uint64_t(0), spellingRevision = 0;
    std::string spellingLanguage;
    std::array<char, 2048> link{};
    ImVec2 floatingPosition{};
    std::pair<std::size_t, std::size_t> floatingSelection{};
    std::size_t focusAt = std::string::npos, emptyAt = std::string::npos;
    std::size_t activationAt = std::string::npos;
    bool documentSelection = false;
    std::size_t gestureAnchor = 0;
    ImRect gestureBounds;
    bool gestureArmed = false;
    bool openLink = false;
    Impl() {
        source.setLanguage(CodeLanguage::markdown);
        source.native().SetWordWrapEnabled(true);
    }
    bool replace(std::size_t a, std::size_t b, const std::string &value) {
        const auto text = source.text();
        if (readOnly || a > b || !boundary(text, a) || !boundary(text, b) ||
            text.size() - (b - a) + value.size() > 32 * 1024 * 1024)
            return false;
        source.native().ReplaceSectionText(position(text, a), position(text, b), value);
        first = a;
        last = a + value.size();
        changed = true;
        return true;
    }
    void refresh() {
        const auto revision = source.native().GetTextRevision();
        if (revision == parsedRevision)
            return;
        if (doc.setMarkdown(source.text())) {
            lineStarts.assign(1, 0);
            const auto &text = doc.markdown();
            for (std::size_t newline = text.find('\n'); newline != std::string::npos;
                 newline = text.find('\n', newline + 1))
                lineStarts.push_back(newline + 1);
        }
        parsedRevision = revision;
    }
    std::size_t offset(TextEditor::DocPos p) const {
        const auto &text = doc.markdown();
        if (p.line >= lineStarts.size())
            return text.size();
        auto at = lineStarts[p.line];
        for (std::size_t glyph = 0; glyph < p.index && at < text.size() && text[at] != '\n';) {
            if (text[at++] == '\r')
                continue;
            while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xc0) == 0x80)
                ++at;
            ++glyph;
        }
        return at;
    }
    void reload() {
        if (activeInput) {
            if (GImGui->InputTextDeactivatedState.ID == activeInput)
                GImGui->InputTextDeactivatedState.ID = 0;
            if (auto *input = ImGui::GetInputTextState(activeInput)) {
                if (GImGui->ActiveId == activeInput)
                    input->ReloadUserBufAndKeepSelection();
                else {
                    input->ID = 0;
                    input->WantReloadUserBuf = false;
                }
            }
        }
    }
    DocumentResult live(const char *id, ImVec2 size, const DocumentOptions &options) {
        ImGui::BeginChild(id, size);
        // InputText intentionally ignores character input on its activation frame. A paragraph
        // transition must nevertheless accept characters already queued by a fast typist.
        if ((focusAt != std::string::npos || activationAt != std::string::npos) && !readOnly &&
            !ImGui::GetIO().InputQueueCharacters.empty()) {
            std::string queued;
            for (const auto c : ImGui::GetIO().InputQueueCharacters) {
                char utf8[5]{};
                ImTextCharToUtf8(utf8, c);
                queued += utf8;
            }
            const auto at =
                std::min(focusAt != std::string::npos ? focusAt : activationAt, source.text().size());
            replace(at, at, queued);
            focusAt = at + queued.size();
            first = last = focusAt;
            ImGui::GetIO().InputQueueCharacters.clear();
            refresh();
        }
        const auto &current = doc.markdown();
        DocumentRenderer renderer{view, doc, options};
        const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        auto &io = ImGui::GetIO();
        if (focused && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            documentSelection = true;
            first = 0;
            last = current.size();
            ImGui::SetKeyOwner(ImGuiKey_A, ImGui::GetID("##selectAll"), ImGuiInputFlags_LockThisFrame);
            ImGui::ClearActiveID();
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
        }
        std::string selectedText;
        if (documentSelection) {
            if (std::min(first, last) == 0 && std::max(first, last) == current.size())
                selectedText = doc.plainText();
            else {
                float y = -FLT_MAX;
                for (const auto &hit : view.hits)
                    if (hit.end > std::min(first, last) && hit.begin < std::max(first, last)) {
                        if (y != hit.bounds.min.y && !selectedText.empty())
                            selectedText += '\n';
                        selectedText += hit.text;
                        y = hit.bounds.min.y;
                    }
            }
        }
        view.hits.clear();
        if (documentSelection && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            ImGui::SetNextFrameWantCaptureKeyboard(true);
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_C))
                ImGui::SetClipboardText(selectedText.c_str());
            std::string inserted;
            for (const auto c : io.InputQueueCharacters) {
                char utf8[5]{};
                ImTextCharToUtf8(utf8, c);
                inserted += utf8;
            }
            const bool cut = (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_X);
            const bool erase =
                ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete);
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_V)) {
                if (const auto *clipboard = ImGui::GetClipboardText())
                    inserted = clipboard;
            }
            if (cut)
                ImGui::SetClipboardText(selectedText.c_str());
            if (!readOnly && (!inserted.empty() || erase || cut)) {
                replace(std::min(first, last), std::max(first, last), inserted);
                focusAt = std::min(first, last) + inserted.size();
                first = last = focusAt;
                io.InputQueueCharacters.clear();
                documentSelection = false;
                fields.clear();
                ImGui::EndChild();
                return {};
            }
        }
        if (ImGui::IsMouseClicked(0))
            gestureArmed = false;
        auto blocks = doc.blocks();
        const auto blankAt = [&](std::size_t at) {
            DocumentBlock blank;
            blank.sourceBegin = blank.sourceEnd = at;
            const auto next = std::find_if(blocks.begin(), blocks.end(),
                                           [&](const auto &b) { return b.sourceBegin >= at; });
            blocks.insert(next, std::move(blank));
        };
        if (emptyAt != std::string::npos && emptyAt <= current.size() &&
            std::none_of(blocks.begin(), blocks.end(),
                         [&](const auto &b) { return b.sourceBegin <= emptyAt && b.sourceEnd > emptyAt; }))
            blankAt(emptyAt);
        if (!readOnly &&
            (blocks.empty() || blocks.back().sourceEnd != current.size() || !blocks.back().inlines.empty()))
            blankAt(current.size());
        const auto requestedFocus = focusAt;
        focusAt = std::string::npos;
        bool focusApplied = false;
        if (requestedFocus != std::string::npos) {
            fields.clear();
            if (GImGui->ActiveId == activeInput)
                ImGui::ClearActiveID();
            if (GImGui->InputTextDeactivatedState.ID == activeInput)
                GImGui->InputTextDeactivatedState.ID = 0;
            if (auto *input = ImGui::GetInputTextState(activeInput)) {
                input->ID = 0;
                input->WantReloadUserBuf = false;
            }
        }
        std::vector<std::tuple<std::size_t, std::size_t, std::string>> edits;
        LiveField *pending = nullptr;
        std::size_t blockIndex = 0;
        for (std::size_t n = 0; n < blocks.size() + 2; ++n) {
            ScopedID key{int(n)};
            auto &field = fields[n];
            const auto inputId = ImGui::GetID("##rich");
            const bool cached =
                field.cached && GImGui->ActiveId == inputId &&
                field.revision == source.native().GetTextRevision() && field.regionFirst <= current.size() &&
                current.compare(field.regionFirst, field.canonical.size(), field.canonical) == 0;
            if (field.cached && !cached) {
                field.cached = false;
                if (GImGui->InputTextDeactivatedState.ID == inputId)
                    GImGui->InputTextDeactivatedState.ID = 0;
                if (auto *input = ImGui::GetInputTextState(inputId)) {
                    if (GImGui->ActiveId == inputId)
                        input->ReloadUserBufAndKeepSelection();
                    else {
                        input->ID = 0;
                        input->WantReloadUserBuf = false;
                    }
                }
            }
            const bool empty = blocks.empty() && n == 0;
            if (blockIndex >= blocks.size() && !cached && !empty)
                break;
            DocumentBlock blank;
            blank.sourceBegin = blank.sourceEnd = 0;
            const auto &b = cached ? field.block : blockIndex < blocks.size() ? blocks[blockIndex] : blank;
            const std::size_t consumed = cached ? field.blocks : blockIndex < blocks.size() ? 1 : 0;
            const bool synthetic = b.inlines.empty() && b.text.empty() && b.sourceBegin == b.sourceEnd;
            const bool special =
                b.kind == DocumentBlockKind::table || b.kind == DocumentBlockKind::rule ||
                b.sourceBegin > b.sourceEnd ||
                (b.kind == DocumentBlockKind::code && (b.text.empty() || b.indent > 0 || b.quoteDepth > 0)) ||
                std::any_of(b.inlines.begin(), b.inlines.end(),
                            [](const auto &r) { return r.kind == DocumentInlineKind::image; });
            if (special) {
                if (b.kind == DocumentBlockKind::table) {
                    std::size_t gridColumns = 1;
                    for (const auto &row : b.cells)
                        gridColumns = std::max(gridColumns, row.size());
                    if (b.cells.size() > 100000 || gridColumns > 256 ||
                        b.cells.size() > 1000000 / gridColumns) {
                        callout(Tone::info,
                                tr("Table exceeds native grid limits. Edit it in Source.").c_str());
                    } else {
                        auto &session = tables[n];
                        const auto range = blockRange(b, current);
                        const auto original = current.substr(range.first, range.second - range.first);
                        if (session.source != original) {
                            std::size_t columns = 1;
                            for (const auto &row : b.cells)
                                columns = std::max(columns, row.size());
                            std::vector<GridCell> cells(b.cells.size() * columns);
                            for (std::size_t row = 0; row < b.cells.size(); ++row)
                                for (std::size_t column = 0; column < b.cells[row].size(); ++column) {
                                    auto &cell = cells[row * columns + column];
                                    cell.text = RichText(b.cells[row][column]).markdown();
                                    if (column < b.alignments.size())
                                        cell.alignment = GridAlignment(b.alignments[column]);
                                }
                            session.grid.assign(b.cells.size(), columns, cells);
                            session.grid.clearHistory();
                            session.state = {};
                            session.source = original;
                        }
                        GridOptions gridOptions;
                        gridOptions.readOnly = readOnly;
                        gridOptions.history = false;
                        gridOptions.formatting = false;
                        gridOptions.toolbar = gridOptions.rowHeaders = gridOptions.columnHeaders = false;
                        gridOptions.rowHeight = 26;
                        gridOptions.columnWidth = std::max(80.f, ImGui::GetContentRegionAvail().x / dp(1) /
                                                                     float(session.grid.columns()));
                        gridOptions.display = [](std::size_t, std::size_t, const GridCell &cell) {
                            Document parsed;
                            parsed.setMarkdown(cell.text);
                            return parsed.plainText();
                        };
                        const auto edit = dataGrid("##table", session.state, session.grid,
                                                   {0, dp(std::min(360.f, 12.f + float(session.grid.rows()) *
                                                                                     gridOptions.rowHeight))},
                                                   gridOptions);
                        if (edit.changed) {
                            std::string markup;
                            for (std::size_t row = 0; row < session.grid.rows(); ++row) {
                                markup += '|';
                                for (std::size_t column = 0; column < session.grid.columns(); ++column) {
                                    const auto &text = session.grid.cell(row, column).text;
                                    for (std::size_t at = 0; at < text.size(); ++at) {
                                        char ch = text[at];
                                        if (ch == '|' && (at == 0 || text[at - 1] != '\\'))
                                            markup += '\\';
                                        if (ch == '\n')
                                            markup += "<br>";
                                        else
                                            markup += ch;
                                    }
                                    markup += " |";
                                }
                                if (row + 1 < session.grid.rows() || row == 0)
                                    markup += '\n';
                                if (row == 0) {
                                    markup += '|';
                                    for (std::size_t column = 0; column < session.grid.columns(); ++column) {
                                        auto align = session.grid.cell(0, column).alignment;
                                        markup += align == GridAlignment::center  ? " :---: |"
                                                  : align == GridAlignment::right ? " ---: |"
                                                                                  : " --- |";
                                    }
                                    if (session.grid.rows() > 1)
                                        markup += '\n';
                                }
                            }
                            session.source = markup;
                            edits.emplace_back(range.first, range.second, std::move(markup));
                        }
                    }
                } else if (b.kind == DocumentBlockKind::rule)
                    divider();
                else if (b.kind == DocumentBlockKind::code)
                    renderer.text({{DocumentInlineKind::code, b.text}});
                else
                    renderer.text(b.inlines);
                if (b.kind == DocumentBlockKind::table || b.kind == DocumentBlockKind::code ||
                    b.sourceBegin > b.sourceEnd) {
                    if (ImGui::BeginPopupContextItem("##blockActions")) {
                        if (ImGui::MenuItem(trLabel("Edit source").c_str())) {
                            mode = DocumentMode::source;
                            first = b.sourceBegin;
                            last = first;
                            source.native().SetCursor(position(current, first));
                        }
                        ImGui::EndPopup();
                    }
                }
                if (!renderer.result.resizedImage.empty() && !readOnly) {
                    for (const auto &run : b.inlines)
                        if (run.kind == DocumentInlineKind::image &&
                            run.target == renderer.result.resizedImage && run.sourceBegin < current.size()) {
                            auto start = current.rfind("![", run.sourceBegin);
                            auto end = current.find(']', run.sourceBegin);
                            if (start != std::string::npos && end != std::string::npos && start < end)
                                edits.emplace_back(start + 2, end,
                                                   escapeMarkdown(run.text) + "|" +
                                                       std::to_string(int(renderer.result.resizedWidth)));
                            break;
                        }
                    renderer.result.resizedImage.clear();
                }
                space(6);
                blockIndex += consumed;
                continue;
            }
            const float indent = dp(float(std::max(0, b.indent - 1)) * 18 + float(b.quoteDepth) * 12);
            if (indent > 0)
                ImGui::Indent(indent);
            if (b.task) {
                bool checked = b.checked;
                ImGui::BeginDisabled(readOnly);
                if (ImGui::Checkbox("##task", &checked) && !readOnly)
                    edits.emplace_back(b.taskOffset, b.taskOffset + 1, checked ? "x" : " ");
                ImGui::EndDisabled();
                ImGui::SameLine();
            } else if (b.kind == DocumentBlockKind::list) {
                if (b.number)
                    ImGui::Text("%u.", b.number);
                else
                    ImGui::Bullet();
                ImGui::SameLine();
            }
            const bool code = b.kind == DocumentBlockKind::code;
            if (code)
                textFaint("%s", b.language.c_str());
            RichText rich =
                cached
                    ? field.rich
                    : RichText{code
                                   ? std::vector<DocumentInline>{{DocumentInlineKind::code, b.text, "", false,
                                                                  documentMono, b.sourceBegin, b.sourceEnd}}
                                   : b.inlines};
            auto plain = rich.text();
            field.buffer.assign(plain.begin(), plain.end());
            field.buffer.resize(std::max<std::size_t>(256, plain.size() + 64), 0);
            const float scale = b.kind == DocumentBlockKind::heading ? (b.level == 1   ? 1.5f
                                                                        : b.level == 2 ? 1.25f
                                                                                       : 1.1f)
                                                                     : 1;
            ImGui::PushFont(code && monoFont()                                   ? monoFont()
                            : b.kind == DocumentBlockKind::heading && boldFont() ? boldFont()
                                                                                 : ImGui::GetFont(),
                            ImGui::GetFontSize() * scale);
            ScopedVar fieldPadding{ImGuiStyleVar_FramePadding, dp(0, 2)};
            const float width = std::max(dp(40), ImGui::GetContentRegionAvail().x);
            const auto pendingText = livePendingText(inputId, plain, code, readOnly || documentSelection);
            const float wrap = std::max(dp(1), width - ImGui::GetStyle().ScrollbarSize);
            const float height = std::clamp(float(liveLines(pendingText, wrap)) * ImGui::GetFontSize() +
                                                ImGui::GetStyle().FramePadding.y * 2,
                                            std::max(dp(20), ImGui::GetFontSize() + dp(4)), dp(900));
            auto sourceOffset = [&](std::size_t pos, bool right) {
                if (cached && !field.projection.empty())
                    return field.projection[std::min(pos, field.projection.size() - 1)];
                std::size_t sum = 0;
                for (const auto &run : rich.runs()) {
                    if (pos < sum + run.text.size() || (!right && pos == sum + run.text.size())) {
                        const auto local = pos - sum;
                        if (run.text.size() != run.sourceEnd - run.sourceBegin)
                            return local == 0 ? run.sourceBegin : run.sourceEnd;
                        return run.sourceBegin + local;
                    }
                    sum += run.text.size();
                }
                return !code && right && !synthetic ? blockRange(b, current).second : b.sourceEnd;
            };
            const auto focusRange =
                synthetic ? std::make_pair(b.sourceBegin, b.sourceEnd) : blockRange(b, current);
            if (!focusApplied && requestedFocus != std::string::npos && requestedFocus >= focusRange.first &&
                requestedFocus <= focusRange.second) {
                std::size_t cursor = 0;
                while (cursor < plain.size() && sourceOffset(cursor, true) < requestedFocus)
                    cursor = nextDocumentChar(plain, cursor);
                field.requestCursor = int(cursor);
                activationAt = requestedFocus;
                ImGui::SetKeyboardFocusHere();
                focusApplied = true;
            }
            if (!readOnly && (GImGui->ActiveId == inputId || field.requestCursor >= 0)) {
                const auto *input = ImGui::GetInputTextState(inputId);
                const auto cursor = std::size_t(field.requestCursor >= 0 ? field.requestCursor
                                                : input                  ? std::max(0, input->GetCursorPos())
                                                                         : 0);
                const bool collapsed = field.requestCursor >= 0 ||
                                       (input && input->GetSelectionStart() == input->GetSelectionEnd());
                if (collapsed && ImGui::IsKeyPressed(ImGuiKey_Enter) && !io.KeyShift && !code) {
                    auto at = cursor == plain.size() ? b.sourceEnd : sourceOffset(cursor, true);
                    if (cursor == plain.size() && !synthetic)
                        at = blockRange(b, current).second;
                    // InputText processes queued characters before Enter. The source split must
                    // use that same order when both arrive in one frame.
                    if (pendingText.size() > plain.size())
                        at += pendingText.size() - plain.size();
                    const auto newline = preserveCRLF ? "\r\n" : "\n";
                    std::string insert = std::string(newline) + newline;
                    if (b.kind == DocumentBlockKind::list && !pendingText.empty()) {
                        auto next = b;
                        if (next.number)
                            ++next.number;
                        next.checked = false;
                        insert = std::string(newline) + prefix(next);
                    }
                    if (b.kind == DocumentBlockKind::list && pendingText.empty()) {
                        const auto range = blockRange(b, current);
                        edits.emplace_back(range.first, range.second, "");
                        focusAt = emptyAt = range.first;
                    } else {
                        edits.emplace_back(at, at, insert);
                        focusAt = emptyAt = at + insert.size();
                    }
                    ImGui::SetKeyOwner(ImGuiKey_Enter, ImGui::GetID("##paragraph"),
                                       ImGuiInputFlags_LockThisFrame);
                }
                if (collapsed && cursor == 0 && blockIndex &&
                    ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
                    const auto &previous = blocks[blockIndex - 1];
                    if (previous.kind != DocumentBlockKind::table &&
                        previous.kind != DocumentBlockKind::code) {
                        const auto a = blockRange(previous, current);
                        const auto z =
                            synthetic ? std::make_pair(b.sourceBegin, b.sourceEnd) : blockRange(b, current);
                        const auto before = liveMarkup(previous, RichText(previous.inlines));
                        edits.emplace_back(a.first, z.second, before + liveMarkup(DocumentBlock{}, rich));
                        focusAt = a.first + before.size();
                        emptyAt = std::string::npos;
                        ImGui::SetKeyOwner(ImGuiKey_Backspace, ImGui::GetID("##join"),
                                           ImGuiInputFlags_LockThisFrame);
                    }
                }
                const bool back = collapsed && cursor == 0 &&
                                  (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) ||
                                   ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false));
                const bool forward = collapsed && cursor == plain.size() &&
                                     (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) ||
                                      ImGui::IsKeyPressed(ImGuiKey_RightArrow, false));
                if (back && blockIndex)
                    focusAt = blocks[blockIndex - 1].sourceEnd;
                if (forward && blockIndex + 1 < blocks.size())
                    focusAt = blocks[blockIndex + 1].sourceBegin;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, alpha(palette().text, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, alpha(palette().bg0, 0));
            ImGui::PushStyleColor(ImGuiCol_NavCursor, alpha(palette().accent, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            const bool edit = ImGui::InputTextMultiline(
                "##rich", field.buffer.data(), field.buffer.size(), {width, height},
                ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackResize |
                    ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_NoUndoRedo |
                    (readOnly || documentSelection ? ImGuiInputTextFlags_ReadOnly : 0),
                liveCallback, &field);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemClicked()) {
                documentSelection = false;
                gestureAnchor = sourceOffset(std::size_t(std::max(0, field.cursor)), true);
                gestureBounds = {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
                gestureArmed = true;
            }
            if (ImGui::IsItemActive() && !documentSelection) {
                activeInput = inputId;
                activationAt = std::string::npos;
                first = sourceOffset(std::size_t(std::max(0, field.first)), true);
                last = sourceOffset(std::size_t(std::max(0, field.last)), field.first == field.last);
            }
            if (edit) {
                const std::string next(field.buffer.data());
                std::size_t a = 0, z = 0;
                while (a < std::min(plain.size(), next.size()) && plain[a] == next[a])
                    ++a;
                while (a && !boundary(plain, a))
                    --a;
                while (z < plain.size() - a && z < next.size() - a &&
                       plain[plain.size() - 1 - z] == next[next.size() - 1 - z])
                    ++z;
                while (z && (!boundary(plain, plain.size() - z) || !boundary(next, next.size() - z)))
                    --z;
                // Map through the unmodified runs: replacement runs have no source offsets yet.
                const auto insertionAt = synthetic ? b.sourceBegin : sourceOffset(a, true);
                rich.replace(a, plain.size() - z, next.substr(a, next.size() - a - z));
                auto range =
                    synthetic ? std::make_pair(b.sourceBegin, b.sourceEnd)
                    : cached  ? std::make_pair(field.regionFirst, field.regionFirst + field.canonical.size())
                              : blockRange(b, current);
                auto replacement = liveMarkup(b, rich);
                // Insert typed Markdown into the canonical source, then project the caret back onto
                // visible text. This recognizes headings and delimiters instead of escaping them.
                const auto typed = next.substr(a, next.size() - a - z);
                if (!code && a == plain.size() - z) {
                    const auto at = insertionAt;
                    auto inserted = typed;
                    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Enter) && !inserted.empty() &&
                        inserted.back() == '\n')
                        inserted.replace(inserted.size() - 1, 1, "<br>");
                    if (synthetic && at && current.substr(0, at).find_last_not_of("\r\n") == at - 1)
                        inserted = "\n\n" + inserted;
                    if (preserveCRLF) {
                        std::string crlf;
                        for (char ch : inserted) {
                            if (ch == '\n' && (crlf.empty() || crlf.back() != '\r'))
                                crlf += '\r';
                            crlf += ch;
                        }
                        inserted = std::move(crlf);
                    }
                    edits.emplace_back(at, at, inserted);
                    auto canonical = current.substr(range.first, range.second - range.first);
                    canonical.insert(at - range.first, inserted);
                    Document parsed;
                    parsed.setMarkdown(canonical);
                    // Keep the native input/caret for ordinary typing and soft line breaks.
                    // Only reproject when Markdown actually changes the visible text or block type.
                    auto visible = next;
                    while (!visible.empty() && std::strchr(" \t\r\n", visible.back()))
                        visible.pop_back();
                    const auto parsedText = parsed.blocks().size() == 1
                                                ? RichText(parsed.blocks().front().inlines).text()
                                                : std::string{};
                    const bool sameText =
                        parsed.blocks().size() == 1 && (parsedText == next || parsedText == visible);
                    if ((sameText && parsed.blocks().front().kind == b.kind) ||
                        (parsed.blocks().empty() && visible.empty())) {
                        field.rich = sameText ? RichText(parsed.blocks().front().inlines) : rich;
                        if (sameText && parsedText.size() < next.size())
                            field.rich.replace(parsedText.size(), parsedText.size(),
                                               next.substr(parsedText.size()));
                        field.block = b;
                        field.block.sourceEnd = range.first + canonical.size();
                        field.canonical = std::move(canonical);
                        field.regionFirst = range.first;
                        field.cached = true;
                        pending = &field;
                    } else {
                        focusAt = at + inserted.size();
                        field.cached = false;
                    }
                } else {
                    if (code) {
                        if (!cached && range.first) {
                            auto previous = current.rfind('\n', range.first > 1 ? range.first - 2 : 0);
                            previous = previous == std::string::npos ? 0 : previous + 1;
                            const auto line = current.substr(previous, range.first - previous);
                            if (line.find("```") != std::string::npos ||
                                line.find("~~~") != std::string::npos)
                                range.first = previous;
                        }
                        std::string fence = "```";
                        while (next.find(fence) != std::string::npos)
                            fence += '`';
                        replacement = fence + b.language + "\n" + next +
                                      (next.empty() || next.back() != '\n' ? "\n" : "") + fence;
                    }
                    if (preserveCRLF) {
                        std::string crlf;
                        for (char ch : replacement) {
                            if (ch == '\n' && (crlf.empty() || crlf.back() != '\r'))
                                crlf += '\r';
                            crlf += ch;
                        }
                        replacement = std::move(crlf);
                    }
                    field.rich = rich;
                    field.block = b;
                    field.block.sourceEnd = range.first + replacement.size();
                    field.canonical = replacement;
                    field.regionFirst = range.first;
                    if (synthetic && range.first && current[range.first - 1] != '\n') {
                        field.canonical = "\n\n" + field.canonical;
                        replacement = field.canonical;
                    }
                    field.cached = true;
                    pending = &field;
                    edits.emplace_back(range.first, range.second, std::move(replacement));
                }
            }
            if (code &&
                (field.lexicalSource != rich.text() || field.languageRevision != codeLanguageRevision())) {
                field.lexicalSource = rich.text();
                field.languageRevision = codeLanguageRevision();
                field.tokens = highlightCode(field.lexicalSource, codeLanguageFromName(b.language));
            }
            decorateLive(inputId, rich, code ? &field.tokens : nullptr, code ? nullptr : &issues, &view,
                         &renderer.result, cached ? &field.projection : nullptr,
                         documentSelection ? std::make_pair(std::min(first, last), std::max(first, last))
                                           : std::pair<std::size_t, std::size_t>{});
            ImGui::PopFont();
            if (indent > 0)
                ImGui::Unindent(indent);
            if (b.kind != DocumentBlockKind::list)
                space(3);
            blockIndex += consumed;
        }
        if (gestureArmed && ImGui::IsMouseDragging(0) && !gestureBounds.Contains(io.MousePos) &&
            !view.hits.empty()) {
            const DocumentHit *nearest = &view.hits.front();
            float distance = FLT_MAX;
            for (const auto &hit : view.hits) {
                const auto dx = io.MousePos.x - hit.bounds.min.x, dy = io.MousePos.y - hit.bounds.min.y;
                const auto next = dx * dx + dy * dy * 16;
                if (next < distance) {
                    nearest = &hit;
                    distance = next;
                }
            }
            first = gestureAnchor;
            last = nearest->end;
            documentSelection = true;
            ImGui::ClearActiveID();
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
        }
        if (!ImGui::IsMouseDown(0))
            gestureArmed = false;
        if (doc.blocks().empty()) {
            if (ghostButton(trLabel("Start writing").c_str()) && !readOnly)
                replace(0, 0, tr("New paragraph").c_str());
        }
        ImGui::EndChild();
        for (auto it = edits.rbegin(); it != edits.rend(); ++it)
            replace(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
        if (focusAt != std::string::npos)
            first = last = focusAt;
        if (pending) {
            const auto updated = source.text();
            pending->cached =
                pending->regionFirst <= updated.size() &&
                updated.compare(pending->regionFirst, pending->canonical.size(), pending->canonical) == 0;
            if (pending->cached) {
                pending->revision = source.native().GetTextRevision();
                projectField(*pending);
                first = pending->projection[std::min<std::size_t>(std::size_t(std::max(0, pending->first)),
                                                                  pending->projection.size() - 1)];
                last = pending->projection[std::min<std::size_t>(std::size_t(std::max(0, pending->last)),
                                                                 pending->projection.size() - 1)];
            } else
                reload();
        }
        documentLightbox(view, options);
        return renderer.result;
    }
};
DocumentEditor::DocumentEditor() : impl_(new Impl) {}
DocumentEditor::~DocumentEditor() = default;
void DocumentEditor::setMarkdown(const std::string &value) {
    if (value.size() > 32 * 1024 * 1024)
        throw std::length_error("Document exceeds 32 MiB");
    impl_->preserveCRLF = value.find("\r\n") != std::string::npos;
    impl_->source.setText(value, true);
    impl_->parsedRevision = ~std::size_t(0);
    impl_->first = impl_->last = 0;
    impl_->fields.clear();
    impl_->focusAt = impl_->emptyAt = impl_->activationAt = std::string::npos;
    impl_->documentSelection = false;
    impl_->view = {};
    impl_->refresh();
    impl_->reload();
}
std::string DocumentEditor::markdown() const {
    return document().markdown();
}
const Document &DocumentEditor::document() const {
    impl_->refresh();
    return impl_->doc;
}
void DocumentEditor::setMode(DocumentMode mode) {
    auto &s = *impl_;
    if (s.mode != mode && (mode == DocumentMode::source || mode == DocumentMode::split)) {
        const auto &text = document().markdown();
        s.source.native().SelectRegion(position(text, s.first), position(text, s.last));
        s.source.native().ScrollToLine(position(text, s.first).line);
    }
    s.mode = mode;
}
DocumentMode DocumentEditor::mode() const {
    return impl_->mode;
}
bool DocumentEditor::select(std::size_t first, std::size_t last) {
    const auto &value = document().markdown();
    if (!boundary(value, first) || !boundary(value, last))
        return false;
    impl_->first = first;
    impl_->last = last;
    impl_->source.native().SelectRegion(position(value, first), position(value, last));
    return true;
}
std::pair<std::size_t, std::size_t> DocumentEditor::selection() const {
    return {impl_->first, impl_->last};
}
void DocumentEditor::focus() {
    impl_->focusAt = std::min(impl_->last, document().markdown().size());
    impl_->documentSelection = false;
    if (impl_->mode == DocumentMode::source || impl_->mode == DocumentMode::split)
        impl_->source.native().SetFocus();
}
bool DocumentEditor::insert(const std::string &value) {
    auto &s = *impl_;
    return s.replace(std::min(s.first, s.last), std::max(s.first, s.last), value);
}
bool DocumentEditor::canUndo() const {
    return impl_->source.native().CanUndo();
}
bool DocumentEditor::canRedo() const {
    return impl_->source.native().CanRedo();
}
bool DocumentEditor::command(DocumentCommand command, const std::string &argument) {
    auto &s = *impl_;
    if (command == DocumentCommand::table && !s.readOnly)
        return insert("\n\n| A | B |\n| --- | --- |\n|   |   |\n");
    if (command == DocumentCommand::find) {
        setMode(DocumentMode::source);
        s.source.native().OpenFindReplaceWindow();
        return true;
    }
    if (s.readOnly)
        return false;
    if (command == DocumentCommand::undo || command == DocumentCommand::redo) {
        if (command == DocumentCommand::undo)
            s.source.native().Undo();
        else
            s.source.native().Redo();
        s.changed = true;
        s.reload();
        return true;
    }
    const auto value = markdown();
    auto first = std::min(s.first, s.last), last = std::max(s.first, s.last);
    if (last > value.size())
        return false;
    const unsigned flag = command == DocumentCommand::bold        ? documentBold
                          : command == DocumentCommand::italic    ? documentItalic
                          : command == DocumentCommand::underline ? documentUnderline
                          : command == DocumentCommand::strike    ? documentStrike
                          : command == DocumentCommand::spoiler   ? documentSpoiler
                          : command == DocumentCommand::code      ? documentMono
                                                                  : 0;
    // In Live, change the rich spans instead of inserting delimiters into a possibly
    // escaped/entity-encoded source range. Source mode intentionally edits raw Markdown.
    if (s.mode == DocumentMode::live && first != last && (flag || command == DocumentCommand::link)) {
        s.refresh();
        for (std::size_t index = 0; index < s.doc.blocks().size(); ++index) {
            const auto &block = s.doc.blocks()[index];
            if (first < block.sourceBegin || last > block.sourceEnd ||
                block.kind == DocumentBlockKind::code || block.kind == DocumentBlockKind::table)
                continue;
            bool remove = flag != 0, found = false;
            for (const auto &run : block.inlines)
                if (run.sourceEnd > first && run.sourceBegin < last) {
                    found = true;
                    remove = remove && (run.style & flag) != 0;
                }
            if (!found)
                break;
            std::vector<DocumentInline> runs;
            std::size_t flat = 0, visibleFirst = 0, visibleLast = 0;
            bool haveFirst = false;
            for (const auto &run : block.inlines) {
                if (run.sourceEnd <= first || run.sourceBegin >= last ||
                    run.kind == DocumentInlineKind::image) {
                    runs.push_back(run);
                    flat += run.text.size();
                    continue;
                }
                const bool exact = run.sourceEnd - run.sourceBegin == run.text.size();
                const auto a = exact && first > run.sourceBegin ? first - run.sourceBegin : 0;
                const auto b = exact ? std::min(run.text.size(), last - run.sourceBegin) : run.text.size();
                if (!haveFirst) {
                    visibleFirst = flat + a;
                    haveFirst = true;
                }
                visibleLast = flat + b;
                if (a) {
                    auto copy = run;
                    copy.text.resize(a);
                    runs.push_back(copy);
                }
                auto selected = run;
                selected.text = run.text.substr(a, b - a);
                if (flag)
                    selected.style = remove ? selected.style & ~flag : selected.style | flag;
                else
                    selected.target = argument;
                runs.push_back(std::move(selected));
                if (b < run.text.size()) {
                    auto copy = run;
                    copy.text.erase(0, b);
                    runs.push_back(copy);
                }
                flat += run.text.size();
            }
            const auto range = blockRange(block, value);
            const auto replacement = liveMarkup(block, RichText(std::move(runs)));
            if (!s.replace(range.first, range.second, replacement))
                return false;
            s.refresh();
            if (index < s.doc.blocks().size()) {
                const auto &updated = s.doc.blocks()[index];
                auto sourcePosition = [&](std::size_t position, bool right) {
                    std::size_t cursor = 0;
                    for (const auto &run : updated.inlines) {
                        if (position < cursor + run.text.size() ||
                            (!right && position == cursor + run.text.size()))
                            return run.sourceBegin +
                                   std::min(position - cursor, run.sourceEnd - run.sourceBegin);
                        cursor += run.text.size();
                    }
                    return updated.sourceEnd;
                };
                s.first = sourcePosition(visibleFirst, true);
                s.last = sourcePosition(visibleLast, false);
            }
            s.reload();
            return true;
        }
    }
    std::string open, close;
    switch (command) {
    case DocumentCommand::bold:
        open = close = "**";
        break;
    case DocumentCommand::italic:
        open = close = "*";
        break;
    case DocumentCommand::underline:
        open = "<u>";
        close = "</u>";
        break;
    case DocumentCommand::strike:
        open = close = "~~";
        break;
    case DocumentCommand::spoiler:
        open = close = "||";
        break;
    case DocumentCommand::code:
        open = close = "`";
        break;
    case DocumentCommand::link:
        open = "[";
        close = "](<" + argument + ">)";
        break;
    default: {
        const auto start = first ? value.rfind('\n', first - 1) : std::string::npos;
        first = start == std::string::npos ? 0 : start + 1;
        auto end = value.find('\n', last);
        last = end == std::string::npos ? value.size() : end;
        const char *mark = command == DocumentCommand::heading1   ? "# "
                           : command == DocumentCommand::heading2 ? "## "
                           : command == DocumentCommand::heading3 ? "### "
                           : command == DocumentCommand::bullet   ? "- "
                           : command == DocumentCommand::numbered ? "1. "
                           : command == DocumentCommand::task     ? "- [ ] "
                           : command == DocumentCommand::quote    ? "> "
                                                                  : "  ";
        std::string replacement;
        std::istringstream lines(value.substr(first, last - first));
        std::string line;
        while (std::getline(lines, line)) {
            if (!replacement.empty())
                replacement += '\n';
            if (command == DocumentCommand::outdent) {
                if (line.rfind("  ", 0) == 0)
                    line.erase(0, 2);
                replacement += line;
            } else
                replacement += std::string(mark) + line;
        }
        return s.replace(first, last, replacement);
    }
    }
    const auto selected = first == last ? tr("Text") : value.substr(first, last - first);
    if (first >= open.size() && value.compare(first - open.size(), open.size(), open) == 0 &&
        value.compare(last, close.size(), close) == 0)
        return s.replace(first - open.size(), last + close.size(), selected);
    const bool ok = s.replace(first, last, open + selected + close);
    if (ok) {
        s.first = first + open.size();
        s.last = s.first + selected.size();
        s.reload();
    }
    return ok;
}
DocumentEditorResult DocumentEditor::draw(const char *id, ImVec2 size, const DocumentEditorOptions &options) {
    ScopedID scope{id};
    auto &s = *impl_;
    s.readOnly = options.readOnly;
    s.source.setReadOnly(options.readOnly);
    s.refresh();
    DocumentEditorResult result;
    if (size.y <= 0)
        size.y = dp(480);
    if (size.x <= 0)
        size.x = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##editor", size);
    if (options.spelling &&
        (s.spellingDocument != s.doc.revision() || s.spellingRevision != options.spellingRevision ||
         s.spellingLanguage != options.spellingLanguage)) {
        s.issues = options.spelling(s.doc.markdown(), options.spellingLanguage);
        if (s.issues.size() > 4096)
            s.issues.resize(4096);
        const auto &raw = s.doc.markdown();
        s.issues.erase(std::remove_if(s.issues.begin(), s.issues.end(),
                                      [&](const auto &issue) {
                                          return issue.begin >= issue.end || !boundary(raw, issue.begin) ||
                                                 !boundary(raw, issue.end);
                                      }),
                       s.issues.end());
        s.spellingDocument = s.doc.revision();
        s.spellingRevision = options.spellingRevision;
        s.spellingLanguage = options.spellingLanguage;
    } else if (!options.spelling)
        s.issues.clear();
    if (options.modeSelector) {
        const char *labels[]{"Live", "Side by side", "Source", "Reading"};
        int mode = int(s.mode);
        ImGui::SetNextItemWidth(dp(170));
        std::vector<std::string> translated;
        const char *items[4];
        for (int n = 0; n < 4; ++n) {
            translated.push_back(tr(labels[n]));
        }
        for (int n = 0; n < 4; ++n)
            items[n] = translated[n].c_str();
        if (ImGui::Combo("##mode", &mode, items, 4))
            setMode(DocumentMode(mode));
    }
    bool linkRequested = s.openLink;
    s.openLink = false;
    if (options.toolbar) {
        ImGui::BeginDisabled(options.readOnly);
        const std::pair<const char *, DocumentCommand> buttons[]{{"Bold", DocumentCommand::bold},
                                                                 {"Italic", DocumentCommand::italic},
                                                                 {"Underline", DocumentCommand::underline},
                                                                 {"Strikethrough", DocumentCommand::strike},
                                                                 {"Spoiler", DocumentCommand::spoiler},
                                                                 {"Code", DocumentCommand::code},
                                                                 {"Task list", DocumentCommand::task},
                                                                 {"Undo", DocumentCommand::undo},
                                                                 {"Redo", DocumentCommand::redo}};
        const char *glyphs[]{"B", "I", "U", "S", "||", "</>", icon::check, icon::undo, icon::redo};
        if (options.modeSelector)
            ImGui::SameLine();
        for (std::size_t n = 0; n < std::size(buttons); ++n) {
            if (n)
                ImGui::SameLine(0, dp(2));
            if (iconButton(glyphs[n], tr(buttons[n].first).c_str()))
                command(buttons[n].second);
        }
        ImGui::SameLine(0, dp(2));
        if (iconButton(icon::more, tr("More formatting").c_str()))
            ImGui::OpenPopup("##formatMore");
        if (ImGui::BeginPopup("##formatMore")) {
            const std::pair<const char *, DocumentCommand> extra[]{
                {"Heading 1", DocumentCommand::heading1},     {"Heading 2", DocumentCommand::heading2},
                {"Heading 3", DocumentCommand::heading3},     {"Bullet list", DocumentCommand::bullet},
                {"Numbered list", DocumentCommand::numbered}, {"Quote", DocumentCommand::quote},
                {"Indent", DocumentCommand::indent},          {"Outdent", DocumentCommand::outdent},
                {"Insert table", DocumentCommand::table}};
            for (const auto &item : extra)
                if (ImGui::Selectable(tr(item.first).c_str()))
                    command(item.second);
            if (ImGui::Selectable(tr("Insert link").c_str()))
                linkRequested = true;
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }
    if (!s.issues.empty()) {
        if (ghostButton(trLabel("Spelling suggestions").c_str()))
            ImGui::OpenPopup("##spelling");
        if (ImGui::BeginPopup("##spelling")) {
            ImGui::BeginDisabled(options.readOnly);
            for (std::size_t n = 0; n < s.issues.size(); ++n) {
                ScopedID issueId{int(n)};
                const auto &issue = s.issues[n];
                if (ImGui::BeginMenu(issue.word.c_str())) {
                    for (std::size_t j = 0; j < std::min<std::size_t>(16, issue.suggestions.size()); ++j)
                        if (ImGui::Selectable(issue.suggestions[j].c_str())) {
                            s.replace(issue.begin, issue.end, issue.suggestions[j]);
                            s.reload();
                        }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        auto &io = ImGui::GetIO();
        const bool mod = io.KeyCtrl || io.KeySuper;
        if (mod) {
            if (ImGui::IsKeyPressed(ImGuiKey_B))
                command(DocumentCommand::bold);
            if (ImGui::IsKeyPressed(ImGuiKey_I))
                command(DocumentCommand::italic);
            if (ImGui::IsKeyPressed(ImGuiKey_U))
                command(DocumentCommand::underline);
            if (ImGui::IsKeyPressed(ImGuiKey_E))
                command(DocumentCommand::code);
            if (ImGui::IsKeyPressed(ImGuiKey_K))
                linkRequested = true;
            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_X))
                command(DocumentCommand::strike);
            if (s.mode == DocumentMode::live && ImGui::IsKeyPressed(ImGuiKey_Y))
                command(DocumentCommand::redo);
            if (s.mode == DocumentMode::live && ImGui::IsKeyPressed(ImGuiKey_Z))
                command(io.KeyShift ? DocumentCommand::redo : DocumentCommand::undo);
            if (ImGui::IsKeyPressed(ImGuiKey_F))
                command(DocumentCommand::find);
            bool pasted = false;
            if (!options.readOnly && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_V) && options.pasteRich) {
                const auto value = options.pasteRich();
                if (!value.empty()) {
                    ImGui::SetKeyOwner(ImGuiKey_V, ImGui::GetID("##richPaste"),
                                       ImGuiInputFlags_LockThisFrame);
                    insert(value);
                    s.reload();
                    pasted = true;
                }
            }
            if (!pasted && !options.readOnly && !io.KeyShift && s.first != s.last &&
                ImGui::IsKeyPressed(ImGuiKey_V)) {
                const char *clipboard = ImGui::GetClipboardText();
                const std::string url = clipboard ? clipboard : "";
                if ((url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0) &&
                    url.find_first_of("\r\n<> ") == std::string::npos) {
                    ImGui::SetKeyOwner(ImGuiKey_V, ImGui::GetID("##urlPaste"), ImGuiInputFlags_LockThisFrame);
                    command(DocumentCommand::link, url);
                }
            }
            if (io.KeyShift) {
                if (ImGui::IsKeyPressed(ImGuiKey_1))
                    command(DocumentCommand::heading1);
                if (ImGui::IsKeyPressed(ImGuiKey_2))
                    command(DocumentCommand::heading2);
                if (ImGui::IsKeyPressed(ImGuiKey_3))
                    command(DocumentCommand::heading3);
                if (ImGui::IsKeyPressed(ImGuiKey_7))
                    command(DocumentCommand::numbered);
                if (ImGui::IsKeyPressed(ImGuiKey_8))
                    command(DocumentCommand::bullet);
                if (ImGui::IsKeyPressed(ImGuiKey_9))
                    command(DocumentCommand::task);
                if (ImGui::IsKeyPressed(ImGuiKey_Period))
                    command(DocumentCommand::quote);
            } else {
                if (ImGui::IsKeyPressed(ImGuiKey_1))
                    setMode(DocumentMode::live);
                if (ImGui::IsKeyPressed(ImGuiKey_2))
                    setMode(DocumentMode::split);
                if (ImGui::IsKeyPressed(ImGuiKey_3))
                    setMode(DocumentMode::source);
                if (ImGui::IsKeyPressed(ImGuiKey_4))
                    setMode(DocumentMode::reading);
            }
        }
    }
    if (linkRequested && !options.readOnly)
        ImGui::OpenPopup("##insertLink");
    if (ImGui::BeginPopup("##insertLink")) {
        ImGui::InputText(trLabel("URL").c_str(), s.link.data(), s.link.size());
        if (primaryButton(trLabel("Insert").c_str())) {
            command(DocumentCommand::link, s.link.data());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    auto room = ImGui::GetContentRegionAvail();
    s.refresh();
    auto docOptions = options.document;
    docOptions.tasks = !options.readOnly;
    docOptions.resizeImages = !options.readOnly;
    if (s.mode == DocumentMode::live)
        result.document = s.live("##live", room, docOptions);
    else if (s.mode == DocumentMode::reading)
        result.document = documentView("##read", s.view, s.doc, room, docOptions);
    else {
        s.source.native().SetShowLineNumbersEnabled(options.lineNumbers);
        const bool split = s.mode == DocumentMode::split;
        const auto before = s.source.native().GetTextRevision();
        s.source.draw("##source", {split ? room.x * .5f : room.x, room.y});
        const auto selection = s.source.native().GetMainCursorSelection();
        s.refresh();
        if (s.selectionRevision != s.parsedRevision || s.sourceSelection.start != selection.start ||
            s.sourceSelection.end != selection.end) {
            s.sourceFirst = s.offset(selection.start);
            s.sourceLast = selection.start == selection.end ? s.sourceFirst : s.offset(selection.end);
            s.sourceSelection = selection;
            s.selectionRevision = s.parsedRevision;
        }
        s.first = s.sourceFirst;
        s.last = s.sourceLast;
        if (before != s.parsedRevision)
            s.changed = true;
        if (split) {
            ImGui::SameLine();
            docOptions.scrollToSource =
                s.offset(s.source.native().VisPos2DocPos({s.source.native().GetFirstVisibleRow(), 0}));
            result.document = documentView(
                "##preview", s.view, s.doc,
                {std::max(dp(1), room.x * .5f - ImGui::GetStyle().ItemSpacing.x), room.y}, docOptions);
        }
    }
    if (result.document.taskOffset != std::string::npos && !options.readOnly)
        s.replace(result.document.taskOffset, result.document.taskOffset + 1,
                  result.document.taskChecked ? "x" : " ");
    if (options.drop && !options.readOnly) {
        auto *window = ImGui::GetCurrentWindow();
        if (ImGui::BeginDragDropTargetCustom(window->InnerRect, ImGui::GetID("##documentDrop"))) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(options.dropType.c_str())) {
                const auto value = options.drop(
                    {static_cast<const unsigned char *>(payload->Data), std::size_t(payload->DataSize)});
                if (!value.empty())
                    insert(value);
            }
            ImGui::EndDragDropTarget();
        }
    }
    const auto formatName = "##documentFormat" + std::to_string(ImGui::GetID("##editor"));
    const auto *formatWindow = ImGui::FindWindowByName(formatName.c_str());
    const bool formatFocused =
        formatWindow && GImGui->NavWindow && formatWindow->RootWindow == GImGui->NavWindow->RootWindow;
    if (options.floatingToolbar && !options.readOnly && s.mode == DocumentMode::live && s.first != s.last &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) || formatFocused)) {
        const auto selection = std::make_pair(s.first, s.last);
        const auto *viewport = ImGui::GetMainViewport();
        if (selection != s.floatingSelection) {
            s.floatingSelection = selection;
            const auto mouse = ImGui::GetIO().MousePos;
            s.floatingPosition = {
                std::clamp(mouse.x, viewport->WorkPos.x,
                           viewport->WorkPos.x + std::max(0.f, viewport->WorkSize.x - dp(330))),
                std::clamp(mouse.y - dp(42), viewport->WorkPos.y,
                           viewport->WorkPos.y + std::max(0.f, viewport->WorkSize.y - dp(50)))};
        }
        ImGui::SetNextWindowPos(s.floatingPosition, ImGuiCond_Always);
        if (ImGui::Begin(formatName.c_str(), nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            const char *glyphs[]{"B", "I", "U", "S", "</>", "||", icon::connect};
            const char *tips[]{"Bold", "Italic",  "Underline",  "Strikethrough",
                               "Code", "Spoiler", "Insert link"};
            const DocumentCommand commands[]{DocumentCommand::bold,      DocumentCommand::italic,
                                             DocumentCommand::underline, DocumentCommand::strike,
                                             DocumentCommand::code,      DocumentCommand::spoiler,
                                             DocumentCommand::link};
            for (std::size_t n = 0; n < std::size(commands); ++n) {
                if (n)
                    ImGui::SameLine(0, dp(2));
                if (iconButton(glyphs[n], tr(tips[n]).c_str())) {
                    if (commands[n] == DocumentCommand::link) {
                        s.openLink = true;
                    } else
                        command(commands[n]);
                }
            }
        }
        ImGui::End();
    }
    result.changed = s.changed;
    s.changed = false;
    ImGui::EndChild();
    return result;
}
} // namespace cyui

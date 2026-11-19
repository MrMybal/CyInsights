#include "cyui/modules/PagedCodeEditor.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace cyui {
namespace {
std::uint64_t plus(std::uint64_t a, std::uint64_t b) {
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}
std::uint64_t shifted(std::uint64_t value, std::int64_t delta, std::uint64_t maximum) {
    if (delta < 0) return std::uint64_t(-delta) > value ? 0 : value - std::uint64_t(-delta);
    return std::min(maximum, plus(value, std::uint64_t(delta)));
}
std::uint64_t fromRatio(double value, std::uint64_t maximum) {
    if (!(value > 0)) return 0;
    const auto position = static_cast<long double>(value) * maximum;
    return value >= 1 || position >= maximum ? maximum : static_cast<std::uint64_t>(position);
}
void utf8(std::string &text, std::uint32_t c) {
    if (c < 0x80) text += char(c);
    else if (c < 0x800) { text += char(0xc0 | (c >> 6)); text += char(0x80 | (c & 63)); }
    else if (c < 0x10000) {
        text += char(0xe0 | (c >> 12)); text += char(0x80 | ((c >> 6) & 63)); text += char(0x80 | (c & 63));
    } else if (c <= 0x10ffff) {
        text += char(0xf0 | (c >> 18)); text += char(0x80 | ((c >> 12) & 63));
        text += char(0x80 | ((c >> 6) & 63)); text += char(0x80 | (c & 63));
    }
}
bool validInsertion(std::string_view text) {
    std::size_t at = 0;
    while (at <= text.size()) {
        auto end = text.find_first_of("\r\n", at);
        if (end == std::string_view::npos) end = text.size();
        PagedCodeLineBuilder builder(0, 0, 1);
        PagedCodeLine line;
        if (!builder.append(text.substr(at, end - at)) || !builder.finish(line)) return false;
        if (end == text.size()) break;
        at = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
    }
    return true;
}
} // namespace

struct PagedCodeEditor::Impl {
    struct Row { PagedCodeLine line; std::vector<PagedCodeGlyph> glyphs; };
    PagedCodeDocument document;
    PagedCodeEditorOptions options;
    PagedCodeSelection selection{}, dragStart{};
    Edit<PagedCodeSelection> selectionEvent;
    Edit<PagedCodeReplacement> editEvent;
    std::optional<PagedCodeReplacement> waitingEdit;
    std::optional<PagedCodeRead> reading, readEvent;
    std::vector<Row> rows;
    std::vector<PagedCodeAction> actions;
    std::deque<PagedCodeInput> inputs;
    std::vector<PagedCodeInput> cancelledInput;
    std::size_t queuedBytes = 0;
    std::uint64_t serial = 0, cancelledRead = 0, firstLine = 0, firstColumn = 0;
    std::uint64_t preferredColumn = 0;
    unsigned visibleLines = 20, visibleColumns = 100;
    bool dragging = false, reveal = false, failed = false, accepting = false;
    struct Navigation { std::uint64_t line; bool extend = false, end = false; };
    std::optional<Navigation> navigation;
    struct Horizontal { int direction; bool extend; };
    std::optional<Horizontal> pendingHorizontal;
    bool deleting = false;
    std::string error;

    bool valid() const { return document.id && document.revision && document.lines; }
    std::uint64_t next() {
        if (serial == UINT64_MAX) { error = "View request identifiers exhausted"; return 0; }
        return ++serial;
    }
    bool position(PagedCodePosition p) const { return valid() && p.line < document.lines; }
    const Row *row(std::uint64_t line) const {
        if (rows.empty() || line < rows.front().line.line || line - rows.front().line.line >= rows.size()) return nullptr;
        return &rows[std::size_t(line - rows.front().line.line)];
    }
    std::optional<std::uint64_t> column(PagedCodePosition p) const {
        const auto *value = row(p.line);
        if (!value || p.character > value->line.characters) return {};
        if (p.character == value->line.characters) return value->line.columns;
        for (const auto &glyph : value->glyphs) if (glyph.character == p.character) return glyph.column;
        return {};
    }
    std::optional<PagedCodePosition> hit(const Row &value, std::uint64_t column) const {
        if (column < value.line.firstColumn) return {};
        for (const auto &glyph : value.glyphs) {
            if (column < plus(glyph.column, (glyph.cells + 1) / 2)) return PagedCodePosition{value.line.line, glyph.character};
            if (column < plus(glyph.column, glyph.cells)) return PagedCodePosition{value.line.line, plus(glyph.character, glyph.characters)};
        }
        if (column >= value.line.columns) return PagedCodePosition{value.line.line, value.line.characters};
        return {};
    }
    void cancelRead() {
        if (reading) cancelledRead = reading->id;
        reading.reset(); readEvent.reset();
    }
    void moved(std::uint64_t line, std::uint64_t column) {
        line = valid() ? std::min(line, document.lines - 1) : 0;
        if (line != firstLine || column != firstColumn) {
            firstLine = line; firstColumn = column; failed = false; reveal = false; cancelRead();
        }
    }
    void caret(PagedCodePosition point, bool extend, EditPhase phase = EditPhase::commit) {
        if (!position(point)) return;
        if (!extend) selection.anchor = point;
        selection.caret = point;
        selectionEvent = {phase, selection};
        if (const auto c = column(point)) preferredColumn = *c;
        if (point.line < firstLine) moved(point.line, firstColumn);
        if (point.line >= plus(firstLine, visibleLines)) moved(point.line - visibleLines + 1, firstColumn);
        if (const auto c = column(point)) {
            if (*c < firstColumn) moved(firstLine, *c);
            if (*c >= plus(firstColumn, visibleColumns)) moved(firstLine, *c - visibleColumns + 1);
        } else { reveal = true; cancelRead(); }
    }
    void navigate(std::uint64_t line, bool extend, bool end = false) {
        if (!valid()) return;
        line = std::min(line, document.lines - 1);
        if (const auto *value = row(line)) {
            const auto wanted = std::min(preferredColumn, value->line.columns);
            const auto keep = preferredColumn;
            if (end) { caret({line, value->line.characters}, extend); return; }
            if (const auto point = hit(*value, wanted)) { caret(*point, extend); preferredColumn = keep; }
            else {
                navigation = Navigation{line, extend, end};
                reveal = false; cancelRead();
                moved(line > visibleLines / 2 ? line - visibleLines / 2 : 0,
                      wanted > visibleColumns / 2 ? wanted - visibleColumns / 2 : 0);
            }
        } else {
            navigation = Navigation{line, extend, end};
            reveal = false; cancelRead();
            moved(line > visibleLines / 2 ? line - visibleLines / 2 : 0,
                  preferredColumn > visibleColumns / 2 ? preferredColumn - visibleColumns / 2 : 0);
        }
    }
    void horizontal(int direction, bool extend) {
        if (!selection.empty() && !extend) { caret(direction < 0 ? selection.first() : selection.last(), false); return; }
        const auto point = selection.caret;
        const auto *value = row(point.line);
        if (!value || point.line < firstLine || point.line >= plus(firstLine, visibleLines)) {
            pendingHorizontal = Horizontal{direction, extend};
            moved(point.line > visibleLines / 2 ? point.line - visibleLines / 2 : 0, firstColumn);
            reveal = true; cancelRead(); return;
        }
        if (direction < 0 && !point.character) {
            if (point.line) navigate(point.line - 1, extend, true);
            return;
        }
        if (direction > 0 && point.character == value->line.characters) {
            if (point.line + 1 < document.lines) caret({point.line + 1, 0}, extend);
            return;
        }
        for (const auto &glyph : value->glyphs) {
            if (direction > 0 && glyph.character == point.character) {
                caret({point.line, plus(glyph.character, glyph.characters)}, extend); return;
            }
            if (direction < 0 && plus(glyph.character, glyph.characters) == point.character) {
                caret({point.line, glyph.character}, extend); return;
            }
        }
        // The adjacent scalar is outside this fragment: query the neighbouring cells.
        moved(firstLine, direction < 0 ? shifted(firstColumn, -std::int64_t(visibleColumns / 2 + 1), UINT64_MAX) :
              plus(firstColumn, visibleColumns / 2 + 1));
        pendingHorizontal = Horizontal{direction, extend}; reveal = true;
    }
    bool replacement(std::string_view text) {
        if (!valid() || document.readOnly || waitingEdit || text.size() > options.maxInsertionBytes ||
            (selection.empty() && text.empty())) return false;
        if (!validInsertion(text)) { error = "Replacement is not valid UTF-8 text"; return false; }
        const auto id = next(); if (!id) return false;
        waitingEdit = PagedCodeReplacement{id, document.id, document.revision, selection, std::string(text)};
        editEvent = {EditPhase::commit, *waitingEdit}; error.clear();
        return true;
    }
    bool action(PagedCodeCommand command) {
        if (!valid() || actions.size() >= 16) return false;
        if (document.readOnly && (command == PagedCodeCommand::cut || command == PagedCodeCommand::undo ||
                                  command == PagedCodeCommand::redo || command == PagedCodeCommand::save)) return false;
        if (waitingEdit && (command == PagedCodeCommand::cut || command == PagedCodeCommand::undo ||
                            command == PagedCodeCommand::redo || command == PagedCodeCommand::save)) return false;
        const auto id = next(); if (!id) return false;
        actions.push_back({id, document.id, document.revision, selection, command}); return true;
    }
    void cancelInputs() {
        while (!inputs.empty()) { cancelledInput.push_back(std::move(inputs.front())); inputs.pop_front(); }
        queuedBytes = 0; pendingHorizontal.reset(); deleting = false;
    }
    void enqueue(PagedCodeInput input) {
        if (inputs.size() >= 256 || input.text.size() > options.maxInsertionBytes - std::min(queuedBytes, options.maxInsertionBytes)) {
            error = "Input queue is full; recover the rejected input from cancelledInput";
            // The rejected event is surfaced once along with any later host cancellation.
            cancelledInput.push_back(std::move(input)); return;
        }
        queuedBytes += input.text.size(); inputs.push_back(std::move(input));
    }
    void finishDeletion() {
        if (deleting && !navigation && !pendingHorizontal) {
            deleting = false;
            if (!selection.empty()) replacement({});
        }
    }
    void processInput() {
        if (waitingEdit || navigation || pendingHorizontal || inputs.empty() || !valid()) return;
        auto input = std::move(inputs.front()); inputs.pop_front(); queuedBytes -= input.text.size();
        switch (input.kind) {
        case PagedCodeInputKind::text:
            if (!replacement(input.text) && !input.text.empty()) cancelledInput.push_back(std::move(input));
            break;
        case PagedCodeInputKind::action: action(input.action); break;
        case PagedCodeInputKind::left: horizontal(-1, input.extend); break;
        case PagedCodeInputKind::right: horizontal(1, input.extend); break;
        case PagedCodeInputKind::up: navigate(shifted(selection.caret.line, -1, UINT64_MAX), input.extend); break;
        case PagedCodeInputKind::down: navigate(plus(selection.caret.line, 1), input.extend); break;
        case PagedCodeInputKind::pageUp: navigate(shifted(selection.caret.line, -std::int64_t(visibleLines), UINT64_MAX), input.extend); break;
        case PagedCodeInputKind::pageDown: navigate(plus(selection.caret.line, visibleLines), input.extend); break;
        case PagedCodeInputKind::home: caret({selection.caret.line, 0}, input.extend); break;
        case PagedCodeInputKind::end: navigate(selection.caret.line, input.extend, true); break;
        case PagedCodeInputKind::documentHome: caret({0, 0}, input.extend); break;
        case PagedCodeInputKind::documentEnd: navigate(document.lines - 1, input.extend, true); break;
        case PagedCodeInputKind::selectAll: selection.anchor = {}; navigate(document.lines - 1, true, true); break;
        case PagedCodeInputKind::backspace:
        case PagedCodeInputKind::deleteForward:
            if (document.readOnly) break;
            if (!selection.empty()) replacement({});
            else {
                deleting = true;
                horizontal(input.kind == PagedCodeInputKind::backspace ? -1 : 1, true);
                finishDeletion();
            }
            break;
        }
    }
    bool cached() const {
        if (reveal || rows.empty() || firstLine < rows.front().line.line) return false;
        const auto count = std::min<std::uint64_t>(visibleLines, document.lines - firstLine);
        if (firstLine - rows.front().line.line + count > rows.size()) return false;
        for (std::uint64_t n = 0; n < count; ++n) {
            const auto &value = *row(firstLine + n);
            const auto end = value.glyphs.empty() ? value.line.firstColumn :
                plus(value.glyphs.back().column, value.glyphs.back().cells);
            if (value.line.firstColumn > firstColumn || end < std::min(plus(firstColumn, visibleColumns), value.line.columns)) return false;
        }
        return true;
    }
    void query() {
        if (!valid() || failed || cached()) return;
        auto begin = firstLine > 4 ? firstLine - 4 : 0;
        const auto count = unsigned(std::min<std::uint64_t>(document.lines - begin, visibleLines + 8));
        const auto columns = std::min(8192u, visibleColumns + 16);
        if (reading && reading->firstLine == begin && reading->lines == count &&
            reading->firstColumn == firstColumn && reading->columns == columns &&
            bool(reading->reveal) == reveal && (!reveal || *reading->reveal == selection.caret)) return;
        cancelRead();
        const auto id = next(); if (!id) return;
        reading = PagedCodeRead{id, document.id, document.revision, begin, firstColumn, count, columns,
                               options.tabSize, options.maxPageBytes, reveal ? std::optional<PagedCodePosition>(selection.caret) : std::nullopt};
        readEvent = reading;
    }
};

PagedCodeEditor::PagedCodeEditor() : impl_(std::make_unique<Impl>()) {}
PagedCodeEditor::~PagedCodeEditor() = default;
PagedCodeDocument PagedCodeEditor::document() const { return impl_->document; }
PagedCodeEditorOptions PagedCodeEditor::options() const { return impl_->options; }
PagedCodeSelection PagedCodeEditor::selection() const { return impl_->selection; }
std::uint64_t PagedCodeEditor::firstLine() const { return impl_->firstLine; }
std::uint64_t PagedCodeEditor::firstColumn() const { return impl_->firstColumn; }
std::optional<PagedCodeRead> PagedCodeEditor::pendingRead() const { return impl_->reading; }
void PagedCodeEditor::setDocument(PagedCodeDocument value) {
    auto &s = *impl_;
    const bool identity = value.id != s.document.id;
    const bool changed = identity || value.revision != s.document.revision || value.lines != s.document.lines;
    if (changed || (value.readOnly && !s.document.readOnly)) {
        s.cancelRead(); s.rows.clear(); s.actions.clear(); s.navigation.reset(); s.cancelInputs();
        if (s.waitingEdit) { s.editEvent = {EditPhase::cancel, *s.waitingEdit}; s.waitingEdit.reset(); }
        if (s.dragging) s.selectionEvent = {EditPhase::cancel, s.dragStart};
        s.dragging = false; s.failed = false; s.error.clear();
    }
    s.document = value;
    if (identity || !s.valid()) { s.selection = {}; s.firstLine = s.firstColumn = s.preferredColumn = 0; s.reveal = false; }
    else if (changed) {
        s.selection.anchor.line = std::min(s.selection.anchor.line, value.lines - 1);
        s.selection.caret.line = std::min(s.selection.caret.line, value.lines - 1);
        s.selection.anchor = s.selection.caret;
        s.firstLine = std::min(s.firstLine, value.lines - 1); s.reveal = true;
    }
}
bool PagedCodeEditor::setOptions(const PagedCodeEditorOptions &value) {
    auto &s = *impl_;
    if (value.tabSize < 1 || value.tabSize > 16 || !value.maxPageBytes || value.maxPageBytes > 4 * 1024 * 1024 ||
        !value.maxInsertionBytes || value.maxInsertionBytes > 4 * 1024 * 1024 ||
        (value.newline != "\n" && value.newline != "\r" && value.newline != "\r\n")) return false;
    if (s.options.tabSize != value.tabSize || s.options.maxPageBytes != value.maxPageBytes) {
        s.cancelRead(); s.rows.clear(); s.failed = false;
    }
    s.options = value;
    s.options.textScale = std::isfinite(value.textScale) ? std::clamp(value.textScale, .5f, 3.f) : 1.f;
    return true;
}
bool PagedCodeEditor::setSelection(PagedCodeSelection value, bool reveal) {
    auto &s = *impl_;
    if (!s.position(value.anchor) || !s.position(value.caret) || s.waitingEdit) return false;
    for (const auto point : {value.anchor, value.caret}) {
        if (const auto *row = s.row(point.line)) {
            if (point.character > row->line.characters) return false;
            for (const auto &glyph : row->glyphs)
                if (point.character > glyph.character && point.character < plus(glyph.character, glyph.characters)) return false;
        }
    }
    if (!s.accepting) s.cancelInputs();
    s.selection = value; s.reveal = reveal; s.failed = false;
    s.selectionEvent = {EditPhase::commit, value};
    if (reveal) {
        s.moved(value.caret.line > s.visibleLines / 2 ? value.caret.line - s.visibleLines / 2 : 0, s.firstColumn);
        s.reveal = true; s.cancelRead();
    }
    return true;
}
void PagedCodeEditor::scrollTo(std::uint64_t line, std::uint64_t column) {
    impl_->reveal = false; impl_->navigation.reset(); impl_->moved(line, column);
}
bool PagedCodeEditor::supplyPage(const PagedCodePage &page) {
    auto &s = *impl_;
    if (!s.reading || s.reading->id != page.request || s.document.id != page.document || s.document.revision != page.revision) return false;
    if (!validateCodePage(*s.reading, s.document, page, &s.error)) { s.failed = true; s.reading.reset(); return false; }
    std::vector<Impl::Row> prepared;
    prepared.reserve(page.lines.size());
    for (const auto &line : page.lines) {
        Impl::Row row{line, {}};
        if (!pagedCodeGlyphs(line.text, line.firstCharacter, line.firstColumn, s.options.tabSize, row.glyphs, &s.error)) return false;
        prepared.push_back(std::move(row));
    }
    const bool revealing = bool(s.reading->reveal);
    s.rows.swap(prepared); s.firstColumn = page.firstColumn;
    if (revealing) {
        if (const auto *row = s.row(s.selection.caret.line)) s.selection.caret.character = std::min(s.selection.caret.character, row->line.characters);
        if (const auto *row = s.row(s.selection.anchor.line)) s.selection.anchor.character = std::min(s.selection.anchor.character, row->line.characters);
        if (const auto c = s.column(s.selection.caret)) s.preferredColumn = *c;
    }
    s.reading.reset(); s.readEvent.reset(); s.reveal = s.failed = false; s.error.clear();
    if (s.navigation) {
        const auto pending = *s.navigation; s.navigation.reset(); s.navigate(pending.line, pending.extend, pending.end);
    }
    if (s.pendingHorizontal) {
        const auto pending = *s.pendingHorizontal; s.pendingHorizontal.reset(); s.horizontal(pending.direction, pending.extend);
    }
    s.finishDeletion();
    return true;
}
bool PagedCodeEditor::failRead(std::uint64_t request, const std::string &message) {
    auto &s = *impl_; if (!s.reading || s.reading->id != request) return false;
    s.cancelRead(); s.failed = true; s.error = message.substr(0, 4096); return true;
}
void PagedCodeEditor::retryRead() { impl_->failed = false; impl_->error.clear(); }
bool PagedCodeEditor::replaceSelection(std::string_view text) { return impl_->replacement(text); }
bool PagedCodeEditor::requestAction(PagedCodeCommand action) { return impl_->action(action); }
bool PagedCodeEditor::acceptEdit(std::uint64_t id, PagedCodeDocument document, PagedCodeSelection selection) {
    auto &s = *impl_;
    if (!s.waitingEdit || s.waitingEdit->id != id || s.waitingEdit->document != s.document.id ||
        s.waitingEdit->revision != s.document.revision || document.id != s.document.id ||
        !document.revision || document.revision == s.document.revision || !document.lines ||
        selection.anchor.line >= document.lines || selection.caret.line >= document.lines) return false;
    auto inputs = std::move(s.inputs); const auto bytes = s.queuedBytes; s.queuedBytes = 0;
    s.waitingEdit.reset(); s.editEvent = {};
    setDocument(document);
    s.inputs = std::move(inputs); s.queuedBytes = bytes;
    s.accepting = true;
    const bool selected = setSelection(selection);
    s.accepting = false;
    return selected;
}
bool PagedCodeEditor::rejectEdit(std::uint64_t id, const std::string &message) {
    auto &s = *impl_; if (!s.waitingEdit || s.waitingEdit->id != id) return false;
    s.editEvent = {EditPhase::cancel, *s.waitingEdit}; s.waitingEdit.reset(); s.cancelInputs();
    s.error = message.substr(0, 4096); return true;
}

PagedCodeEditorResult PagedCodeEditor::draw(const char *id, ImVec2 size) {
    auto &s = *impl_;
    PagedCodeEditorResult result;
    ScopedID scope(id);
    ScopedColor background(ImGuiCol_ChildBg, palette().bg0);
    const bool visible = ImGui::BeginChild("##paged", size, ImGuiChildFlags_Borders,
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (visible) {
        const auto origin = ImGui::GetCursorScreenPos();
        const auto available = ImGui::GetContentRegionAvail();
        const auto bar = ImGui::GetStyle().ScrollbarSize;
        const auto statusHeight = ImGui::GetFrameHeightWithSpacing();
        const auto horizontalHeight = ImGui::GetFrameHeight();
        const ImVec2 canvas{std::max(dp(16), available.x - bar - dp(4)),
                            std::max(dp(16), available.y - statusHeight - horizontalHeight - dp(8))};
        ImGui::InvisibleButton("##canvas", canvas, ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        result.focused = ImGui::IsItemFocused() && ImGui::IsWindowFocused();
        const auto wheel = itemWheel();
        auto &io = ImGui::GetIO();
        if (wheel && io.KeyCtrl && s.options.wheelZoom)
            s.options.textScale = std::clamp(s.options.textScale * std::pow(1.1f, wheel), .5f, 3.f);
        ScopedFont font(monoFont(), s.options.textScale);
        const auto cell = std::max(dp(1), ImGui::CalcTextSize("M").x);
        const auto height = ImGui::GetTextLineHeightWithSpacing();
        const auto gutter = s.options.lineNumbers ? std::max(dp(40), ImGui::CalcTextSize(std::to_string(s.document.lines).c_str()).x + dp(16)) : dp(8);
        s.visibleLines = unsigned(std::clamp(std::floor(canvas.y / height), 1.f, 4080.f));
        s.visibleColumns = unsigned(std::clamp(std::floor(std::max(cell, canvas.x - gutter) / cell), 1.f, 8176.f));
        if (wheel && !(io.KeyCtrl && s.options.wheelZoom)) {
            if (io.KeyShift) s.moved(s.firstLine, shifted(s.firstColumn, -std::int64_t(wheel * 8), UINT64_MAX - 8192));
            else s.moved(shifted(s.firstLine, -std::int64_t(wheel * 3), s.valid() ? s.document.lines - 1 : 0), s.firstColumn);
        }
        if (hovered && io.MouseWheelH) {
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
            s.moved(s.firstLine, shifted(s.firstColumn, -std::int64_t(io.MouseWheelH * 8), UINT64_MAX - 8192));
        }
        auto pointAt = [&]() -> std::optional<PagedCodePosition> {
            const auto localY = std::clamp(io.MousePos.y - origin.y, 0.f, canvas.y - dp(1));
            const auto line = plus(s.firstLine, std::uint64_t(localY / height));
            const auto *row = s.row(line); if (!row) return {};
            const auto x = std::max(0.f, io.MousePos.x - origin.x - gutter);
            return s.hit(*row, plus(s.firstColumn, std::uint64_t(x / cell)));
        };
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !s.waitingEdit) {
            if (const auto point = pointAt()) {
                s.dragStart = s.selection; s.dragging = true;
                s.caret(*point, io.KeyShift, EditPhase::begin);
            }
        }
        if (s.dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (io.MousePos.y < origin.y) s.moved(shifted(s.firstLine, -1, UINT64_MAX), s.firstColumn);
                if (io.MousePos.y > origin.y + canvas.y) s.moved(plus(s.firstLine, 1), s.firstColumn);
                if (io.MousePos.x < origin.x + gutter) s.moved(s.firstLine, shifted(s.firstColumn, -1, UINT64_MAX));
                if (io.MousePos.x > origin.x + canvas.x) s.moved(s.firstLine, plus(s.firstColumn, 1));
                if (const auto point = pointAt()) if (*point != s.selection.caret) s.caret(*point, true, EditPhase::update);
            } else {
                s.dragging = false; s.selectionEvent = {EditPhase::commit, s.selection};
            }
        }
        if (result.focused && s.valid()) {
            ImGui::SetNextFrameWantCaptureKeyboard(true);
            const auto mod = io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
            const auto shortcut = [&](ImGuiKeyChord keys) { return ImGui::Shortcut(keys, ImGuiInputFlags_Repeat); };
            const bool extend = io.KeyShift;
            if (shortcut(ImGuiKey_Escape) && s.dragging) {
                s.selection = s.dragStart; s.dragging = false;
                s.selectionEvent = {EditPhase::cancel, s.selection};
            }
            const auto command = [&](PagedCodeCommand value) { s.enqueue({PagedCodeInputKind::action, {}, false, value}); };
            const auto motion = [&](ImGuiKeyChord key, PagedCodeInputKind kind) {
                if (!shortcut((extend ? ImGuiMod_Shift : 0) | key)) return false;
                s.enqueue({kind, {}, extend}); return true;
            };
            if (s.options.keyboardShortcuts) {
                if (shortcut(mod | ImGuiKey_C)) command(PagedCodeCommand::copy);
                else if (shortcut(mod | ImGuiKey_X)) command(PagedCodeCommand::cut);
                else if (shortcut(mod | ImGuiKey_Z)) command(PagedCodeCommand::undo);
                else if (shortcut(mod | ImGuiMod_Shift | ImGuiKey_Z) || shortcut(mod | ImGuiKey_Y)) command(PagedCodeCommand::redo);
                else if (shortcut(mod | ImGuiKey_S)) command(PagedCodeCommand::save);
                else if (shortcut(mod | ImGuiKey_F)) command(PagedCodeCommand::find);
                else if (shortcut(ImGuiKey_F3)) command(PagedCodeCommand::findNext);
                else if (shortcut(ImGuiMod_Shift | ImGuiKey_F3)) command(PagedCodeCommand::findPrevious);
            }
            (void)(motion(ImGuiKey_UpArrow, PagedCodeInputKind::up) || motion(ImGuiKey_DownArrow, PagedCodeInputKind::down) ||
                motion(ImGuiKey_PageUp, PagedCodeInputKind::pageUp) || motion(ImGuiKey_PageDown, PagedCodeInputKind::pageDown) ||
                motion(ImGuiKey_LeftArrow, PagedCodeInputKind::left) || motion(ImGuiKey_RightArrow, PagedCodeInputKind::right) ||
                motion(ImGuiKey_Home, PagedCodeInputKind::home) || motion(ImGuiKey_End, PagedCodeInputKind::end) ||
                motion(mod | ImGuiKey_Home, PagedCodeInputKind::documentHome) || motion(mod | ImGuiKey_End, PagedCodeInputKind::documentEnd));
            if (s.options.keyboardShortcuts && shortcut(mod | ImGuiKey_A)) s.enqueue({PagedCodeInputKind::selectAll});
            if (!s.document.readOnly) {
                    if (s.options.keyboardShortcuts && shortcut(mod | ImGuiKey_V)) {
                        if (const auto *text = ImGui::GetClipboardText()) {
                            std::size_t bytes = 0; while (bytes <= s.options.maxInsertionBytes && text[bytes]) ++bytes;
                            if (bytes <= s.options.maxInsertionBytes) s.enqueue({PagedCodeInputKind::text, {text, bytes}});
                            else s.error = "Clipboard text exceeds the insertion budget";
                        }
                    } else if (shortcut(ImGuiKey_Enter)) s.enqueue({PagedCodeInputKind::text, s.options.newline});
                    else if (shortcut(ImGuiKey_Tab)) s.enqueue({PagedCodeInputKind::text, "\t"});
                    else if (shortcut(ImGuiKey_Backspace)) s.enqueue({PagedCodeInputKind::backspace});
                    else if (shortcut(ImGuiKey_Delete)) s.enqueue({PagedCodeInputKind::deleteForward});
                    if (!(io.KeyCtrl && !io.KeyAlt) && !io.KeySuper) {
                        std::string text;
                        for (const auto character : io.InputQueueCharacters)
                            if (character >= 32 && character != 127 && !(character >= 0xd800 && character <= 0xdfff)) utf8(text, character);
                        if (!text.empty()) s.enqueue({PagedCodeInputKind::text, std::move(text)});
                    }
                    auto *context = ImGui::GetCurrentContext();
                    context->PlatformImeData.WantTextInput = true;
                    context->PlatformImeData.WantVisible = true;
                    context->PlatformImeData.InputLineHeight = height;
                    context->PlatformImeData.InputPos = {origin.x + gutter, origin.y};
                    if (const auto column = s.column(s.selection.caret)) {
                        if (*column >= s.firstColumn && s.selection.caret.line >= s.firstLine) {
                            context->PlatformImeData.InputPos = {origin.x + gutter + float(*column - s.firstColumn) * cell,
                                origin.y + float(s.selection.caret.line - s.firstLine) * height};
                        }
                    }
                    context->PlatformImeData.ViewportId = ImGui::GetCurrentWindow()->Viewport->ID;
            }
            io.InputQueueCharacters.resize(0);
        }
        s.processInput(); s.query();
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, {origin.x + canvas.x, origin.y + canvas.y}, true);
        draw->AddRectFilled(origin, {origin.x + canvas.x, origin.y + canvas.y}, palette().bg0);
        if (s.options.lineNumbers) {
            draw->AddRectFilled(origin, {origin.x + gutter - dp(4), origin.y + canvas.y}, palette().bg1);
            draw->AddLine({origin.x + gutter - dp(4), origin.y}, {origin.x + gutter - dp(4), origin.y + canvas.y}, palette().line);
        }
        if (!s.valid()) {
            ImGui::SetCursorScreenPos({origin.x + gutter, origin.y + dp(16)});
            emptyState(tr("No document").c_str(), tr("Open a document through the host application.").c_str());
        }
        const auto first = s.selection.first(), last = s.selection.last();
        for (unsigned n = 0; s.valid() && n < s.visibleLines && n < s.document.lines - s.firstLine; ++n) {
            const auto lineNumber = s.firstLine + n;
            const auto y = origin.y + float(n) * height;
            const auto *row = s.row(lineNumber);
            if (s.options.lineNumbers) {
                const auto label = std::to_string(lineNumber + 1);
                draw->AddText({origin.x + gutter - dp(12) - ImGui::CalcTextSize(label.c_str()).x, y},
                              lineNumber == s.selection.caret.line ? palette().accent : palette().textFaint, label.c_str());
            }
            if (!row) { draw->AddText({origin.x + gutter, y}, palette().textFaint, tr("Loading page...").c_str()); continue; }
            if (s.selection.empty() && lineNumber == s.selection.caret.line)
                draw->AddRectFilled({origin.x + gutter, y}, {origin.x + canvas.x, y + height}, alpha(palette().accent, .06f));
            draw->PushClipRect({origin.x + gutter, origin.y}, {origin.x + canvas.x, origin.y + canvas.y}, true);
            std::size_t token = 0;
            for (const auto &glyph : row->glyphs) {
                if (plus(glyph.column, glyph.cells) <= s.firstColumn) continue;
                if (glyph.column >= plus(s.firstColumn, s.visibleColumns + 1)) break;
                const double offset = glyph.column >= s.firstColumn ? double(glyph.column - s.firstColumn) : -double(s.firstColumn - glyph.column);
                const float x = origin.x + gutter + float(offset) * cell;
                const PagedCodePosition p{lineNumber, glyph.character};
                if (!s.selection.empty() && !(p < first) && p < last)
                    draw->AddRectFilled({x, y}, {x + glyph.cells * cell, y + height}, palette().selection);
                while (token < row->line.tokens.size() && row->line.tokens[token].start + row->line.tokens[token].length <= glyph.firstByte) ++token;
                const auto role = token < row->line.tokens.size() && row->line.tokens[token].start <= glyph.firstByte ?
                    row->line.tokens[token].role : TokenRole::text;
                if (glyph.value != '\t') {
                    const auto *begin = row->line.text.data() + glyph.firstByte;
                    draw->AddText({x, y}, tokenColor(role), begin, begin + glyph.bytes);
                }
            }
            if (!s.selection.empty() && first.line <= lineNumber && lineNumber < last.line && row->line.columns >= s.firstColumn) {
                const auto x = origin.x + gutter + float(std::min<std::uint64_t>(row->line.columns - s.firstColumn, s.visibleColumns)) * cell;
                draw->AddRectFilled({x, y}, {x + cell, y + height}, palette().selection);
            }
            if (result.focused && lineNumber == s.selection.caret.line) if (const auto c = s.column(s.selection.caret)) {
                if (*c >= s.firstColumn && *c < plus(s.firstColumn, s.visibleColumns)) {
                    const auto x = origin.x + gutter + float(*c - s.firstColumn) * cell;
                    draw->AddLine({x, y}, {x, y + height}, palette().text, dp(1));
                }
            }
            draw->PopClipRect();
        }
        draw->PopClipRect();
        if (s.valid()) {
            ImGui::SetCursorScreenPos({origin.x + canvas.x + dp(2), origin.y});
            const auto maximum = s.document.lines > s.visibleLines ? s.document.lines - s.visibleLines : 0;
            double value = maximum ? 1 - double(std::min(s.firstLine, maximum)) / double(maximum) : 1;
            const double zero = 0, one = 1;
            if (ImGui::VSliderScalar("##vertical", {bar, canvas.y}, ImGuiDataType_Double, &value, &zero, &one, ""))
                s.moved(fromRatio(1 - value, maximum), s.firstColumn);
            std::uint64_t extent = std::max(s.document.columnsHint, plus(s.firstColumn, s.visibleColumns));
            for (const auto &row : s.rows) extent = std::max(extent, row.line.columns);
            const auto maxColumn = extent > s.visibleColumns ? extent - s.visibleColumns : 0;
            value = maxColumn ? double(s.firstColumn) / double(maxColumn) : 0;
            ImGui::SetCursorScreenPos({origin.x + gutter, origin.y + canvas.y + dp(2)});
            ImGui::SetNextItemWidth(std::max(dp(1), canvas.x - gutter));
            if (ImGui::SliderScalar("##horizontal", ImGuiDataType_Double, &value, &zero, &one, ""))
                s.moved(s.firstLine, fromRatio(value, maxColumn));
        }
        ImGui::SetCursorScreenPos({origin.x, origin.y + canvas.y + horizontalHeight + dp(6)});
        const auto status = !s.valid() ? tr("No document") : s.failed ? tr("Page unavailable") :
            s.waitingEdit ? tr("Waiting for the application...") : s.reading ? tr("Loading page...") :
            s.document.readOnly ? tr("Read-only") : tr("Paged code editor");
        ImGui::TextColored(vec(s.error.empty() ? palette().textDim : palette().error), "%s  |  %s:%s", status.c_str(),
                           std::to_string(plus(s.selection.caret.line, 1)).c_str(),
                           std::to_string(plus(s.selection.caret.character, 1)).c_str());
        if (s.failed) { ImGui::SameLine(); if (button(trLabel("Retry##paged-retry").c_str())) retryRead(); }
        if (!s.error.empty()) tooltip(s.error.c_str());
    }
    ImGui::EndChild();
    result.read = s.readEvent; s.readEvent.reset();
    result.edit = std::move(s.editEvent); s.editEvent = {};
    result.selection = s.selectionEvent; s.selectionEvent = {};
    result.actions.swap(s.actions);
    result.cancelledInput.swap(s.cancelledInput);
    result.cancelledRead = s.cancelledRead; s.cancelledRead = 0;
    result.waitingForPage = bool(s.reading); result.waitingForEdit = bool(s.waitingEdit); result.error = s.error;
    return result;
}
} // namespace cyui

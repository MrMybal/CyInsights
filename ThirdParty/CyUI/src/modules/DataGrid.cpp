#include "cyui/modules/DataGrid.h"
#include "ModuleInternal.h"
#include <deque>
#include <stdexcept>
#include <limits>
#include <cctype>
#include <variant>

namespace cyui {
namespace {
struct Snapshot {
    std::size_t rows = 0, columns = 0;
    std::vector<GridCell> cells;
    std::size_t textBytes = 0;
};
struct CellEdit {
    std::size_t index;
    GridCell cell;
};
struct Change {
    std::variant<Snapshot, std::vector<CellEdit>> saved;
    std::size_t bytes = 0;
};
constexpr std::size_t byteLimit = 32 * 1024 * 1024;
void dimensions(std::size_t r, std::size_t c) {
    if (!r || !c || r > 100000 || c > 256 || r > 1000000 / c)
        throw std::length_error("Grid dimensions exceed limits");
}
void validateCell(const GridCell &c) {
    if (c.decimals < -1 || c.decimals > 15 || (c.style & ~15u) ||
        static_cast<unsigned>(c.alignment) > static_cast<unsigned>(GridAlignment::right))
        throw std::invalid_argument("Invalid grid cell format");
    if (c.text.size() > 1024 * 1024)
        throw std::length_error("Grid cell exceeds limit");
}
void validate(Snapshot &s) {
    dimensions(s.rows, s.columns);
    if (s.cells.size() != s.rows * s.columns)
        throw std::invalid_argument("Grid snapshot dimensions mismatch");
    s.textBytes = 0;
    for (const auto &c : s.cells) {
        validateCell(c);
        s.textBytes += c.text.size();
        if (s.textBytes > byteLimit)
            throw std::length_error("Grid text exceeds limit");
    }
}
bool equalCell(const GridCell &a, const GridCell &b) {
    return a.text == b.text && a.style == b.style && a.alignment == b.alignment && a.decimals == b.decimals &&
           a.literal == b.literal;
}
int gridInputCallback(ImGuiInputTextCallbackData *data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter && data->EventChar == '\t')
        return 1;
    return detail::resizeText(data);
}
std::string quoteCell(const std::string &s, char separator) {
    if (s.find_first_of(std::string(1, separator) + "\r\n\"") == std::string::npos)
        return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '\"')
            out += '\"';
        out += c;
    }
    return out + '\"';
}
} // namespace
struct GridModel::Impl {
    Snapshot value;
    std::deque<Change> undo, redo;
    std::size_t historyBytes = 0;
    std::uint64_t revision = 1;
    // The entry holds the next values on arrival and the inverse change after swapping.
    void apply(std::deque<Change> &destination) {
        auto &change = destination.back();
        change.bytes = sizeof(Change);
        if (auto *snapshot = std::get_if<Snapshot>(&change.saved)) {
            std::swap(*snapshot, value);
            change.bytes += snapshot->cells.size() * sizeof(GridCell) + snapshot->textBytes;
        } else {
            auto &edits = std::get<std::vector<CellEdit>>(change.saved);
            change.bytes += edits.capacity() * sizeof(CellEdit);
            for (auto &edit : edits) {
                auto &cell = value.cells[edit.index];
                value.textBytes = value.textBytes - cell.text.size() + edit.cell.text.size();
                std::swap(cell, edit.cell);
                change.bytes += edit.cell.text.size();
            }
        }
        historyBytes += change.bytes;
        while (undo.size() + redo.size() > 64 || historyBytes > byteLimit) {
            historyBytes -= destination.front().bytes;
            destination.pop_front();
        }
        ++revision;
    }
    bool perform(Change next) {
        undo.push_back(std::move(next));
        for (const auto &change : redo)
            historyBytes -= change.bytes;
        redo.clear();
        apply(undo);
        return true;
    }
    bool restore(std::deque<Change> &from, std::deque<Change> &to) {
        if (from.empty())
            return false;
        to.push_back(std::move(from.back()));
        from.pop_back();
        historyBytes -= to.back().bytes;
        apply(to);
        return true;
    }
    bool commit(Snapshot next) {
        validate(next);
        if (value.rows != next.rows || value.columns != next.columns)
            return perform({std::move(next)});
        std::vector<CellEdit> edits;
        for (std::size_t i = 0; i < next.cells.size(); ++i)
            if (!equalCell(value.cells[i], next.cells[i]))
                edits.push_back({i, std::move(next.cells[i])});
        return !edits.empty() && perform({std::move(edits)});
    }
    bool edit(std::size_t index, GridCell next) {
        validateCell(next);
        const auto &current = value.cells[index];
        if (equalCell(current, next))
            return false;
        if (value.textBytes - current.text.size() + next.text.size() > byteLimit)
            throw std::length_error("Grid text exceeds limit");
        std::vector<CellEdit> edits;
        edits.push_back({index, std::move(next)});
        return perform({std::move(edits)});
    }
};
GridRange GridRange::normalized() const {
    return {{std::min(first.row, last.row), std::min(first.column, last.column)},
            {std::max(first.row, last.row), std::max(first.column, last.column)}};
}
bool GridRange::contains(GridAddress a) const {
    auto r = normalized();
    return a.row >= r.first.row && a.row <= r.last.row && a.column >= r.first.column &&
           a.column <= r.last.column;
}
GridModel::GridModel(std::size_t r, std::size_t c) : impl_(new Impl) {
    dimensions(r, c);
    impl_->value = {r, c, std::vector<GridCell>(r * c)};
}
GridModel::~GridModel() = default;
GridModel::GridModel(GridModel &&) noexcept = default;
GridModel &GridModel::operator=(GridModel &&) noexcept = default;
GridModel::GridModel(const GridModel &o) : impl_(new Impl) {
    impl_->value = o.impl_->value;
    impl_->revision = o.impl_->revision;
}
GridModel &GridModel::operator=(const GridModel &o) {
    if (this != &o)
        *this = GridModel(o);
    return *this;
}
std::size_t GridModel::rows() const {
    return impl_->value.rows;
}
std::size_t GridModel::columns() const {
    return impl_->value.columns;
}
std::uint64_t GridModel::revision() const {
    return impl_->revision;
}
const GridCell &GridModel::cell(std::size_t r, std::size_t c) const {
    if (r >= rows() || c >= columns())
        throw std::out_of_range("Grid address");
    return impl_->value.cells[r * columns() + c];
}
std::vector<GridCell> GridModel::cells() const {
    return impl_->value.cells;
}
bool GridModel::assign(std::size_t r, std::size_t c, const std::vector<GridCell> &cells) {
    return impl_->commit({r, c, cells});
}
bool GridModel::setCell(std::size_t r, std::size_t c, const GridCell &v) {
    cell(r, c);
    return impl_->edit(r * columns() + c, v);
}
bool GridModel::setText(std::size_t r, std::size_t c, const std::string &t) {
    auto v = cell(r, c);
    v.text = t;
    v.literal = false;
    return impl_->edit(r * columns() + c, std::move(v));
}
bool GridModel::resize(std::size_t r, std::size_t c) {
    dimensions(r, c);
    Snapshot n{r, c, std::vector<GridCell>(r * c)};
    for (std::size_t y = 0; y < std::min(r, rows()); ++y)
        for (std::size_t x = 0; x < std::min(c, columns()); ++x)
            n.cells[y * c + x] = cell(y, x);
    return impl_->commit(std::move(n));
}
bool GridModel::insertRows(std::size_t at, std::size_t n) {
    if (at > rows() || !n)
        return false;
    if (n > 100000 - rows())
        throw std::length_error("Grid row limit");
    dimensions(rows() + n, columns());
    auto s = impl_->value;
    s.rows += n;
    s.cells.insert(s.cells.begin() + at * columns(), n * columns(), {});
    return impl_->commit(std::move(s));
}
bool GridModel::insertColumns(std::size_t at, std::size_t n) {
    if (at > columns() || !n)
        return false;
    if (n > 256 - columns())
        throw std::length_error("Grid column limit");
    dimensions(rows(), columns() + n);
    Snapshot s{rows(), columns() + n, {}};
    s.cells.reserve(s.rows * s.columns);
    for (std::size_t r = 0; r < rows(); ++r) {
        for (std::size_t c = 0; c < s.columns; ++c)
            s.cells.push_back(c >= at && c < at + n ? GridCell{} : cell(r, c < at ? c : c - n));
    }
    return impl_->commit(std::move(s));
}
bool GridModel::removeRows(std::size_t at, std::size_t n) {
    if (at >= rows() || !n || n >= rows() || n > rows() - at)
        return false;
    auto s = impl_->value;
    s.cells.erase(s.cells.begin() + at * columns(), s.cells.begin() + (at + n) * columns());
    s.rows -= n;
    return impl_->commit(std::move(s));
}
bool GridModel::removeColumns(std::size_t at, std::size_t n) {
    if (at >= columns() || !n || n >= columns() || n > columns() - at)
        return false;
    Snapshot s{rows(), columns() - n, {}};
    for (std::size_t r = 0; r < rows(); ++r)
        for (std::size_t c = 0; c < columns(); ++c)
            if (c < at || c >= at + n)
                s.cells.push_back(cell(r, c));
    return impl_->commit(std::move(s));
}
bool GridModel::clear(GridRange range) {
    auto s = impl_->value;
    auto a = range.normalized();
    for (std::size_t r = a.first.row; r < rows() && r <= a.last.row; ++r)
        for (std::size_t c = a.first.column; c < columns() && c <= a.last.column; ++c)
            s.cells[r * columns() + c].text.clear();
    return impl_->commit(std::move(s));
}
bool GridModel::format(GridRange range, unsigned style, GridAlignment align, int decimals) {
    auto s = impl_->value;
    auto a = range.normalized();
    for (std::size_t r = a.first.row; r < rows() && r <= a.last.row; ++r)
        for (std::size_t c = a.first.column; c < columns() && c <= a.last.column; ++c) {
            auto &v = s.cells[r * columns() + c];
            v.style = style;
            v.alignment = align;
            v.decimals = std::clamp(decimals, -1, 15);
        }
    return impl_->commit(std::move(s));
}
std::string GridModel::copy(GridRange range, char separator) const {
    auto a = range.normalized();
    std::string out;
    for (std::size_t r = a.first.row; r < rows() && r <= a.last.row; ++r) {
        if (r > a.first.row)
            out += '\n';
        for (std::size_t c = a.first.column; c < columns() && c <= a.last.column; ++c) {
            if (c > a.first.column)
                out += separator;
            out += quoteCell(cell(r, c).text, separator);
        }
    }
    return out;
}
std::vector<std::vector<std::string>> gridParseDelimited(const std::string &text, char sep) {
    if (text.size() > 32 * 1024 * 1024)
        throw std::length_error("Clipboard exceeds grid limit");
    std::vector<std::vector<std::string>> out(1, std::vector<std::string>(1));
    bool quoted = false, closed = false;
    std::size_t cells = 1;
    for (std::size_t at = 0; at < text.size(); ++at) {
        char ch = text[at];
        auto &value = out.back().back();
        if (quoted) {
            if (ch == '\"') {
                if (at + 1 < text.size() && text[at + 1] == '\"') {
                    value += '\"';
                    ++at;
                } else {
                    quoted = false;
                    closed = true;
                }
            } else
                value += ch;
        } else if (ch == '\"' && value.empty() && !closed)
            quoted = true;
        else if (ch == sep) {
            out.back().emplace_back();
            closed = false;
            ++cells;
        } else if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && at + 1 < text.size() && text[at + 1] == '\n')
                ++at;
            if (at + 1 == text.size())
                break;
            out.emplace_back(1);
            closed = false;
            ++cells;
        } else {
            if (closed)
                throw std::invalid_argument("Unexpected data after quoted cell");
            value += ch;
        }
        if (out.size() > 100000 || out.back().size() > 256 || cells > 1000000 ||
            out.back().back().size() > 1024 * 1024)
            throw std::length_error("Clipboard grid exceeds limits");
    }
    if (quoted)
        throw std::invalid_argument("Unterminated quoted cell");
    return out;
}
bool GridModel::paste(GridAddress at, const std::string &text, char sep) {
    auto values = gridParseDelimited(text, sep);
    std::size_t c = 0;
    for (const auto &r : values)
        c = std::max(c, r.size());
    if (at.row > 100000 || at.column > 256)
        throw std::length_error("Grid paste address");
    const auto nr = std::max(rows(), at.row + values.size()), nc = std::max(columns(), at.column + c);
    dimensions(nr, nc);
    Snapshot s{nr, nc, std::vector<GridCell>(nr * nc)};
    for (std::size_t r = 0; r < rows(); ++r)
        for (std::size_t x = 0; x < columns(); ++x)
            s.cells[r * nc + x] = cell(r, x);
    for (std::size_t r = 0; r < values.size(); ++r)
        for (std::size_t x = 0; x < values[r].size(); ++x)
            s.cells[(r + at.row) * nc + x + at.column].text = values[r][x];
    return impl_->commit(std::move(s));
}
bool GridModel::undo() {
    return impl_->restore(impl_->undo, impl_->redo);
}
bool GridModel::redo() {
    return impl_->restore(impl_->redo, impl_->undo);
}
bool GridModel::canUndo() const {
    return !impl_->undo.empty();
}
bool GridModel::canRedo() const {
    return !impl_->redo.empty();
}
void GridModel::clearHistory() {
    impl_->undo.clear();
    impl_->redo.clear();
    impl_->historyBytes = 0;
}
std::string gridColumnName(std::size_t c) {
    std::string out;
    do {
        out.insert(out.begin(), char('A' + c % 26));
        c = c / 26;
        if (!c)
            break;
        --c;
    } while (true);
    return out;
}
std::string gridAddressName(GridAddress a) {
    return gridColumnName(a.column) + std::to_string(a.row + 1);
}
bool gridParseAddress(const std::string &s, GridAddress &a) {
    std::size_t at = 0, c = 0, r = 0;
    if (at < s.size() && s[at] == '$')
        ++at;
    auto first = at;
    while (at < s.size() && std::isalpha(static_cast<unsigned char>(s[at]))) {
        c = c * 26 + std::toupper(static_cast<unsigned char>(s[at])) - 'A' + 1;
        if (c > 16384)
            return false;
        ++at;
    }
    if (at == first)
        return false;
    if (at < s.size() && s[at] == '$')
        ++at;
    first = at;
    while (at < s.size() && std::isdigit(static_cast<unsigned char>(s[at]))) {
        r = r * 10 + s[at++] - '0';
        if (r > 1048576)
            return false;
    }
    if (at != s.size() || at == first || !r)
        return false;
    a = {r - 1, c - 1};
    return true;
}

GridResult dataGrid(const char *id, GridState &s, GridModel &m, ImVec2 size, const GridOptions &o) {
    ScopedID scope{id};
    GridResult result;
    auto before = m.revision();
    if (size.x <= 0)
        size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0)
        size.y = dp(320);
    ImGui::BeginChild("##grid", size);
    auto clamp = [&](GridAddress a) {
        return GridAddress{std::min(a.row, m.rows() - 1), std::min(a.column, m.columns() - 1)};
    };
    s.anchor = clamp(s.anchor);
    s.current = clamp(s.current);
    if (s.editing && (s.editRevision != m.revision() || o.readOnly))
        s.editing = false;
    auto start = [&]() {
        if (o.readOnly)
            return;
        s.editing = true;
        s.editingCell = s.current;
        s.draft = m.cell(s.current.row, s.current.column).text;
        s.editRevision = m.revision();
        s.focusEditor = true;
        s.reveal = true;
    };
    auto commit = [&]() {
        if (s.editing && !o.readOnly) {
            if (o.beforeChange)
                o.beforeChange();
            m.setText(s.editingCell.row, s.editingCell.column, s.draft);
            result.committed = true;
        }
        s.editing = false;
    };
    auto action = [&](const std::function<void()> &fn) {
        try {
            commit();
            if (o.beforeChange)
                o.beforeChange();
            fn();
            s.error.clear();
        } catch (const std::exception &e) {
            s.error = e.what();
        }
    };
    if (o.toolbar) {
        ImGui::BeginDisabled(o.readOnly);
        if (ghostButton(trLabel("Add row").c_str()))
            action([&] {
                result.structure = 1;
                result.at = m.rows();
                result.count = 1;
                m.insertRows(m.rows());
            });
        ImGui::SameLine();
        if (ghostButton(trLabel("Add column").c_str()))
            action([&] {
                result.structure = 2;
                result.at = m.columns();
                result.count = 1;
                m.insertColumns(m.columns());
            });
        if (o.history) {
            ImGui::SameLine();
            if (ghostButton(trLabel("Undo").c_str()))
                action([&] { m.undo(); });
            ImGui::SameLine();
            if (ghostButton(trLabel("Redo").c_str()))
                action([&] { m.redo(); });
        }
        if (o.formatting) {
            auto applyFormat = [&](auto fn) {
                action([&] {
                    auto cells = m.cells();
                    auto range = s.selection();
                    for (std::size_t r = range.first.row; r < m.rows() && r <= range.last.row; ++r)
                        for (std::size_t c = range.first.column; c < m.columns() && c <= range.last.column;
                             ++c)
                            fn(cells[r * m.columns() + c]);
                    m.assign(m.rows(), m.columns(), cells);
                });
            };
            ImGui::SameLine();
            if (ghostButton(trLabel("Bold").c_str())) {
                const bool remove = (m.cell(s.current.row, s.current.column).style & 1) != 0;
                applyFormat(
                    [&](GridCell &cell) { cell.style = remove ? cell.style & ~1u : cell.style | 1u; });
            }
            ImGui::SameLine();
            if (ghostButton(trLabel("Italic").c_str())) {
                const bool remove = (m.cell(s.current.row, s.current.column).style & 2) != 0;
                applyFormat(
                    [&](GridCell &cell) { cell.style = remove ? cell.style & ~2u : cell.style | 2u; });
            }
            ImGui::SameLine();
            if (ghostButton(trLabel("Format cells").c_str()))
                ImGui::OpenPopup("##formatCells");
            if (ImGui::BeginPopup("##formatCells")) {
                for (const auto &pair :
                     std::vector<std::pair<const char *, GridAlignment>>{{"Left", GridAlignment::left},
                                                                         {"Center", GridAlignment::center},
                                                                         {"Right", GridAlignment::right}})
                    if (ImGui::Selectable(tr(pair.first).c_str()))
                        applyFormat([&](GridCell &cell) { cell.alignment = pair.second; });
                if (ImGui::Selectable(tr("Two decimals").c_str()))
                    applyFormat([](GridCell &cell) { cell.decimals = 2; });
                if (ImGui::Selectable(tr("Automatic precision").c_str()))
                    applyFormat([](GridCell &cell) { cell.decimals = -1; });
                ImGui::EndPopup();
            }
        }
        ImGui::EndDisabled();
    }
    if (!s.error.empty())
        textColoured(palette().error, "%s", s.error.c_str());
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    auto &io = ImGui::GetIO();
    if (focused && s.hasSelection && !s.editing) {
        const bool mod = io.KeyCtrl || io.KeySuper;
        if (o.clipboard && mod && ImGui::IsKeyPressed(ImGuiKey_C))
            ImGui::SetClipboardText(m.copy(s.selection()).c_str());
        if (!o.readOnly) {
            if (o.clipboard && mod && ImGui::IsKeyPressed(ImGuiKey_V))
                action([&] {
                    const char *p = ImGui::GetClipboardText();
                    if (p)
                        m.paste(s.selection().first, p);
                });
            if (o.history && mod && ImGui::IsKeyPressed(ImGuiKey_Z))
                action([&] { io.KeyShift ? m.redo() : m.undo(); });
            if (o.history && mod && ImGui::IsKeyPressed(ImGuiKey_Y))
                action([&] { m.redo(); });
            if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                action([&] { m.clear(s.selection()); });
            if (ImGui::IsKeyPressed(ImGuiKey_F2) || ImGui::IsKeyPressed(ImGuiKey_Enter))
                start();
        }
        auto move = [&](int row, int col) {
            s.current.row = std::size_t(
                std::clamp<std::int64_t>(std::int64_t(s.current.row) + row, 0, std::int64_t(m.rows() - 1)));
            s.current.column = std::size_t(std::clamp<std::int64_t>(std::int64_t(s.current.column) + col, 0,
                                                                    std::int64_t(m.columns() - 1)));
            if (!io.KeyShift)
                s.anchor = s.current;
            result.selectionChanged = true;
            s.reveal = true;
        };
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            move(0, -1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            move(0, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            move(-1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            move(1, 0);
    }
    const int lead = o.rowHeaders ? 1 : 0;
    const auto tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##cells", int(m.columns()) + lead, tableFlags, ImGui::GetContentRegionAvail())) {
        if (lead)
            ImGui::TableSetupColumn("##row", ImGuiTableColumnFlags_WidthFixed, dp(44));
        for (std::size_t c = 0; c < m.columns(); ++c)
            ImGui::TableSetupColumn(gridColumnName(c).c_str(), ImGuiTableColumnFlags_WidthFixed,
                                    dp(c < s.widths.size() ? s.widths[c] : o.columnWidth));
        if (o.freezeHeaders)
            ImGui::TableSetupScrollFreeze(lead, o.columnHeaders ? 1 : 0);
        if (o.columnHeaders)
            ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin(int(m.rows()), dp(o.rowHeight));
        if (s.reveal)
            clip.IncludeItemByIndex(int(s.current.row));
        while (clip.Step())
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                ImGui::TableNextRow(0, dp(o.rowHeight));
                if (lead) {
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", row + 1);
                }
                for (std::size_t col = 0; col < m.columns(); ++col) {
                    if (!ImGui::TableNextColumn())
                        continue;
                    ScopedID rkey{row};
                    ScopedID ckey{int(col)};
                    const GridAddress address{std::size_t(row), col};
                    const auto &cell = m.cell(row, col);
                    if (s.hasSelection && s.selection().contains(address))
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, alpha(palette().accent, .2f));
                    if (s.editing && s.editingCell == address) {
                        ImGui::SetNextItemWidth(-dp(1));
                        if (s.focusEditor) {
                            ImGui::SetKeyboardFocusHere();
                            s.focusEditor = false;
                        }
                        const auto inputId = ImGui::GetID("##edit");
                        bool submitted = ImGui::InputText(
                            "##edit", s.draft.data(), s.draft.capacity() + 1,
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
                                ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackCharFilter |
                                ImGuiInputTextFlags_CallbackResize,
                            gridInputCallback, &s.draft);
                        const bool tab = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Tab);
                        if (ImGui::IsItemActive())
                            ImGui::SetKeyOwner(ImGuiKey_Tab, inputId);
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                            s.editing = false;
                        } else if (submitted || tab || ImGui::IsItemDeactivatedAfterEdit()) {
                            action([&] {});
                            if (submitted || tab) {
                                if (tab) {
                                    if (io.KeyShift) {
                                        if (s.current.column)
                                            --s.current.column;
                                        else if (s.current.row) {
                                            --s.current.row;
                                            s.current.column = m.columns() - 1;
                                        }
                                    } else if (++s.current.column >= m.columns()) {
                                        s.current.column = 0;
                                        s.current.row = std::min(m.rows() - 1, s.current.row + 1);
                                    }
                                } else
                                    s.current.row = std::min(m.rows() - 1, s.current.row + 1);
                                s.anchor = s.current;
                                result.selectionChanged = true;
                                s.reveal = true;
                            }
                        }
                    } else {
                        const auto shown = o.display ? o.display(row, col, cell) : cell.text;
                        if (cell.style & 1)
                            ImGui::PushFont(boldFont() ? boldFont() : ImGui::GetFont(), ImGui::GetFontSize());
                        const auto position = ImGui::GetCursorScreenPos();
                        if (ImGui::Selectable("##cell", false, ImGuiSelectableFlags_AllowDoubleClick,
                                              {0, dp(o.rowHeight) - ImGui::GetStyle().CellPadding.y * 2})) {
                            s.current = address;
                            if (!io.KeyShift || !s.hasSelection)
                                s.anchor = address;
                            s.hasSelection = true;
                            result.selectionChanged = true;
                            if (ImGui::IsMouseDoubleClicked(0))
                                start();
                        }
                        auto shownAt = position;
                        const float textWidth = ImGui::CalcTextSize(shown.c_str()).x;
                        if (cell.alignment == GridAlignment::right)
                            shownAt.x += std::max(0.f, ImGui::GetContentRegionAvail().x - textWidth -
                                                           ImGui::GetStyle().CellPadding.x);
                        else if (cell.alignment == GridAlignment::center)
                            shownAt.x += std::max(0.f, (ImGui::GetContentRegionAvail().x - textWidth) * .5f);
                        auto *draw = ImGui::GetWindowDrawList();
                        const auto first = draw->VtxBuffer.Size;
                        draw->AddText(shownAt, palette().text, shown.c_str());
                        if (cell.style & 2)
                            for (int v = first; v < draw->VtxBuffer.Size; ++v)
                                draw->VtxBuffer[v].pos.x +=
                                    (shownAt.y + ImGui::GetFontSize() - draw->VtxBuffer[v].pos.y) * .14f;
                        if (cell.style & 4)
                            draw->AddLine({shownAt.x, shownAt.y + ImGui::GetFontSize()},
                                          {shownAt.x + textWidth, shownAt.y + ImGui::GetFontSize()},
                                          palette().text, dp(1));
                        if (cell.style & 8)
                            draw->AddLine({shownAt.x, shownAt.y + ImGui::GetFontSize() * .5f},
                                          {shownAt.x + textWidth, shownAt.y + ImGui::GetFontSize() * .5f},
                                          palette().text, dp(1));
                        if (cell.style & 1)
                            ImGui::PopFont();
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(0) && s.hasSelection) {
                            s.current = address;
                            result.selectionChanged = true;
                        }
                    }
                    if (ImGui::BeginPopupContextItem("##cellMenu")) {
                        s.current = address;
                        if (!s.hasSelection || !s.selection().contains(address))
                            s.anchor = address;
                        s.hasSelection = true;
                        ImGui::BeginDisabled(o.readOnly);
                        if (ImGui::MenuItem(tr("Insert row").c_str()))
                            action([&] {
                                result.structure = 1;
                                result.at = address.row;
                                result.count = 1;
                                m.insertRows(address.row);
                            });
                        if (ImGui::MenuItem(tr("Insert column").c_str()))
                            action([&] {
                                result.structure = 2;
                                result.at = address.column;
                                result.count = 1;
                                m.insertColumns(address.column);
                            });
                        if (ImGui::MenuItem(tr("Delete rows").c_str(), nullptr, false, m.rows() > 1))
                            action([&] {
                                auto range = s.selection();
                                result.structure = -1;
                                result.at = range.first.row;
                                result.count = range.last.row - range.first.row + 1;
                                m.removeRows(result.at, result.count);
                            });
                        if (ImGui::MenuItem(tr("Delete columns").c_str(), nullptr, false, m.columns() > 1))
                            action([&] {
                                auto range = s.selection();
                                result.structure = -2;
                                result.at = range.first.column;
                                result.count = range.last.column - range.first.column + 1;
                                m.removeColumns(result.at, result.count);
                            });
                        ImGui::EndDisabled();
                        ImGui::EndPopup();
                    }
                }
                if (s.reveal && std::size_t(row) == s.current.row) {
                    auto *table = ImGui::GetCurrentTable();
                    auto *window = table->InnerWindow;
                    const auto &column = table->Columns[int(s.current.column) + lead];
                    if (column.MinX < window->InnerClipRect.Min.x ||
                        column.MaxX > window->InnerClipRect.Max.x)
                        ImGui::SetScrollFromPosX(window, (column.MinX + column.MaxX) * .5f - window->Pos.x,
                                                 .5f);
                    if (table->RowPosY1 < window->InnerClipRect.Min.y ||
                        table->RowPosY2 > window->InnerClipRect.Max.y)
                        ImGui::SetScrollFromPosY(
                            window, (table->RowPosY1 + table->RowPosY2) * .5f - window->Pos.y, .5f);
                    s.reveal = false;
                }
            }
        ImGui::EndTable();
    }
    result.changed = before != m.revision();
    ImGui::EndChild();
    return result;
}
} // namespace cyui

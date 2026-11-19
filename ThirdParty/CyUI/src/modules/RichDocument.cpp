#include "cyui/modules/RichDocument.h"
#include "DocumentLayout.h"
#include "ModuleInternal.h"
#include <stdexcept>
#include <deque>
#include <sstream>

namespace cyui {
namespace {
float mm(float value) {
    return value * 96.f / 25.4f;
}
std::string plain(const RichBlock &b) {
    std::string out;
    for (const auto &r : b.runs)
        out += r.text;
    return out;
}
std::vector<DocumentInline> slice(const std::vector<DocumentInline> &runs, std::size_t first,
                                  std::size_t last) {
    std::vector<DocumentInline> out;
    std::size_t at = 0;
    for (auto r : runs) {
        auto end = at + r.text.size();
        if (end > first && at < last) {
            r.text = r.text.substr(first > at ? first - at : 0, std::min(last, end) - std::max(first, at));
            out.push_back(std::move(r));
        }
        at = end;
    }
    return out;
}
std::size_t documentCost(const RichDocumentData &d) {
    std::size_t bytes = d.blocks.size() * sizeof(RichBlock) + d.page.header.size() + d.page.footer.size();
    for (const auto &asset : d.assets)
        bytes += asset.bytes.size() + asset.path.size();
    for (const auto &block : d.blocks) {
        bytes += block.image.size() + block.caption.size();
        for (const auto &run : block.runs)
            bytes += run.text.size();
        bytes += block.table.rows() * block.table.columns() * sizeof(GridCell);
        for (const auto &cell : block.table.cells())
            bytes += cell.text.size();
    }
    return bytes;
}
void validate(const RichDocumentData &d) {
    const auto &p = d.page;
    if (!std::isfinite(p.widthMM) || !std::isfinite(p.heightMM) || p.widthMM < 50 || p.widthMM > 2000 ||
        p.heightMM < 50 || p.heightMM > 2000 || !std::isfinite(p.leftMM) || !std::isfinite(p.rightMM) ||
        !std::isfinite(p.topMM) || !std::isfinite(p.bottomMM) ||
        std::min({p.leftMM, p.rightMM, p.topMM, p.bottomMM}) < 0 || p.leftMM + p.rightMM > p.widthMM - 10 ||
        p.topMM + p.bottomMM > p.heightMM - 10)
        throw std::invalid_argument("Invalid document page or margins");
    if (d.blocks.size() > 10000)
        throw std::length_error("Document block limit");
    std::size_t bytes = 0, cells = 0;
    for (const auto &b : d.blocks) {
        if (b.kind == RichBlockKind::table) {
            cells += b.table.rows() * b.table.columns();
            if (cells > 1000000)
                throw std::length_error("Document table cell limit");
            for (const auto &c : b.table.cells())
                bytes += c.text.size();
        }
        if (b.kind == RichBlockKind::image &&
            (!std::isfinite(b.imageWidthMM) || !std::isfinite(b.imageHeightMM) || b.imageWidthMM <= 0 ||
             b.imageHeightMM <= 0 || b.imageWidthMM > 10000 || b.imageHeightMM > 10000))
            throw std::invalid_argument("Invalid image dimensions");
        for (const auto &r : b.runs)
            bytes += r.text.size();
        if (!std::isfinite(b.paragraph.points) || b.paragraph.points < 4 || b.paragraph.points > 144 ||
            !std::isfinite(b.paragraph.lineSpacing) || b.paragraph.lineSpacing < .5f ||
            b.paragraph.lineSpacing > 5 || !std::isfinite(b.paragraph.before) ||
            !std::isfinite(b.paragraph.after) || b.paragraph.before < 0 || b.paragraph.after < 0 ||
            b.paragraph.before > 10000 || b.paragraph.after > 10000 || !std::isfinite(b.paragraph.indent) ||
            std::abs(b.paragraph.indent) > 2000)
            throw std::invalid_argument("Invalid paragraph style");
    }
    for (const auto &a : d.assets)
        bytes += a.bytes.size();
    if (bytes > 128 * 1024 * 1024)
        throw std::length_error("Rich document byte limit");
}
std::string pageText(std::string s, std::size_t page, std::size_t count) {
    for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
             {"{page}", std::to_string(page + 1)}, {"{pages}", std::to_string(count)}}) {
        std::size_t at = 0;
        while ((at = s.find(pair.first, at)) != std::string::npos) {
            s.replace(at, pair.first.size(), pair.second);
            at += pair.second.size();
        }
    }
    return s;
}
} // namespace
void validateRichDocument(const RichDocumentData &d) {
    validate(d);
}
std::vector<RichPage> layoutRichDocument(const RichDocumentData &d, float scale) {
    validate(d);
    if (!std::isfinite(scale) || scale <= 0 || scale > 100)
        throw std::invalid_argument("Invalid page scale");
    std::vector<RichPage> pages(1);
    const float width = (d.page.widthMM - d.page.leftMM - d.page.rightMM) * scale,
                height = (d.page.heightMM - d.page.topMM - d.page.bottomMM) * scale;
    float y = 0;
    auto next = [&] {
        if (pages.size() >= 10000)
            throw std::length_error("Document page limit");
        pages.emplace_back();
        y = 0;
    };
    for (std::size_t index = 0; index < d.blocks.size(); ++index) {
        const auto &b = d.blocks[index];
        if (b.kind == RichBlockKind::pageBreak) {
            next();
            continue;
        }
        if (b.kind == RichBlockKind::image) {
            const float fit =
                std::min({1.f, width / (b.imageWidthMM * scale), height / (b.imageHeightMM * scale)});
            float h = b.imageHeightMM * scale * fit;
            if (y + h > height)
                next();
            pages.back().fragments.push_back({index, 0, 0, y, h, false});
            y += h + 8;
            continue;
        }
        if (b.kind == RichBlockKind::table) {
            const float rowHeight = std::min(height, 28 * scale / (96.f / 25.4f));
            std::size_t row = 0;
            while (row < b.table.rows()) {
                if (y + rowHeight > height)
                    next();
                auto count = std::max<std::size_t>(
                    1, std::min<std::size_t>(b.table.rows() - row, std::size_t((height - y) / rowHeight)));
                pages.back().fragments.push_back({index, row, row + count, y, rowHeight * count, true});
                y += rowHeight * count + 8;
                row += count;
            }
            continue;
        }
        auto value = plain(b);
        const float size = b.paragraph.points * scale * 25.4f / 72.f,
                    line = std::min(height, size * b.paragraph.lineSpacing);
        auto *font = ImGui::GetFont();
        std::vector<std::pair<std::size_t, std::size_t>> lines;
        std::size_t at = 0;
        while (at < value.size()) {
            auto *end = ImFontCalcWordWrapPositionEx(
                font, size, value.data() + at, value.data() + value.size(),
                std::max(size, width - std::clamp(b.paragraph.indent * scale, 0.f, width - scale)),
                ImDrawTextFlags_WrapKeepBlanks);
            auto offset = std::size_t(end - value.data());
            if (offset <= at)
                offset = nextDocumentChar(value, at);
            while (offset < value.size() && value[offset] != '\n' &&
                   (value[offset] == ' ' || value[offset] == '\t'))
                ++offset;
            if (offset < value.size() && value[offset] == '\n')
                ++offset;
            lines.push_back({at, offset});
            at = offset;
        }
        if (lines.empty())
            lines.push_back({0, 0});
        y += b.paragraph.before * scale * 25.4f / 72.f;
        std::size_t first = 0;
        while (first < lines.size()) {
            if (y + line > height)
                next();
            auto count = std::max<std::size_t>(
                1, std::min<std::size_t>(lines.size() - first, std::size_t((height - y) / line)));
            for (std::size_t row = 0; row < count; ++row) {
                pages.back().fragments.push_back(
                    {index, lines[first + row].first, lines[first + row].second, y, line, false});
                y += line;
            }
            first += count;
        }
        y += b.paragraph.after * scale * 25.4f / 72.f;
    }
    return pages;
}
RichDocumentData richDocumentFromMarkdown(const std::string &md) {
    Document parsed;
    parsed.setMarkdown(md);
    RichDocumentData out;
    for (const auto &b : parsed.blocks()) {
        RichBlock block;
        block.runs = b.inlines;
        block.paragraph.heading = b.kind == DocumentBlockKind::heading ? b.level : 0;
        if (block.paragraph.heading)
            block.paragraph.points = block.paragraph.heading == 1 ? 24.f : 18.f;
        if (b.kind == DocumentBlockKind::table) {
            block.kind = RichBlockKind::table;
            std::size_t cols = 1;
            for (const auto &r : b.rows)
                cols = std::max(cols, r.size());
            std::vector<GridCell> cells(b.rows.size() * cols);
            for (std::size_t r = 0; r < b.rows.size(); ++r)
                for (std::size_t c = 0; c < b.rows[r].size(); ++c) {
                    cells[r * cols + c].text = b.rows[r][c];
                    if (r == 0)
                        cells[r * cols + c].style = 1;
                }
            block.table.assign(std::max<std::size_t>(1, b.rows.size()), cols, cells);
            block.table.clearHistory();
        } else if (b.kind == DocumentBlockKind::code)
            block.runs = {{DocumentInlineKind::code, b.text, "", false, documentMono}};
        else if (b.inlines.size() == 1 && b.inlines[0].kind == DocumentInlineKind::image) {
            block.kind = RichBlockKind::image;
            block.image = b.inlines[0].target;
            block.caption = b.inlines[0].text;
        } else if (b.kind == DocumentBlockKind::list) {
            DocumentInline marker;
            marker.text = b.task     ? (b.checked ? "☑ " : "☐ ")
                          : b.number ? std::to_string(b.number) + ". "
                                     : "• ";
            block.runs.insert(block.runs.begin(), marker);
            block.paragraph.indent = float(b.indent) * 5;
        }
        out.blocks.push_back(std::move(block));
    }
    return out;
}
std::string richDocumentMarkdown(const RichDocumentData &d) {
    std::string out;
    for (const auto &b : d.blocks) {
        if (!out.empty())
            out += "\n\n";
        if (b.kind == RichBlockKind::paragraph) {
            out += std::string(std::size_t(std::clamp(b.paragraph.heading, 0, 6)), '#');
            if (b.paragraph.heading)
                out += ' ';
            out += RichText(b.runs).markdown();
        } else if (b.kind == RichBlockKind::image)
            out += "![" + b.caption + "](<" + b.image + ">)";
        else if (b.kind == RichBlockKind::pageBreak)
            out += "<!-- page break -->";
        else {
            for (std::size_t r = 0; r < b.table.rows(); ++r) {
                out += '|';
                for (std::size_t c = 0; c < b.table.columns(); ++c) {
                    auto value = b.table.cell(r, c).text;
                    for (char ch : value) {
                        if (ch == '|')
                            out += '\\';
                        out += ch == '\n' ? ' ' : ch;
                    }
                    out += " |";
                }
                out += '\n';
                if (r == 0) {
                    out += '|';
                    for (std::size_t c = 0; c < b.table.columns(); ++c)
                        out += " --- |";
                    out += '\n';
                }
            }
        }
    }
    return out;
}
struct RichDocumentEditor::Impl {
    RichDocumentData data;
    std::vector<RichPage> cachedPages;
    ImFont *layoutFont = nullptr;
    bool layoutDirty = true;
    std::deque<RichDocumentData> undo, redo;
    std::size_t selected = ~std::size_t(0), editing = ~std::size_t(0);
    float zoom = .8f;
    DocumentEditor text;
    GridState grid;
    GridModel table;
    RichPageSettings settings;
    bool openEdit = false, openPage = false;
    RichParagraphStyle paragraph;
    RichBlock imageDraft;
    std::string error;
    void save() {
        layoutDirty = true;
        undo.push_back(data);
        redo.clear();
        std::size_t bytes = 0;
        for (const auto &snapshot : undo)
            bytes += documentCost(snapshot);
        while (!undo.empty() && (undo.size() > 32 || bytes > 32 * 1024 * 1024)) {
            bytes -= documentCost(undo.front());
            undo.pop_front();
        }
    }
    void edit(std::size_t index) {
        if (index >= data.blocks.size())
            return;
        selected = editing = index;
        const auto &b = data.blocks[index];
        paragraph = b.paragraph;
        if (b.kind == RichBlockKind::table) {
            table = b.table;
            grid = {};
        } else if (b.kind == RichBlockKind::image) {
            imageDraft = b;
        } else {
            text.setMarkdown(RichText(b.runs).markdown());
            text.setMode(DocumentMode::live);
        }
        openEdit = true;
    }
};
RichDocumentEditor::RichDocumentEditor() : impl_(new Impl) {}
RichDocumentEditor::~RichDocumentEditor() = default;
void RichDocumentEditor::setDocument(const RichDocumentData &d) {
    validate(d);
    impl_->data = d;
    impl_->layoutDirty = true;
    impl_->undo.clear();
    impl_->redo.clear();
    impl_->selected = impl_->editing = ~std::size_t(0);
}
const RichDocumentData &RichDocumentEditor::document() const {
    return impl_->data;
}
bool RichDocumentEditor::undo() {
    auto &s = *impl_;
    if (s.undo.empty())
        return false;
    s.redo.push_back(s.data);
    s.data = std::move(s.undo.back());
    s.layoutDirty = true;
    s.undo.pop_back();
    s.selected = ~std::size_t(0);
    return true;
}
bool RichDocumentEditor::redo() {
    auto &s = *impl_;
    if (s.redo.empty())
        return false;
    s.undo.push_back(s.data);
    s.data = std::move(s.redo.back());
    s.layoutDirty = true;
    s.redo.pop_back();
    s.selected = ~std::size_t(0);
    return true;
}
std::size_t RichDocumentEditor::selectedBlock() const {
    return impl_->selected;
}
bool RichDocumentEditor::selectBlock(std::size_t index) {
    if (index >= impl_->data.blocks.size())
        return false;
    impl_->selected = index;
    return true;
}
bool RichDocumentEditor::editBlock(std::size_t index) {
    if (index >= impl_->data.blocks.size() || impl_->data.blocks[index].kind == RichBlockKind::pageBreak)
        return false;
    impl_->edit(index);
    return true;
}
void RichDocumentEditor::setZoom(float zoom) {
    if (!std::isfinite(zoom))
        throw std::invalid_argument("Invalid document zoom");
    impl_->zoom = std::clamp(zoom, .25f, 1.5f);
}
bool RichDocumentEditor::draw(const char *id, ImVec2 size, const RichDocumentOptions &o) {
    ScopedID scope{id};
    auto &s = *impl_;
    const auto pagePopup = tr("Page setup") + "###richPage" + std::to_string(ImGui::GetID("##pageSetup"));
    const auto editPopup = tr("Edit selection") + "###richEdit" + std::to_string(ImGui::GetID("##editBlock"));
    bool changed = false;
    if (size.x <= 0)
        size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0)
        size.y = dp(540);
    ImGui::BeginChild("##richDocument", size);
    auto appendBlock = [&](RichBlock block) {
        try {
            auto next = s.data;
            next.blocks.push_back(std::move(block));
            validate(next);
            s.save();
            s.data = std::move(next);
            changed = true;
            s.error.clear();
            return true;
        } catch (const std::exception &error) {
            s.error = error.what();
            return false;
        }
    };
    if (o.toolbar) {
        ImGui::BeginDisabled(o.readOnly);
        if (ghostButton(trLabel("Add paragraph").c_str())) {
            RichBlock b;
            b.runs = {{DocumentInlineKind::text, tr("New paragraph")}};
            if (appendBlock(std::move(b)))
                s.edit(s.data.blocks.size() - 1);
        }
        ImGui::SameLine();
        if (ghostButton(trLabel("Insert table").c_str())) {
            RichBlock b;
            b.kind = RichBlockKind::table;
            if (appendBlock(std::move(b)))
                s.edit(s.data.blocks.size() - 1);
        }
        ImGui::SameLine();
        if (ghostButton(trLabel("Page break").c_str())) {
            RichBlock b;
            b.kind = RichBlockKind::pageBreak;
            appendBlock(std::move(b));
        }
        ImGui::SameLine();
        if (ghostButton(trLabel("Page setup").c_str())) {
            s.settings = s.data.page;
            s.openPage = true;
        }
        ImGui::SameLine();
        if (ghostButton(trLabel("Undo").c_str()))
            changed |= undo();
        ImGui::SameLine();
        if (ghostButton(trLabel("Redo").c_str()))
            changed |= redo();
        if (s.selected < s.data.blocks.size()) {
            if (ghostButton(trLabel("Edit selection").c_str()))
                s.edit(s.selected);
            ImGui::SameLine();
            if (ghostButton(trLabel("Delete block").c_str())) {
                s.save();
                s.data.blocks.erase(s.data.blocks.begin() + s.selected);
                s.selected = ~std::size_t(0);
                changed = true;
            }
            ImGui::SameLine();
            if (ghostButton(trLabel("Move up").c_str()) && s.selected > 0 &&
                s.selected < s.data.blocks.size()) {
                s.save();
                std::swap(s.data.blocks[s.selected], s.data.blocks[s.selected - 1]);
                --s.selected;
                changed = true;
            }
            ImGui::SameLine();
            if (ghostButton(trLabel("Move down").c_str()) && s.selected + 1 < s.data.blocks.size()) {
                s.save();
                std::swap(s.data.blocks[s.selected], s.data.blocks[s.selected + 1]);
                ++s.selected;
                changed = true;
            }
        }
        if (o.importImage && ghostButton(trLabel("Insert image").c_str())) {
            auto b = o.importImage();
            if (!b.image.empty()) {
                b.kind = RichBlockKind::image;
                appendBlock(std::move(b));
            }
        }
        ImGui::EndDisabled();
        ImGui::SetNextItemWidth(dp(140));
        ImGui::SliderFloat(trLabel("Zoom").c_str(), &s.zoom, .25f, 1.5f, "%.2fx");
    }
    if (!s.error.empty())
        callout(Tone::error, s.error.c_str());
    if (s.layoutDirty || s.layoutFont != ImGui::GetFont()) {
        s.cachedPages = layoutRichDocument(s.data);
        s.layoutFont = ImGui::GetFont();
        s.layoutDirty = false;
    }
    const auto &pages = s.cachedPages;
    const float z = s.zoom;
    const float width = dp(mm(s.data.page.widthMM) * z), height = dp(mm(s.data.page.heightMM) * z);
    DocumentState reader;
    Document dummy;
    DocumentOptions options;
    options.services = o.services;
    options.imageZoom = true;
    ImGui::BeginChild("##pages", ImGui::GetContentRegionAvail(), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (std::size_t page = 0; page < pages.size(); ++page) {
        ScopedID pkey{int(page)};
        const auto start = ImGui::GetCursorScreenPos();
        const float offset = std::max(0.f, (ImGui::GetContentRegionAvail().x - width) * .5f);
        ImGui::SetCursorScreenPos({start.x + offset, start.y});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, palette().bg1);
        const bool pageVisible =
            ImGui::BeginChild("##page", {width, height}, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (!pageVisible) {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::SetCursorScreenPos({start.x, start.y + height + dp(22)});
            ImGui::Dummy({width, dp(1)});
            continue;
        }
        const auto origin = ImGui::GetWindowPos();
        auto *draw = ImGui::GetWindowDrawList();
        const float left = dp(mm(s.data.page.leftMM) * z), top = dp(mm(s.data.page.topMM) * z),
                    bodyWidth = width - left - dp(mm(s.data.page.rightMM) * z);
        draw->AddText({origin.x + left, origin.y + dp(mm(s.data.page.topMM * .5f) * z)}, palette().textDim,
                      pageText(s.data.page.header, page, pages.size()).c_str());
        draw->AddText({origin.x + left, origin.y + height - dp(mm(s.data.page.bottomMM * .6f) * z)},
                      palette().textDim, pageText(s.data.page.footer, page, pages.size()).c_str());
        for (const auto &fragment : pages[page].fragments) {
            ScopedID bkey{int(fragment.block)};
            ScopedID fragmentKey{int(fragment.first)};
            const auto &b = s.data.blocks[fragment.block];
            const float indent =
                std::clamp(dp(mm(b.paragraph.indent) * z), 0.f, std::max(0.f, bodyWidth - dp(mm(1) * z)));
            const ImVec2 pos{origin.x + left + indent, origin.y + top + dp(fragment.y * z)};
            ImGui::SetCursorScreenPos(pos);
            const ImVec2 bounds{bodyWidth - indent, std::max(dp(12), dp(fragment.height * z))};
            ImGui::InvisibleButton("##block", bounds);
            if (ImGui::IsItemClicked()) {
                s.selected = fragment.block;
                if (!o.readOnly && ImGui::IsMouseDoubleClicked(0))
                    s.edit(fragment.block);
            }
            if (s.selected == fragment.block)
                draw->AddRect(pos, {pos.x + bounds.x, pos.y + bounds.y}, palette().accent, 0, 0, dp(1));
            ImGui::SetCursorScreenPos(pos);
            if (b.kind == RichBlockKind::paragraph) {
                ImGui::PushFont(ImGui::GetFont(), dp(b.paragraph.points * 96.f / 72.f * z));
                DocumentRenderer render{reader, dummy, options};
                render.text(slice(b.runs, fragment.first, fragment.last), 1,
                            b.paragraph.alignment == GridAlignment::right    ? 3
                            : b.paragraph.alignment == GridAlignment::center ? 2
                                                                             : 0);
                ImGui::PopFont();
            } else if (b.kind == RichBlockKind::image) {
                auto image = o.services && o.services->image ? o.services->image(b.image) : ImageData{};
                if (image.texture)
                    draw->AddImage(image.texture, pos,
                                   {pos.x + dp(fragment.height * z) * b.imageWidthMM / b.imageHeightMM,
                                    pos.y + dp(fragment.height * z)},
                                   image.uvMin, image.uvMax);
                else {
                    textDim("%s", b.caption.c_str());
                }
            } else if (b.kind == RichBlockKind::table) {
                float rowHeight = bounds.y / float(fragment.last - fragment.first),
                      columnWidth = bounds.x / float(b.table.columns());
                for (std::size_t r = fragment.first; r < fragment.last; ++r)
                    for (std::size_t c = 0; c < b.table.columns(); ++c) {
                        ImVec2 a{pos.x + columnWidth * c, pos.y + rowHeight * (r - fragment.first)};
                        draw->AddRect(a, {a.x + columnWidth, a.y + rowHeight}, palette().line);
                        draw->PushClipRect(a, {a.x + columnWidth, a.y + rowHeight}, true);
                        const auto &cell = b.table.cell(r, c);
                        draw->AddText((cell.style & 1) && boldFont() ? boldFont() : ImGui::GetFont(),
                                      dp(11 * 96.f / 72.f * z), {a.x + dp(3), a.y + dp(3)}, palette().text,
                                      cell.text.c_str());
                        draw->PopClipRect();
                    }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({start.x, start.y + height + dp(22)});
        ImGui::Dummy({width, dp(1)});
    }
    ImGui::EndChild();
    if (s.openPage) {
        ImGui::OpenPopup(pagePopup.c_str());
        s.openPage = false;
    }
    if (ImGui::BeginPopupModal(pagePopup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto &p = s.settings;
        ImGui::BeginDisabled(o.readOnly);
        ImGui::InputFloat(trLabel("Page width (mm)").c_str(), &p.widthMM);
        ImGui::InputFloat(trLabel("Page height (mm)").c_str(), &p.heightMM);
        ImGui::InputFloat(trLabel("Left margin (mm)").c_str(), &p.leftMM);
        ImGui::InputFloat(trLabel("Right margin (mm)").c_str(), &p.rightMM);
        ImGui::InputFloat(trLabel("Top margin (mm)").c_str(), &p.topMM);
        ImGui::InputFloat(trLabel("Bottom margin (mm)").c_str(), &p.bottomMM);
        detail::inputText(trLabel("Header").c_str(), p.header);
        detail::inputText(trLabel("Footer").c_str(), p.footer);
        if (primaryButton(trLabel("Apply").c_str())) {
            auto test = s.data;
            test.page = p;
            try {
                validate(test);
                s.save();
                s.data.page = p;
                changed = true;
                ImGui::CloseCurrentPopup();
                s.error.clear();
            } catch (const std::exception &e) {
                s.error = e.what();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ghostButton(trLabel("Cancel").c_str()))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (s.openEdit) {
        ImGui::OpenPopup(editPopup.c_str());
        s.openEdit = false;
    }
    ImGui::SetNextWindowSize(dp(900, 620), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(editPopup.c_str(), nullptr, ImGuiWindowFlags_None)) {
        if (s.editing < s.data.blocks.size()) {
            const bool table = s.data.blocks[s.editing].kind == RichBlockKind::table;
            const bool image = s.data.blocks[s.editing].kind == RichBlockKind::image;
            if (image) {
                detail::inputText(trLabel("Image caption").c_str(), s.imageDraft.caption);
                ImGui::InputFloat(trLabel("Image width (mm)").c_str(), &s.imageDraft.imageWidthMM);
                ImGui::InputFloat(trLabel("Image height (mm)").c_str(), &s.imageDraft.imageHeightMM);
            } else if (table) {
                GridOptions gridOptions;
                gridOptions.readOnly = o.readOnly;
                dataGrid("##tableEditor", s.grid, s.table, {0, ImGui::GetContentRegionAvail().y - dp(45)},
                         gridOptions);
            } else {
                auto &p = s.paragraph;
                ImGui::SetNextItemWidth(dp(100));
                ImGui::SliderFloat(trLabel("Font size").c_str(), &p.points, 4, 72, "%.0f pt");
                ImGui::SameLine();
                int align = std::max(0, int(p.alignment) - 1);
                const char *alignments[]{"Left", "Center", "Right"};
                std::string a0 = tr(alignments[0]), a1 = tr(alignments[1]), a2 = tr(alignments[2]);
                const char *items[]{a0.c_str(), a1.c_str(), a2.c_str()};
                ImGui::SetNextItemWidth(dp(120));
                if (ImGui::Combo(trLabel("Alignment").c_str(), &align, items, 3))
                    p.alignment = GridAlignment(align + 1);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(dp(100));
                ImGui::SliderFloat(trLabel("Line spacing").c_str(), &p.lineSpacing, .8f, 3, "%.1f");
                DocumentEditorOptions textOptions;
                textOptions.document.services = o.services;
                textOptions.readOnly = o.readOnly;
                textOptions.modeSelector = false;
                s.text.draw("##paragraph", {0, ImGui::GetContentRegionAvail().y - dp(45)}, textOptions);
            }
            ImGui::BeginDisabled(o.readOnly);
            if (primaryButton(trLabel("Apply").c_str())) {
                try {
                    auto next = s.data;
                    if (image)
                        next.blocks[s.editing] = s.imageDraft;
                    else if (table) {
                        next.blocks[s.editing].table = s.table;
                        next.blocks[s.editing].table.clearHistory();
                    } else {
                        auto converted = richDocumentFromMarkdown(s.text.markdown());
                        for (auto &block : converted.blocks)
                            block.paragraph = s.paragraph;
                        next.blocks.erase(next.blocks.begin() + s.editing);
                        next.blocks.insert(next.blocks.begin() + s.editing, converted.blocks.begin(),
                                           converted.blocks.end());
                    }
                    validate(next);
                    s.save();
                    s.data = std::move(next);
                    changed = true;
                    s.error.clear();
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception &error) {
                    s.error = error.what();
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ghostButton(trLabel("Cancel").c_str()))
                ImGui::CloseCurrentPopup();
        } else
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    return changed;
}
} // namespace cyui

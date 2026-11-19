#include "cyui/modules/CodePaging.h"
#include <algorithm>

namespace cyui {
namespace {
bool fail(std::string *error, const char *message) {
    if (error) *error = message;
    return false;
}
bool add(std::uint64_t a, std::uint64_t b, std::uint64_t &result) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) return false;
    result = a + b;
    return true;
}
unsigned sequence(unsigned char c) {
    return c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 :
           c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
}
bool decode(std::string_view text, std::uint32_t &value) {
    if (text.empty() || sequence(static_cast<unsigned char>(text[0])) != text.size()) return false;
    const auto first = static_cast<unsigned char>(text[0]);
    value = first & (text.size() == 1 ? 0x7f : text.size() == 2 ? 0x1f : text.size() == 3 ? 0x0f : 7);
    for (std::size_t i = 1; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if ((c & 0xc0) != 0x80) return false;
        value = (value << 6) | (c & 0x3f);
    }
    return !(value == 0 || value == '\r' || value == '\n' ||
             (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff ||
             (text.size() == 2 && value < 0x80) || (text.size() == 3 && value < 0x800) ||
             (text.size() == 4 && value < 0x10000));
}
bool boundary(std::string_view text, std::size_t at) {
    return at <= text.size() && (at == text.size() || (static_cast<unsigned char>(text[at]) & 0xc0) != 0x80);
}
} // namespace

bool pagedCodeGlyphs(std::string_view text, std::uint64_t character,
                     std::uint64_t column, unsigned tabSize,
                     std::vector<PagedCodeGlyph> &output, std::string *error) {
    if (tabSize < 1 || tabSize > 16 || text.size() > 65536)
        return fail(error, "Invalid tab size or oversized line fragment");
    std::vector<PagedCodeGlyph> glyphs;
    glyphs.reserve(text.size());
    for (std::size_t byte = 0; byte < text.size();) {
        const auto count = sequence(static_cast<unsigned char>(text[byte]));
        std::uint32_t value = 0;
        if (!count || count > text.size() - byte || !decode(text.substr(byte, count), value))
            return fail(error, "Invalid UTF-8 or line terminator in line fragment");
        const unsigned units = value > 0xffff ? 2 : 1;
        const unsigned cells = value == '\t' ? tabSize - unsigned(column % tabSize) : units;
        glyphs.push_back({byte, count, character, column, value, units, cells});
        if (!add(character, units, character) || !add(column, cells, column))
            return fail(error, "Line coordinates overflow");
        byte += count;
    }
    output.swap(glyphs);
    if (error) error->clear();
    return true;
}

bool validateCodePage(const PagedCodeRead &request, const PagedCodeDocument &document,
                      const PagedCodePage &page, std::string *error) {
    if (!request.id || !document.id || !document.revision || !document.lines ||
        request.document != document.id || request.revision != document.revision ||
        page.request != request.id || page.document != document.id || page.revision != document.revision)
        return fail(error, "Stale page identity or revision");
    if (!request.lines || request.lines > 4096 || !request.columns || request.columns > 8192 ||
        !request.maxBytes || request.maxBytes > 4 * 1024 * 1024 ||
        request.tabSize < 1 || request.tabSize > 16 || request.firstLine >= document.lines)
        return fail(error, "Invalid page request bounds");
    const auto count = std::min<std::uint64_t>(request.lines, document.lines - request.firstLine);
    if (page.lines.size() != count || (!request.reveal && page.firstColumn != request.firstColumn))
        return fail(error, "Incomplete or misplaced page");
    std::uint64_t until = 0;
    if (!add(page.firstColumn, request.columns, until)) return fail(error, "Page columns overflow");
    std::size_t bytes = 0, tokenCount = 0;
    bool revealed = !request.reveal;
    for (std::size_t i = 0; i < page.lines.size(); ++i) {
        const auto &line = page.lines[i];
        if (line.line != request.firstLine + i || line.firstCharacter > line.characters ||
            line.firstColumn > line.columns || line.columns < line.characters || line.firstColumn < line.firstCharacter ||
            line.firstByte < line.firstCharacter || (!line.firstCharacter && (line.firstColumn || line.firstByte)))
            return fail(error, "Invalid line coordinates");
        if (line.text.size() > 65536 || line.tokens.size() > 65536 - tokenCount)
            return fail(error, "Page fragment or token budget exceeded");
        tokenCount += line.tokens.size();
        const auto size = line.text.size() + line.tokens.size() * sizeof(CodeToken) + sizeof(PagedCodeLine);
        if (size > request.maxBytes - bytes) return fail(error, "Page byte budget exceeded");
        bytes += size;
        std::vector<PagedCodeGlyph> glyphs;
        if (!pagedCodeGlyphs(line.text, line.firstCharacter, line.firstColumn, request.tabSize, glyphs, error)) return false;
        const auto endCharacter = glyphs.empty() ? line.firstCharacter : glyphs.back().character + glyphs.back().characters;
        const auto endColumn = glyphs.empty() ? line.firstColumn : glyphs.back().column + glyphs.back().cells;
        if (endCharacter > line.characters || endColumn > line.columns ||
            (endCharacter == line.characters && endColumn != line.columns) ||
            line.firstColumn > page.firstColumn || endColumn < std::min(until, line.columns))
            return fail(error, "Line fragment does not cover the requested columns");
        std::size_t previous = 0;
        for (const auto &token : line.tokens) {
            if (!token.length || token.start < previous || token.start > line.text.size() ||
                token.length > line.text.size() - token.start ||
                !boundary(line.text, token.start) || !boundary(line.text, token.start + token.length) ||
                int(token.role) < 0 || token.role >= TokenRole::count)
                return fail(error, "Invalid UTF-8 token span");
            previous = token.start + token.length;
        }
        if (request.reveal && request.reveal->line == line.line) {
            std::optional<std::uint64_t> column;
            if (request.reveal->character >= line.characters) column = line.columns;
            for (const auto &glyph : glyphs)
                if (glyph.character == request.reveal->character) column = glyph.column;
            if (column && *column >= page.firstColumn && *column < until) revealed = true;
        }
    }
    if (!revealed) return fail(error, "Requested position is not a visible UTF-16 boundary");
    if (error) error->clear();
    return true;
}

PagedCodeLineBuilder::PagedCodeLineBuilder(std::uint64_t line, std::uint64_t first,
                                           std::uint32_t columns, unsigned tabSize,
                                           std::optional<std::uint64_t> revealCharacter)
    : wanted_(first), tabSize_(tabSize), revealCharacter_(revealCharacter) {
    value_.line = line;
    if (!columns || columns > 8192 || tabSize < 1 || tabSize > 16 || !add(first, columns, until_))
        error_ = "Invalid line window";
}
bool PagedCodeLineBuilder::scalar(std::string_view bytes, std::uint32_t value) {
    const unsigned characters = value > 0xffff ? 2 : 1;
    const unsigned cells = value == '\t' ? tabSize_ - unsigned(value_.columns % tabSize_) : characters;
    std::uint64_t nextColumn = 0, nextCharacter = 0, nextByte = 0;
    if (!add(value_.columns, cells, nextColumn) || !add(value_.characters, characters, nextCharacter) ||
        !add(byte_, bytes.size(), nextByte)) { error_ = "Line coordinates overflow"; return false; }
    if (revealCharacter_) {
        if (*revealCharacter_ == value_.characters) { revealedColumn_ = value_.columns; revealedByte_ = byte_; }
        if (*revealCharacter_ > value_.characters && *revealCharacter_ < nextCharacter) {
            error_ = "Position splits a UTF-16 surrogate pair"; return false;
        }
    }
    if (nextColumn > wanted_ && value_.columns < until_) {
        if (!started_) {
            started_ = true; value_.firstByte = byte_;
            value_.firstCharacter = value_.characters; value_.firstColumn = value_.columns;
        }
        value_.text.append(bytes.data(), bytes.size());
    }
    value_.characters = nextCharacter; value_.columns = nextColumn; byte_ = nextByte;
    return true;
}
bool PagedCodeLineBuilder::append(std::string_view chunk) {
    if (finished_ || !error_.empty()) return false;
    for (const char c : chunk) {
        pending_ += c;
        const auto count = sequence(static_cast<unsigned char>(pending_[0]));
        if (!count || pending_.size() > count) { error_ = "Invalid UTF-8 line"; return false; }
        if (pending_.size() == count) {
            std::uint32_t value = 0;
            if (!decode(pending_, value)) { error_ = "Invalid UTF-8 or line terminator"; return false; }
            if (!scalar(pending_, value)) return false;
            pending_.clear();
        }
    }
    return true;
}
bool PagedCodeLineBuilder::finish(PagedCodeLine &line, std::string *error) {
    if (finished_) return fail(error, "Line builder was already consumed");
    if (!pending_.empty()) error_ = "Truncated UTF-8 line";
    if (!error_.empty()) { if (error) *error = error_; return false; }
    if (revealCharacter_ && *revealCharacter_ >= value_.characters) {
        revealedColumn_ = value_.columns; revealedByte_ = byte_;
    }
    if (!started_) {
        value_.firstCharacter = value_.characters;
        value_.firstColumn = value_.columns;
        value_.firstByte = byte_;
    }
    line = std::move(value_); finished_ = true;
    if (error) error->clear();
    return true;
}
} // namespace cyui

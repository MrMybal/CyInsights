#pragma once
#include "cyui/modules/Code.h"
#include <limits>

// Presentation contract, owned by CyUI::Code. No file, engine or worker ownership.
#define CYUI_CODE_PAGING 1
namespace cyui {
struct PagedCodePosition {
    std::uint64_t line = 0, character = 0; // zero-based line, UTF-16 code units
};
inline bool operator==(PagedCodePosition a, PagedCodePosition b) {
    return a.line == b.line && a.character == b.character;
}
inline bool operator!=(PagedCodePosition a, PagedCodePosition b) { return !(a == b); }
inline bool operator<(PagedCodePosition a, PagedCodePosition b) {
    return a.line < b.line || (a.line == b.line && a.character < b.character);
}
struct PagedCodeSelection {
    PagedCodePosition anchor{}, caret{};
    bool empty() const { return anchor == caret; }
    PagedCodePosition first() const { return caret < anchor ? caret : anchor; }
    PagedCodePosition last() const { return caret < anchor ? anchor : caret; }
};
struct PagedCodeDocument {
    ItemId id = 0;                 // stable document identity, nonzero
    std::uint64_t revision = 0;    // opaque, nonzero content revision
    std::uint64_t lines = 1;       // includes an empty final line when present
    std::uint64_t columnsHint = 0; // optional display-cell extent; never a text-size limit
    bool readOnly = false;
};
struct PagedCodeRead {
    std::uint64_t id = 0;
    ItemId document = 0;
    std::uint64_t revision = 0, firstLine = 0, firstColumn = 0;
    std::uint32_t lines = 0, columns = 0, tabSize = 4;
    std::size_t maxBytes = 0;
    // A host resolves this UTF-16 position and may adjust page.firstColumn to reveal it.
    std::optional<PagedCodePosition> reveal;
};
struct PagedCodeLine {
    std::uint64_t line = 0;
    std::uint64_t firstCharacter = 0, firstColumn = 0, firstByte = 0;
    std::uint64_t characters = 0, columns = 0; // complete line metrics, not fragment lengths
    std::string text;                         // bounded UTF-8 fragment, no line terminators/BOM
    std::vector<CodeToken> tokens;            // fragment-local UTF-8 byte spans with host context
};
struct PagedCodePage {
    std::uint64_t request = 0;
    ItemId document = 0;
    std::uint64_t revision = 0, firstColumn = 0;
    std::vector<PagedCodeLine> lines;          // one fragment for every requested logical line
};
struct PagedCodeGlyph {
    std::size_t firstByte = 0, bytes = 0;
    std::uint64_t character = 0, column = 0;
    std::uint32_t value = 0, characters = 0, cells = 0;
};
// Strict UTF-8; one cell per BMP scalar/two per supplementary scalar, tabs at tab stops.
// Complex-script shaping, bidi, grapheme-cluster navigation and soft wrapping are not provided.
CYUI_API bool pagedCodeGlyphs(std::string_view, std::uint64_t character,
                             std::uint64_t column, unsigned tabSize,
                             std::vector<PagedCodeGlyph> &, std::string *error = nullptr);
CYUI_API bool validateCodePage(const PagedCodeRead &, const PagedCodeDocument &,
                              const PagedCodePage &, std::string *error = nullptr);

// Outside draw: builds a bounded horizontal fragment while scanning one line in chunks.
// append accepts arbitrary UTF-8 chunk boundaries. The host retains all I/O/index ownership.
class CYUI_API PagedCodeLineBuilder {
  public:
    PagedCodeLineBuilder(std::uint64_t line, std::uint64_t firstColumn,
                         std::uint32_t columns, unsigned tabSize = 4,
                         std::optional<std::uint64_t> revealCharacter = {});
    bool append(std::string_view);
    bool finish(PagedCodeLine &, std::string *error = nullptr);
    std::optional<std::uint64_t> revealedColumn() const { return revealedColumn_; }
    std::optional<std::uint64_t> revealedByte() const { return revealedByte_; }
  private:
    PagedCodeLine value_;
    std::string pending_, error_;
    std::uint64_t wanted_ = 0, until_ = 0, byte_ = 0;
    unsigned tabSize_ = 4;
    bool started_ = false, finished_ = false;
    std::optional<std::uint64_t> revealCharacter_, revealedColumn_, revealedByte_;
    bool scalar(std::string_view, std::uint32_t);
};
} // namespace cyui

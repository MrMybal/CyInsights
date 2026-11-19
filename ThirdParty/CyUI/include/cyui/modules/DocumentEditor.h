#pragma once
#include "cyui/modules/DocumentViews.h"
#include <memory>
namespace cyui {
// UTF-8 rich text model. Positions are byte boundaries; invalid edits are rejected.
class CYUI_API RichText {
  public:
    RichText() = default;
    explicit RichText(std::vector<DocumentInline> runs) : runs_(std::move(runs)) {}
    std::string text() const;
    std::string markdown() const;
    bool replace(std::size_t first, std::size_t last, const std::string &text);
    const std::vector<DocumentInline> &runs() const { return runs_; }

  private:
    std::vector<DocumentInline> runs_;
};
enum class DocumentMode { live, split, source, reading };
enum class DocumentCommand {
    bold,
    italic,
    underline,
    strike,
    spoiler,
    code,
    link,
    heading1,
    heading2,
    heading3,
    bullet,
    numbered,
    task,
    quote,
    indent,
    outdent,
    table,
    undo,
    redo,
    find
};
struct DocumentSpellingIssue {
    std::size_t begin = 0, end = 0;
    std::string word;
    std::vector<std::string> suggestions;
};
struct DocumentEditorOptions {
    DocumentOptions document;
    bool toolbar = true, modeSelector = true, readOnly = false, lineNumbers = false;
    // Requests clipboard HTML/image import, file drop or spell checking through host services.
    // The callback returns Markdown, with media already registered in the host's resource store.
    std::function<std::string()> pasteRich;
    bool floatingToolbar = true;
    // ImGui payload bytes, never interpreted as filenames by CyUI. Return imported Markdown.
    std::string dropType = "CYUI_MARKDOWN";
    std::function<std::string(ArrayView<unsigned char>)> drop;
    std::string spellingLanguage = "en";
    std::function<std::vector<DocumentSpellingIssue>(const std::string &, const std::string &)> spelling;
    std::uint64_t spellingRevision = 0;
};
struct DocumentEditorResult {
    bool changed = false;
    DocumentResult document;
};
class CYUI_API DocumentEditor {
  public:
    DocumentEditor();
    ~DocumentEditor();
    DocumentEditor(const DocumentEditor &) = delete;
    DocumentEditor &operator=(const DocumentEditor &) = delete;
    void setMarkdown(const std::string &);
    std::string markdown() const;
    // Current parsed document and its revision. The editor owns the snapshot.
    const Document &document() const;
    void setMode(DocumentMode);
    DocumentMode mode() const;
    // Source UTF-8 byte offsets, shared by all modes. Selection must be on codepoint boundaries.
    bool select(std::size_t first, std::size_t last);
    std::pair<std::size_t, std::size_t> selection() const;
    // Request native keyboard focus at the current source selection, including in Live.
    void focus();
    bool insert(const std::string &markdown);
    bool command(DocumentCommand, const std::string &argument = {});
    bool canUndo() const;
    bool canRedo() const;
    DocumentEditorResult draw(const char *id, ImVec2 size = {}, const DocumentEditorOptions &options = {});

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cyui

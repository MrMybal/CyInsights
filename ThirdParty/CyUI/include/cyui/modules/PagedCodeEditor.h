#pragma once
#include "cyui/modules/CodePaging.h"
#include <memory>

namespace cyui {
struct PagedCodeReplacement {
    std::uint64_t id = 0;
    ItemId document = 0;
    std::uint64_t revision = 0;
    PagedCodeSelection selection{};
    std::string text; // replacement of ordered selection, UTF-8; host owns newline policy
};
enum class PagedCodeCommand { copy, cut, undo, redo, save, find, findNext, findPrevious };
enum class PagedCodeInputKind {
    text, backspace, deleteForward, left, right, up, down, pageUp, pageDown,
    home, end, documentHome, documentEnd, selectAll, action
};
struct PagedCodeInput {
    PagedCodeInputKind kind = PagedCodeInputKind::text;
    std::string text;
    bool extend = false;
    PagedCodeCommand action = PagedCodeCommand::copy;
};
struct PagedCodeAction {
    std::uint64_t id = 0;
    ItemId document = 0;
    std::uint64_t revision = 0;
    PagedCodeSelection selection{};
    PagedCodeCommand command = PagedCodeCommand::copy;
};
struct PagedCodeEditorResult {
    std::optional<PagedCodeRead> read; // issued once; perform/cache the query outside draw
    Edit<PagedCodeReplacement> edit;  // atomic commit proposal, issued once; never applied by CyUI
    Edit<PagedCodeSelection> selection;
    std::vector<PagedCodeAction> actions;
    // Preserve these intents for host recovery when an external revision/rejection
    // cancels typing queued behind an asynchronous edit. Never silently replay them.
    std::vector<PagedCodeInput> cancelledInput;
    std::uint64_t cancelledRead = 0;
    bool focused = false, waitingForPage = false, waitingForEdit = false;
    std::string error;
};
struct PagedCodeEditorOptions {
    unsigned tabSize = 4;
    std::size_t maxPageBytes = 4 * 1024 * 1024, maxInsertionBytes = 1024 * 1024;
    float textScale = 1.f;
    bool wheelZoom = true, lineNumbers = true, keyboardShortcuts = true;
    std::string newline = "\n";
};
// CyUI::CodeEditor, C++20. One instance per view, used only on the UI thread.
// Only bounded prepared fragments are retained. The application owns documents,
// asynchronous reads, syntax context, clipboard exports, search and Undo/Redo.
/**
 * @doc api-paged-code-editor
 * @locale en
 * @title PagedCodeEditor
 * @section API
 * @symbol cyui::PagedCodeEditor
 * Virtual code editing over host-owned, revisioned line fragments (C++20).
 *
 * ## Contract {#contract}
 * Keep one view per stable widget ID. Call all methods on its UI thread.
 * `draw` emits one-shot bounded reads, guarded atomic edit proposals and actions.
 * Prepare pages outside drawing; `supplyPage` rejects stale IDs/revisions and
 * malformed UTF-8 spans. The host commits one Undo transaction before
 * `acceptEdit`, or calls `rejectEdit`; preserve `cancelledInput` for recovery.
 * Positions use zero-based UTF-16; syntax uses fragment-local UTF-8 bytes.
 * CyUI owns neither files, workers, Undo history nor language servers.
 * [Provider, coordinates and limits](doc:paged-code).
 */
/**
 * @doc api-paged-code-editor
 * @locale fr
 * @title PagedCodeEditor
 * @section API
 * @symbol cyui::PagedCodeEditor
 * Édition virtuelle par fragments de lignes révisés appartenant à l’hôte (C++20).
 *
 * ## Contrat {#contract}
 * Gardez une vue par ID de widget stable. Appelez ses méthodes sur le thread UI.
 * `draw` émet une fois les lectures bornées, propositions atomiques et actions.
 * Préparez les pages hors dessin ; `supplyPage` refuse IDs/révisions périmés et
 * plages UTF-8 incorrectes. L’hôte valide une transaction Undo avant
 * `acceptEdit`, ou appelle `rejectEdit` ; conservez `cancelledInput` pour récupération.
 * Positions en UTF-16 depuis zéro ; syntaxe en octets UTF-8 locaux au fragment.
 * CyUI ne possède ni fichiers, ni workers, ni historique Undo, ni serveurs de langage.
 * [Fournisseur, coordonnées et limites](doc:paged-code).
 */
class CYUI_API PagedCodeEditor {
  public:
    PagedCodeEditor();
    ~PagedCodeEditor();
    PagedCodeEditor(const PagedCodeEditor &) = delete;
    PagedCodeEditor &operator=(const PagedCodeEditor &) = delete;
    void setDocument(PagedCodeDocument);
    PagedCodeDocument document() const;
    bool setOptions(const PagedCodeEditorOptions &);
    PagedCodeEditorOptions options() const;
    PagedCodeSelection selection() const;
    bool setSelection(PagedCodeSelection, bool reveal = true);
    void scrollTo(std::uint64_t line, std::uint64_t column = 0);
    std::uint64_t firstLine() const;
    std::uint64_t firstColumn() const;
    std::optional<PagedCodeRead> pendingRead() const;
    bool supplyPage(const PagedCodePage &); // rejects stale IDs/revisions/coordinates/invalid spans
    bool failRead(std::uint64_t request, const std::string &message);
    void retryRead();
    bool replaceSelection(std::string_view); // queues one atomic guarded proposal
    bool requestAction(PagedCodeCommand);    // host keymaps may use this with keyboardShortcuts=false
    // Acknowledge only after the host committed the matching replacement.
    bool acceptEdit(std::uint64_t id, PagedCodeDocument, PagedCodeSelection);
    bool rejectEdit(std::uint64_t id, const std::string &message = {});
    PagedCodeEditorResult draw(const char *id, ImVec2 size = {});
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cyui

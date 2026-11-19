#pragma once
#include "cyui/modules/Code.h"

namespace cyui {
enum class DiffKind { context, added, removed, hunk, notice };
struct DiffRow {
    DiffKind kind = DiffKind::context;
    int oldLine = 0, newLine = 0;
    CodeLine code;
    std::size_t inlineStart = 0, inlineEnd = 0;
};
struct DiffPair {
    int left = -1, right = -1;
};
class CYUI_API DiffDocument {
  public:
    void setTexts(const std::string &before, const std::string &after, CodeLanguage = CodeLanguage::plain,
                  std::size_t workBudget = 4000000);
    void setUnified(const std::string &, CodeLanguage = CodeLanguage::plain);
    const std::vector<DiffRow> &rows() const {
        return rows_;
    }
    const std::vector<DiffPair> &pairs() const {
        return pairs_;
    }
    const std::vector<int> &hunks() const {
        return hunks_;
    }
    bool approximate() const {
        return approximate_;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    std::size_t columns() const {
        return columns_;
    }

  private:
    void prepare(CodeLanguage);
    std::vector<DiffRow> rows_;
    std::vector<DiffPair> pairs_;
    std::vector<int> hunks_;
    std::size_t columns_ = 0;
    bool approximate_ = false;
    std::uint64_t revision_ = 0;
};
struct DiffState {
    bool split = true;
    // Shared vertical offset, reported after input; assigning it requests a scroll.
    float scroll = 0;
    int selected = -1, focus = -1, hunk = -1;
    std::string query;
    float textScale = 1.f; // Shared by both split columns; 0.5..3
    bool wheelZoom = true;
};
struct DiffResult {
    int row = -1, oldLine = 0, newLine = 0;
};
// Split columns share one vertical canvas and one horizontal text offset.
// Line-number gutters stay visible when horizontally scrolling long source lines.
/**
 * @doc api-diff-viewer
 * @locale en
 * @title diffViewer
 * @section API
 * @symbol cyui::diffViewer
 * Draw a prepared code comparison without invoking Git or modifying its sources.
 *
 * ## Contract {#contract}
 * `DiffResult diffViewer(const char* id, DiffState&, const DiffDocument&, ImVec2 size = {})`
 * borrows the document during drawing. Keep one state per view, and use a stable
 * widget ID. Size is in screen pixels; wrap design dimensions in `dp`.
 * The result reports row, oldLine and newLine; zero line numbers mean no source
 * line. The host resolves navigation. Both split columns share one scroll canvas.
 * [Preparation and scrolling](doc:diff). [Example](doc:module-recipes#code-editing-and-git-style-diff-presentation).
 */
/**
 * @doc api-diff-viewer
 * @locale fr
 * @title diffViewer
 * @section API
 * @symbol cyui::diffViewer
 * Dessine une comparaison préparée sans exécuter Git ni modifier ses sources.
 *
 * ## Contrat {#contract}
 * `DiffResult diffViewer(const char* id, DiffState&, const DiffDocument&, ImVec2 size = {})`
 * emprunte le document pendant le dessin. Gardez un état par vue et un ID stable.
 * La taille est en pixels écran : appliquez `dp` aux dimensions de conception.
 * Le résultat indique row, oldLine et newLine ; une ligne nulle signifie aucune
 * ligne source. L’hôte ouvre la cible. Les colonnes partagent un seul canevas.
 * [Préparation et défilement](doc:diff). [Exemple](doc:module-recipes#code-editing-and-git-style-diff-presentation).
 */
CYUI_API DiffResult diffViewer(const char *id, DiffState &, const DiffDocument &, ImVec2 size = {});
} // namespace cyui

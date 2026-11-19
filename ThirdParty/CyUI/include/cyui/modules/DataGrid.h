#pragma once
/**
 * @doc api-data-grid
 * @locale en
 * @title Editable cell grids
 * @section API
 * @symbol cyui::dataGrid
 * Link `CyUI::DataGrid` for the bounded `GridModel`, rectangular selection, native cell input, TSV/CSV and undo. Core is its only dependency. Cell edits return `GridResult`; programmatic model changes use transactions.
 *
 * ## Usage {#usage}
 * [Module guide](doc:office#grid).
 */
/**
 * @doc api-data-grid
 * @locale fr
 * @title Grilles de cellules éditables
 * @section API
 * @symbol cyui::dataGrid
 * Liez `CyUI::DataGrid` pour le modèle borné `GridModel`, les sélections rectangulaires, la saisie native, TSV/CSV et l’annulation. Core est sa seule dépendance. Le widget retourne `GridResult` ; les mutations du modèle sont transactionnelles.
 *
 * ## Utilisation {#usage}
 * [Guide du module](doc:office#grid).
 */
#include "cyui/Core.h"
#include <memory>
#include <functional>
#include <vector>

namespace cyui {
struct GridAddress {
    std::size_t row = 0, column = 0;
    bool operator==(const GridAddress &o) const {
        return row == o.row && column == o.column;
    }
};
struct CYUI_API GridRange {
    GridAddress first{}, last{}; // inclusive; normalized by normalized()
    GridRange normalized() const;
    bool contains(GridAddress) const;
};
enum class GridAlignment { automatic, left, center, right };
struct GridCell {
    std::string text;
    unsigned style = 0; // bold=1, italic=2, underline=4, strike=8
    GridAlignment alignment = GridAlignment::automatic;
    int decimals = -1;
    bool literal = false; // imported text remains text even when it looks numeric or starts '='
};
// Dense, bounded reusable cell model. Each mutation is one undo transaction.
// Limits: 100000 rows, 256 columns, 1000000 cells, 32 MiB text, 1 MiB/cell.
class CYUI_API GridModel {
  public:
    GridModel(std::size_t rows = 8, std::size_t columns = 5);
    GridModel(const GridModel &);
    GridModel &operator=(const GridModel &);
    GridModel(GridModel &&) noexcept;
    GridModel &operator=(GridModel &&) noexcept;
    ~GridModel();
    std::size_t rows() const;
    std::size_t columns() const;
    std::uint64_t revision() const;
    const GridCell &cell(std::size_t row, std::size_t column) const;
    bool setCell(std::size_t row, std::size_t column, const GridCell &);
    bool setText(std::size_t row, std::size_t column, const std::string &);
    bool resize(std::size_t rows, std::size_t columns);
    bool insertRows(std::size_t at, std::size_t count = 1);
    bool insertColumns(std::size_t at, std::size_t count = 1);
    bool removeRows(std::size_t at, std::size_t count = 1);
    bool removeColumns(std::size_t at, std::size_t count = 1);
    bool clear(GridRange);
    bool format(GridRange, unsigned style, GridAlignment = GridAlignment::automatic, int decimals = -1);
    std::string copy(GridRange, char separator = '\t') const;
    bool paste(GridAddress, const std::string &, char separator = '\t');
    // Replace a whole snapshot atomically. Used by importers and structural formula edits.
    bool assign(std::size_t rows, std::size_t columns, const std::vector<GridCell> &);
    std::vector<GridCell> cells() const;
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearHistory();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
CYUI_API std::string gridColumnName(std::size_t column);
CYUI_API std::string gridAddressName(GridAddress);
CYUI_API bool gridParseAddress(const std::string &, GridAddress &);
CYUI_API std::vector<std::vector<std::string>> gridParseDelimited(const std::string &, char separator = '\t');
struct GridState {
    GridAddress anchor{}, current{}, editingCell{};
    bool hasSelection = false, editing = false, focusEditor = false;
    bool reveal = false;
    std::string draft;
    std::uint64_t editRevision = 0;
    std::vector<float> widths; // logical units, optional initial column widths
    std::string error;
    GridRange selection() const {
        return GridRange{anchor, current}.normalized();
    }
};
struct GridOptions {
    bool readOnly = false, toolbar = true, rowHeaders = true, columnHeaders = true, freezeHeaders = true;
    float rowHeight = 28, columnWidth = 120;
    // Pure display formatting; input, clipboard and formulas still use the original cell text.
    std::function<std::string(std::size_t, std::size_t, const GridCell &)> display;
    bool history = true;
    bool formatting = true;
    bool clipboard = true;
    std::function<void()> beforeChange; // lets a workbook own a larger atomic transaction
};
struct GridResult {
    bool changed = false, selectionChanged = false, committed = false;
    int structure = 0; // 1 rows inserted, 2 columns inserted, -1/-2 removed
    std::size_t at = 0, count = 0;
};
CYUI_API GridResult dataGrid(const char *id, GridState &, GridModel &, ImVec2 size = {},
                             const GridOptions & = {});
} // namespace cyui

#pragma once
/**
 * @doc api-spreadsheet
 * @locale en
 * @title Worksheets and formulas
 * @section API
 * @symbol cyui::spreadsheet
 * Link `CyUI::Spreadsheet`. Keep `Workbook` and `SpreadsheetState` between frames. The grid shows calculated values; the formula bar retains source. The evaluator supports a bounded arithmetic/A1/function subset. Unknown functions remain visible errors and source is preserved.
 *
 * ## Usage {#usage}
 * [Module guide](doc:office#spreadsheet).
 */
/**
 * @doc api-spreadsheet
 * @locale fr
 * @title Feuilles et formules
 * @section API
 * @symbol cyui::spreadsheet
 * Liez `CyUI::Spreadsheet`. Conservez `Workbook` et `SpreadsheetState` entre les frames. La grille affiche les résultats et la barre conserve les formules sources. Le calcul couvre un sous-ensemble borné d’arithmétique, de références A1 et de fonctions. Les fonctions inconnues restent signalées avec leur source.
 *
 * ## Utilisation {#usage}
 * [Guide du module](doc:office#spreadsheet).
 */
#include "cyui/modules/DataGrid.h"
namespace cyui {
struct SpreadsheetSheet {
    std::string name = "Sheet1";
    GridModel grid;
};
struct Workbook {
    std::vector<SpreadsheetSheet> sheets{{}};
};
struct CYUI_API SpreadsheetValue {
    double number = 0;
    std::string text, error;
    bool numeric = false, empty = true;
    std::string display(int decimals = -1) const;
};
// Bounded arithmetic/reference evaluator. Never executes scripts, macros or external links.
// Unknown functions are preserved as text in the cell and evaluate to #NAME?.
CYUI_API std::vector<std::vector<SpreadsheetValue>> evaluateWorkbook(const Workbook &);
// Structural edits shift same/cross-sheet A1 references, including absolute ones.
// Removed referenced cells become #REF!. Returns false for an invalid operation.
CYUI_API bool spreadsheetStructure(Workbook &, std::size_t sheet, int operation, std::size_t at,
                                   std::size_t count = 1);
// Rename a sheet and update its formula references as one transaction.
CYUI_API bool spreadsheetRenameSheet(Workbook &, std::size_t sheet, const std::string &name);
CYUI_API std::string spreadsheetFormulaFromODF(const std::string &);
CYUI_API std::string spreadsheetFormulaToODF(const std::string &);
CYUI_API std::string spreadsheetTranslateFormula(const std::string &, std::int64_t rows,
                                                 std::int64_t columns);
struct SpreadsheetState {
    std::size_t sheet = 0, requestedSheet = ~std::size_t(0);
    GridState grid;
    std::string formula, sheetName;
    GridAddress formulaCell{};
    std::size_t formulaSheet = ~std::size_t(0);
    bool formulaActive = false;
    std::vector<Workbook> undo, redo;
    std::vector<std::uint64_t> revisions;
    std::vector<std::vector<SpreadsheetValue>> values;
    std::string copiedText;
    GridAddress copyOrigin{};
    std::size_t copyRows = 0, copyColumns = 0;
    std::vector<GridCell> copiedCells;
};
struct SpreadsheetOptions {
    bool readOnly = false, toolbar = true;
};
CYUI_API bool spreadsheetUndo(Workbook &, SpreadsheetState &);
CYUI_API bool spreadsheetRedo(Workbook &, SpreadsheetState &);
CYUI_API bool spreadsheet(const char *id, SpreadsheetState &, Workbook &, ImVec2 size = {},
                          const SpreadsheetOptions & = {});
} // namespace cyui

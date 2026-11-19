#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <functional>
#include <string>
#include <variant>
#include "cyui/modules/Inspector.h"
namespace cyui {
using DataCell = PropertyValue;
struct DataColumn {
    ItemId id = 0;
    std::string label;
    bool editable = false, bounded = false;
    double minimum = 0, maximum = 1;
    float width = 140;
    PropertyMetadata metadata;
};
struct DataRow {
    ItemId id = 0;
    std::vector<DataCell> cells;
    bool readOnly = false;
};
struct DataSort {
    std::size_t column = 0;
    bool descending = false;
};
struct DataViewState {
    std::vector<std::string> filters;
    std::vector<DataSort> sort;
    std::vector<std::size_t> columnOrder; // visible model-column indices in display order; empty = natural
    std::vector<ItemId> selection;
    ItemId anchor = 0, active = 0, editing = 0;
    std::size_t anchorColumn = 0, activeColumn = 0, editingColumn = 0;
    std::string draft, error;
    bool editFocus = false;
    std::uint64_t revision = 0;
    InspectorState editors;
    std::optional<Property> richProperty;
};
struct DataCellEdit {
    ItemId row = 0, column = 0;
    Edit<DataCell> edit;
};
struct DataViewOptions {
    bool readOnly = false, filters = true;
    std::function<std::string(ItemId, ItemId, const DataCell &)> validate;
    std::uint64_t revision = 0; // external model replacement cancels an active cell edit
    InspectorOptions editors;
};
struct DataPasteResult {
    std::vector<DataCellEdit> edits;
    std::string error;
    explicit operator bool() const {
        return error.empty() && !edits.empty();
    }
};
CYUI_API std::string dataCellText(const DataCell &);
CYUI_API std::vector<std::size_t> dataViewRows(ArrayView<DataRow>, const DataViewState &);
CYUI_API std::string copyDataCells(ArrayView<DataRow>, const DataViewState &);
CYUI_API DataPasteResult pasteDataCells(ArrayView<DataColumn>, ArrayView<DataRow>, const DataViewState &,
                                        const std::string &, const DataViewOptions & = {});
CYUI_API std::vector<DataCellEdit> dataView(const char *id, DataViewState &, ArrayView<DataColumn>,
                                            ArrayView<DataRow>, ImVec2 size = {},
                                            const DataViewOptions & = {});
} // namespace cyui

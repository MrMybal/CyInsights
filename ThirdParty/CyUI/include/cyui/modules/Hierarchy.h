#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <functional>
#include <string>
#include <unordered_set>
namespace cyui {
struct HierarchyItem {
    ItemId id = 0, parent = 0;
    std::string label, kind;
    bool visible = true, locked = false;
};
enum class HierarchyAction { rename, remove, duplicate, visibility, locking, reparent };
struct HierarchyRequest {
    HierarchyAction action = HierarchyAction::rename;
    std::vector<ItemId> ids;
    ItemId parent = 0, before = 0; // before 0 appends; root parent is 0
    std::string text;
    bool value = false;
};
struct HierarchyRow {
    std::size_t index = 0;
    int depth = 0;
    bool children = false;
};
struct HierarchyState {
    std::string query, renameText;
    std::vector<ItemId> selection;
    std::unordered_set<ItemId> collapsed;
    ItemId anchor = 0, renaming = 0;
    bool renameFocus = false;
    ItemId focused = 0;
    bool scrollToFocus = false;
};
struct HierarchyOptions {
    bool readOnly = false, visibility = true, locking = true;
    std::function<bool(ArrayView<ItemId>, ItemId)> canReparent;
};
// Nonzero unique IDs. Missing parents become roots; invalid cycles are shown once.
CYUI_API std::vector<HierarchyRow> hierarchyRows(ArrayView<HierarchyItem>, const HierarchyState &);
CYUI_API bool canReparentHierarchy(ArrayView<HierarchyItem>, ArrayView<ItemId>, ItemId parent);
enum class HierarchyNavigation { up, down, left, right, home, end, pageUp, pageDown, toggle, selectAll };
CYUI_API void navigateHierarchy(ArrayView<HierarchyItem>, HierarchyState &, HierarchyNavigation,
                                bool extend = false, bool additive = false, std::size_t page = 10);
CYUI_API std::vector<HierarchyRequest> hierarchy(const char *id, HierarchyState &, ArrayView<HierarchyItem>,
                                                 ImVec2 size = {}, const HierarchyOptions & = {});
} // namespace cyui

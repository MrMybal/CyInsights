#include "cyui/modules/Hierarchy.h"
#include "ModuleInternal.h"
#include <unordered_map>
namespace cyui {
std::vector<HierarchyRow> hierarchyRows(ArrayView<HierarchyItem> items, const HierarchyState &state) {
    std::unordered_map<ItemId, std::size_t> index;
    std::unordered_map<ItemId, std::vector<std::size_t>> children;
    for (std::size_t i = 0; i < items.count; ++i)
        if (items[i].id)
            index.emplace(items[i].id, i);
    for (std::size_t i = 0; i < items.count; ++i)
        if (items[i].id && index[items[i].id] == i)
            children[index.count(items[i].parent) ? items[i].parent : 0].push_back(i);
    std::unordered_set<ItemId> matching;
    const auto query = detail::folded(state.query);
    if (!query.empty())
        for (std::size_t i = 0; i < items.count; ++i)
            if (detail::matchesPair(items[i].label, items[i].kind, query)) {
                auto key = items[i].id;
                std::unordered_set<ItemId> seen;
                while (key && index.count(key) && seen.insert(key).second) {
                    matching.insert(key);
                    key = items[index[key]].parent;
                }
            }
    std::vector<HierarchyRow> result;
    std::unordered_set<ItemId> seen;
    auto walk = [&](std::size_t root) {
        std::vector<std::pair<std::size_t, int>> stack{{root, 0}};
        while (!stack.empty()) {
            auto [i, depth] = stack.back();
            stack.pop_back();
            const auto &item = items[i];
            if (!seen.insert(item.id).second)
                continue;
            if (!query.empty() && !matching.count(item.id))
                continue;
            const auto found = children.find(item.id);
            const bool has = found != children.end() && !found->second.empty();
            result.push_back({i, depth, has});
            if (has && (!query.empty() || !state.collapsed.count(item.id)))
                for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
                    stack.push_back({*it, depth + 1});
        }
    };
    for (auto i : children[0])
        walk(i);
    // Reach cycles without resurrecting descendants of deliberately collapsed roots.
    for (std::size_t i = 0; i < items.count; ++i)
        if (items[i].id && !seen.count(items[i].id)) {
            std::unordered_set<ItemId> path;
            auto id = items[i].id;
            bool cycle = false;
            while (id && index.count(id)) {
                if (!path.insert(id).second) {
                    cycle = true;
                    break;
                }
                id = items[index[id]].parent;
            }
            if (cycle)
                walk(i);
        }
    return result;
}
bool canReparentHierarchy(ArrayView<HierarchyItem> items, ArrayView<ItemId> moved, ItemId parent) {
    if (!moved.count)
        return false;
    std::unordered_map<ItemId, const HierarchyItem *> index;
    for (std::size_t i = 0; i < items.count; ++i)
        if (items[i].id && !index.emplace(items[i].id, &items[i]).second)
            return false;
    if (parent && !index.count(parent))
        return false;
    if (parent && index[parent]->locked)
        return false;
    std::unordered_set<ItemId> selected;
    for (std::size_t i = 0; i < moved.count; ++i) {
        auto p = index.find(moved[i]);
        if (p == index.end() || p->second->locked)
            return false;
        selected.insert(moved[i]);
    }
    std::unordered_set<ItemId> seen;
    while (parent) {
        if (selected.count(parent) || !seen.insert(parent).second)
            return false;
        auto p = index.find(parent);
        if (p == index.end())
            break;
        parent = p->second->parent;
    }
    return true;
}
void navigateHierarchy(ArrayView<HierarchyItem> items, HierarchyState &s, HierarchyNavigation key,
                       bool extend, bool additive, std::size_t page) {
    const auto rows = hierarchyRows(items, s);
    if (rows.empty()) {
        s.focused = 0;
        return;
    }
    if (key == HierarchyNavigation::selectAll) {
        s.selection.clear();
        for (const auto &row : rows)
            s.selection.push_back(items[row.index].id);
        s.focused = s.selection.front();
        s.anchor = s.focused;
        s.scrollToFocus = true;
        return;
    }
    const auto previous = s.focused ? s.focused : (s.selection.empty() ? 0 : s.selection.back());
    auto found = std::find_if(rows.begin(), rows.end(),
                              [&](const auto &row) { return items[row.index].id == previous; });
    std::size_t at = found == rows.end() ? 0 : std::size_t(found - rows.begin());
    const auto old = at;
    const auto id = items[rows[at].index].id;
    if (key == HierarchyNavigation::toggle) {
        selectItem(s.selection, id, true);
        s.focused = id;
        s.anchor = id;
        return;
    }
    if (key == HierarchyNavigation::up)
        at = at ? at - 1 : 0;
    else if (key == HierarchyNavigation::down)
        at = found == rows.end() ? 0 : std::min(at + 1, rows.size() - 1);
    else if (key == HierarchyNavigation::home)
        at = 0;
    else if (key == HierarchyNavigation::end)
        at = rows.size() - 1;
    else if (key == HierarchyNavigation::pageUp)
        at = at > page ? at - page : 0;
    else if (key == HierarchyNavigation::pageDown)
        at = std::min(at + page, rows.size() - 1);
    else if (key == HierarchyNavigation::right) {
        if (rows[at].children && s.collapsed.erase(id)) {
            s.focused = id;
            s.scrollToFocus = true;
            return;
        }
        if (at + 1 < rows.size() && rows[at + 1].depth > rows[at].depth)
            ++at;
    } else if (key == HierarchyNavigation::left) {
        if (rows[at].children && !s.collapsed.count(id) && s.query.empty()) {
            s.collapsed.insert(id);
            s.focused = id;
            return;
        }
        while (at && rows[at - 1].depth >= rows[old].depth)
            --at;
        if (at)
            --at;
        else
            at = old;
    }
    s.focused = items[rows[at].index].id;
    s.scrollToFocus = true;
    if (extend) {
        if (!s.anchor)
            s.anchor = items[rows[old].index].id;
        auto anchor = std::find_if(rows.begin(), rows.end(),
                                   [&](const auto &row) { return items[row.index].id == s.anchor; });
        const auto from = anchor == rows.end() ? old : std::size_t(anchor - rows.begin());
        if (!additive)
            s.selection.clear();
        for (auto n = std::min(from, at); n <= std::max(from, at); ++n)
            if (!selected(s.selection, items[rows[n].index].id))
                s.selection.push_back(items[rows[n].index].id);
    } else if (!additive) {
        s.selection = {s.focused};
        s.anchor = s.focused;
    }
}
std::vector<HierarchyRequest> hierarchy(const char *id, HierarchyState &s, ArrayView<HierarchyItem> items,
                                        ImVec2 size, const HierarchyOptions &o) {
    ScopedID scope(id);
    std::vector<HierarchyRequest> result;
    ImGui::SetNextItemWidth(-1);
    detail::inputText("##search", s.query);
    if (size.y > 0)
        size.y = std::max(dp(1), size.y - ImGui::GetFrameHeightWithSpacing());
    ImGui::BeginChild("##tree", size, ImGuiChildFlags_Borders);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput &&
        !s.renaming) {
        const std::pair<ImGuiKey, HierarchyNavigation> keys[]{
            {ImGuiKey_UpArrow, HierarchyNavigation::up},
            {ImGuiKey_DownArrow, HierarchyNavigation::down},
            {ImGuiKey_LeftArrow, HierarchyNavigation::left},
            {ImGuiKey_RightArrow, HierarchyNavigation::right},
            {ImGuiKey_Home, HierarchyNavigation::home},
            {ImGuiKey_End, HierarchyNavigation::end},
            {ImGuiKey_PageUp, HierarchyNavigation::pageUp},
            {ImGuiKey_PageDown, HierarchyNavigation::pageDown},
            {ImGuiKey_Space, HierarchyNavigation::toggle}};
        for (const auto &key : keys)
            if (ImGui::IsKeyPressed(key.first))
                navigateHierarchy(items, s, key.second, ImGui::GetIO().KeyShift, ImGui::GetIO().KeyCtrl,
                                  std::size_t(std::max(1.f, ImGui::GetContentRegionAvail().y /
                                                                ImGui::GetFrameHeightWithSpacing())));
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
            navigateHierarchy(items, s, HierarchyNavigation::selectAll);
    }
    const auto rows = hierarchyRows(items, s);
    const float height = ImGui::GetFrameHeightWithSpacing();
    ImGuiListClipper clip;
    clip.Begin(int(rows.size()), height);
    for (std::size_t n = 0; n < rows.size(); ++n)
        if ((s.scrollToFocus && items[rows[n].index].id == s.focused) ||
            items[rows[n].index].id == s.renaming)
            clip.IncludeItemByIndex(int(n));
    auto request = [&](HierarchyAction action, const std::vector<ItemId> &ids) {
        HierarchyRequest r;
        r.action = action;
        r.ids = ids;
        result.push_back(std::move(r));
    };
    auto drop = [&](ItemId parent, ItemId before) {
        if (auto payload = ImGui::AcceptDragDropPayload("CYUI_HIERARCHY")) {
            if (payload->DataSize <= 0 || payload->DataSize % sizeof(ItemId))
                return;
            auto ids = ArrayView<ItemId>(static_cast<const ItemId *>(payload->Data),
                                         std::size_t(payload->DataSize) / sizeof(ItemId));
            if (canReparentHierarchy(items, ids, parent) && (!o.canReparent || o.canReparent(ids, parent))) {
                HierarchyRequest r;
                r.action = HierarchyAction::reparent;
                r.ids.assign(ids.data, ids.data + ids.count);
                r.parent = parent;
                r.before = before;
                result.push_back(std::move(r));
            }
        }
    };
    while (clip.Step())
        for (int n = clip.DisplayStart; n < clip.DisplayEnd; ++n) {
            const auto &row = rows[std::size_t(n)];
            const auto &item = items[row.index];
            ItemScope key(item.id);
            if (row.depth)
                ImGui::Indent(dp(16) * float(row.depth));
            if (row.children) {
                if (ghostButton(s.collapsed.count(item.id) ? ">" : "v")) {
                    if (!s.collapsed.erase(item.id))
                        s.collapsed.insert(item.id);
                }
            } else
                ImGui::Dummy({ImGui::GetFrameHeight(), ImGui::GetFrameHeight()});
            ImGui::SameLine();
            const float reserved = dp((o.visibility ? 30.f : 0.f) + (o.locking ? 30.f : 0.f));
            if (s.renaming == item.id) {
                if (s.renameFocus) {
                    ImGui::SetKeyboardFocusHere();
                    s.renameFocus = false;
                }
                ImGui::SetNextItemWidth(std::max(dp(40), ImGui::GetContentRegionAvail().x - reserved));
                if (detail::inputText("##rename", s.renameText, ImGuiInputTextFlags_EnterReturnsTrue) &&
                    !o.readOnly && !item.locked && !s.renameText.empty()) {
                    request(HierarchyAction::rename, {item.id});
                    result.back().text = s.renameText;
                    s.renaming = 0;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape) || o.readOnly)
                    s.renaming = 0;
            } else {
                if (ImGui::Selectable(item.label.c_str(), selected(s.selection, item.id), 0,
                                      {std::max(dp(40), ImGui::GetContentRegionAvail().x - reserved),
                                       ImGui::GetFrameHeight()})) {
                    const auto &io = ImGui::GetIO();
                    if (io.KeyShift && s.anchor) {
                        auto anchor = std::find_if(rows.begin(), rows.end(), [&](const auto &r) {
                            return items[r.index].id == s.anchor;
                        });
                        if (anchor != rows.end()) {
                            if (!io.KeyCtrl)
                                s.selection.clear();
                            const auto a = std::size_t(anchor - rows.begin()), b = std::size_t(n);
                            for (auto at = std::min(a, b); at <= std::max(a, b); ++at)
                                if (!selected(s.selection, items[rows[at].index].id))
                                    s.selection.push_back(items[rows[at].index].id);
                        }
                    } else {
                        selectItem(s.selection, item.id, io.KeyCtrl);
                        s.anchor = item.id;
                    }
                    s.focused = item.id;
                }
                if (s.scrollToFocus && s.focused == item.id) {
                    ImGui::SetScrollHereY();
                    s.scrollToFocus = false;
                }
                if (!o.readOnly && !item.locked && ImGui::BeginDragDropSource()) {
                    auto ids = selected(s.selection, item.id) ? s.selection : std::vector<ItemId>{item.id};
                    ImGui::SetDragDropPayload("CYUI_HIERARCHY", ids.data(), ids.size() * sizeof(ItemId));
                    ImGui::TextUnformatted(item.label.c_str());
                    ImGui::EndDragDropSource();
                }
                if (!o.readOnly && ImGui::BeginDragDropTarget()) {
                    drop(ImGui::GetIO().KeyShift ? item.parent : item.id,
                         ImGui::GetIO().KeyShift ? item.id : 0);
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem()) {
                    const auto ids =
                        selected(s.selection, item.id) ? s.selection : std::vector<ItemId>{item.id};
                    if (ImGui::MenuItem(trLabel("Rename").c_str(), "F2", false,
                                        !o.readOnly && !item.locked)) {
                        s.renaming = item.id;
                        s.renameText = item.label;
                        s.renameFocus = true;
                    }
                    if (ImGui::MenuItem(trLabel("Duplicate").c_str(), nullptr, false,
                                        !o.readOnly && !item.locked))
                        request(HierarchyAction::duplicate, ids);
                    if (ImGui::MenuItem(trLabel("Remove").c_str(), "Delete", false,
                                        !o.readOnly && !item.locked))
                        request(HierarchyAction::remove, ids);
                    ImGui::EndPopup();
                }
            }
            if (o.visibility) {
                ImGui::SameLine();
                ScopedDisabled disabled(o.readOnly || item.locked);
                if (ghostButton(item.visible ? "V" : "-")) {
                    request(HierarchyAction::visibility, {item.id});
                    result.back().value = !item.visible;
                }
            }
            if (o.locking) {
                ImGui::SameLine();
                ScopedDisabled disabled(o.readOnly);
                if (ghostButton(item.locked ? "L" : "-")) {
                    request(HierarchyAction::locking, {item.id});
                    result.back().value = !item.locked;
                }
            }
            if (row.depth)
                ImGui::Unindent(dp(16) * float(row.depth));
        }
    if (!o.readOnly) {
        ImGui::Selectable(trLabel("Drop here to move to root").c_str());
        if (ImGui::BeginDragDropTarget()) {
            drop(0, 0);
            ImGui::EndDragDropTarget();
        }
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput &&
        !o.readOnly) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            std::vector<ItemId> ids;
            for (auto key : s.selection)
                for (std::size_t i = 0; i < items.count; ++i)
                    if (items[i].id == key && !items[i].locked)
                        ids.push_back(key);
            if (!ids.empty())
                request(HierarchyAction::remove, ids);
        }
        if ((ImGui::IsKeyPressed(ImGuiKey_F2) || ImGui::IsKeyPressed(ImGuiKey_Enter)) &&
            s.selection.size() == 1)
            for (std::size_t i = 0; i < items.count; ++i)
                if (items[i].id == s.selection[0] && !items[i].locked) {
                    s.renaming = items[i].id;
                    s.renameText = items[i].label;
                    s.renameFocus = true;
                }
    }
    ImGui::EndChild();
    return result;
}
} // namespace cyui

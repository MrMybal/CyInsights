#include "cyui/modules/Inspector.h"
#include "ModuleInternal.h"
#include <unordered_set>
#include <cfloat>
#include <sstream>
#include <iomanip>
#include <locale>
#include <climits>

namespace cyui {
namespace {
struct DrawEdit {
    bool changed = false, immediate = false, activated = false, active = false;
    bool deactivated = false, edited = false;
    bool cancelled = false;
    void observe() {
        activated |= ImGui::IsItemActivated();
        active |= ImGui::IsItemActive();
        deactivated |= ImGui::IsItemDeactivated();
        edited |= ImGui::IsItemDeactivatedAfterEdit();
    }
    void merge(const DrawEdit &b) {
        changed |= b.changed;
        immediate |= b.immediate;
        activated |= b.activated;
        active |= b.active;
        deactivated |= b.deactivated;
        edited |= b.edited;
        cancelled |= b.cancelled;
    }
};
Property childField(const Property &root, const PropertyValue &value) {
    Property field;
    field.id = root.id;
    field.value = value;
    field.readOnly = root.readOnly;
    field.bounded = root.bounded;
    field.minimum = root.minimum;
    field.maximum = root.maximum;
    field.integerMinimum = root.integerMinimum;
    field.integerMaximum = root.integerMaximum;
    field.choices = root.choices;
    field.password = root.password;
    return field;
}
std::string numberText(const PropertyValue &value) {
    return std::visit(
        [](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                std::ostringstream text;
                text.imbue(std::locale::classic());
                text << std::setprecision(std::numeric_limits<T>::max_digits10) << n;
                return text.str();
            } else
                return {};
        },
        value);
}
DrawEdit drawValue(PropertyValue &, const Property &, const InspectorOptions &, InspectorState &,
                   std::vector<std::string> &, bool = false);
bool isCollection(const PropertyValue &value) {
    return std::holds_alternative<PropertyArray>(value) || std::holds_alternative<PropertySet>(value) ||
           std::holds_alternative<PropertyMap>(value);
}
DrawEdit drawCollection(PropertyValue &collection, const Property &root, const InspectorOptions &options,
                        InspectorState &state, std::vector<std::string> &path) {
    DrawEdit out;
    const auto listId = ImGui::GetID("##array");
    auto &list = state.lists[listId];
    list.renderedRows = 0;
    auto *array = std::get_if<PropertyArray>(&collection);
    auto *set = std::get_if<PropertySet>(&collection);
    auto *map = std::get_if<PropertyMap>(&collection);
    auto *values = array ? &array->values : set ? &set->values : nullptr;
    const auto count = values ? values->size() : map->entries.size();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##collectionSearch", tr("Filter collection...").c_str(), list.query.data(),
                             list.query.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                             detail::resizeText, &list.query);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr("Filter collection...").c_str());
    if (list.indexedSize != count || list.indexedQuery != list.query) {
        if (list.indexedQuery != list.query)
            list.selection.clear();
        list.visible.clear();
        const auto query = detail::folded(list.query);
        for (std::size_t n = 0; n < count; ++n) {
            bool matches = query.empty();
            if (!matches) {
                const auto &value = values ? (*values)[n] : map->entries[n].value;
                try {
                    matches = detail::containsFolded(propertyValueKey(value), query) ||
                              (map && detail::containsFolded(propertyValueKey(map->entries[n].key), query));
                } catch (const std::exception &) {
                    matches = detail::containsFolded(propertyValueText(value), query);
                }
            }
            if (matches)
                list.visible.push_back(n);
        }
        list.indexedSize = count;
        list.indexedQuery = list.query;
        list.selection.erase(
            std::remove_if(list.selection.begin(), list.selection.end(), [&](auto n) { return n >= count; }),
            list.selection.end());
    }
    int command = 0;
    std::size_t before = std::size_t(-1);
    std::string commandText;
    auto pick = [&](std::size_t row, bool extend, bool additive) {
        if (extend) {
            auto a = std::find(list.visible.begin(), list.visible.end(), list.anchor),
                 b = std::find(list.visible.begin(), list.visible.end(), row);
            if (a == list.visible.end())
                a = b;
            if (!additive)
                list.selection.clear();
            if (a > b)
                std::swap(a, b);
            for (; a != list.visible.end() && a <= b; ++a)
                if (std::find(list.selection.begin(), list.selection.end(), *a) == list.selection.end())
                    list.selection.push_back(*a);
        } else {
            auto found = std::find(list.selection.begin(), list.selection.end(), row);
            if (!additive)
                list.selection = {row};
            else if (found == list.selection.end())
                list.selection.push_back(row);
            else
                list.selection.erase(found);
            list.anchor = row;
        }
    };
    auto drop = [&](std::size_t row) {
        if (auto payload = ImGui::AcceptDragDropPayload("CYUI_PROPERTY_ROWS")) {
            if (payload->DataSize == sizeof(ImGuiID) &&
                *static_cast<const ImGuiID *>(payload->Data) == listId) {
                command = 3;
                before = row;
            }
        }
    };
    if (ImGui::BeginChild("##array", {0, dp(std::max(60.f, options.arrayHeight))}, ImGuiChildFlags_Borders)) {
        const auto pitch = ImGui::GetFrameHeightWithSpacing();
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput) {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
                list.selection = list.visible;
            if (!root.readOnly && ImGui::IsKeyPressed(ImGuiKey_Delete) && !list.selection.empty())
                command = 1;
        }
        ImGuiListClipper clip;
        clip.Begin(int(list.visible.size()), pitch);
        if (list.activeRow >= 0) {
            const auto active =
                std::find(list.visible.begin(), list.visible.end(), std::size_t(list.activeRow));
            if (active != list.visible.end())
                clip.IncludeItemByIndex(int(active - list.visible.begin()));
        }
        while (clip.Step())
            for (int at = clip.DisplayStart; at < clip.DisplayEnd; ++at) {
                const auto row = list.visible[std::size_t(at)];
                ++list.renderedRows;
                ScopedID scope{int(row)};
                const float y = ImGui::GetCursorPosY();
                const bool selected =
                    std::find(list.selection.begin(), list.selection.end(), row) != list.selection.end();
                if (ImGui::Selectable(std::to_string(row).c_str(), selected, 0,
                                      {dp(44), ImGui::GetFrameHeight()}))
                    pick(row, ImGui::GetIO().KeyShift, ImGui::GetIO().KeyCtrl);
                bool context = ImGui::IsItemClicked(ImGuiMouseButton_Right);
                if (array && !root.readOnly && ImGui::BeginDragDropSource()) {
                    if (!selected)
                        list.selection = {row};
                    ImGui::SetDragDropPayload("CYUI_PROPERTY_ROWS", &listId, sizeof(listId));
                    textDim("%zu", list.selection.size());
                    ImGui::EndDragDropSource();
                }
                if (array && !root.readOnly && ImGui::BeginDragDropTarget()) {
                    drop(row);
                    ImGui::EndDragDropTarget();
                }
                ImGui::SameLine(dp(52));
                const float width = std::max(dp(40), ImGui::GetContentRegionAvail().x - dp(30));
                path.push_back(std::to_string(row));
                DrawEdit edit;
                if (map) {
                    ImGui::SetNextItemWidth((width - ImGui::GetStyle().ItemInnerSpacing.x) * .5f);
                    {
                        ScopedID key("key");
                        path.push_back("key");
                        edit.merge(drawValue(map->entries[row].key, root, options, state, path));
                        path.pop_back();
                    }
                    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                    ImGui::SetNextItemWidth((width - ImGui::GetStyle().ItemInnerSpacing.x) * .5f);
                    {
                        ScopedID key("value");
                        path.push_back("value");
                        edit.merge(drawValue(map->entries[row].value, root, options, state, path));
                        path.pop_back();
                    }
                } else {
                    ImGui::SetNextItemWidth(width);
                    edit = drawValue((*values)[row], root, options, state, path);
                }
                path.pop_back();
                out.merge(edit);
                if (edit.changed)
                    list.indexedSize = std::size_t(-1);
                if (edit.active || edit.activated)
                    list.activeRow = int(row);
                else if (list.activeRow == int(row))
                    list.activeRow = -1;
                auto &value = values ? (*values)[row] : map->entries[row].value;
                context |= ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
                           ImGui::IsMouseReleased(ImGuiMouseButton_Right);
                if (options.clipboard && context)
                    ImGui::OpenPopup("##elementMenu");
                if (options.clipboard && ImGui::BeginPopup("##elementMenu")) {
                    if (!selected)
                        list.selection = {row};
                    if (ImGui::MenuItem(trLabel("Copy as JSON").c_str(), nullptr, false, !root.password)) {
                        try {
                            ImGui::SetClipboardText(propertyToJson(childField(root, value)).c_str());
                        } catch (const std::exception &e) {
                            state.clipboardError = e.what();
                        }
                    }
                    if (ImGui::MenuItem(trLabel("Paste to selection").c_str(), nullptr, false,
                                        !root.readOnly)) {
                        command = 4;
                        const auto text = ImGui::GetClipboardText();
                        commandText = text ? text : "";
                    }
                    if (ImGui::MenuItem(trLabel("Duplicate selection").c_str(), nullptr, false,
                                        !root.readOnly && array))
                        command = 2;
                    if (ImGui::MenuItem(trLabel("Move up").c_str(), nullptr, false,
                                        !root.readOnly && array && row > 0)) {
                        command = 3;
                        before = row - 1;
                    }
                    if (ImGui::MenuItem(trLabel("Move down").c_str(), nullptr, false,
                                        !root.readOnly && array && row + 1 < count)) {
                        command = 3;
                        before = row + 2;
                    }
                    if (ImGui::MenuItem(trLabel("Remove selection").c_str(), nullptr, false, !root.readOnly))
                        command = 1;
                    ImGui::EndPopup();
                }
                ImGui::SameLine();
                {
                    ScopedDisabled disabled(root.readOnly);
                    if (ghostButton("-")) {
                        list.selection = {row};
                        command = 1;
                    }
                }
                ImGui::SetCursorPosY(y + pitch - ImGui::GetStyle().ItemSpacing.y);
                ImGui::Dummy({0, 0});
            }
        if (array && ImGui::GetDragDropPayload()) {
            ImGui::Selectable(trLabel("Drop at end").c_str());
            if (ImGui::BeginDragDropTarget()) {
                drop(count);
                ImGui::EndDragDropTarget();
            }
        }
    }
    ImGui::EndChild();
    bool add = false;
    {
        ScopedDisabled disabled(root.readOnly);
        add = ghostButton(trLabel("Add element").c_str());
    }
    if (add && count < std::size_t(INT_MAX)) {
        if (array) {
            PropertyValue added =
                array->elementTemplate
                    ? *array->elementTemplate
                    : (array->values.empty() ? PropertyValue{std::string{}} : array->values.back());
            array->values.push_back(std::move(added));
            out.changed = out.immediate = true;
            list.indexedSize = std::size_t(-1);
        } else {
            PropertyValue key = map ? (map->keyTemplate       ? *map->keyTemplate
                                       : map->entries.empty() ? PropertyValue{std::string{}}
                                                              : map->entries.back().key)
                                    : PropertyValue{};
            PropertyValue value = map ? (map->valueTemplate     ? *map->valueTemplate
                                         : map->entries.empty() ? PropertyValue{std::string{}}
                                                                : map->entries.back().value)
                                      : (set->elementTemplate  ? *set->elementTemplate
                                         : set->values.empty() ? PropertyValue{std::string{}}
                                                               : set->values.back());
            state.pending[listId] =
                map ? PropertyValue{PropertyObject{"MapEntry",
                                                   {{"Key", std::move(key)}, {"Value", std::move(value)}}}}
                    : std::move(value);
            list.error.clear();
            ImGui::OpenPopup("##addElement");
        }
    }
    ImGui::SameLine();
    {
        ScopedDisabled disabled(root.readOnly || list.selection.empty());
        if (ghostButton(trLabel("Edit selection...").c_str())) {
            const auto row = list.selection.front();
            const auto &value = values ? (*values)[row] : map->entries[row].value;
            list.bulkText = numberText(value);
            if (list.bulkText.empty()) {
                try {
                    list.bulkText = propertyToJson(childField(root, value));
                } catch (const std::exception &e) {
                    list.error = e.what();
                }
            }
            ImGui::OpenPopup("##bulk");
        }
    }
    ImGui::SameLine();
    textDim("%zu / %zu", list.selection.size(), count);
    ImGui::SetNextWindowSize(dp(520, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##addElement")) {
        auto found = state.pending.find(listId);
        if (found == state.pending.end())
            ImGui::CloseCurrentPopup();
        else {
            auto &pending = found->second;
            if (map) {
                for (auto &field : std::get<PropertyObject>(pending).fields) {
                    ScopedID key(field.first.c_str());
                    textDim("%s", tr(field.first.c_str()).c_str());
                    ImGui::SetNextItemWidth(-1);
                    path.push_back("new");
                    path.push_back(field.first == "Key" ? "key" : "value");
                    drawValue(field.second, root, options, state, path);
                    path.pop_back();
                    path.pop_back();
                }
            } else {
                ImGui::SetNextItemWidth(-1);
                path.push_back("new");
                drawValue(pending, root, options, state, path);
                path.pop_back();
            }
            if (button(trLabel("Apply").c_str()) && !root.readOnly) {
                auto candidate = collection;
                if (map) {
                    const auto &entry = std::get<PropertyObject>(pending);
                    std::get<PropertyMap>(candidate).entries.push_back(
                        {entry.fields.at("Key"), entry.fields.at("Value")});
                } else
                    std::get<PropertySet>(candidate).values.push_back(pending);
                if (normalizePropertyValue(childField(root, collection), candidate, list.error)) {
                    collection = std::move(candidate);
                    out.changed = out.immediate = true;
                    list.indexedSize = std::size_t(-1);
                    ImGui::CloseCurrentPopup();
                }
            }
            out.active |= !out.immediate;
        }
        if (!list.error.empty())
            textColoured(palette().error, "%s", tr(list.error.c_str()).c_str());
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##bulk")) {
        if (list.selection.empty())
            ImGui::CloseCurrentPopup();
        textDim("%s", tr("Enter an expression or typed property JSON.").c_str());
        ImGui::SetNextItemWidth(dp(420));
        const bool accepted =
            detail::inputText("##bulkText", list.bulkText, ImGuiInputTextFlags_EnterReturnsTrue);
        if ((accepted || button(trLabel("Apply").c_str())) && !root.readOnly && !list.selection.empty()) {
            const auto row = list.selection.front();
            const auto &value = values ? (*values)[row] : map->entries[row].value;
            const auto first = list.bulkText.find_first_not_of(" \t\r\n");
            const bool json = first != std::string::npos && list.bulkText[first] == '{';
            command = json || numberText(value).empty() ? 4 : 5;
            commandText = list.bulkText;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        out.active |= !out.immediate;
        ImGui::EndPopup();
    }
    if (command) {
        const PropertyCollectionAction actions[]{
            PropertyCollectionAction::remove, PropertyCollectionAction::duplicate,
            PropertyCollectionAction::move, PropertyCollectionAction::assign,
            PropertyCollectionAction::expression};
        auto change = editPropertyCollection(childField(root, collection), list.selection,
                                             actions[command - 1], commandText, before);
        list.error = change.error;
        if (change) {
            collection = std::move(change.edits.front().edit.value);
            out.changed = out.immediate = true;
            list.indexedSize = std::size_t(-1);
            list.activeRow = -1;
            list.selection.clear();
            state.elements.clear();
            ImGui::ClearActiveID();
        }
    }
    if (!list.error.empty())
        textColoured(palette().error, "%s", tr(list.error.c_str()).c_str());
    return out;
}
DrawEdit drawValue(PropertyValue &value, const Property &root, const InspectorOptions &options,
                   InspectorState &state, std::vector<std::string> &path, bool fullArray) {
    DrawEdit out;
    ScopedDisabled disabled(root.readOnly && !isCollection(value) &&
                            !std::holds_alternative<PropertyObject>(value) &&
                            !std::holds_alternative<PropertyOptional>(value));
    const bool numeric = std::holds_alternative<std::int64_t>(value) ||
                         std::holds_alternative<float>(value) || std::holds_alternative<double>(value);
    ImGui::BeginGroup();
    if (auto b = std::get_if<bool>(&value)) {
        out.changed = ImGui::Checkbox("##value", b);
        out.immediate = true;
        out.observe();
    } else if (auto integer = std::get_if<std::int64_t>(&value)) {
        out.changed = ImGui::DragScalar("##value", ImGuiDataType_S64, integer, 1,
                                        root.bounded ? &root.integerMinimum : nullptr,
                                        root.bounded ? &root.integerMaximum : nullptr, "%lld",
                                        ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto number = std::get_if<double>(&value)) {
        out.changed =
            ImGui::DragScalar("##value", ImGuiDataType_Double, number, .01f,
                              root.bounded ? &root.minimum : nullptr, root.bounded ? &root.maximum : nullptr,
                              "%.6g", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto single = std::get_if<float>(&value)) {
        const float low = float(root.minimum), high = float(root.maximum);
        out.changed = ImGui::DragScalar("##value", ImGuiDataType_Float, single, .01f,
                                        root.bounded ? &low : nullptr, root.bounded ? &high : nullptr, "%.6g",
                                        ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto s = std::get_if<std::string>(&value)) {
        if (!root.choices.empty()) {
            if (ImGui::BeginCombo("##value", tr(s->c_str()).c_str())) {
                for (const auto &choice : root.choices)
                    if (ImGui::Selectable(trLabel(choice.c_str()).c_str(), choice == *s)) {
                        *s = choice;
                        out.changed = out.immediate = true;
                    }
                ImGui::EndCombo();
            }
        } else
            out.changed = detail::inputText("##value", *s, root.password ? ImGuiInputTextFlags_Password : 0);
        out.observe();
    } else if (auto v = std::get_if<PropertyVector>(&value)) {
        const auto width =
            (ImGui::CalcItemWidth() - ImGui::GetStyle().ItemInnerSpacing.x * float(v->values.size() - 1)) /
            float(std::max(std::size_t(1), v->values.size()));
        for (std::size_t component = 0; component < v->values.size(); ++component) {
            ScopedID scope{int(component)};
            if (component)
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(width);
            PropertyValue componentValue = v->values[component];
            path.push_back(std::to_string(component));
            auto edit = drawValue(componentValue, root, options, state, path);
            path.pop_back();
            if (edit.changed)
                v->values[component] = std::get<double>(componentValue);
            out.merge(edit);
        }
    } else if (auto color = std::get_if<PropertyColor>(&value)) {
        out.changed = ImGui::ColorEdit4("##value", color->rgba.data());
        out.observe();
    } else if (auto asset = std::get_if<PropertyAsset>(&value)) {
        const float width = ImGui::CalcItemWidth();
        ImGui::SetNextItemWidth(std::max(dp(30), width - dp(34)));
        out.changed = detail::inputText("##value", asset->path);
        out.observe();
        ImGui::SameLine();
        if (button("...")) {
            if (options.browseAssetAt)
                options.browseAssetAt(root.id, path);
            else if (options.browseAsset)
                options.browseAsset(root.id);
        }
    } else if (auto custom = std::get_if<PropertyCustom>(&value)) {
        if (options.editCustom) {
            const auto edit = options.editCustom(root.id, path, *custom);
            if (edit.phase == EditPhase::update || edit.phase == EditPhase::commit) {
                *custom = edit.value;
                out.changed = true;
            }
            out.immediate = edit.committed();
            out.activated = edit.phase == EditPhase::begin;
            out.cancelled = edit.phase == EditPhase::cancel;
            out.active = edit.phase == EditPhase::begin || edit.phase == EditPhase::update;
            out.observe();
        } else {
            const auto editorId = ImGui::GetID("##jsonState");
            if (button(trLabel("Edit JSON...").c_str())) {
                auto &draft = state.elements[editorId];
                draft.expression = custom->json;
                draft.error.clear();
                draft.expressionOpen = true;
                ImGui::OpenPopup("##json");
            }
            if (ImGui::BeginPopup("##json")) {
                auto &draft = state.elements[editorId];
                if (!draft.expressionOpen || root.readOnly)
                    ImGui::CloseCurrentPopup();
                if (ImGui::IsWindowAppearing()) {
                    draft.expression = custom->json;
                    draft.error.clear();
                }
                detail::inputText("##jsonText", draft.expression, 0, dp(420, 140));
                if (button(trLabel("Apply").c_str()) && draft.expressionOpen && !root.readOnly) {
                    auto candidate = PropertyValue{PropertyCustom{custom->type, draft.expression}};
                    if (normalizePropertyValue(childField(root, value), candidate, draft.error)) {
                        value = std::move(candidate);
                        out.changed = out.immediate = true;
                        draft.expressionOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!draft.error.empty())
                    textColoured(palette().error, "%s", draft.error.c_str());
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    ImGui::CloseCurrentPopup();
                out.active |= !out.immediate;
                ImGui::EndPopup();
            }
        }
    } else if (auto enumeration = std::get_if<PropertyEnum>(&value)) {
        if (ImGui::BeginCombo("##value", propertyValueText(value).c_str())) {
            for (const auto &item : enumeration->items) {
                const bool selected = enumeration->flags
                                          ? (item.first ? (enumeration->value & item.first) == item.first
                                                        : enumeration->value == 0)
                                          : enumeration->value == item.first;
                if (ImGui::Selectable(trLabel(item.second.c_str()).c_str(), selected,
                                      enumeration->flags ? ImGuiSelectableFlags_NoAutoClosePopups : 0)) {
                    enumeration->value =
                        enumeration->flags && item.first
                            ? (selected ? enumeration->value & ~item.first : enumeration->value | item.first)
                            : item.first;
                    out.changed = out.immediate = true;
                }
            }
            ImGui::EndCombo();
        }
        out.observe();
    } else if (auto optional = std::get_if<PropertyOptional>(&value)) {
        bool present = bool(optional->value);
        const float width = ImGui::CalcItemWidth();
        {
            ScopedDisabled readonly(root.readOnly);
            if (ImGui::Checkbox("##present", &present)) {
                if (present)
                    optional->value = std::make_unique<PropertyValue>(optional->elementTemplate
                                                                          ? *optional->elementTemplate
                                                                          : PropertyValue{std::string{}});
                else
                    optional->value.reset();
                out.changed = out.immediate = true;
            }
            out.observe();
        }
        ImGui::SameLine();
        if (optional->value) {
            ImGui::SetNextItemWidth(std::max(dp(30), width - ImGui::GetFrameHeightWithSpacing()));
            ScopedID scope("optional");
            path.push_back("value");
            out.merge(drawValue(*optional->value, root, options, state, path));
            path.pop_back();
        } else
            textDim("null");
    } else if (isCollection(value) && fullArray)
        out.merge(drawCollection(value, root, options, state, path));
    else {
        const std::string label = propertyValueText(value);
        if (button((label + "###details").c_str()))
            ImGui::OpenPopup("##details");
        ImGui::SetNextWindowSize(dp(520, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##details")) {
            if (isCollection(value))
                out.merge(drawCollection(value, root, options, state, path));
            else {
                auto &object = std::get<PropertyObject>(value);
                for (auto &field : object.fields) {
                    ScopedID scope(field.first.c_str());
                    const auto definition = propertyObjectField(root, object, field.first);
                    textDim("%s", tr(definition.label.c_str()).c_str());
                    if (!definition.help.empty() && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", tr(definition.help.c_str()).c_str());
                    ImGui::SetNextItemWidth(-1);
                    path.push_back(field.first);
                    out.merge(drawValue(field.second, definition, options, state, path));
                    path.pop_back();
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            out.active |= !out.immediate;
            ImGui::EndPopup();
        }
    }
    if (numeric) {
        const auto editorId = ImGui::GetID("##numericState");
        if (!root.readOnly && ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                               (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))) {
            auto &draft = state.elements[editorId];
            draft.before = value;
            draft.expression = numberText(value);
            draft.error.clear();
            draft.expressionOpen = true;
            ImGui::OpenPopup("##number");
        }
        if (ImGui::BeginPopup("##number")) {
            auto &draft = state.elements[editorId];
            if (!draft.expressionOpen)
                ImGui::CloseCurrentPopup();
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            textDim("%s", tr("Use Value for the original value").c_str());
            if (detail::inputText("##expressionText", draft.expression,
                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                PropertyValue candidate;
                if (evaluatePropertyExpression(childField(root, draft.before), draft.expression, candidate,
                                               draft.error)) {
                    value = std::move(candidate);
                    out.changed = out.immediate = true;
                    draft.expressionOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!draft.error.empty())
                textColoured(palette().error, "%s", draft.error.c_str());
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                draft.expressionOpen = false;
                ImGui::CloseCurrentPopup();
            }
            out.active |= draft.expressionOpen;
            ImGui::EndPopup();
        }
    }
    ImGui::EndGroup();
    return out;
}
} // namespace
Edit<PropertyValue> propertyEditor(const char *id, InspectorState &s, const Property &p,
                                   std::uint64_t revision, const PropertyValidator &validate,
                                   const InspectorOptions &options) {
    ScopedID scope(id);
    ItemScope key(p.id);
    bool cancelled = false;
    if (s.revision != revision) {
        const auto found = s.drafts.find(p.id);
        cancelled = found != s.drafts.end() && found->second.active;
        s.drafts.clear();
        s.elements.clear();
        s.pending.clear();
        s.lists.clear();
        s.revision = revision;
    }
    auto &draft = s.drafts[p.id];
    const bool cached = isCollection(p.value) || std::holds_alternative<PropertyObject>(p.value) ||
                        std::holds_alternative<PropertyOptional>(p.value);
    if (!draft.active && (!cached || !draft.initialized)) {
        draft.value = draft.before = p.value;
        draft.initialized = true;
    }
    std::vector<std::string> path;
    auto edit = drawValue(draft.value, p, options, s, path);
    if (p.readOnly) {
        if (edit.changed)
            draft.value = p.value;
        return {};
    }
    EditPhase phase = cancelled ? EditPhase::cancel : EditPhase::none;
    if (edit.activated && !draft.active) {
        draft.before = p.value;
        draft.active = true;
        phase = EditPhase::begin;
    }
    if (edit.changed) {
        if (!draft.active)
            draft.before = p.value;
        draft.active = true;
        if (normalizePropertyValue(p, draft.value, draft.error))
            phase = EditPhase::update;
    }
    if ((edit.immediate && edit.changed) || edit.edited) {
        if (normalizePropertyValue(p, draft.value, draft.error) && validate)
            draft.error = validate(p.id, draft.value);
        if (draft.error.empty()) {
            phase = EditPhase::commit;
            draft.active = draft.initialized = false;
        } else
            phase = EditPhase::none;
    }
    if (draft.active &&
        (edit.cancelled || ((edit.active || edit.deactivated) && ImGui::IsKeyPressed(ImGuiKey_Escape)))) {
        draft.value = draft.before;
        draft.active = draft.initialized = false;
        draft.error.clear();
        phase = EditPhase::cancel;
    } else if (draft.active && edit.deactivated && !edit.active && !edit.edited && !edit.changed) {
        draft.value = draft.before;
        draft.active = draft.initialized = false;
        phase = EditPhase::cancel;
    }
    if (!draft.error.empty() && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr(draft.error.c_str()).c_str());
    return phase == EditPhase::none ? Edit<PropertyValue>{} : Edit<PropertyValue>{phase, draft.value};
}
bool propertyMatches(const TextFilter &filter, const Property &property, const char *context) {
    if (!filter.active())
        return true;
    const std::string path = context ? context : "";
    return filter.matches(property.label, path + " " + tr(path.c_str()) + " " + property.help + " " +
                                              tr(property.help.c_str()));
}
std::vector<PropertyEdit> inspector(const char *id, InspectorState &state, ArrayView<Property> properties,
                                    std::uint64_t revision, const PropertyValidator &validate) {
    return inspector(id, state, properties, revision, validate, {});
}
std::vector<PropertyEdit> inspector(const char *id, InspectorState &s, ArrayView<Property> properties,
                                    std::uint64_t revision, const PropertyValidator &validate,
                                    const InspectorOptions &options) {
    std::vector<PropertyEdit> result;
    ScopedID scope(id);
    for (auto &list : s.lists)
        list.second.renderedRows = 0;
    std::unordered_set<ItemId> present;
    if (s.revision != revision) {
        for (auto &entry : s.drafts)
            if (entry.second.active) {
                if (entry.second.activeId && ImGui::GetActiveID() == entry.second.activeId)
                    ImGui::ClearActiveID();
                result.push_back({entry.first, {EditPhase::cancel, entry.second.before}});
            }
        s.drafts.clear();
        s.elements.clear();
        s.pending.clear();
        for (auto &entry : s.lists) {
            entry.second.activeRow = -1;
            entry.second.indexedSize = std::size_t(-1);
            entry.second.selection.clear();
        }
        s.revision = revision;
    }
    if (!beginProperties("##properties"))
        return result;
    std::size_t visible = 0;
    std::unordered_set<std::string> categories;
    for (std::size_t i = 0; i < properties.count; ++i) {
        const auto &p = properties[i];
        present.insert(p.id);
        if (options.filter && !propertyMatches(*options.filter, p, options.filterContext)) {
            const auto it = s.drafts.find(p.id);
            if (it != s.drafts.end() && it->second.active) {
                ItemScope key(p.id);
                if (ImGui::GetActiveID() == ImGui::GetID("##value") ||
                    (it->second.activeId && ImGui::GetActiveID() == it->second.activeId))
                    ImGui::ClearActiveID();
                auto &draft = it->second;
                result.push_back({p.id, {EditPhase::cancel, draft.before}});
                draft.value = draft.before;
                draft.active = false;
                draft.cancelled = true;
                draft.error.clear();
            }
            continue;
        }
        ++visible;
        if (options.categories && !p.category.empty() && categories.insert(p.category).second) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const auto categoryLabel =
                (s.collapsedCategories.count(p.category) ? "> " : "v ") + trLabel(p.category.c_str());
            if (ImGui::Selectable(categoryLabel.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                if (!s.collapsedCategories.erase(p.category))
                    s.collapsedCategories.insert(p.category);
            if (options.clipboard && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(trLabel("Copy category as JSON").c_str()))
                    try {
                        ImGui::SetClipboardText(propertyCategoryToJson(properties, p.category).c_str());
                    } catch (const std::exception &e) {
                        s.clipboardError = e.what();
                    }
                if (ImGui::MenuItem(trLabel("Paste category").c_str())) {
                    auto text = ImGui::GetClipboardText();
                    auto paste = propertyCategoryFromJson(properties, p.category, text ? text : "", validate);
                    s.clipboardError = paste.error;
                    if (paste) {
                        result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                        s.drafts.clear();
                        s.elements.clear();
                        for (auto &list : s.lists)
                            list.second.activeRow = -1;
                    }
                }
                ImGui::EndPopup();
            }
        }
        if (options.categories && !p.category.empty() && s.collapsedCategories.count(p.category) &&
            !(options.filter && options.filter->active())) {
            auto found = s.drafts.find(p.id);
            if (found != s.drafts.end() && found->second.active) {
                auto &draft = found->second;
                ItemScope key(p.id);
                if (ImGui::GetActiveID() == ImGui::GetID("##value") ||
                    (draft.activeId && ImGui::GetActiveID() == draft.activeId))
                    ImGui::ClearActiveID();
                result.push_back({p.id, {EditPhase::cancel, draft.before}});
                draft.active = draft.expressionOpen = false;
                draft.value = draft.before;
            }
            continue;
        }
        auto &draft = s.drafts[p.id];
        const bool cached = isCollection(p.value) || std::holds_alternative<PropertyObject>(p.value) ||
                            std::holds_alternative<PropertyOptional>(p.value);
        if (!draft.active && (!cached || !draft.initialized)) {
            draft.value = p.value;
            draft.before = p.value;
            draft.initialized = true;
        }
        ItemScope key(p.id);
        property(tr(p.label.c_str()).c_str(), p.help.empty() ? nullptr : tr(p.help.c_str()).c_str());
        if (options.clipboard && ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##propertyMenu");
        bool changed = false, immediate = false;
        DrawEdit compound;
        bool compoundDrawn = false;
        {
            ScopedDisabled disabled(p.readOnly && p.value.index() <= 4);
            if (auto *boolValue = std::get_if<bool>(&draft.value)) {
                changed = toggleSwitch("##value", boolValue);
                immediate = true;
            } else if (auto *intValue = std::get_if<std::int64_t>(&draft.value)) {
                const std::int64_t *low = p.bounded ? &p.integerMinimum : nullptr,
                                   *high = p.bounded ? &p.integerMaximum : nullptr;
                changed = ImGui::DragScalar("##value", ImGuiDataType_S64, intValue, 1.f, low, high, "%lld",
                                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
            } else if (auto *doubleValue = std::get_if<double>(&draft.value)) {
                const double *low = p.bounded ? &p.minimum : nullptr,
                             *high = p.bounded ? &p.maximum : nullptr;
                changed = ImGui::DragScalar("##value", ImGuiDataType_Double, doubleValue, .01f, low, high,
                                            "%.6g", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
            } else if (auto *floatValue = std::get_if<float>(&draft.value)) {
                changed = ImGui::DragFloat("##value", floatValue, .01f, p.bounded ? float(p.minimum) : 0.f,
                                           p.bounded ? float(p.maximum) : 0.f, "%.6g",
                                           ImGuiSliderFlags_NoInput |
                                               (p.bounded ? ImGuiSliderFlags_AlwaysClamp : 0));
            } else if (auto *value = std::get_if<std::string>(&draft.value)) {
                if (!p.choices.empty()) {
                    if (ImGui::BeginCombo("##value", tr(value->c_str()).c_str())) {
                        for (const auto &choice : p.choices)
                            if (ImGui::Selectable(trLabel(choice.c_str()).c_str(), choice == *value)) {
                                *value = choice;
                                changed = true;
                            }
                        ImGui::EndCombo();
                    }
                    immediate = true;
                } else
                    changed =
                        detail::inputText("##value", *value, p.password ? ImGuiInputTextFlags_Password : 0,
                                          p.multiline ? ImVec2(-1, dp(84)) : ImVec2{});
            } else {
                std::vector<std::string> path;
                compound = drawValue(draft.value, p, options, s, path, true);
                compoundDrawn = true;
                changed = compound.changed;
                immediate = compound.immediate;
            }
        }
        if (p.readOnly && changed) {
            draft.value = p.value;
            changed = false;
        }
        const bool numeric = std::holds_alternative<std::int64_t>(p.value) ||
                             std::holds_alternative<double>(p.value) ||
                             std::holds_alternative<float>(p.value);
        const bool expressionRequested = numeric && !p.readOnly &&
                                         ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                                          (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)));
        EditPhase phase = EditPhase::none;
        const bool itemActive = compoundDrawn ? compound.active : ImGui::IsItemActive();
        const bool deactivated = compoundDrawn ? compound.deactivated : ImGui::IsItemDeactivated();
        const bool edited = compoundDrawn ? compound.edited : ImGui::IsItemDeactivatedAfterEdit();
        if (itemActive && ImGui::GetActiveID())
            draft.activeId = ImGui::GetActiveID();
        // Escape cancels whoever ends the edit: this code (still active), ImGui's own drag cancel or
        // keyboard navigation (deactivated this frame). Either way the draft must not commit.
        const bool escaped =
            draft.active && (compound.cancelled ||
                             ((itemActive || deactivated) && ImGui::IsKeyPressed(ImGuiKey_Escape, false)));
        // The frame after ClearActiveID() ImGui reports the deactivation of an edit already cancelled.
        const bool stale = draft.cancelled && !itemActive;
        draft.cancelled = false;
        if (stale) {
        } else if (!p.readOnly && escaped) {
            if (itemActive) {
                ImGui::ClearActiveID();
                draft.cancelled = true;
            }
            phase = EditPhase::cancel;
            draft.value = draft.before;
            draft.active = false;
            draft.error.clear();
        } else if (!p.readOnly) {
            if (compoundDrawn ? compound.activated : ImGui::IsItemActivated()) {
                draft.active = true;
                draft.before = p.value;
                phase = EditPhase::begin;
            }
            if (changed) {
                changed = normalizePropertyValue(p, draft.value, draft.error);
                draft.active = true;
                phase = changed ? EditPhase::update : EditPhase::none;
                if (changed)
                    draft.error.clear();
            }
            if ((immediate && changed) || edited) {
                const bool valid = normalizePropertyValue(p, draft.value, draft.error);
                if (valid && validate)
                    draft.error = validate(p.id, draft.value);
                if (draft.error.empty()) {
                    phase = EditPhase::commit;
                    draft.active = false;
                    draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                } else
                    phase = EditPhase::none;
            }
            if (deactivated && !itemActive && !edited && draft.active) {
                phase = EditPhase::cancel;
                draft.active = false;
                draft.value = draft.before;
            }
        }
        if (phase != EditPhase::none)
            result.push_back({p.id, {phase, draft.value}});
        bool openExpression = expressionRequested;
        if (options.clipboard && !isCollection(p.value) && ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##propertyMenu");
        if (ImGui::BeginPopup("##propertyMenu")) {
            if (ImGui::MenuItem(trLabel("Copy as JSON").c_str(), nullptr, false, !p.password && !p.mixed))
                try {
                    ImGui::SetClipboardText(propertyToJson(p).c_str());
                } catch (const std::exception &e) {
                    s.clipboardError = e.what();
                }
            if (ImGui::MenuItem(trLabel("Paste value").c_str(), nullptr, false, !p.readOnly)) {
                auto text = ImGui::GetClipboardText();
                auto paste = propertyFromJson(p, text ? text : "", validate);
                s.clipboardError = paste.error;
                if (paste) {
                    result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                    draft.active = false;
                    draft.expressionOpen = false;
                    draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                }
            }
            if (numeric &&
                ImGui::MenuItem(trLabel("Edit expression...").c_str(), nullptr, false, !p.readOnly))
                openExpression = true;
            if (p.defaultValue &&
                ImGui::MenuItem(trLabel("Reset to default").c_str(), nullptr, false, !p.readOnly)) {
                Property source = p;
                source.value = *p.defaultValue;
                auto paste = propertyFromJson(p, propertyToJson(source), validate);
                s.clipboardError = paste.error;
                if (paste) {
                    result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                    draft.active = draft.expressionOpen = draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                }
            }
            ImGui::EndPopup();
        }
        if (p.mixed) {
            ImGui::SameLine();
            textFaint("%s", tr("Multiple values").c_str());
        }
        if (openExpression) {
            draft.expressionOpen = draft.expressionFresh = true;
            draft.before = p.value;
            draft.expression = std::visit(
                [](const auto &v) -> std::string {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_integral_v<T>)
                        return std::to_string(v);
                    else if constexpr (std::is_floating_point_v<T>) {
                        std::ostringstream text;
                        text.imbue(std::locale::classic());
                        text << std::setprecision(std::numeric_limits<T>::max_digits10) << v;
                        return text.str();
                    } else
                        return {};
                },
                p.value);
            ImGui::OpenPopup("##expression");
        }
        if (draft.expressionOpen && (p.readOnly || s.revision != revision))
            draft.expressionOpen = false;
        if (ImGui::BeginPopup("##expression")) {
            if (draft.expressionFresh) {
                ImGui::SetKeyboardFocusHere();
                draft.expressionFresh = false;
            }
            textDim("%s", tr("Use Value for the original value").c_str());
            if (detail::inputText("##expressionText", draft.expression,
                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                Property original = p;
                original.value = draft.before;
                PropertyValue value;
                if (!p.readOnly &&
                    evaluatePropertyExpression(original, draft.expression, value, draft.error)) {
                    if (validate)
                        draft.error = validate(p.id, value);
                    if (draft.error.empty()) {
                        result.push_back({p.id, {EditPhase::commit, value}});
                        draft.active = draft.expressionOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !draft.expressionOpen) {
                draft.expressionOpen = false;
                ImGui::CloseCurrentPopup();
            }
            if (!draft.error.empty())
                textColoured(palette().error, "%s", tr(draft.error.c_str()).c_str());
            ImGui::EndPopup();
        }
        if (!draft.error.empty())
            textColoured(palette().error, tr("%s").c_str(), draft.error.c_str());
    }
    endProperties();
    if (!s.clipboardError.empty())
        textColoured(palette().error, "%s", tr(s.clipboardError.c_str()).c_str());
    if (!visible && options.filter && options.filter->active())
        textDim("%s", tr("No matching properties").c_str());
    for (auto it = s.drafts.begin(); it != s.drafts.end();)
        if (!present.count(it->first))
            it = s.drafts.erase(it);
        else
            ++it;
    return result;
}
} // namespace cyui

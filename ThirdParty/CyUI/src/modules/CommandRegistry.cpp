#include "cyui/modules/Commands.h"
#include "cyui/detail/Json.h"
#include "ModuleInternal.h"
#include <unordered_set>
namespace cyui {
namespace {
bool validChord(ImGuiKeyChord chord) {
    auto key = chord & ~ImGuiMod_Mask_;
    return chord == 0 || (key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_MouseLeft &&
                          (chord & ~(ImGuiMod_Mask_ | 0x7ff)) == 0);
}
int priority(const std::string &context, const std::vector<std::string> &active) {
    if (context.empty())
        return int(active.size());
    auto i = std::find(active.begin(), active.end(), context);
    return i == active.end() ? -1 : int(i - active.begin());
}
} // namespace
ImGuiKeyChord parseShortcut(const std::string &text) {
    if (text.empty())
        return 0;
    ImGuiKeyChord result = 0;
    bool key = false;
    std::size_t at = 0;
    while (at <= text.size()) {
        auto end = text.find('+', at);
        if (end == std::string::npos)
            end = text.size();
        auto part = detail::folded(text.substr(at, end - at));
        part.erase(
            std::remove_if(part.begin(), part.end(), [](unsigned char c) { return std::isspace(c) != 0; }),
            part.end());
        ImGuiKeyChord mod = part == "ctrl" || part == "control"                     ? ImGuiMod_Ctrl
                            : part == "shift"                                       ? ImGuiMod_Shift
                            : part == "alt"                                         ? ImGuiMod_Alt
                            : part == "super" || part == "cmd" || part == "command" ? ImGuiMod_Super
                                                                                    : 0;
        if (mod) {
            if (result & mod)
                return 0;
            result |= mod;
        } else {
            if (key)
                return 0;
            ImGuiKey found = ImGuiKey_None;
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_MouseLeft; ++k)
                if (detail::folded(ImGui::GetKeyName(ImGuiKey(k))) == part) {
                    found = ImGuiKey(k);
                    break;
                }
            if (found == ImGuiKey_None)
                return 0;
            result |= found;
            key = true;
        }
        if (end == text.size())
            break;
        at = end + 1;
    }
    return key && validChord(result) ? result : 0;
}
std::string shortcutText(ImGuiKeyChord chord) {
    if (!chord || !validChord(chord))
        return {};
    std::string s;
    if (chord & ImGuiMod_Ctrl)
        s += "Ctrl+";
    if (chord & ImGuiMod_Shift)
        s += "Shift+";
    if (chord & ImGuiMod_Alt)
        s += "Alt+";
    if (chord & ImGuiMod_Super)
        s += "Super+";
    return s + ImGui::GetKeyName(ImGuiKey(chord & ~ImGuiMod_Mask_));
}
bool CommandRegistry::add(Command command, ImGuiKeyChord chord) {
    if (!command.id || find(command.id))
        return false;
    if (!chord && !command.shortcut.empty()) {
        chord = parseShortcut(command.shortcut);
        if (!chord)
            return false;
    }
    if (!validChord(chord))
        return false;
    for (const auto &e : entries_)
        if (chord && e.chord == chord && e.command.context == command.context)
            return false;
    command.shortcut = shortcutText(chord);
    entries_.push_back({std::move(command), chord});
    return true;
}
bool CommandRegistry::remove(ItemId id) {
    auto i =
        std::find_if(entries_.begin(), entries_.end(), [&](const auto &e) { return e.command.id == id; });
    if (i == entries_.end())
        return false;
    entries_.erase(i);
    return true;
}
Command *CommandRegistry::find(ItemId id) {
    for (auto &e : entries_)
        if (e.command.id == id)
            return &e.command;
    return nullptr;
}
const Command *CommandRegistry::find(ItemId id) const {
    for (const auto &e : entries_)
        if (e.command.id == id)
            return &e.command;
    return nullptr;
}
bool CommandRegistry::rebind(ItemId id, ImGuiKeyChord chord) {
    if (!validChord(chord))
        return false;
    auto p = find(id);
    if (!p)
        return false;
    for (const auto &e : entries_)
        if (chord && e.command.id != id && e.chord == chord && e.command.context == p->context)
            return false;
    for (auto &e : entries_)
        if (e.command.id == id) {
            e.chord = chord;
            e.command.shortcut = shortcutText(chord);
            return true;
        }
    return false;
}
std::vector<Command> CommandRegistry::commands(const std::vector<std::string> &active) const {
    std::vector<Command> out;
    for (const auto &e : entries_)
        if (priority(e.command.context, active) >= 0)
            out.push_back(e.command);
    return out;
}
std::vector<CommandConflict> CommandRegistry::conflicts() const {
    std::vector<CommandConflict> out;
    for (std::size_t a = 0; a < entries_.size(); ++a)
        for (auto b = a + 1; b < entries_.size(); ++b)
            if (entries_[a].chord && entries_[a].chord == entries_[b].chord &&
                entries_[a].command.context == entries_[b].command.context)
                out.push_back({entries_[a].command.id, entries_[b].command.id, entries_[a].chord});
    return out;
}
ItemId CommandRegistry::match(ImGuiKeyChord chord, const std::vector<std::string> &active,
                              bool typing) const {
    ItemId id = 0;
    int best = int(active.size()) + 1;
    for (const auto &e : entries_) {
        auto rank = priority(e.command.context, active);
        if (e.chord && e.chord == chord && rank >= 0 && rank < best && e.command.enabled &&
            (!typing || e.command.allowWhileTyping)) {
            best = rank;
            id = e.command.id;
        }
    }
    return id;
}
ItemId CommandRegistry::poll(const std::vector<std::string> &active) const {
    std::unordered_set<ImGuiKeyChord> seen;
    for (const auto &e : entries_)
        if (e.chord && seen.insert(e.chord).second) {
            auto id = match(e.chord, active, ImGui::GetIO().WantTextInput);
            if (id && ImGui::Shortcut(e.chord, ImGuiInputFlags_RouteFocused))
                return id;
        }
    return 0;
}
bool CommandRegistry::menuItem(ItemId id) const {
    auto p = find(id);
    return p &&
           ImGui::MenuItem(trLabel(p->label.c_str()).c_str(), p->shortcut.c_str(), p->checked, p->enabled);
}
bool CommandRegistry::toolbarButton(ItemId id, const char *icon) const {
    auto p = find(id);
    if (!p)
        return false;
    ItemScope key(id);
    ScopedDisabled disabled(!p->enabled);
    auto tip = tr(p->label.c_str()) + (p->shortcut.empty() ? "" : " (" + p->shortcut + ")");
    return iconButton(icon, tip.c_str(), p->checked);
}
std::string CommandRegistry::bindingsJson() const {
    auto j = detail::Json::obj();
    j.fields["format"] = detail::Json::str("cyui.commands");
    j.fields["version"] = detail::Json::num("1");
    auto a = detail::Json::list();
    for (const auto &e : entries_) {
        auto v = detail::Json::obj();
        v.fields["id"] = detail::Json::num(std::to_string(e.command.id));
        v.fields["shortcut"] = detail::Json::str(shortcutText(e.chord));
        a.items.push_back(v);
    }
    j.fields["bindings"] = a;
    return detail::jsonDump(j);
}
bool CommandRegistry::loadBindingsJson(const std::string &text, std::string &error) {
    try {
        auto j = detail::jsonParse(text);
        if (j.get("format").str() != "cyui.commands" || j.get("version").integer<int>() != 1)
            throw std::invalid_argument("Unsupported binding format");
        auto copy = *this;
        const auto &a = j.get("bindings");
        if (a.kind != detail::Json::array)
            throw std::invalid_argument("Expected bindings array");
        std::unordered_set<ItemId> ids;
        for (const auto &v : a.items) {
            auto id = v.get("id").integer<ItemId>();
            if (!copy.find(id) || !ids.insert(id).second)
                throw std::invalid_argument("Unknown or duplicate command ID");
        }
        for (auto &e : copy.entries_)
            if (ids.count(e.command.id))
                e.chord = 0;
        for (const auto &v : a.items) {
            auto id = v.get("id").integer<ItemId>();
            auto shortcut = v.get("shortcut").str();
            auto chord = parseShortcut(shortcut);
            if ((!shortcut.empty() && !chord) || !copy.rebind(id, chord))
                throw std::invalid_argument("Invalid or conflicting shortcut");
        }
        *this = std::move(copy);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool shortcutEditor(const char *id, ImGuiKeyChord &chord) {
    ScopedID scope(id);
    bool changed = false;
    const auto text = shortcutText(chord);
    if (button((text.empty() ? tr("Set shortcut") : text).c_str()))
        ImGui::OpenPopup("##capture");
    if (ImGui::BeginPopup("##capture")) {
        textDim("%s", tr("Press a shortcut; Escape cancels").c_str());
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            chord = 0;
            changed = true;
            ImGui::CloseCurrentPopup();
        } else
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_MouseLeft; ++k)
                if ((k < ImGuiKey_LeftCtrl || k > ImGuiKey_RightSuper) &&
                    ImGui::IsKeyPressed(ImGuiKey(k), false)) {
                    chord = k | ImGui::GetIO().KeyMods;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                    break;
                }
        ImGui::EndPopup();
    }
    return changed;
}
} // namespace cyui

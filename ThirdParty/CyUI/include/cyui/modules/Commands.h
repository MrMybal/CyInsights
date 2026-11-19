#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <string>

namespace cyui {
struct Command {
    ItemId id = 0;
    std::string label, shortcut, category;
    bool enabled = true;
    std::string context; // empty = global; first active context has priority
    bool allowWhileTyping = false, checked = false;
};
struct CommandState {
    std::string query;
    int selected = 0;
    bool wasOpen = false;
};
CYUI_API std::vector<std::size_t> rankCommands(ArrayView<Command>, const std::string &query);
// Returns a requested command ID, 0 for none (give commands nonzero IDs). The host validates and
// executes it.
CYUI_API ItemId commandPalette(const char *id, bool &open, CommandState &, ArrayView<Command>,
                               ImVec2 size = {});
struct CommandConflict {
    ItemId first = 0, second = 0;
    ImGuiKeyChord chord = 0;
};
CYUI_API ImGuiKeyChord parseShortcut(const std::string &); // 0 rejects invalid/empty chords
CYUI_API std::string shortcutText(ImGuiKeyChord);
class CYUI_API CommandRegistry {
  public:
    bool add(Command, ImGuiKeyChord chord = 0); // unique nonzero IDs; parses shortcut when chord is 0
    bool remove(ItemId);
    bool rebind(ItemId, ImGuiKeyChord); // chord 0 unbinds; conflicts are rejected within a context
    Command *find(ItemId);
    const Command *find(ItemId) const;
    std::vector<Command> commands(const std::vector<std::string> &contexts = {}) const;
    std::vector<CommandConflict> conflicts() const;
    ItemId match(ImGuiKeyChord, const std::vector<std::string> &contexts = {}, bool typing = false) const;
    ItemId poll(const std::vector<std::string> &contexts = {}) const; // once/frame in the focused host window
    bool menuItem(ItemId) const;
    bool toolbarButton(ItemId, const char *icon) const;
    std::string bindingsJson() const;
    bool loadBindingsJson(const std::string &, std::string &error); // atomic; unknown IDs rejected
  private:
    struct Entry {
        Command command;
        ImGuiKeyChord chord = 0;
    };
    std::vector<Entry> entries_;
};
// One key capture, Escape cancels, Backspace clears. Returns true on change.
CYUI_API bool shortcutEditor(const char *id, ImGuiKeyChord &chord);
} // namespace cyui

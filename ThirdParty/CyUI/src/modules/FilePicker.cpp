#include "cyui/modules/FilePicker.h"
#include "ModuleInternal.h"
namespace cyui {
namespace {
bool filename(const std::string &name) {
    return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:") == std::string::npos &&
           std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; });
}
std::string clean(const std::string &path) {
    if (path.empty())
        return {};
    std::string text = path;
    std::replace(text.begin(), text.end(), '\\', '/');
    if (text[0] != '/' && !(text.size() >= 3 && text[1] == ':' && text[2] == '/'))
        return {};
    const std::string prefix = text[0] == '/' ? "/" : text.substr(0, 3);
    std::vector<std::string> parts;
    std::size_t at = prefix.size();
    while (at <= text.size()) {
        auto end = text.find('/', at);
        if (end == std::string::npos)
            end = text.size();
        auto part = text.substr(at, end - at);
        if (part == "..") {
            if (parts.empty())
                return {};
            parts.pop_back();
        } else if (!part.empty() && part != ".") {
            if (!filename(part))
                return {};
            parts.push_back(part);
        }
        if (end == text.size())
            break;
        at = end + 1;
    }
    std::string out = prefix;
    for (const auto &part : parts) {
        if (out.back() != '/')
            out += '/';
        out += part;
    }
    return out;
}
std::string join(const std::string &dir, const std::string &name) {
    return dir + (dir.empty() || dir.back() == '/' ? "" : "/") + name;
}
} // namespace
std::string normalizeFilePath(const std::string &path, const std::string &root) {
    const auto base = clean(root), value = clean(path);
    if (base.empty() || value.empty())
        return {};
    if (value == base)
        return value;
    const auto prefix = base.back() == '/' ? base : base + '/';
    return value.compare(0, prefix.size(), prefix) == 0 ? value : std::string{};
}
bool fileMatchesFilter(const std::string &name, const FileFilter &filter) {
    if (filter.extensions.empty())
        return true;
    auto lower = detail::folded(name);
    for (auto ext : filter.extensions) {
        ext = detail::folded(ext);
        if (ext == "*" || ext == "*.*")
            return true;
        if (!ext.empty() && ext[0] != '.')
            ext = '.' + ext;
        if (!ext.empty() && lower.size() >= ext.size() &&
            lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0)
            return true;
    }
    return false;
}
FilePickerResult validateFilePicker(const FilePickerState &s, const FileListing &listing,
                                    const FilePickerOptions &o, bool overwriteConfirmed) {
    FilePickerResult r;
    r.directory = normalizeFilePath(s.directory, o.root);
    if (r.directory.empty() || listing.directory != s.directory || listing.loading || !listing.error.empty())
        return r;
    if (o.mode == FilePickerMode::directory) {
        r.accepted = true;
        r.paths = {r.directory};
        return r;
    }
    if (o.mode == FilePickerMode::saveFile) {
        if (!filename(s.filename))
            return r;
        if (!o.filters.empty() &&
            (s.filter >= o.filters.size() || !fileMatchesFilter(s.filename, o.filters[s.filter])))
            return r;
        const auto found = std::find_if(listing.entries.begin(), listing.entries.end(),
                                        [&](const auto &e) { return e.name == s.filename; });
        if (found != listing.entries.end()) {
            if (found->directory)
                return r;
            if (o.confirmOverwrite && !overwriteConfirmed) {
                r.overwriteRequested = true;
                r.paths = {join(r.directory, s.filename)};
                return r;
            }
        }
        r.paths = {join(r.directory, s.filename)};
        r.accepted = true;
        return r;
    }
    if (s.selection.empty() || (o.mode == FilePickerMode::openFile && s.selection.size() != 1))
        return r;
    for (const auto &name : s.selection) {
        auto found = std::find_if(listing.entries.begin(), listing.entries.end(),
                                  [&](const auto &e) { return e.name == name; });
        if (!filename(name) || found == listing.entries.end() || found->directory ||
            (!o.filters.empty() &&
             (s.filter >= o.filters.size() || !fileMatchesFilter(name, o.filters[s.filter])))) {
            r.paths.clear();
            return r;
        }
        r.paths.push_back(join(r.directory, name));
    }
    r.accepted = !r.paths.empty();
    return r;
}
FilePickerResult filePicker(const char *id, FilePickerState &s, const FileListing &listing, ImVec2 size,
                            const FilePickerOptions &o) {
    ScopedID scope(id);
    FilePickerResult r;
    auto navigate = [&](const std::string &path, bool record) {
        auto next = normalizeFilePath(path, o.root);
        if (next.empty()) {
            s.error = "Path is outside the allowed root";
            return;
        }
        if (next == s.directory)
            return;
        s.directory = s.pathText = next;
        s.selection.clear();
        s.pendingOverwrite.clear();
        s.error.clear();
        r.navigate = true;
        r.directory = next;
        if (record) {
            if (s.history.empty())
                s.history.push_back(listing.directory.empty() ? o.root : listing.directory);
            s.history.resize(std::min(s.historyIndex + 1, s.history.size()));
            s.history.push_back(next);
            s.historyIndex = s.history.size() - 1;
        }
    };
    if (normalizeFilePath(s.directory, o.root).empty())
        navigate(o.root, true);
    {
        ScopedDisabled disabled(s.history.empty() || s.historyIndex == 0);
        if (ghostButton("<")) {
            --s.historyIndex;
            navigate(s.history[s.historyIndex], false);
        }
    }
    ImGui::SameLine();
    {
        ScopedDisabled disabled(s.history.empty() || s.historyIndex + 1 >= s.history.size());
        if (ghostButton(">")) {
            ++s.historyIndex;
            navigate(s.history[s.historyIndex], false);
        }
    }
    ImGui::SameLine();
    if (ghostButton(trLabel("Up").c_str())) {
        auto end = s.directory.find_last_of('/');
        navigate(end == 0 ? "/" : s.directory.substr(0, end), true);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (detail::inputText("##path", s.pathText, ImGuiInputTextFlags_EnterReturnsTrue))
        navigate(s.pathText, true);
    // Clickable breadcrumbs use the same validated navigation path as typed input.
    const auto root = normalizeFilePath(o.root, o.root);
    if (!root.empty()) {
        if (ghostButton(root.c_str()))
            navigate(root, true);
        std::size_t at = root.size();
        while (at < s.directory.size()) {
            if (s.directory[at] == '/') {
                ++at;
                continue;
            }
            auto end = s.directory.find('/', at);
            if (end == std::string::npos)
                end = s.directory.size();
            ImGui::SameLine();
            ScopedID crumb{int(at)};
            if (ghostButton(s.directory.substr(at, end - at).c_str()))
                navigate(s.directory.substr(0, end), true);
            at = end;
        }
    }
    ImGui::SetNextItemWidth(-1);
    detail::inputText("##search", s.query);
    ImVec2 content = size;
    if (content.y > 0)
        content.y = std::max(dp(40), content.y - ImGui::GetFrameHeightWithSpacing() * 6);
    else
        content.y =
            std::max(dp(80), ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3);
    ImGui::BeginChild("##files", content, ImGuiChildFlags_Borders);
    bool submit = false;
    if (listing.directory != s.directory || listing.loading)
        textDim("%s", tr("Loading...").c_str());
    else if (!listing.error.empty())
        textColoured(palette().error, "%s", listing.error.c_str());
    else {
        std::vector<std::size_t> entries;
        const auto query = detail::folded(s.query);
        for (std::size_t i = 0; i < listing.entries.size(); ++i) {
            const auto &e = listing.entries[i];
            if (!filename(e.name))
                continue;
            if (!query.empty() && !detail::containsFolded(e.name, query))
                continue;
            if (!e.directory && (!o.filters.empty() && (s.filter >= o.filters.size() ||
                                                        !fileMatchesFilter(e.name, o.filters[s.filter]))))
                continue;
            entries.push_back(i);
        }
        std::stable_sort(entries.begin(), entries.end(), [&](auto a, auto b) {
            const auto &x = listing.entries[a];
            const auto &y = listing.entries[b];
            if (x.directory != y.directory)
                return x.directory;
            return detail::folded(x.name) < detail::folded(y.name);
        });
        ImGuiListClipper clip;
        clip.Begin(int(entries.size()));
        while (clip.Step())
            for (int n = clip.DisplayStart; n < clip.DisplayEnd; ++n) {
                const auto &e = listing.entries[entries[std::size_t(n)]];
                ScopedID item(e.name.c_str());
                const bool chosen =
                    std::find(s.selection.begin(), s.selection.end(), e.name) != s.selection.end();
                if (ImGui::Selectable(((e.directory ? "[+] " : "") + e.name).c_str(), chosen,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (e.directory) {
                        if (ImGui::IsMouseDoubleClicked(0))
                            navigate(join(s.directory, e.name), true);
                    } else {
                        if (o.mode == FilePickerMode::openFiles && ImGui::GetIO().KeyCtrl) {
                            auto it = std::find(s.selection.begin(), s.selection.end(), e.name);
                            if (it == s.selection.end())
                                s.selection.push_back(e.name);
                            else
                                s.selection.erase(it);
                        } else
                            s.selection = {e.name};
                        s.filename = e.name;
                        if (ImGui::IsMouseDoubleClicked(0))
                            submit = true;
                    }
                }
            }
    }
    ImGui::EndChild();
    if (!o.filters.empty()) {
        ImGui::SetNextItemWidth(dp(180));
        const auto filter = std::min(s.filter, o.filters.size() - 1);
        if (ImGui::BeginCombo("##filter", tr(o.filters[filter].label.c_str()).c_str())) {
            for (std::size_t i = 0; i < o.filters.size(); ++i)
                if (ImGui::Selectable(trLabel(o.filters[i].label.c_str()).c_str(), s.filter == i)) {
                    s.filter = i;
                    s.selection.clear();
                }
            ImGui::EndCombo();
        }
    }
    if (o.mode == FilePickerMode::saveFile) {
        ImGui::SetNextItemWidth(-1);
        if (detail::inputText("##filename", s.filename, ImGuiInputTextFlags_EnterReturnsTrue))
            submit = true;
    }
    if (primaryButton(trLabel(o.mode == FilePickerMode::saveFile ? "Save" : "Open").c_str()))
        submit = true;
    ImGui::SameLine();
    if (button(trLabel("Cancel").c_str()))
        r.cancelled = true;
    if (submit) {
        auto accepted = validateFilePicker(s, listing, o);
        if (accepted.overwriteRequested) {
            s.pendingOverwrite = accepted.paths.front();
            ImGui::OpenPopup(trLabel("Replace existing file?").c_str());
        } else if (accepted.accepted)
            r = std::move(accepted);
        else
            s.error = "Select a valid file or folder";
    }
    if (ImGui::BeginPopupModal(trLabel("Replace existing file?").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(s.pendingOverwrite.c_str());
        if (dangerButton(trLabel("Replace").c_str())) {
            auto accepted = validateFilePicker(s, listing, o, true);
            if (accepted.accepted && accepted.paths.front() == s.pendingOverwrite)
                r = std::move(accepted);
            else
                s.error = "The file selection changed";
            s.pendingOverwrite.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button(trLabel("Cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s.pendingOverwrite.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!s.error.empty())
        textColoured(palette().error, "%s", tr(s.error.c_str()).c_str());
    return r;
}
} // namespace cyui

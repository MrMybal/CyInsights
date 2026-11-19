#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <string>
namespace cyui {
enum class FilePickerMode { openFile, openFiles, saveFile, directory };
struct FileEntry {
    ItemId id = 0;
    std::string name;
    bool directory = false;
    std::uint64_t bytes = 0;
    std::string modified;
};
// The application supplies async filesystem/VFS results; CyUI performs no I/O.
struct FileListing {
    std::string directory;
    std::vector<FileEntry> entries;
    bool loading = false;
    std::string error;
};
struct FileFilter {
    std::string label;
    std::vector<std::string> extensions;
}; // .png or png, case insensitive; empty = all
struct FilePickerOptions {
    FilePickerMode mode = FilePickerMode::openFile;
    std::string root = "/";
    std::vector<FileFilter> filters;
    bool confirmOverwrite = true;
};
struct FilePickerState {
    std::string directory = "/", pathText = "/", filename, query, error, pendingOverwrite;
    std::vector<std::string> selection, history;
    std::size_t historyIndex = 0, filter = 0;
};
struct FilePickerResult {
    bool accepted = false, cancelled = false, navigate = false, overwriteRequested = false;
    std::string directory;
    std::vector<std::string> paths;
};
CYUI_API std::string normalizeFilePath(const std::string &path,
                                       const std::string &root = "/"); // empty = rejected
CYUI_API bool fileMatchesFilter(const std::string &, const FileFilter &);
CYUI_API FilePickerResult validateFilePicker(const FilePickerState &, const FileListing &,
                                             const FilePickerOptions &, bool overwriteConfirmed = false);
CYUI_API FilePickerResult filePicker(const char *id, FilePickerState &, const FileListing &, ImVec2 size = {},
                                     const FilePickerOptions & = {});
} // namespace cyui

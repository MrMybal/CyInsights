#pragma once
#include "cyui/modules/DocumentViews.h"
namespace cyui {
enum class DocumentEncoding { utf8, utf16LE, utf16BE };
struct DocumentPackage {
    std::string markdown, previews = "{}";
    std::vector<DocumentAsset> assets;
    DocumentEncoding encoding = DocumentEncoding::utf8;
    bool bom = false;
    std::string manifest; // preserved archive metadata; empty selects the version-1 default
};
struct DocumentIOLimits {
    std::size_t archiveBytes = 64 * 1024 * 1024, expandedBytes = 128 * 1024 * 1024,
                entryBytes = 32 * 1024 * 1024, entries = 4096;
};
// Shared bounded ZIP container for optional document formats. Stored writer,
// stored/DEFLATE reader; no extraction, encryption, ZIP64 or external resolution.
CYUI_API std::vector<DocumentAsset> readDocumentArchive(ArrayView<unsigned char>, const DocumentIOLimits & = {});
CYUI_API std::vector<unsigned char> writeDocumentArchive(const std::vector<DocumentAsset> &, const DocumentIOLimits & = {});
// Memory-only APIs. No filesystem, network, browser, or OS clipboard dependency.
// Throw std::invalid_argument/length_error on malformed input or exceeded budgets.
CYUI_API DocumentPackage decodeMarkdown(ArrayView<unsigned char> bytes);
CYUI_API std::vector<unsigned char> encodeMarkdown(const DocumentPackage &);
CYUI_API DocumentPackage readCyMD(ArrayView<unsigned char> bytes, const DocumentIOLimits & = {});
CYUI_API std::vector<unsigned char> writeCyMD(const DocumentPackage &, const DocumentIOLimits & = {});
// Self-contained HTML with referenced package assets embedded as data URLs.
CYUI_API std::string documentPackageHtml(const DocumentPackage &, const std::string &title = "Document");
// Useful for validating host-managed asset paths before resolving/loading them.
CYUI_API bool documentAssetPath(const std::string &);
// Clipboard HTML -> Markdown. Scripts/styles are discarded; remote images are
// converted only by an explicit resource importer (empty result omits the image).
CYUI_API std::string
markdownFromHtml(const std::string &html,
                 const std::function<std::string(const std::string &)> &importImage = {});
} // namespace cyui

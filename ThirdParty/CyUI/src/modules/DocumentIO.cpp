#include "cyui/modules/DocumentIO.h"
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4668) // miniz tests optional configuration macros with #if.
#endif
#include "../../third_party/miniz/miniz_tinfl.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include <cctype>
#include <cstring>
#include <regex>
#include <stdexcept>
namespace cyui {
namespace detail {
std::string decodeDocumentEntity(const std::string &);
}
namespace {
using Bytes = std::vector<unsigned char>;
[[noreturn]] void malformed() {
    throw std::invalid_argument("Malformed or unsupported document archive");
}
void utf8(std::string &s, unsigned cp) {
    if (cp < 128)
        s += char(cp);
    else if (cp < 2048) {
        s += char(0xc0 | (cp >> 6));
        s += char(0x80 | (cp & 63));
    } else if (cp < 65536) {
        s += char(0xe0 | (cp >> 12));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    } else {
        s += char(0xf0 | (cp >> 18));
        s += char(0x80 | ((cp >> 12) & 63));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    }
}
unsigned readChar(const std::string &s, std::size_t &at) {
    const unsigned char a = static_cast<unsigned char>(s[at++]);
    if (a < 128)
        return a;
    const unsigned n = a >= 0xf0 ? 3 : a >= 0xe0 ? 2 : a >= 0xc2 ? 1 : 0;
    if (!n || at + n > s.size())
        malformed();
    unsigned cp = a & ((1u << (6 - n)) - 1);
    for (unsigned i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[at++]);
        if ((c & 0xc0) != 0x80)
            malformed();
        cp = (cp << 6) | (c & 63);
    }
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || (n == 1 && cp < 128) || (n == 2 && cp < 2048) ||
        (n == 3 && cp < 65536))
        malformed();
    return cp;
}
unsigned u16(ArrayView<unsigned char> b, std::size_t p) {
    if (p > b.count || b.count - p < 2)
        malformed();
    return b[p] | (unsigned(b[p + 1]) << 8);
}
std::uint32_t u32(ArrayView<unsigned char> b, std::size_t p) {
    return u16(b, p) | (std::uint32_t(u16(b, p + 2)) << 16);
}
void put16(Bytes &b, std::size_t n) {
    b.push_back(static_cast<unsigned char>(n & 255));
    b.push_back(static_cast<unsigned char>((n >> 8) & 255));
}
void put32(Bytes &b, std::uint32_t n) {
    put16(b, n & 65535);
    put16(b, n >> 16);
}
std::uint32_t crc(ArrayView<unsigned char> data) {
    std::uint32_t value = ~0u;
    for (std::size_t i = 0; i < data.count; ++i) {
        value ^= data[i];
        for (int n = 0; n < 8; ++n)
            value = (value >> 1) ^ (0xedb88320u & std::uint32_t(0 - int(value & 1)));
    }
    return ~value;
}
std::string lower(std::string value) {
    for (auto &c : value)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
void checkSize(std::size_t value, std::size_t max) {
    if (value > max)
        throw std::length_error("Document resource budget exceeded");
}
std::string attr(const std::string &tag, const std::string &key) {
    const std::regex expression("(?:^|\\s)" + key + R"attr(\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))attr",
                                std::regex::icase);
    std::smatch m;
    if (!std::regex_search(tag, m, expression))
        return {};
    return m[1].matched ? m[1].str() : m[2].matched ? m[2].str() : m[3].str();
}
} // namespace
bool documentAssetPath(const std::string &path) {
    if (path.empty() || path.size() > 4096 || path[0] == '/' ||
        path.find_first_of("\\:\0", 0, 3) != std::string::npos)
        return false;
    std::size_t at = 0;
    while (at < path.size()) {
        auto end = path.find('/', at);
        if (end == std::string::npos)
            end = path.size();
        const auto piece = path.substr(at, end - at);
        if (piece.empty() || piece == "." || piece == ".." || piece.back() == '.' || piece.back() == ' ')
            return false;
        for (unsigned char c : piece)
            if (c < 32 || c == 127)
                return false;
        at = end + 1;
    }
    return true;
}
DocumentPackage decodeMarkdown(ArrayView<unsigned char> bytes) {
    checkSize(bytes.count, 32 * 1024 * 1024);
    DocumentPackage doc;
    std::size_t at = 0;
    if (bytes.count >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) {
        doc.bom = true;
        at = 3;
    } else if (bytes.count >= 2 &&
               ((bytes[0] == 0xff && bytes[1] == 0xfe) || (bytes[0] == 0xfe && bytes[1] == 0xff))) {
        doc.bom = true;
        doc.encoding = bytes[0] == 0xff ? DocumentEncoding::utf16LE : DocumentEncoding::utf16BE;
        at = 2;
    }
    if (doc.encoding == DocumentEncoding::utf8) {
        if (bytes.count > at)
            doc.markdown.assign(reinterpret_cast<const char *>(bytes.data + at), bytes.count - at);
        std::size_t offset = 0;
        while (offset < doc.markdown.size())
            readChar(doc.markdown, offset);
    } else {
        if ((bytes.count - at) % 2)
            malformed();
        auto word = [&](std::size_t p) {
            return doc.encoding == DocumentEncoding::utf16LE ? u16(bytes, p)
                                                             : unsigned(bytes[p]) * 256 + bytes[p + 1];
        };
        while (at < bytes.count) {
            unsigned cp = word(at);
            at += 2;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (at == bytes.count)
                    malformed();
                const auto lo = word(at);
                at += 2;
                if (lo < 0xdc00 || lo > 0xdfff)
                    malformed();
                cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
            } else if (cp >= 0xdc00 && cp <= 0xdfff)
                malformed();
            utf8(doc.markdown, cp);
        }
    }
    return doc;
}
std::vector<unsigned char> encodeMarkdown(const DocumentPackage &doc) {
    checkSize(doc.markdown.size(), 32 * 1024 * 1024);
    Bytes out;
    if (doc.encoding == DocumentEncoding::utf8) {
        if (doc.bom)
            out = {0xef, 0xbb, 0xbf};
        out.insert(out.end(), doc.markdown.begin(), doc.markdown.end());
        return out;
    }
    auto word = [&](unsigned value) {
        if (doc.encoding == DocumentEncoding::utf16LE)
            put16(out, value);
        else {
            out.push_back(static_cast<unsigned char>(value >> 8));
            out.push_back(static_cast<unsigned char>(value & 255));
        }
    };
    if (doc.bom)
        word(0xfeff);
    std::size_t at = 0;
    while (at < doc.markdown.size()) {
        auto cp = readChar(doc.markdown, at);
        if (cp < 0x10000)
            word(cp);
        else {
            cp -= 0x10000;
            word(0xd800 + (cp >> 10));
            word(0xdc00 + (cp & 1023));
        }
    }
    return out;
}
std::vector<DocumentAsset> readDocumentArchive(ArrayView<unsigned char> bytes,
                                               const DocumentIOLimits &limit) {
    checkSize(bytes.count, limit.archiveBytes);
    if (bytes.count < 22)
        malformed();
    std::size_t eocd = bytes.count - 22;
    for (;;) {
        if (u32(bytes, eocd) == 0x06054b50 && eocd + 22 + u16(bytes, eocd + 20) == bytes.count)
            break;
        if (eocd == 0 || bytes.count - eocd > 65557)
            malformed();
        --eocd;
    }
    if (u16(bytes, eocd + 4) || u16(bytes, eocd + 6) || u16(bytes, eocd + 8) != u16(bytes, eocd + 10))
        malformed();
    const auto count = u16(bytes, eocd + 10);
    checkSize(count, limit.entries);
    const auto central = u32(bytes, eocd + 16), centralSize = u32(bytes, eocd + 12);
    if (central > eocd || centralSize > eocd - central || central + centralSize != eocd)
        malformed();
    std::unordered_map<std::string, Bytes> files;
    std::unordered_set<std::string> names;
    std::size_t cursor = central, total = 0;
    for (unsigned entry = 0; entry < count; ++entry) {
        if (u32(bytes, cursor) != 0x02014b50)
            malformed();
        const auto flags = u16(bytes, cursor + 8), method = u16(bytes, cursor + 10);
        const auto compressed = u32(bytes, cursor + 20), expanded = u32(bytes, cursor + 24),
                   nameSize = u16(bytes, cursor + 28), extra = u16(bytes, cursor + 30),
                   comment = u16(bytes, cursor + 32), local = u32(bytes, cursor + 42);
        const auto mode = u32(bytes, cursor + 38) >> 16;
        if ((mode & 0170000) == 0120000)
            malformed();
        if (flags & 1 || method != 0 && method != 8 || cursor > eocd ||
            eocd - cursor < 46u + nameSize + extra + comment || u16(bytes, cursor + 34))
            malformed();
        std::string name(reinterpret_cast<const char *>(bytes.data + cursor + 46), nameSize);
        cursor += 46u + nameSize + extra + comment;
        if (!name.empty() && name.back() == '/') {
            if (!documentAssetPath(name.substr(0, name.size() - 1)))
                malformed();
            continue;
        }
        if (!documentAssetPath(name) || !names.insert(lower(name)).second)
            malformed();
        checkSize(expanded, limit.entryBytes);
        if (expanded > limit.expandedBytes - total)
            throw std::length_error("Archive expansion budget exceeded");
        total += expanded;
        if (local >= central || u32(bytes, local) != 0x04034b50 || u16(bytes, local + 6) != flags ||
            u16(bytes, local + 8) != method)
            malformed();
        const auto localName = u16(bytes, local + 26), localExtra = u16(bytes, local + 28);
        const std::size_t data = std::size_t(local) + 30 + localName + localExtra;
        if (data > central || compressed > central - data || localName != nameSize ||
            std::memcmp(bytes.data + local + 30, name.data(), nameSize))
            malformed();
        Bytes decoded(expanded);
        if (method == 0) {
            if (compressed != expanded)
                malformed();
            if (expanded)
                std::memcpy(decoded.data(), bytes.data + data, expanded);
        } else {
            unsigned char empty = 0;
            const auto got = tinfl_decompress_mem_to_mem(decoded.empty() ? &empty : decoded.data(),
                                                         std::max<std::size_t>(1, expanded),
                                                         bytes.data + data, compressed, 0);
            if (got != expanded)
                malformed();
        }
        if (crc(decoded) != u32(bytes, cursor - (46u + nameSize + extra + comment) + 16))
            malformed();
        files.emplace(std::move(name), std::move(decoded));
    }
    if (cursor != eocd)
        malformed();
    std::vector<DocumentAsset> entries;
    for (auto &file : files)
        entries.push_back({file.first, std::move(file.second)});
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.path < b.path; });
    return entries;
}
DocumentPackage readCyMD(ArrayView<unsigned char> bytes, const DocumentIOLimits &limit) {
    std::unordered_map<std::string, Bytes> files;
    for (auto &entry : readDocumentArchive(bytes, limit))
        files.emplace(entry.path, std::move(entry.bytes));
    if (!files.count("document.md"))
        malformed();
    if (files.count("mimetype") &&
        std::string(files["mimetype"].begin(), files["mimetype"].end()) != "application/x-cymd")
        malformed();
    auto doc = decodeMarkdown(files["document.md"]);
    if (files.count("previews.json"))
        doc.previews.assign(files["previews.json"].begin(), files["previews.json"].end());
    if (files.count("manifest.json"))
        doc.manifest.assign(files["manifest.json"].begin(), files["manifest.json"].end());
    for (auto &entry : files)
        if (entry.first != "document.md" && entry.first != "manifest.json" && entry.first != "mimetype" &&
            entry.first != "previews.json")
            doc.assets.push_back({entry.first, std::move(entry.second)});
    std::sort(doc.assets.begin(), doc.assets.end(),
              [](const auto &a, const auto &b) { return a.path < b.path; });
    return doc;
}
std::vector<unsigned char> writeCyMD(const DocumentPackage &doc, const DocumentIOLimits &limit) {
    auto bytes = [](const std::string &s) { return Bytes(s.begin(), s.end()); };
    std::vector<DocumentAsset> entries{
        {"mimetype", bytes("application/x-cymd")},
        {"manifest.json",
         bytes(doc.manifest.empty()
                   ? "{\"format\":\"cymd\",\"version\":1,\"main\":\"document.md\",\"generator\":\"CyUI\"}"
                   : doc.manifest)},
        {"document.md", bytes(doc.markdown)},
        {"previews.json", bytes(doc.previews)}};
    entries.insert(entries.end(), doc.assets.begin(), doc.assets.end());
    return writeDocumentArchive(entries, limit);
}
std::vector<unsigned char> writeDocumentArchive(const std::vector<DocumentAsset> &entries,
                                                const DocumentIOLimits &limit) {
    checkSize(entries.size(), std::min<std::size_t>(65534, limit.entries));
    Bytes out, central;
    std::unordered_set<std::string> names;
    std::size_t total = 0;
    for (const auto &entry : entries) {
        if (!documentAssetPath(entry.path) || !names.insert(lower(entry.path)).second)
            malformed();
        checkSize(entry.path.size(), 65535);
        checkSize(entry.bytes.size(), std::min<std::size_t>(limit.entryBytes, 0xffffffffu));
        if (entry.bytes.size() > limit.expandedBytes - total)
            throw std::length_error("Archive expansion budget exceeded");
        total += entry.bytes.size();
        const auto offset = out.size(), size = entry.bytes.size();
        const auto checksum = crc(entry.bytes);
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0x800);
        put16(out, 0);
        put32(out, 0);
        put32(out, checksum);
        put32(out, std::uint32_t(size));
        put32(out, std::uint32_t(size));
        put16(out, entry.path.size());
        put16(out, 0);
        out.insert(out.end(), entry.path.begin(), entry.path.end());
        out.insert(out.end(), entry.bytes.begin(), entry.bytes.end());
        put32(central, 0x02014b50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0x800);
        put16(central, 0);
        put32(central, 0);
        put32(central, checksum);
        put32(central, std::uint32_t(size));
        put32(central, std::uint32_t(size));
        put16(central, entry.path.size());
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, std::uint32_t(offset));
        central.insert(central.end(), entry.path.begin(), entry.path.end());
        checkSize(out.size() + central.size() + 22, limit.archiveBytes);
    }
    const auto offset = out.size();
    out.insert(out.end(), central.begin(), central.end());
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, entries.size());
    put16(out, entries.size());
    put32(out, std::uint32_t(central.size()));
    put32(out, std::uint32_t(offset));
    put16(out, 0);
    return out;
}
std::string documentPackageHtml(const DocumentPackage &package, const std::string &title) {
    Document doc;
    doc.setMarkdown(package.markdown);
    std::string output = documentHtml(doc, title);
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (const auto &asset : package.assets) {
        if (!documentAssetPath(asset.path))
            malformed();
        checkSize(asset.bytes.size(), 32 * 1024 * 1024);
        std::string escapedPath;
        for (char c : asset.path) {
            if (c == '&')
                escapedPath += "&amp;";
            else if (c == '\"')
                escapedPath += "&quot;";
            else if (c == '<')
                escapedPath += "&lt;";
            else if (c == '>')
                escapedPath += "&gt;";
            else
                escapedPath += c;
        }
        const auto quoted = "\"" + escapedPath + "\"";
        if (output.find(quoted) == std::string::npos)
            continue;
        const auto dot = asset.path.rfind('.');
        const auto ext = dot == std::string::npos ? "" : lower(asset.path.substr(dot));
        static const std::unordered_map<std::string, std::string> types{
            {".png", "image/png"},   {".jpg", "image/jpeg"},      {".jpeg", "image/jpeg"},
            {".webp", "image/webp"}, {".svg", "image/svg+xml"},   {".gif", "image/gif"},
            {".bmp", "image/bmp"},   {".avif", "image/avif"},     {".mp4", "video/mp4"},
            {".webm", "video/webm"}, {".mov", "video/quicktime"}, {".mp3", "audio/mpeg"},
            {".wav", "audio/wav"},   {".ogg", "audio/ogg"},       {".flac", "audio/flac"}};
        const auto found = types.find(ext);
        std::string encoded =
            "\"data:" + (found == types.end() ? std::string("application/octet-stream") : found->second) +
            ";base64,";
        for (std::size_t i = 0; i < asset.bytes.size(); i += 3) {
            const unsigned a = asset.bytes[i], b = i + 1 < asset.bytes.size() ? asset.bytes[i + 1] : 0,
                           c = i + 2 < asset.bytes.size() ? asset.bytes[i + 2] : 0;
            const auto n = (std::uint32_t(a) << 16) | (std::uint32_t(b) << 8) | std::uint32_t(c);
            encoded += alphabet[(n >> 18) & 63];
            encoded += alphabet[(n >> 12) & 63];
            encoded += i + 1 < asset.bytes.size() ? alphabet[(n >> 6) & 63] : '=';
            encoded += i + 2 < asset.bytes.size() ? alphabet[n & 63] : '=';
        }
        encoded += '\"';
        std::size_t at = 0;
        while ((at = output.find(quoted, at)) != std::string::npos) {
            output.replace(at, quoted.size(), encoded);
            at += encoded.size();
            checkSize(output.size(), 256 * 1024 * 1024);
        }
    }
    return output;
}
std::string markdownFromHtml(const std::string &html,
                             const std::function<std::string(const std::string &)> &importImage) {
    checkSize(html.size(), 32 * 1024 * 1024);
    std::string out;
    std::vector<std::string> links;
    int list = 0;
    bool pre = false;
    bool tableHeader = false, tableCell = false;
    std::size_t tableColumns = 0;
    std::string skip;
    auto newline = [&] {
        if (!out.empty() && out.back() != '\n')
            out += '\n';
    };
    for (std::size_t at = 0; at < html.size();) {
        if (html[at] != '<') {
            const auto end = html.find('<', at);
            if (skip.empty()) {
                const auto raw = html.substr(at, end == std::string::npos ? end : end - at);
                std::string text;
                for (std::size_t offset = 0; offset < raw.size();) {
                    const auto semi = raw[offset] == '&' ? raw.find(';', offset) : std::string::npos;
                    if (semi != std::string::npos && semi - offset < 50) {
                        text += detail::decodeDocumentEntity(raw.substr(offset, semi - offset + 1));
                        offset = semi + 1;
                    } else
                        text += raw[offset++];
                }
                for (char c : text) {
                    if (!pre && std::strchr("\\`*_{}[]<>#~|", c))
                        out += '\\';
                    out += tableCell && (c == '\r' || c == '\n') ? ' ' : c;
                }
            }
            at = end == std::string::npos ? html.size() : end;
            continue;
        }
        if (html.compare(at, 4, "<!--") == 0) {
            auto end = html.find("-->", at + 4);
            at = end == std::string::npos ? html.size() : end + 3;
            continue;
        }
        auto end = at + 1;
        char quote = 0;
        for (; end < html.size(); ++end) {
            const char c = html[end];
            if (quote) {
                if (c == quote)
                    quote = 0;
            } else if (c == '\'' || c == '"')
                quote = c;
            else if (c == '>')
                break;
        }
        if (end == html.size())
            break;
        const auto tag = html.substr(at + 1, end - at - 1);
        at = end + 1;
        const bool close = !tag.empty() && tag[0] == '/';
        auto name = lower(tag.substr(close ? 1 : 0));
        name = name.substr(0, name.find_first_of(" \t\r\n/"));
        if (!skip.empty()) {
            if (close && name == skip)
                skip.clear();
            continue;
        }
        if (name == "script" || name == "style" || name == "head" || name == "iframe" || name == "object") {
            if (!close)
                skip = name;
            continue;
        }
        if (name == "br")
            out += '\n';
        else if (name == "p" || name == "div" || name == "section") {
            if (tableCell)
                out += ' ';
            else {
                newline();
                if (close)
                    out += '\n';
            }
        } else if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
            newline();
            if (!close)
                out += std::string(std::size_t(name[1] - '0'), '#') + " ";
            else
                out += '\n';
        } else if (name == "strong" || name == "b")
            out += "**";
        else if (name == "em" || name == "i")
            out += '*';
        else if (name == "s" || name == "del" || name == "strike")
            out += "~~";
        else if (name == "u" || name == "mark" || name == "kbd")
            out += "<" + std::string(close ? "/" : "") + name + ">";
        else if (name == "pre") {
            newline();
            out += "```\n";
            pre = !close;
        } else if (name == "code" && !pre)
            out += '`';
        else if (name == "blockquote") {
            newline();
            if (!close)
                out += "> ";
        } else if (name == "ul" || name == "ol") {
            newline();
            list += close ? -1 : 1;
            list = std::clamp(list, 0, 128);
        } else if (name == "li") {
            newline();
            if (!close)
                out += std::string(std::size_t(std::max(0, list - 1)) * 2, ' ') + "- ";
        } else if (name == "a") {
            if (!close) {
                links.push_back(attr(tag, "href"));
                out += '[';
            } else if (!links.empty()) {
                out += "](<" + links.back() + ">)";
                links.pop_back();
            }
        } else if (name == "img" && !close) {
            auto source = attr(tag, "src");
            if (importImage)
                source = importImage(source);
            else
                source.clear();
            if (!source.empty())
                out += "![" + attr(tag, "alt") + "](<" + source + ">)";
        } else if (name == "table") {
            newline();
            tableHeader = tableCell = false;
        } else if (name == "tr") {
            if (!close) {
                newline();
                tableColumns = 0;
                out += '|';
            } else {
                newline();
                if (!tableHeader && tableColumns) {
                    out += '|';
                    for (std::size_t column = 0; column < tableColumns; ++column)
                        out += " --- |";
                    out += '\n';
                    tableHeader = true;
                }
            }
        } else if (name == "td" || name == "th") {
            tableCell = !close;
            if (close)
                out += " |";
            else
                ++tableColumns;
        } else if (name == "input" && !close && lower(attr(tag, "type")) == "checkbox")
            out += lower(tag).find("checked") != std::string::npos ? "[x] " : "[ ] ";
    }
    return out;
}
} // namespace cyui

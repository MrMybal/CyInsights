#include "cyui/modules/OfficeIO.h"
#include "../../third_party/pugixml/pugixml.hpp"
#include <sstream>
#include <unordered_map>
#include <set>
#include <regex>
#include <charconv>
#include <iomanip>

namespace cyui {
namespace {
using Node = pugi::xml_node;
using Bytes = std::vector<unsigned char>;
using Files = std::unordered_map<std::string, Bytes>;
constexpr const char *W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
constexpr const char *R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
constexpr const char *S = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
constexpr const char *O = "urn:oasis:names:tc:opendocument:xmlns:office:1.0";
std::string local(const char *s) {
    std::string value = s ? s : "";
    auto at = value.find(':');
    return at == std::string::npos ? value : value.substr(at + 1);
}
Node child(Node n, const std::string &name) {
    for (auto c : n.children())
        if (local(c.name()) == name)
            return c;
    return {};
}
std::string attr(Node n, const std::string &name, const std::string &fallback = "") {
    for (auto a : n.attributes())
        if (local(a.name()) == name)
            return a.value();
    return fallback;
}
Node descendant(Node n, const std::string &name) {
    for (auto c : n.children()) {
        if (local(c.name()) == name)
            return c;
        auto found = descendant(c, name);
        if (found)
            return found;
    }
    return {};
}
void issue(std::vector<OfficeIssue> &out, const OfficeOptions &o, const std::string &part,
           const std::string &feature, const std::string &message) {
    if (o.strict)
        throw std::invalid_argument(feature + ": " + message);
    if (std::none_of(out.begin(), out.end(),
                     [&](const auto &i) { return i.part == part && i.feature == feature; }))
        out.push_back({part, feature, message});
}
void validateXML(pugi::xml_document &doc, const Bytes &bytes, const OfficeOptions &o) {
    if (bytes.size() > o.limits.entryBytes)
        throw std::length_error("Office XML byte limit");
    auto result = doc.load_buffer(bytes.data(), bytes.size(),
                                  pugi::parse_default | pugi::parse_ws_pcdata | pugi::parse_doctype);
    if (!result)
        throw std::invalid_argument(std::string("Invalid Office XML: ") + result.description());
    std::vector<std::pair<Node, unsigned>> stack{{doc, 0}};
    std::size_t count = 0;
    while (!stack.empty()) {
        auto item = stack.back();
        stack.pop_back();
        if (++count > 1000000 || item.second > 128)
            throw std::length_error("Office XML node/depth limit");
        if (item.first.type() == pugi::node_doctype)
            throw std::invalid_argument("Office XML DTD is unsupported");
        for (auto c : item.first.children())
            stack.push_back({c, item.second + 1});
    }
}
Bytes encoded(const std::string &s) {
    return Bytes(s.begin(), s.end());
}
Bytes xmlBytes(const pugi::xml_document &doc) {
    std::ostringstream out;
    doc.save(out, "", pugi::format_raw, pugi::encoding_utf8);
    return encoded(out.str());
}
void set(Node n, const char *key, const std::string &value) {
    auto a = n.attribute(key);
    if (!a)
        a = n.append_attribute(key);
    a.set_value(value.c_str());
}
void set(Node n, const char *key, double value) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(12) << value;
    set(n, key, s.str());
}
Node add(Node n, const char *name, const std::string &value = "") {
    auto c = n.append_child(name);
    if (!value.empty())
        c.text().set(value.c_str());
    return c;
}
double decimal(const std::string &s, double fallback = 0) {
    if (s.empty())
        return fallback;
    const char *begin = s.data();
    const char *end = begin + s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;
    if (begin < end && *begin == '+')
        ++begin;
    double n = 0;
    auto parsed = std::from_chars(begin, end, n);
    return parsed.ec == std::errc{} && std::isfinite(n) ? n : fallback;
}
std::size_t integer(const std::string &s, std::size_t limit, std::size_t fallback = 1) {
    if (s.empty())
        return fallback;
    std::size_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9' || n > (limit - std::size_t(c - '0')) / 10)
            throw std::length_error("Office repetition/index limit");
        n = n * 10 + std::size_t(c - '0');
    }
    if (n > limit)
        throw std::length_error("Office repetition/index limit");
    return n;
}
float length(const std::string &s, float fallback = 0) {
    float n = float(decimal(s, fallback));
    if (s.find("cm") != std::string::npos)
        n *= 10;
    else if (s.find("in") != std::string::npos)
        n *= 25.4f;
    else if (s.find("pt") != std::string::npos)
        n *= 25.4f / 72;
    return n;
}
std::string mm(float n) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << n << "mm";
    return out.str();
}
std::string pt(float n) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << n << "pt";
    return out.str();
}
bool wordField(const std::string &instruction, const std::string &expected) {
    auto first = instruction.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    auto end = instruction.find_first_of(" \t\r\n\\", first);
    auto field = instruction.substr(first, end - first);
    for (auto &c : field)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return field == expected;
}
std::string textContent(Node n) {
    auto name = local(n.name());
    if (n.type() == pugi::node_pcdata || n.type() == pugi::node_cdata)
        return n.value();
    if (name == "tab")
        return "\t";
    if (name == "line-break" || name == "br")
        return "\n";
    if (name == "s")
        return std::string(integer(attr(n, "c"), 100000), ' ');
    if (name == "fldSimple") {
        auto instr = attr(n, "instr");
        if (wordField(instr, "NUMPAGES"))
            return "{pages}";
        if (wordField(instr, "PAGE"))
            return "{page}";
    }
    if (name == "page-number")
        return "{page}";
    if (name == "page-count")
        return "{pages}";
    std::string out;
    for (auto c : n.children())
        out += textContent(c);
    return out;
}
Files loadFiles(ArrayView<unsigned char> data, OfficeFormat format, const OfficeOptions &o) {
    Files files;
    if (!data.count || !data.data)
        throw std::invalid_argument("Empty Office input");
    if (format == OfficeFormat::fodt || format == OfficeFormat::fods) {
        if (data.count > o.limits.entryBytes)
            throw std::length_error("Flat ODF byte limit");
        files["content.xml"] = Bytes(data.data, data.data + data.count);
    } else
        for (auto &a : readDocumentArchive(data, o.limits))
            files.emplace(a.path, std::move(a.bytes));
    if (files.empty())
        throw std::invalid_argument("Empty Office package");
    return files;
}
const Bytes &part(const Files &files, const std::string &name) {
    auto i = files.find(name);
    if (i == files.end())
        throw std::invalid_argument("Missing Office part: " + name);
    return i->second;
}
std::string resolve(const std::string &base, const std::string &target) {
    if (target.empty() || target.find('\\') != std::string::npos || target.find(':') != std::string::npos)
        return {};
    std::vector<std::string> parts;
    std::string path =
        target[0] == '/'
            ? target.substr(1)
            : base.substr(0, base.rfind('/') == std::string::npos ? 0 : base.rfind('/') + 1) + target;
    std::istringstream stream(path);
    std::string item;
    while (std::getline(stream, item, '/')) {
        if (item == "..") {
            if (parts.empty())
                return {};
            parts.pop_back();
        } else if (item != "." && !item.empty())
            parts.push_back(item);
    }
    std::string out;
    for (auto &p : parts) {
        if (!out.empty())
            out += '/';
        out += p;
    }
    return documentAssetPath(out) ? out : "";
}
struct Rel {
    std::string target, type;
    bool external = false;
};
std::unordered_map<std::string, Rel> relationships(const Files &files, const std::string &name,
                                                   const OfficeOptions &o) {
    std::unordered_map<std::string, Rel> out;
    auto i = files.find(name);
    if (i == files.end())
        return out;
    pugi::xml_document xml;
    validateXML(xml, i->second, o);
    for (auto n : xml.document_element().children())
        out[attr(n, "Id")] = {attr(n, "Target"), attr(n, "Type"), attr(n, "TargetMode") == "External"};
    return out;
}
std::string officeMain(const Files &files, bool sheet, const OfficeOptions &o) {
    auto rels = relationships(files, "_rels/.rels", o);
    for (const auto &r : rels)
        if (r.second.type.size() >= 15 &&
            r.second.type.compare(r.second.type.size() - 15, 15, "/officeDocument") == 0 &&
            !r.second.external) {
            auto target = resolve("", r.second.target);
            if (!target.empty())
                return target;
        }
    const std::string fallback = sheet ? "xl/workbook.xml" : "word/document.xml";
    if (files.count(fallback))
        return fallback;
    throw std::invalid_argument("Missing Office document relationship");
}
std::string relPart(const std::string &main) {
    auto slash = main.rfind('/');
    return (slash == std::string::npos ? "" : main.substr(0, slash + 1)) + "_rels/" +
           main.substr(slash == std::string::npos ? 0 : slash + 1) + ".rels";
}
std::vector<Node> odfTableRows(Node table) {
    std::vector<Node> rows;
    std::function<void(Node)> collect = [&](Node parent) {
        for (auto n : parent.children()) {
            auto name = local(n.name());
            if (name == "table-row")
                rows.push_back(n);
            else if (name == "table-header-rows" || name == "table-row-group" || name == "table-rows")
                collect(n);
        }
    };
    collect(table);
    return rows;
}
void namespaces(Node root, bool odf) {
    if (odf) {
        set(root, "xmlns:office", O);
        set(root, "office:version", "1.3");
        set(root, "xmlns:text", "urn:oasis:names:tc:opendocument:xmlns:text:1.0");
        set(root, "xmlns:table", "urn:oasis:names:tc:opendocument:xmlns:table:1.0");
        set(root, "xmlns:style", "urn:oasis:names:tc:opendocument:xmlns:style:1.0");
        set(root, "xmlns:fo", "urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0");
        set(root, "xmlns:svg", "urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0");
        set(root, "xmlns:draw", "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0");
        set(root, "xmlns:xlink", "http://www.w3.org/1999/xlink");
        set(root, "xmlns:of", "urn:oasis:names:tc:opendocument:xmlns:of:1.2");
        set(root, "xmlns:number", "urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0");
    } else {
        set(root, "xmlns:w", W);
        set(root, "xmlns:r", R);
    }
}
void relationship(Node root, const std::string &id, const std::string &type, const std::string &target,
                  bool external = false) {
    auto n = add(root, "Relationship");
    set(n, "Id", id);
    set(n, "Type", std::string(R) + "/" + type);
    set(n, "Target", target);
    if (external)
        set(n, "TargetMode", "External");
}
void contentType(Node root, const std::string &path, const std::string &type) {
    auto n = add(root, "Override");
    set(n, "PartName", "/" + path);
    set(n, "ContentType", type);
}
void packageRoots(std::vector<DocumentAsset> &files, const std::string &main, const std::string &type,
                  pugi::xml_document &types) {
    auto root = add(types, "Types");
    set(root, "xmlns", "http://schemas.openxmlformats.org/package/2006/content-types");
    auto rel = add(root, "Default");
    set(rel, "Extension", "rels");
    set(rel, "ContentType", "application/vnd.openxmlformats-package.relationships+xml");
    auto xml = add(root, "Default");
    set(xml, "Extension", "xml");
    set(xml, "ContentType", "application/xml");
    contentType(root, main, type);
    pugi::xml_document rels;
    auto relroot = add(rels, "Relationships");
    set(relroot, "xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
    relationship(relroot, "rId1", "officeDocument", main);
    files.push_back({"_rels/.rels", xmlBytes(rels)});
}
std::string imageType(const std::string &path) {
    auto dot = path.rfind('.');
    auto ext = dot == std::string::npos ? "" : path.substr(dot + 1);
    for (auto &c : ext)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == "png")
        return "image/png";
    if (ext == "jpg" || ext == "jpeg")
        return "image/jpeg";
    if (ext == "gif")
        return "image/gif";
    if (ext == "svg")
        return "image/svg+xml";
    if (ext == "webp")
        return "image/webp";
    return "application/octet-stream";
}
void checkParts(const Files &files, std::vector<OfficeIssue> &issues, const OfficeOptions &o) {
    for (const auto &f : files) {
        auto n = f.first;
        if (n.find("vbaProject") != std::string::npos || n.find("scripts/") != std::string::npos)
            issue(issues, o, n, "macros", "Macros are not executed or exported.");
        if (n.find("embeddings/") != std::string::npos)
            issue(issues, o, n, "embedded-object", "Embedded application objects are not exported.");
        if (n.find("comments") != std::string::npos || n.find("footnotes") != std::string::npos ||
            n.find("endnotes") != std::string::npos || n.find("charts/") != std::string::npos ||
            n.find("pivotTables/") != std::string::npos || n.find("externalLinks/") != std::string::npos)
            issue(issues, o, n, "unsupported-part",
                  "This part has no native editable representation and is omitted on export.");
    }
}
unsigned runStyle(Node properties, unsigned base = 0) {
    for (auto n : properties.children()) {
        auto name = local(n.name());
        bool on = attr(n, "val") != "0" && attr(n, "val") != "false" && attr(n, "val") != "none";
        if (name == "b")
            base = on ? base | documentBold : base & ~documentBold;
        if (name == "i")
            base = on ? base | documentItalic : base & ~documentItalic;
        if (name == "u")
            base = on ? base | documentUnderline : base & ~documentUnderline;
        if (name == "strike")
            base = on ? base | documentStrike : base & ~documentStrike;
        if (name == "highlight")
            base |= documentHighlight;
    }
    return base;
}
std::string wordPlain(Node root) {
    struct Field {
        std::string instruction;
        bool skip = false;
    };
    std::vector<Field> fields;
    std::string out;
    auto emit = [&](Field &f) {
        if (wordField(f.instruction, "NUMPAGES")) {
            out += "{pages}";
            f.skip = true;
        } else if (wordField(f.instruction, "PAGE")) {
            out += "{page}";
            f.skip = true;
        }
    };
    std::function<void(Node)> walk = [&](Node n) {
        auto name = local(n.name());
        if (name == "fldSimple") {
            auto instruction = attr(n, "instr");
            if (wordField(instruction, "NUMPAGES")) {
                out += "{pages}";
                return;
            }
            if (wordField(instruction, "PAGE")) {
                out += "{page}";
                return;
            }
        }
        if (name == "fldChar") {
            auto type = attr(n, "fldCharType");
            if (type == "begin")
                fields.push_back({});
            else if (type == "separate" && !fields.empty())
                emit(fields.back());
            else if (type == "end" && !fields.empty())
                fields.pop_back();
            return;
        }
        if (name == "instrText") {
            if (!fields.empty())
                fields.back().instruction += textContent(n);
            return;
        }
        if (name == "t") {
            if (fields.empty() || !fields.back().skip)
                out += textContent(n);
            return;
        }
        if (name == "tab")
            out += '\t';
        if (name == "br")
            out += '\n';
        if (name == "p" && !out.empty() && out.back() != '\n')
            out += '\n';
        for (auto c : n.children())
            if (c.type() == pugi::node_element)
                walk(c);
    };
    walk(root);
    return out;
}
void wordRuns(Node n, std::vector<DocumentInline> &runs, const std::unordered_map<std::string, Rel> &rels,
              unsigned style = 0, std::string link = "") {
    const auto name = local(n.name());
    if (name == "r")
        style = runStyle(child(n, "rPr"), style);
    if (name == "hyperlink") {
        auto i = rels.find(attr(n, "id"));
        if (i != rels.end())
            link = i->second.target;
    }
    if (name == "t" || name == "tab" || name == "br") {
        auto value = name == "t" ? textContent(n) : name == "tab" ? "\t" : "\n";
        runs.push_back(
            {link.empty() ? DocumentInlineKind::text : DocumentInlineKind::link, value, link, false, style});
        return;
    }
    if (name == "instrText" || name == "del" || name == "drawing" || name == "pPr")
        return;
    for (auto c : n.children())
        if (c.type() == pugi::node_element)
            wordRuns(c, runs, rels, style, link);
}
void wordText(Node parent, const std::string &value) {
    std::size_t start = 0;
    for (std::size_t at = 0; at <= value.size(); ++at) {
        if (at == value.size() || value[at] == '\n' || value[at] == '\t') {
            if (at > start) {
                auto t = add(parent, "w:t", value.substr(start, at - start));
                set(t, "xml:space", "preserve");
            }
            if (at < value.size())
                add(parent, value[at] == '\n' ? "w:br" : "w:tab");
            start = at + 1;
        }
    }
}
void wordRun(Node p, const DocumentInline &run, Node rels, std::size_t &id) {
    Node parent = p;
    if (!run.target.empty()) {
        parent = add(p, "w:hyperlink");
        const auto rid = "rIdLink" + std::to_string(++id);
        set(parent, "r:id", rid);
        relationship(rels, rid, "hyperlink", run.target, true);
    }
    auto r = add(parent, "w:r");
    if (run.style) {
        auto pr = add(r, "w:rPr");
        if (run.style & documentBold)
            add(pr, "w:b");
        if (run.style & documentItalic)
            add(pr, "w:i");
        if (run.style & documentStrike)
            add(pr, "w:strike");
        if (run.style & documentHighlight)
            set(add(pr, "w:highlight"), "w:val", "yellow");
        if (run.style & documentUnderline)
            set(add(pr, "w:u"), "w:val", "single");
    }
    wordText(r, run.text);
}
void wordFields(Node p, const std::string &text) {
    std::size_t at = 0;
    while (at < text.size()) {
        auto page = text.find("{page}", at), pages = text.find("{pages}", at);
        auto found = std::min(page, pages);
        if (found == std::string::npos) {
            wordText(add(p, "w:r"), text.substr(at));
            break;
        }
        wordText(add(p, "w:r"), text.substr(at, found - at));
        auto field = add(p, "w:fldSimple");
        set(field, "w:instr", found == page ? "PAGE" : "NUMPAGES");
        wordText(add(field, "w:r"), "1");
        at = found + (found == page ? 6 : 7);
    }
}
void odfFields(Node p, const std::string &text) {
    std::size_t at = 0;
    while (at < text.size()) {
        auto page = text.find("{page}", at), pages = text.find("{pages}", at);
        auto found = std::min(page, pages);
        if (found == std::string::npos) {
            p.append_child(pugi::node_pcdata).set_value(text.substr(at).c_str());
            break;
        }
        p.append_child(pugi::node_pcdata).set_value(text.substr(at, found - at).c_str());
        auto field = add(p, found == page ? "text:page-number" : "text:page-count", "1");
        if (found == page)
            set(field, "text:select-page", "current");
        at = found + (found == page ? 6 : 7);
    }
}
struct OdfStyle {
    unsigned bits = 0;
    RichParagraphStyle paragraph;
    bool hasSize = false, breakBefore = false;
    std::string parent;
    std::string numberStyle;
    int decimals = -1;
};
using OdfStyles = std::unordered_map<std::string, OdfStyle>;
void collectODFStyles(Node root, OdfStyles &styles) {
    for (auto n : root.children()) {
        auto name = local(n.name());
        if (name == "style" || name == "default-style") {
            OdfStyle style;
            style.parent = attr(n, "parent-style-name");
            style.numberStyle = attr(n, "data-style-name");
            auto t = child(n, "text-properties");
            if (attr(t, "font-weight") == "bold")
                style.bits |= documentBold;
            if (attr(t, "font-style") == "italic")
                style.bits |= documentItalic;
            if (!attr(t, "text-underline-style").empty() && attr(t, "text-underline-style") != "none")
                style.bits |= documentUnderline;
            if (!attr(t, "text-line-through-style").empty() && attr(t, "text-line-through-style") != "none")
                style.bits |= documentStrike;
            if (!attr(t, "font-size").empty()) {
                style.paragraph.points = float(decimal(attr(t, "font-size"), 11));
                style.hasSize = true;
            }
            auto p = child(n, "paragraph-properties");
            style.breakBefore = attr(p, "break-before") == "page";
            auto a = attr(p, "text-align");
            style.paragraph.alignment = a == "center"                ? GridAlignment::center
                                        : a == "right" || a == "end" ? GridAlignment::right
                                                                     : GridAlignment::left;
            style.paragraph.before = length(attr(p, "margin-top")) * 72 / 25.4f;
            style.paragraph.after = length(attr(p, "margin-bottom")) * 72 / 25.4f;
            style.paragraph.indent = length(attr(p, "margin-left"));
            if (attr(p, "line-height").find('%') != std::string::npos)
                style.paragraph.lineSpacing = float(decimal(attr(p, "line-height"), 120) / 100);
            styles[attr(n, "name", name == "default-style" ? "__default_" + attr(n, "family") : "")] = style;
        }
        if (name == "number-style") {
            auto precision = attr(child(n, "number"), "decimal-places");
            if (!precision.empty())
                styles["__number_" + attr(n, "name")].decimals = int(integer(precision, 15, 0));
        }
        collectODFStyles(n, styles);
    }
}
OdfStyle resolvedStyle(const OdfStyles &styles, const std::string &name, unsigned depth = 0) {
    if (depth > 32)
        throw std::invalid_argument("Cyclic/deep ODF style hierarchy");
    auto i = styles.find(name);
    if (i == styles.end())
        return {};
    auto value = i->second;
    if (!value.parent.empty()) {
        auto parent = resolvedStyle(styles, value.parent, depth + 1);
        value.bits |= parent.bits;
        if (!value.hasSize)
            value.paragraph.points = parent.paragraph.points;
        if (value.decimals < 0)
            value.decimals = parent.decimals;
    }
    auto number = styles.find("__number_" + value.numberStyle);
    if (number != styles.end())
        value.decimals = number->second.decimals;
    return value;
}
void odfRuns(Node n, std::vector<DocumentInline> &runs, const OdfStyles &styles, unsigned flags = 0,
             std::string link = "") {
    auto name = local(n.name());
    auto style = attr(n, "style-name");
    if (!style.empty())
        flags |= resolvedStyle(styles, style).bits;
    if (name == "a")
        link = attr(n, "href");
    if (n.type() == pugi::node_pcdata || n.type() == pugi::node_cdata || name == "s" || name == "tab" ||
        name == "line-break") {
        runs.push_back({link.empty() ? DocumentInlineKind::text : DocumentInlineKind::link, textContent(n),
                        link, false, flags});
        return;
    }
    if (name == "frame" || name == "image" || name == "annotation")
        return;
    for (auto c : n.children())
        odfRuns(c, runs, styles, flags, link);
}
void odfText(Node parent, const std::string &text) {
    std::size_t at = 0, start = 0;
    while (at < text.size()) {
        char c = text[at];
        if (c == '\n' || c == '\t' || c == ' ') {
            if (at > start)
                parent.append_child(pugi::node_pcdata).set_value(text.substr(start, at - start).c_str());
            if (c == '\n')
                add(parent, "text:line-break");
            else if (c == '\t')
                add(parent, "text:tab");
            else {
                auto end = at;
                while (end < text.size() && text[end] == ' ')
                    ++end;
                auto spaces = add(parent, "text:s");
                if (end - at > 1)
                    set(spaces, "text:c", double(end - at));
                at = end - 1;
            }
            start = at + 1;
        }
        ++at;
    }
    if (start < text.size())
        parent.append_child(pugi::node_pcdata).set_value(text.substr(start).c_str());
}
void odfRun(Node parent, const DocumentInline &run, Node styles, std::size_t &id) {
    if (!run.target.empty()) {
        parent = add(parent, "text:a");
        set(parent, "xlink:type", "simple");
        set(parent, "xlink:href", run.target);
    }
    if (run.style) {
        const auto name = "T" + std::to_string(++id);
        auto style = add(styles, "style:style");
        set(style, "style:name", name);
        set(style, "style:family", "text");
        auto properties = add(style, "style:text-properties");
        if (run.style & documentBold)
            set(properties, "fo:font-weight", "bold");
        if (run.style & documentItalic)
            set(properties, "fo:font-style", "italic");
        if (run.style & documentUnderline)
            set(properties, "style:text-underline-style", "solid");
        if (run.style & documentStrike)
            set(properties, "style:text-line-through-style", "solid");
        if (run.style & documentHighlight)
            set(properties, "fo:background-color", "#ffff00");
        parent = add(parent, "text:span");
        set(parent, "text:style-name", name);
    }
    odfText(parent, run.text);
}
void odfManifest(std::vector<DocumentAsset> &files, const std::string &mimetype) {
    pugi::xml_document manifest;
    auto root = add(manifest, "manifest:manifest");
    set(root, "xmlns:manifest", "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
    set(root, "manifest:version", "1.3");
    auto main = add(root, "manifest:file-entry");
    set(main, "manifest:full-path", "/");
    set(main, "manifest:media-type", mimetype);
    for (const auto &file : files)
        if (file.path != "mimetype") {
            auto entry = add(root, "manifest:file-entry");
            set(entry, "manifest:full-path", file.path);
            set(entry, "manifest:media-type",
                file.path.size() > 4 && file.path.substr(file.path.size() - 4) == ".xml"
                    ? "text/xml"
                    : imageType(file.path));
        }
    files.insert(files.begin(), {"mimetype", encoded(mimetype)});
    files.push_back({"META-INF/manifest.xml", xmlBytes(manifest)});
}
void documentWarnings(const RichDocumentData &d, std::vector<OfficeIssue> &issues, const OfficeOptions &o) {
    for (const auto &b : d.blocks)
        for (const auto &r : b.runs)
            if (r.style & (documentSpoiler | documentKeyboard | documentMono))
                issue(issues, o, "content", "native-text-style",
                      "Spoiler, keyboard and code presentation is exported as visible text with common "
                      "emphasis only.");
}
} // namespace

bool officeFormatFromExtension(const std::string &path, OfficeFormat &format) {
    auto at = path.rfind('.');
    std::string ext = path.substr(at == std::string::npos ? 0 : at + 1);
    for (auto &c : ext)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == "odt" || ext == "ott")
        format = OfficeFormat::odt;
    else if (ext == "docx" || ext == "dotx")
        format = OfficeFormat::docx;
    else if (ext == "ods" || ext == "ots")
        format = OfficeFormat::ods;
    else if (ext == "xlsx" || ext == "xltx")
        format = OfficeFormat::xlsx;
    else if (ext == "fodt")
        format = OfficeFormat::fodt;
    else if (ext == "fods")
        format = OfficeFormat::fods;
    else
        return false;
    return true;
}

OfficeDocument readOfficeDocument(ArrayView<unsigned char> bytes, OfficeFormat format,
                                  const OfficeOptions &o) {
    if (format != OfficeFormat::odt && format != OfficeFormat::docx && format != OfficeFormat::fodt)
        throw std::invalid_argument("Not a text document format");
    auto files = loadFiles(bytes, format, o);
    OfficeDocument result;
    checkParts(files, result.issues, o);
    auto &out = result.document;
    std::size_t documentCells = 0;
    if (format == OfficeFormat::docx) {
        auto main = officeMain(files, false, o);
        pugi::xml_document xml;
        validateXML(xml, part(files, main), o);
        auto body = child(xml.document_element(), "body");
        if (descendant(body, "rFonts") || descendant(body, "color") || descendant(body, "bidi"))
            issue(result.issues, o, main, "typography",
                  "Font families, arbitrary text colors and bidirectional layout use the host font/theme "
                  "instead of original typography.");
        if (!body)
            throw std::invalid_argument("Missing Word document body");
        auto rels = relationships(files, relPart(main), o);
        for (const auto &rel : rels)
            if (rel.second.type.find("/styles") != std::string::npos)
                issue(result.issues, o, main, "named-styles",
                      "Named style definitions are not preserved; direct common run and paragraph formatting "
                      "is imported.");
        for (auto n : body.children()) {
            const auto name = local(n.name());
            if (name == "p") {
                auto drawing = descendant(n, "blip");
                if (drawing) {
                    auto rel = rels.find(attr(drawing, "embed"));
                    if (rel != rels.end() && !rel->second.external) {
                        auto path = resolve(main, rel->second.target);
                        if (!path.empty() && files.count(path)) {
                            RichBlock image;
                            image.kind = RichBlockKind::image;
                            image.image = path;
                            image.caption =
                                attr(descendant(n, "docPr"), "descr", attr(descendant(n, "docPr"), "name"));
                            auto extent = descendant(n, "extent");
                            image.imageWidthMM = float(decimal(attr(extent, "cx"), 3600000) / 36000);
                            image.imageHeightMM = float(decimal(attr(extent, "cy"), 2340000) / 36000);
                            out.assets.push_back({path, files[path]});
                            out.blocks.push_back(std::move(image));
                        }
                    } else
                        issue(result.issues, o, main, "external-image",
                              "An external or missing image is not loaded.");
                }
                RichBlock block;
                auto pr = child(n, "pPr");
                if (child(pr, "pageBreakBefore")) {
                    RichBlock page;
                    page.kind = RichBlockKind::pageBreak;
                    out.blocks.push_back(page);
                }
                const auto style = attr(child(pr, "pStyle"), "val");
                if (child(pr, "outlineLvl")) {
                    block.paragraph.heading = int(integer(attr(child(pr, "outlineLvl"), "val"), 8, 0)) + 1;
                }
                if (style.find("Heading") == 0) {
                    block.paragraph.heading = std::clamp(int(decimal(style.substr(7), 1)), 1, 6);
                    block.paragraph.points = block.paragraph.heading == 1 ? 24.f : 18.f;
                }
                const auto jc = attr(child(pr, "jc"), "val");
                if (jc == "both" || jc == "distribute")
                    issue(result.issues, o, main, "justification",
                          "Justified text is shown left-aligned in the native view.");
                block.paragraph.alignment = jc == "center"  ? GridAlignment::center
                                            : jc == "right" ? GridAlignment::right
                                                            : GridAlignment::left;
                auto spacing = child(pr, "spacing");
                block.paragraph.before = float(decimal(attr(spacing, "before")) / 20);
                block.paragraph.after = float(decimal(attr(spacing, "after"), 120) / 20);
                if (attr(spacing, "lineRule", "auto") == "auto" && !attr(spacing, "line").empty())
                    block.paragraph.lineSpacing = float(decimal(attr(spacing, "line"), 288) / 240);
                block.paragraph.indent = float(decimal(attr(child(pr, "ind"), "left")) / 1440 * 25.4);
                auto rpr = descendant(n, "rPr");
                if (!attr(child(rpr, "sz"), "val").empty())
                    block.paragraph.points = float(decimal(attr(child(rpr, "sz"), "val"), 22) / 2);
                wordRuns(n, block.runs, rels);
                if (!block.runs.empty() || !drawing)
                    out.blocks.push_back(std::move(block));
                if (child(pr, "numPr"))
                    issue(result.issues, o, main, "list-numbering",
                          "List numbering definitions are not recreated; paragraph text is retained.");
                if (descendant(n, "ins") || descendant(n, "del"))
                    issue(result.issues, o, main, "tracked-changes",
                          "Visible current text is imported; revision history is omitted.");
                if (descendant(n, "br") && attr(descendant(n, "br"), "type") == "page") {
                    RichBlock page;
                    page.kind = RichBlockKind::pageBreak;
                    out.blocks.push_back(page);
                }
            } else if (name == "tbl") {
                RichBlock block;
                block.kind = RichBlockKind::table;
                std::vector<std::vector<GridCell>> rows;
                std::size_t columns = 1;
                for (auto tr : n.children())
                    if (local(tr.name()) == "tr") {
                        rows.emplace_back();
                        for (auto tc : tr.children())
                            if (local(tc.name()) == "tc") {
                                GridCell cell;
                                bool first = true;
                                for (auto p : tc.children())
                                    if (local(p.name()) == "p") {
                                        std::vector<DocumentInline> runs;
                                        wordRuns(p, runs, rels);
                                        if (!first)
                                            cell.text += '\n';
                                        for (const auto &r : runs) {
                                            cell.text += r.text;
                                            cell.style |= r.style;
                                        }
                                        first = false;
                                    }
                                if (child(child(tc, "tcPr"), "gridSpan") ||
                                    child(child(tc, "tcPr"), "vMerge"))
                                    issue(result.issues, o, main, "merged-cells",
                                          "Merged cells are flattened into the imported grid.");
                                rows.back().push_back(std::move(cell));
                                if (rows.back().size() > 256)
                                    throw std::length_error("Document table column limit");
                            }
                        columns = std::max(columns, rows.back().size());
                        if (rows.size() > 100000 || rows.size() * columns > 1000000)
                            throw std::length_error("Document table cell limit");
                    }
                if (!rows.empty()) {
                    documentCells += rows.size() * columns;
                    if (documentCells > 1000000)
                        throw std::length_error("Document total cell limit");
                    std::vector<GridCell> cells(rows.size() * columns);
                    for (std::size_t r = 0; r < rows.size(); ++r)
                        std::copy(rows[r].begin(), rows[r].end(), cells.begin() + r * columns);
                    block.table.assign(rows.size(), columns, cells);
                    block.table.clearHistory();
                    out.blocks.push_back(std::move(block));
                }
            } else if (name != "sectPr")
                issue(result.issues, o, main, "document-element",
                      "An unsupported document element is omitted.");
        }
        auto section = child(body, "sectPr");
        auto size = child(section, "pgSz"), margin = child(section, "pgMar");
        if (size) {
            out.page.widthMM = float(decimal(attr(size, "w"), 11906) * 25.4 / 1440);
            out.page.heightMM = float(decimal(attr(size, "h"), 16838) * 25.4 / 1440);
        }
        if (margin) {
            out.page.leftMM = float(decimal(attr(margin, "left"), 1134) * 25.4 / 1440);
            out.page.rightMM = float(decimal(attr(margin, "right"), 1134) * 25.4 / 1440);
            out.page.topMM = float(decimal(attr(margin, "top"), 1134) * 25.4 / 1440);
            out.page.bottomMM = float(decimal(attr(margin, "bottom"), 1134) * 25.4 / 1440);
        }
        for (const auto &type : {"headerReference", "footerReference"}) {
            auto id = attr(child(section, type), "id");
            auto i = rels.find(id);
            if (i != rels.end() && !i->second.external) {
                auto path = resolve(main, i->second.target);
                if (files.count(path)) {
                    pugi::xml_document partXML;
                    validateXML(partXML, files[path], o);
                    auto value = wordPlain(partXML.document_element());
                    if (type == std::string("headerReference"))
                        out.page.header = value;
                    else
                        out.page.footer = value;
                }
            }
        }
    } else {
        pugi::xml_document xml, stylesXML;
        validateXML(xml, part(files, "content.xml"), o);
        OdfStyles styles;
        collectODFStyles(xml, styles);
        if (files.count("styles.xml")) {
            validateXML(stylesXML, files["styles.xml"], o);
            collectODFStyles(stylesXML, styles);
        }
        auto body = descendant(xml, "body");
        auto text = child(body, "text");
        if (!text)
            throw std::invalid_argument("Missing ODF text body");
        auto defaultStyle = resolvedStyle(styles, "__default_paragraph");
        std::function<void(Node)> blocks = [&](Node parent) {
            for (auto n : parent.children()) {
                const auto name = local(n.name());
                if (name == "p" || name == "h") {
                    auto frames = descendant(n, "frame");
                    if (frames) {
                        auto image = child(frames, "image");
                        auto path = resolve("content.xml", attr(image, "href"));
                        if (!path.empty() && files.count(path)) {
                            RichBlock b;
                            b.kind = RichBlockKind::image;
                            b.image = path;
                            b.caption = attr(frames, "name");
                            b.imageWidthMM = length(attr(frames, "width"), 100);
                            b.imageHeightMM = length(attr(frames, "height"), 65);
                            out.assets.push_back({path, files[path]});
                            out.blocks.push_back(std::move(b));
                        } else
                            issue(result.issues, o, "content.xml", "image",
                                  "An external or missing image is not loaded.");
                    }
                    RichBlock b;
                    const auto style = resolvedStyle(styles, attr(n, "style-name"));
                    if (style.breakBefore) {
                        RichBlock page;
                        page.kind = RichBlockKind::pageBreak;
                        out.blocks.push_back(page);
                    }
                    b.paragraph = style.paragraph;
                    if (!style.hasSize)
                        b.paragraph.points = defaultStyle.hasSize ? defaultStyle.paragraph.points : 11;
                    b.paragraph.heading = name == "h" ? int(integer(attr(n, "outline-level"), 6)) : 0;
                    odfRuns(n, b.runs, styles, defaultStyle.bits);
                    if (!b.runs.empty() || !frames)
                        out.blocks.push_back(std::move(b));
                    auto props = styles.find(attr(n, "style-name"));
                    (void)props;
                } else if (name == "table") {
                    RichBlock b;
                    b.kind = RichBlockKind::table;
                    std::vector<std::vector<GridCell>> rows;
                    std::size_t cols = 1;
                    for (auto row : odfTableRows(n)) {
                        std::vector<GridCell> cells;
                        for (auto c : row.children())
                            if (local(c.name()) == "table-cell" || local(c.name()) == "covered-table-cell") {
                                GridCell cell;
                                std::vector<DocumentInline> runs;
                                odfRuns(c, runs, styles);
                                for (const auto &r : runs) {
                                    cell.text += r.text;
                                    cell.style |= r.style;
                                }
                                auto repeat = integer(attr(c, "number-columns-repeated"), 256);
                                if (cells.size() + repeat > 256)
                                    throw std::length_error("ODF text table column limit");
                                cells.insert(cells.end(), repeat, cell);
                                if (attr(c, "number-columns-spanned", "1") != "1" ||
                                    attr(c, "number-rows-spanned", "1") != "1")
                                    issue(result.issues, o, "content.xml", "merged-cells",
                                          "Merged cells are flattened into the imported grid.");
                            }
                        auto repeat = integer(attr(row, "number-rows-repeated"), 100000);
                        if (rows.size() + repeat > 100000)
                            throw std::length_error("ODF text table row limit");
                        cols = std::max(cols, cells.size());
                        if ((rows.size() + repeat) * cols > 1000000)
                            throw std::length_error("ODF text table cell limit");
                        rows.insert(rows.end(), repeat, cells);
                    }
                    if (!rows.empty()) {
                        documentCells += rows.size() * cols;
                        if (documentCells > 1000000)
                            throw std::length_error("Document total cell limit");
                        std::vector<GridCell> cells(rows.size() * cols);
                        for (std::size_t r = 0; r < rows.size(); ++r)
                            std::copy(rows[r].begin(), rows[r].end(), cells.begin() + r * cols);
                        b.table.assign(rows.size(), cols, cells);
                        b.table.clearHistory();
                        out.blocks.push_back(std::move(b));
                    }
                } else if (name == "soft-page-break") {
                    RichBlock b;
                    b.kind = RichBlockKind::pageBreak;
                    out.blocks.push_back(b);
                } else if (name == "list" || name == "list-item" || name == "section") {
                    if (name == "list")
                        issue(result.issues, o, "content.xml", "list-numbering",
                              "List contents are retained as paragraphs without numbering definitions.");
                    blocks(n);
                } else if (n.type() == pugi::node_element && name != "sequence-decls")
                    issue(result.issues, o, "content.xml", "document-element",
                          "An unsupported document element is omitted.");
                if (out.blocks.size() > 10000)
                    throw std::length_error("Rich document block limit");
            }
        };
        blocks(text);
        auto layout =
            descendant(stylesXML.document_element() ? Node(stylesXML) : Node(xml), "page-layout-properties");
        if (layout) {
            out.page.widthMM = length(attr(layout, "page-width"), 210);
            out.page.heightMM = length(attr(layout, "page-height"), 297);
            auto all = length(attr(layout, "margin"), 20);
            out.page.leftMM = length(attr(layout, "margin-left"), all);
            out.page.rightMM = length(attr(layout, "margin-right"), all);
            out.page.topMM = length(attr(layout, "margin-top"), all);
            out.page.bottomMM = length(attr(layout, "margin-bottom"), all);
        }
        auto master = descendant(stylesXML.document_element() ? Node(stylesXML) : Node(xml), "master-page");
        out.page.header = textContent(child(master, "header"));
        out.page.footer = textContent(child(master, "footer"));
    }
    validateRichDocument(out);
    return result;
}

std::vector<unsigned char> writeOfficeDocument(const RichDocumentData &d, OfficeFormat format,
                                               std::vector<OfficeIssue> *reported, const OfficeOptions &o) {
    if (format != OfficeFormat::docx && format != OfficeFormat::odt && format != OfficeFormat::fodt)
        throw std::invalid_argument("Not a text document format");
    validateRichDocument(d);
    std::vector<OfficeIssue> issues;
    documentWarnings(d, issues, o);
    std::vector<DocumentAsset> files;
    std::unordered_map<std::string, const DocumentAsset *> assets;
    for (const auto &a : d.assets) {
        if (!documentAssetPath(a.path))
            throw std::invalid_argument("Invalid document asset");
        assets[a.path] = &a;
    }
    std::size_t serial = 0;
    if (format == OfficeFormat::docx) {
        pugi::xml_document xml, types, rels;
        packageRoots(files, "word/document.xml",
                     "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml",
                     types);
        auto relroot = add(rels, "Relationships");
        set(relroot, "xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        auto root = add(xml, "w:document");
        namespaces(root, false);
        set(root, "xmlns:wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing");
        set(root, "xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        set(root, "xmlns:pic", "http://schemas.openxmlformats.org/drawingml/2006/picture");
        auto body = add(root, "w:body");
        for (const auto &b : d.blocks) {
            if (b.kind == RichBlockKind::table) {
                auto table = add(body, "w:tbl");
                auto pr = add(table, "w:tblPr");
                auto borders = add(pr, "w:tblBorders");
                for (const char *name :
                     {"w:top", "w:left", "w:bottom", "w:right", "w:insideH", "w:insideV"}) {
                    auto border = add(borders, name);
                    set(border, "w:val", "single");
                    set(border, "w:sz", "4");
                    set(border, "w:color", "auto");
                }
                auto grid = add(table, "w:tblGrid");
                double width =
                    (d.page.widthMM - d.page.leftMM - d.page.rightMM) * 1440 / 25.4 / b.table.columns();
                for (std::size_t c = 0; c < b.table.columns(); ++c)
                    set(add(grid, "w:gridCol"), "w:w", width);
                for (std::size_t r = 0; r < b.table.rows(); ++r) {
                    auto row = add(table, "w:tr");
                    for (std::size_t c = 0; c < b.table.columns(); ++c) {
                        auto cell = add(row, "w:tc");
                        auto props = add(cell, "w:tcPr");
                        auto cw = add(props, "w:tcW");
                        set(cw, "w:w", width);
                        set(cw, "w:type", "dxa");
                        const auto &value = b.table.cell(r, c);
                        wordRun(add(cell, "w:p"),
                                {DocumentInlineKind::text, value.text, "", false, value.style}, relroot,
                                serial);
                    }
                }
                continue;
            }
            auto p = add(body, "w:p");
            if (b.kind == RichBlockKind::pageBreak) {
                set(add(add(p, "w:r"), "w:br"), "w:type", "page");
                continue;
            }
            if (b.kind == RichBlockKind::image) {
                auto found = assets.find(b.image);
                if (found == assets.end()) {
                    issue(issues, o, "word/document.xml", "image",
                          "An image has no packaged bytes and is omitted.");
                    continue;
                }
                const auto dot = b.image.rfind('.');
                const auto ext = dot == std::string::npos ? ".bin" : b.image.substr(dot);
                const auto path = "word/media/image" + std::to_string(++serial) + ext;
                files.push_back({path, found->second->bytes});
                contentType(types.document_element(), path, imageType(path));
                const auto rid = "rIdImage" + std::to_string(serial);
                relationship(relroot, rid, "image", "media/" + path.substr(path.rfind('/') + 1));
                auto drawing = add(add(p, "w:r"), "w:drawing"), in = add(drawing, "wp:inline");
                auto extent = add(in, "wp:extent");
                set(extent, "cx", b.imageWidthMM * 36000);
                set(extent, "cy", b.imageHeightMM * 36000);
                auto docPr = add(in, "wp:docPr");
                set(docPr, "id", double(serial));
                set(docPr, "name", b.caption.empty() ? "Image" : b.caption);
                set(docPr, "descr", b.caption);
                auto graphic = add(in, "a:graphic"), data = add(graphic, "a:graphicData");
                set(data, "uri", "http://schemas.openxmlformats.org/drawingml/2006/picture");
                auto pic = add(data, "pic:pic"), nv = add(pic, "pic:nvPicPr");
                auto cNv = add(nv, "pic:cNvPr");
                set(cNv, "id", double(serial));
                set(cNv, "name", b.caption.empty() ? "Image" : b.caption);
                add(nv, "pic:cNvPicPr");
                auto fill = add(pic, "pic:blipFill");
                set(add(fill, "a:blip"), "r:embed", rid);
                add(add(fill, "a:stretch"), "a:fillRect");
                auto sp = add(pic, "pic:spPr"), xf = add(sp, "a:xfrm");
                auto off = add(xf, "a:off");
                set(off, "x", "0");
                set(off, "y", "0");
                auto ex = add(xf, "a:ext");
                set(ex, "cx", b.imageWidthMM * 36000);
                set(ex, "cy", b.imageHeightMM * 36000);
                auto geometry = add(sp, "a:prstGeom");
                set(geometry, "prst", "rect");
                add(geometry, "a:avLst");
                continue;
            }
            auto pr = add(p, "w:pPr");
            auto size = add(add(pr, "w:rPr"), "w:sz");
            set(size, "w:val", b.paragraph.points * 2);
            if (b.paragraph.heading)
                set(add(pr, "w:outlineLvl"), "w:val", double(b.paragraph.heading - 1));
            set(add(pr, "w:jc"), "w:val",
                b.paragraph.alignment == GridAlignment::right    ? "right"
                : b.paragraph.alignment == GridAlignment::center ? "center"
                                                                 : "left");
            auto spacing = add(pr, "w:spacing");
            set(spacing, "w:before", b.paragraph.before * 20);
            set(spacing, "w:after", b.paragraph.after * 20);
            set(spacing, "w:line", b.paragraph.lineSpacing * 240);
            set(spacing, "w:lineRule", "auto");
            set(add(pr, "w:ind"), "w:left", b.paragraph.indent * 1440 / 25.4);
            for (const auto &run : b.runs) {
                wordRun(p, run, relroot, serial);
                auto r = p.last_child();
                if (local(r.name()) == "hyperlink")
                    r = r.last_child();
                auto rpr = child(r, "rPr");
                if (!rpr)
                    rpr = r.prepend_child("w:rPr");
                auto sizeNode = rpr.child("w:highlight")
                                    ? rpr.insert_child_before("w:sz", rpr.child("w:highlight"))
                                : rpr.child("w:u") ? rpr.insert_child_before("w:sz", rpr.child("w:u"))
                                                   : rpr.append_child("w:sz");
                set(sizeNode, "w:val", b.paragraph.points * 2);
            }
        }
        auto section = add(body, "w:sectPr");
        auto size = add(section, "w:pgSz");
        set(size, "w:w", std::round(d.page.widthMM * 1440 / 25.4));
        set(size, "w:h", std::round(d.page.heightMM * 1440 / 25.4));
        if (d.page.widthMM > d.page.heightMM)
            set(size, "w:orient", "landscape");
        auto margins = add(section, "w:pgMar");
        set(margins, "w:top", std::round(d.page.topMM * 1440 / 25.4));
        set(margins, "w:bottom", std::round(d.page.bottomMM * 1440 / 25.4));
        set(margins, "w:left", std::round(d.page.leftMM * 1440 / 25.4));
        set(margins, "w:right", std::round(d.page.rightMM * 1440 / 25.4));
        set(margins, "w:header", "400");
        set(margins, "w:footer", "400");
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{{"footer", d.page.footer},
                                                                                 {"header", d.page.header}}) {
            if (pair.second.empty())
                continue;
            pugi::xml_document hf;
            auto hroot = add(hf, pair.first == "header" ? "w:hdr" : "w:ftr");
            namespaces(hroot, false);
            wordFields(add(hroot, "w:p"), pair.second);
            auto name = "word/" + pair.first + "1.xml";
            files.push_back({name, xmlBytes(hf)});
            const auto rid = "rId" + pair.first;
            relationship(relroot, rid, pair.first, pair.first + "1.xml");
            auto reference =
                section.prepend_child(pair.first == "header" ? "w:headerReference" : "w:footerReference");
            set(reference, "w:type", "default");
            set(reference, "r:id", rid);
            contentType(types.document_element(), name,
                        "application/vnd.openxmlformats-officedocument.wordprocessingml." + pair.first +
                            "+xml");
        }
        files.push_back({"word/document.xml", xmlBytes(xml)});
        files.push_back({"word/_rels/document.xml.rels", xmlBytes(rels)});
        files.push_back({"[Content_Types].xml", xmlBytes(types)});
    } else {
        pugi::xml_document xml, styles;
        auto root = add(xml, format == OfficeFormat::fodt ? "office:document" : "office:document-content");
        namespaces(root, true);
        if (format == OfficeFormat::fodt)
            set(root, "office:mimetype", "application/vnd.oasis.opendocument.text");
        auto autoStyles = add(root, "office:automatic-styles");
        auto body = add(add(root, "office:body"), "office:text");
        auto styleRoot = add(styles, "office:document-styles");
        namespaces(styleRoot, true);
        auto namedStyles = add(styleRoot, "office:styles");
        auto normal = add(namedStyles, "style:default-style");
        set(normal, "style:family", "paragraph");
        set(add(normal, "style:text-properties"), "fo:font-size", "11pt");
        auto automatic = add(styleRoot, "office:automatic-styles"),
             layout = add(automatic, "style:page-layout");
        set(layout, "style:name", "pm1");
        auto properties = add(layout, "style:page-layout-properties");
        set(properties, "fo:page-width", mm(d.page.widthMM));
        set(properties, "fo:page-height", mm(d.page.heightMM));
        set(properties, "fo:margin-left", mm(d.page.leftMM));
        set(properties, "fo:margin-right", mm(d.page.rightMM));
        set(properties, "fo:margin-top", mm(d.page.topMM));
        set(properties, "fo:margin-bottom", mm(d.page.bottomMM));
        set(properties, "style:print-orientation",
            d.page.widthMM > d.page.heightMM ? "landscape" : "portrait");
        auto master = add(add(styleRoot, "office:master-styles"), "style:master-page");
        set(master, "style:name", "Standard");
        set(master, "style:page-layout-name", "pm1");
        if (!d.page.header.empty())
            odfFields(add(add(master, "style:header"), "text:p"), d.page.header);
        if (!d.page.footer.empty())
            odfFields(add(add(master, "style:footer"), "text:p"), d.page.footer);
        for (const auto &b : d.blocks) {
            if (b.kind == RichBlockKind::table) {
                auto table = add(body, "table:table");
                set(table, "table:name", "Table" + std::to_string(++serial));
                for (std::size_t c = 0; c < b.table.columns(); ++c)
                    add(table, "table:table-column");
                for (std::size_t r = 0; r < b.table.rows(); ++r) {
                    auto row = add(table, "table:table-row");
                    for (std::size_t c = 0; c < b.table.columns(); ++c) {
                        auto cell = add(row, "table:table-cell");
                        set(cell, "office:value-type", "string");
                        const auto &value = b.table.cell(r, c);
                        odfRun(add(cell, "text:p"),
                               {DocumentInlineKind::text, value.text, "", false, value.style}, autoStyles,
                               serial);
                    }
                }
                continue;
            }
            const auto name = "P" + std::to_string(++serial);
            auto style = add(autoStyles, "style:style");
            set(style, "style:name", name);
            set(style, "style:family", "paragraph");
            set(style, "style:master-page-name", "Standard");
            auto props = add(style, "style:paragraph-properties");
            set(props, "fo:text-align",
                b.paragraph.alignment == GridAlignment::right    ? "right"
                : b.paragraph.alignment == GridAlignment::center ? "center"
                                                                 : "left");
            set(props, "fo:margin-top", pt(b.paragraph.before));
            set(props, "fo:margin-bottom", pt(b.paragraph.after));
            set(props, "fo:margin-left", mm(b.paragraph.indent));
            set(props, "fo:line-height", std::to_string(int(b.paragraph.lineSpacing * 100)) + "%");
            set(add(style, "style:text-properties"), "fo:font-size", pt(b.paragraph.points));
            if (b.kind == RichBlockKind::pageBreak)
                set(props, "fo:break-before", "page");
            auto p = add(body, b.paragraph.heading ? "text:h" : "text:p");
            set(p, "text:style-name", name);
            if (b.paragraph.heading)
                set(p, "text:outline-level", double(b.paragraph.heading));
            if (b.kind == RichBlockKind::image) {
                auto found = assets.find(b.image);
                if (found == assets.end()) {
                    issue(issues, o, "content.xml", "image",
                          "An image has no packaged bytes and is omitted.");
                    continue;
                }
                auto path = "Pictures/image" + std::to_string(++serial) +
                            b.image.substr(b.image.rfind('.') == std::string::npos ? b.image.size()
                                                                                   : b.image.rfind('.'));
                if (format == OfficeFormat::fodt) {
                    issue(issues, o, "content.xml", "flat-image",
                          "Flat ODF image embedding is not exported; use ODT for packaged media.");
                    continue;
                }
                files.push_back({path, found->second->bytes});
                auto frame = add(p, "draw:frame");
                set(frame, "draw:name", b.caption);
                set(frame, "text:anchor-type", "as-char");
                set(frame, "svg:width", mm(b.imageWidthMM));
                set(frame, "svg:height", mm(b.imageHeightMM));
                auto image = add(frame, "draw:image");
                set(image, "xlink:href", path);
                set(image, "xlink:type", "simple");
                set(image, "xlink:show", "embed");
                set(image, "xlink:actuate", "onLoad");
            } else
                for (const auto &run : b.runs)
                    odfRun(p, run, autoStyles, serial);
        }
        if (format == OfficeFormat::fodt) {
            for (auto n : styleRoot.children()) {
                if (local(n.name()) == "automatic-styles") {
                    for (auto c : n.children())
                        autoStyles.append_copy(c);
                } else if (local(n.name()) == "master-styles")
                    root.insert_copy_after(n, autoStyles);
                else
                    root.insert_copy_before(n, autoStyles);
            }
            if (reported)
                *reported = issues;
            return xmlBytes(xml);
        }
        files.push_back({"content.xml", xmlBytes(xml)});
        files.push_back({"styles.xml", xmlBytes(styles)});
        odfManifest(files, "application/vnd.oasis.opendocument.text");
    }
    if (reported)
        *reported = issues;
    return writeDocumentArchive(files, o.limits);
}

OfficeWorkbook readOfficeWorkbook(ArrayView<unsigned char> bytes, OfficeFormat format,
                                  const OfficeOptions &o) {
    if (format != OfficeFormat::xlsx && format != OfficeFormat::ods && format != OfficeFormat::fods)
        throw std::invalid_argument("Not a spreadsheet format");
    auto files = loadFiles(bytes, format, o);
    OfficeWorkbook result;
    result.workbook.sheets.clear();
    checkParts(files, result.issues, o);
    std::size_t total = 0;
    if (format == OfficeFormat::xlsx) {
        auto main = officeMain(files, true, o);
        pugi::xml_document xml;
        validateXML(xml, part(files, main), o);
        auto rels = relationships(files, relPart(main), o);
        std::vector<std::string> strings;
        std::vector<GridCell> styles(1);
        for (const auto &rel : rels) {
            if (rel.second.external)
                continue;
            const auto path = resolve(main, rel.second.target);
            if (rel.second.type.find("/sharedStrings") != std::string::npos && files.count(path)) {
                pugi::xml_document stringXML;
                validateXML(stringXML, files[path], o);
                for (auto si : stringXML.document_element().children())
                    strings.push_back(textContent(si));
            }
            if (rel.second.type.find("/styles") != std::string::npos && files.count(path)) {
                pugi::xml_document styleXML;
                validateXML(styleXML, files[path], o);
                std::vector<unsigned> fonts;
                for (auto font : child(styleXML.document_element(), "fonts").children())
                    fonts.push_back(runStyle(font));
                std::unordered_map<std::size_t, int> precisions;
                for (auto number : child(styleXML.document_element(), "numFmts").children()) {
                    auto code = attr(number, "formatCode");
                    int precision = -1;
                    if (code == "0")
                        precision = 0;
                    else if (code.size() > 2 && code.size() <= 17 && code.substr(0, 2) == "0." &&
                             code.find_first_not_of('0', 2) == std::string::npos)
                        precision = int(code.size() - 2);
                    if (precision >= 0)
                        precisions[integer(attr(number, "numFmtId"), 65535)] = precision;
                }
                styles.clear();
                for (auto xf : child(styleXML.document_element(), "cellXfs").children()) {
                    GridCell style;
                    auto font = integer(attr(xf, "fontId", "0"), 65535, 0);
                    if (font < fonts.size())
                        style.style = fonts[font];
                    auto align = attr(child(xf, "alignment"), "horizontal");
                    style.alignment = align == "center"  ? GridAlignment::center
                                      : align == "right" ? GridAlignment::right
                                                         : GridAlignment::automatic;
                    auto formatId = integer(attr(xf, "numFmtId", "0"), 65535, 0);
                    if (formatId == 1)
                        style.decimals = 0;
                    if (formatId == 2)
                        style.decimals = 2;
                    if (precisions.count(formatId))
                        style.decimals = precisions[formatId];
                    if (formatId > 2 && !precisions.count(formatId))
                        issue(result.issues, o, path, "number-format",
                              "Number formats beyond decimal precision are imported as raw values.");
                    styles.push_back(style);
                }
            }
        }
        for (auto sheet : child(xml.document_element(), "sheets").children()) {
            if (result.workbook.sheets.size() >= 64)
                throw std::length_error("Workbook sheet limit");
            auto rel = rels.find(attr(sheet, "id"));
            if (rel == rels.end() || rel->second.external)
                throw std::invalid_argument("Missing sheet relationship");
            auto path = resolve(main, rel->second.target);
            pugi::xml_document sheetXML;
            validateXML(sheetXML, part(files, path), o);
            std::vector<std::pair<GridAddress, GridCell>> entries;
            std::size_t rows = 1, cols = 1;
            for (auto row : child(sheetXML.document_element(), "sheetData").children()) {
                std::size_t column = 0;
                auto rowIndex = integer(attr(row, "r", "1"), 1048576) - 1;
                for (auto cell : row.children()) {
                    if (local(cell.name()) != "c")
                        continue;
                    GridAddress address{rowIndex, column++};
                    const auto name = attr(cell, "r");
                    if (!name.empty() && !gridParseAddress(name, address))
                        throw std::invalid_argument("Invalid sheet cell address");
                    const auto type = attr(cell, "t");
                    if (type == "b" || type == "e")
                        issue(result.issues, o, path, "cell-type",
                              "Boolean/error cells are retained as visible numeric/text values.");
                    auto value = child(cell, "v");
                    auto formula = child(cell, "f");
                    if (!formula && !value && !child(cell, "is"))
                        continue;
                    if (address.row >= 100000 || address.column >= 256)
                        throw std::length_error("Imported used sheet range exceeds grid limits");
                    GridCell imported;
                    auto style = integer(attr(cell, "s", "0"), 65535, 0);
                    if (style < styles.size())
                        imported = styles[style];
                    if (formula) {
                        if (attr(formula, "t") == "shared" && textContent(formula).empty())
                            issue(result.issues, o, path, "shared-formula",
                                  "A shared formula follower is retained as its cached value.");
                        else
                            imported.text = "=" + textContent(formula);
                        if (!attr(formula, "t").empty() && attr(formula, "t") != "normal" &&
                            attr(formula, "t") != "shared")
                            issue(result.issues, o, path, "array-formula",
                                  "Array/data-table formula semantics are unsupported.");
                    }
                    if (imported.text.empty()) {
                        if (type == "s") {
                            auto index =
                                integer(textContent(value), strings.size() ? strings.size() - 1 : 0, 0);
                            if (index >= strings.size())
                                throw std::invalid_argument("Shared string index");
                            imported.text = strings[index];
                            imported.literal = true;
                        } else if (type == "inlineStr") {
                            imported.text = textContent(child(cell, "is"));
                            imported.literal = true;
                        } else if (type == "str" || type == "d" || type == "e") {
                            imported.text = textContent(value);
                            imported.literal = true;
                        } else
                            imported.text = textContent(value);
                    }
                    rows = std::max(rows, address.row + 1);
                    cols = std::max(cols, address.column + 1);
                    entries.push_back({address, std::move(imported)});
                    if (entries.size() > 1000000)
                        throw std::length_error("Workbook entry limit");
                }
            }
            total += rows * cols;
            if (total > 1000000)
                throw std::length_error("Workbook cell limit");
            SpreadsheetSheet imported;
            imported.name = attr(sheet, "name", "Sheet");
            std::vector<GridCell> cells(rows * cols);
            for (auto &e : entries)
                cells[e.first.row * cols + e.first.column] = std::move(e.second);
            imported.grid.assign(rows, cols, cells);
            imported.grid.clearHistory();
            result.workbook.sheets.push_back(std::move(imported));
            if (child(sheetXML.document_element(), "mergeCells"))
                issue(result.issues, o, path, "merged-cells", "Merged-cell presentation is flattened.");
            if (child(sheetXML.document_element(), "conditionalFormatting") ||
                child(sheetXML.document_element(), "dataValidations"))
                issue(result.issues, o, path, "cell-rules",
                      "Conditional formatting and validation rules are omitted.");
        }
    } else {
        pugi::xml_document xml;
        validateXML(xml, part(files, "content.xml"), o);
        auto spreadsheet = descendant(xml, "spreadsheet");
        if (!spreadsheet)
            throw std::invalid_argument("Missing ODF spreadsheet body");
        OdfStyles styles;
        collectODFStyles(xml, styles);
        if (files.count("styles.xml")) {
            pugi::xml_document styleXML;
            validateXML(styleXML, files["styles.xml"], o);
            collectODFStyles(styleXML, styles);
        }
        for (auto table : spreadsheet.children())
            if (local(table.name()) == "table") {
                if (result.workbook.sheets.size() >= 64)
                    throw std::length_error("Workbook sheet limit");
                std::vector<std::vector<GridCell>> rows;
                std::size_t cols = 1, pendingRows = 0;
                for (auto row : odfTableRows(table)) {
                    std::vector<GridCell> cells;
                    std::size_t pendingCells = 0;
                    for (auto cell : row.children())
                        if (local(cell.name()) == "table-cell" ||
                            local(cell.name()) == "covered-table-cell") {
                            GridCell c;
                            const auto style = resolvedStyle(styles, attr(cell, "style-name"));
                            c.style = style.bits;
                            c.alignment = style.paragraph.alignment;
                            c.decimals = style.decimals;
                            const auto formula = attr(cell, "formula"), type = attr(cell, "value-type");
                            if (type == "currency" || type == "percentage" || type == "date" ||
                                type == "time")
                                issue(result.issues, o, "content.xml", "number-format",
                                      "Special number/date formats are retained as raw values without "
                                      "their original presentation.");
                            if (!formula.empty())
                                c.text = spreadsheetFormulaFromODF(formula);
                            else if (type == "float" || type == "currency" || type == "percentage")
                                c.text = attr(cell, "value");
                            else if (type == "boolean")
                                c.text = attr(cell, "boolean-value") == "true" ? "1" : "0";
                            else {
                                c.text = attr(cell, "string-value");
                                if (c.text.empty()) {
                                    bool first = true;
                                    for (auto p : cell.children())
                                        if (local(p.name()) == "p") {
                                            if (!first)
                                                c.text += '\n';
                                            c.text += textContent(p);
                                            first = false;
                                        }
                                }
                                if (type == "date")
                                    c.text = attr(cell, "date-value");
                                if (type == "time")
                                    c.text = attr(cell, "time-value");
                                c.literal = true;
                            }
                            const auto repeat = integer(attr(cell, "number-columns-repeated"), 16384);
                            if (c.text.empty()) {
                                pendingCells += repeat;
                            } else {
                                if (cells.size() + pendingCells + repeat > 256)
                                    throw std::length_error("ODF used column limit");
                                cells.insert(cells.end(), pendingCells, {});
                                pendingCells = 0;
                                cells.insert(cells.end(), repeat, c);
                            }
                            if (attr(cell, "number-columns-spanned", "1") != "1" ||
                                attr(cell, "number-rows-spanned", "1") != "1")
                                issue(result.issues, o, "content.xml", "merged-cells",
                                      "Merged-cell presentation is flattened.");
                        }
                    auto repeat = integer(attr(row, "number-rows-repeated"), 1048576);
                    if (cells.empty())
                        pendingRows += repeat;
                    else {
                        if (rows.size() + pendingRows + repeat > 100000)
                            throw std::length_error("ODF used row limit");
                        cols = std::max(cols, cells.size());
                        if ((rows.size() + pendingRows + repeat) * cols > 1000000)
                            throw std::length_error("ODF used cell limit");
                        rows.insert(rows.end(), pendingRows, {});
                        pendingRows = 0;
                        rows.insert(rows.end(), repeat, cells);
                    }
                }
                if (rows.empty())
                    rows.emplace_back();
                total += rows.size() * cols;
                if (total > 1000000)
                    throw std::length_error("Workbook cell limit");
                SpreadsheetSheet imported;
                imported.name = attr(table, "name", "Sheet");
                std::vector<GridCell> cells(rows.size() * cols);
                for (std::size_t r = 0; r < rows.size(); ++r)
                    std::copy(rows[r].begin(), rows[r].end(), cells.begin() + r * cols);
                imported.grid.assign(rows.size(), cols, cells);
                imported.grid.clearHistory();
                result.workbook.sheets.push_back(std::move(imported));
            }
    }
    if (result.workbook.sheets.empty())
        throw std::invalid_argument("Workbook has no worksheets");
    auto values = evaluateWorkbook(result.workbook);
    for (std::size_t s = 0; s < values.size(); ++s)
        for (const auto &v : values[s])
            if (v.error == "#NAME?")
                issue(result.issues, o, result.workbook.sheets[s].name, "formula-function",
                      "Unsupported formula functions remain in source and are not calculated natively.");
    return result;
}

std::vector<unsigned char> writeOfficeWorkbook(const Workbook &book, OfficeFormat format,
                                               std::vector<OfficeIssue> *reported, const OfficeOptions &o) {
    if (format != OfficeFormat::xlsx && format != OfficeFormat::ods && format != OfficeFormat::fods)
        throw std::invalid_argument("Not a spreadsheet format");
    if (book.sheets.empty())
        throw std::invalid_argument("Workbook has no sheets");
    auto values = evaluateWorkbook(book);
    std::vector<OfficeIssue> issues;
    std::set<std::string> names;
    for (std::size_t i = 0; i < book.sheets.size(); ++i) {
        const auto &name = book.sheets[i].name;
        if (name.empty() || name.size() > 31 || name.find_first_of("[]:*?/\\") != std::string::npos ||
            name.front() == '\'' || name.back() == '\'' ||
            !names
                 .insert([&] {
                     auto folded = name;
                     for (auto &c : folded)
                         if (static_cast<unsigned char>(c) < 128)
                             c = char(std::toupper(static_cast<unsigned char>(c)));
                     return folded;
                 }())
                 .second)
            throw std::invalid_argument("Invalid or duplicate worksheet name");
        for (const auto &value : values[i])
            if (value.error == "#NAME?")
                issue(issues, o, name, "formula-function",
                      "Formula source is retained; this function has no native calculation result.");
    }
    std::vector<DocumentAsset> files;
    std::size_t serial = 0;
    if (format == OfficeFormat::xlsx) {
        pugi::xml_document workbook, types, rels, styles;
        packageRoots(files, "xl/workbook.xml",
                     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml", types);
        auto root = add(workbook, "workbook");
        set(root, "xmlns", S);
        set(root, "xmlns:r", R);
        auto sheets = add(root, "sheets");
        set(add(root, "calcPr"), "calcId", "191029");
        set(child(root, "calcPr"), "fullCalcOnLoad", "1");
        auto relroot = add(rels, "Relationships");
        set(relroot, "xmlns", "http://schemas.openxmlformats.org/package/2006/relationships");
        auto styleRoot = add(styles, "styleSheet");
        set(styleRoot, "xmlns", S);
        auto fonts = add(styleRoot, "fonts");
        set(fonts, "count", "16");
        for (unsigned flags = 0; flags < 16; ++flags) {
            auto font = add(fonts, "font");
            set(add(font, "sz"), "val", "11");
            set(add(font, "name"), "val", "Calibri");
            if (flags & 1)
                add(font, "b");
            if (flags & 2)
                add(font, "i");
            if (flags & 4)
                add(font, "u");
            if (flags & 8)
                add(font, "strike");
        }
        auto fills = add(styleRoot, "fills");
        set(fills, "count", "2");
        set(add(add(fills, "fill"), "patternFill"), "patternType", "none");
        set(add(add(fills, "fill"), "patternFill"), "patternType", "gray125");
        auto borders = add(styleRoot, "borders");
        set(borders, "count", "1");
        add(borders, "border");
        auto baseXfs = add(styleRoot, "cellStyleXfs");
        set(baseXfs, "count", "1");
        auto base = add(baseXfs, "xf");
        set(base, "numFmtId", "0");
        set(base, "fontId", "0");
        set(base, "fillId", "0");
        set(base, "borderId", "0");
        auto xfs = add(styleRoot, "cellXfs");
        std::vector<std::tuple<unsigned, int, int>> formats{{0, 0, -1}};
        auto styleIndex = [&](const GridCell &c) {
            auto key = std::make_tuple(c.style & 15, int(c.alignment), c.decimals);
            auto it = std::find(formats.begin(), formats.end(), key);
            if (it != formats.end())
                return std::size_t(it - formats.begin());
            formats.push_back(key);
            return formats.size() - 1;
        };
        for (std::size_t s = 0; s < book.sheets.size(); ++s) {
            const auto &sheet = book.sheets[s];
            auto entry = add(sheets, "sheet");
            set(entry, "name", sheet.name);
            set(entry, "sheetId", double(s + 1));
            const auto id = "rIdSheet" + std::to_string(s + 1);
            set(entry, "r:id", id);
            const auto path = "xl/worksheets/sheet" + std::to_string(s + 1) + ".xml";
            relationship(relroot, id, "worksheet", "worksheets/sheet" + std::to_string(s + 1) + ".xml");
            contentType(types.document_element(), path,
                        "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
            pugi::xml_document xml;
            auto ws = add(xml, "worksheet");
            set(ws, "xmlns", S);
            set(add(ws, "dimension"), "ref",
                "A1:" + gridAddressName({sheet.grid.rows() - 1, sheet.grid.columns() - 1}));
            auto data = add(ws, "sheetData");
            for (std::size_t r = 0; r < sheet.grid.rows(); ++r) {
                Node row;
                for (std::size_t c = 0; c < sheet.grid.columns(); ++c) {
                    const auto &cell = sheet.grid.cell(r, c);
                    if (cell.text.empty() && !cell.style && cell.decimals < 0)
                        continue;
                    if (!row) {
                        row = add(data, "row");
                        set(row, "r", double(r + 1));
                    }
                    auto node = add(row, "c");
                    set(node, "r", gridAddressName({r, c}));
                    set(node, "s", double(styleIndex(cell)));
                    const auto &value = values[s][r * sheet.grid.columns() + c];
                    if (!cell.literal && !cell.text.empty() && cell.text[0] == '=') {
                        add(node, "f", cell.text.substr(1));
                        if (!value.error.empty()) {
                            set(node, "t", "e");
                            add(node, "v",
                                value.error == "#CYCLE!"   ? "#REF!"
                                : value.error == "#LIMIT!" ? "#NUM!"
                                                           : value.error);
                        } else if (value.numeric)
                            add(node, "v", value.display());
                        else {
                            set(node, "t", "str");
                            add(node, "v", value.text);
                        }
                    } else if (value.numeric && !cell.literal)
                        add(node, "v", value.display());
                    else {
                        set(node, "t", "inlineStr");
                        auto t = add(add(node, "is"), "t", cell.text);
                        set(t, "xml:space", "preserve");
                    }
                }
            }
            files.push_back({path, xmlBytes(xml)});
        }
        auto numFormats = styleRoot.prepend_child("numFmts");
        std::set<int> precisions;
        for (const auto &f : formats)
            if (std::get<2>(f) > 0)
                precisions.insert(std::get<2>(f));
        set(numFormats, "count", double(precisions.size()));
        for (int precision : precisions) {
            auto n = add(numFormats, "numFmt");
            set(n, "numFmtId", double(164 + precision));
            set(n, "formatCode", "0." + std::string(std::size_t(precision), '0'));
        }
        set(xfs, "count", double(formats.size()));
        for (auto f : formats) {
            auto xf = add(xfs, "xf");
            set(xf, "fontId", double(std::get<0>(f)));
            set(xf, "fillId", "0");
            set(xf, "borderId", "0");
            set(xf, "xfId", "0");
            const auto decimals = std::get<2>(f);
            set(xf, "numFmtId", double(decimals < 0 ? 0 : decimals == 0 ? 1 : 164 + decimals));
            set(xf, "applyFont", "1");
            set(xf, "applyNumberFormat", "1");
            auto align = add(xf, "alignment");
            set(align, "horizontal",
                std::get<1>(f) == int(GridAlignment::right)    ? "right"
                : std::get<1>(f) == int(GridAlignment::center) ? "center"
                : std::get<1>(f) == int(GridAlignment::left)   ? "left"
                                                               : "general");
            set(align, "wrapText", "1");
        }
        auto cellStyles = add(styleRoot, "cellStyles");
        set(cellStyles, "count", "1");
        auto normal = add(cellStyles, "cellStyle");
        set(normal, "name", "Normal");
        set(normal, "xfId", "0");
        set(normal, "builtinId", "0");
        relationship(relroot, "rIdStyles", "styles", "styles.xml");
        contentType(types.document_element(), "xl/styles.xml",
                    "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
        files.push_back({"xl/styles.xml", xmlBytes(styles)});
        files.push_back({"xl/workbook.xml", xmlBytes(workbook)});
        files.push_back({"xl/_rels/workbook.xml.rels", xmlBytes(rels)});
        files.push_back({"[Content_Types].xml", xmlBytes(types)});
    } else {
        pugi::xml_document xml;
        auto root = add(xml, format == OfficeFormat::fods ? "office:document" : "office:document-content");
        namespaces(root, true);
        if (format == OfficeFormat::fods)
            set(root, "office:mimetype", "application/vnd.oasis.opendocument.spreadsheet");
        auto styles = add(root, "office:automatic-styles");
        auto body = add(add(root, "office:body"), "office:spreadsheet");
        for (std::size_t s = 0; s < book.sheets.size(); ++s) {
            const auto &sheet = book.sheets[s];
            auto table = add(body, "table:table");
            set(table, "table:name", sheet.name);
            for (std::size_t r = 0; r < sheet.grid.rows(); ++r) {
                auto row = add(table, "table:table-row");
                for (std::size_t c = 0; c < sheet.grid.columns(); ++c) {
                    const auto &cell = sheet.grid.cell(r, c);
                    const auto &value = values[s][r * sheet.grid.columns() + c];
                    auto node = add(row, "table:table-cell");
                    if (cell.style || cell.alignment != GridAlignment::automatic || cell.decimals >= 0) {
                        const auto name = "ce" + std::to_string(++serial);
                        auto st = add(styles, "style:style");
                        set(st, "style:name", name);
                        set(st, "style:family", "table-cell");
                        if (cell.decimals >= 0) {
                            const auto numberName = "N" + std::to_string(serial);
                            set(st, "style:data-style-name", numberName);
                            auto numberStyle = add(styles, "number:number-style");
                            set(numberStyle, "style:name", numberName);
                            auto number = add(numberStyle, "number:number");
                            set(number, "number:decimal-places", double(std::clamp(cell.decimals, 0, 15)));
                            set(number, "number:min-integer-digits", "1");
                        }
                        auto pr = add(st, "style:text-properties");
                        if (cell.style & 1)
                            set(pr, "fo:font-weight", "bold");
                        if (cell.style & 2)
                            set(pr, "fo:font-style", "italic");
                        if (cell.style & 4)
                            set(pr, "style:text-underline-style", "solid");
                        if (cell.style & 8)
                            set(pr, "style:text-line-through-style", "solid");
                        set(add(st, "style:paragraph-properties"), "fo:text-align",
                            cell.alignment == GridAlignment::right    ? "right"
                            : cell.alignment == GridAlignment::center ? "center"
                                                                      : "left");
                        set(node, "table:style-name", name);
                    }
                    if (!cell.literal && !cell.text.empty() && cell.text[0] == '=')
                        set(node, "table:formula", spreadsheetFormulaToODF(cell.text));
                    if (value.numeric && !cell.literal) {
                        set(node, "office:value-type", "float");
                        set(node, "office:value", value.number);
                        add(node, "text:p", value.display(cell.decimals));
                    } else {
                        set(node, "office:value-type", "string");
                        odfText(add(node, "text:p"), !value.error.empty() ? value.error
                                                     : cell.literal       ? cell.text
                                                                          : value.display());
                    }
                }
            }
        }
        if (format == OfficeFormat::fods) {
            if (reported)
                *reported = issues;
            return xmlBytes(xml);
        }
        files.push_back({"content.xml", xmlBytes(xml)});
        odfManifest(files, "application/vnd.oasis.opendocument.spreadsheet");
    }
    if (reported)
        *reported = issues;
    return writeDocumentArchive(files, o.limits);
}
} // namespace cyui

#include "../../third_party/md4c/md4c.h"
#include "cyui/modules/DocumentViews.h"
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace cyui {
namespace {
std::string entity(const std::string &);
std::string attribute(const MD_ATTRIBUTE &a) {
    const std::string input = a.text ? std::string(a.text, a.size) : std::string{};
    std::string out;
    for (std::size_t at = 0; at < input.size();) {
        if (input[at] == '&') {
            const auto end = input.find(';', at + 1);
            if (end != std::string::npos && end - at < 50) {
                out += entity(input.substr(at, end - at + 1));
                at = end + 1;
                continue;
            }
        }
        out += input[at++];
    }
    return out;
}
std::string escaped(const std::string &s) {
    std::string out;
    for (char c : s)
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    return out;
}
std::string entity(const std::string &s) {
    static const std::unordered_map<std::string, std::string> named{
#include "DocumentEntities.h"
    };
    auto it = named.find(s);
    if (it != named.end())
        return it->second;
    if (s.size() > 3 && s[1] == '#') {
        char *end = nullptr;
        const bool hex = s[2] == 'x' || s[2] == 'X';
        auto cp = std::strtoul(s.c_str() + (hex ? 3 : 2), &end, hex ? 16 : 10);
        if (end && *end == ';' && end[1] == 0) {
            if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
                cp = 0xfffd;
            std::string out;
            if (cp < 0x80)
                out += char(cp);
            else if (cp < 0x800) {
                out += char(0xc0 | (cp >> 6));
                out += char(0x80 | (cp & 63));
            } else if (cp < 0x10000) {
                out += char(0xe0 | (cp >> 12));
                out += char(0x80 | ((cp >> 6) & 63));
                out += char(0x80 | (cp & 63));
            } else {
                out += char(0xf0 | (cp >> 18));
                out += char(0x80 | ((cp >> 12) & 63));
                out += char(0x80 | ((cp >> 6) & 63));
                out += char(0x80 | (cp & 63));
            }
            return out;
        }
    }
    return s;
}
struct Parser {
    const std::string &source;
    std::vector<DocumentBlock> blocks;
    std::vector<DocumentDestination> destinations;
    void destination(const MD_ATTRIBUTE &raw, const std::string &uri, bool isImage) {
        if (raw.raw_size) {
            // MD4C points explicit destinations (including definitions) into its input.
            const auto begin = std::size_t(raw.raw_text - source.data());
            destinations.push_back({begin, begin + raw.raw_size, uri, isImage});
        }
    }
    struct List {
        unsigned number;
        bool first = true, task = false, checked = false;
        std::size_t offset = 0;
    };
    std::vector<List> lists;
    struct Span {
        unsigned style;
        std::string target;
        ItemId spoiler;
        bool image;
    };
    std::vector<Span> stack;
    unsigned style = 0, htmlStyle = 0;
    std::string target;
    ItemId spoiler = 0, nextSpoiler = 1;
    int quotes = 0;
    std::size_t active = std::string::npos, table = std::string::npos;
    bool image = false;
    DocumentInline imageRun;
    std::unordered_map<std::string, unsigned> slugs;
    std::vector<DocumentInline> &runs() {
        auto &b = blocks.at(active);
        return table == active && !b.cells.empty() && !b.cells.back().empty() ? b.cells.back().back()
                                                                              : b.inlines;
    }
    void start(DocumentBlockKind kind) {
        blocks.emplace_back();
        active = blocks.size() - 1;
        auto &b = blocks.back();
        b.kind = kind;
        b.quoteDepth = quotes;
        b.indent = int(lists.size());
        if (kind == DocumentBlockKind::paragraph && !lists.empty() && lists.back().first) {
            auto &list = lists.back();
            b.kind = DocumentBlockKind::list;
            b.number = list.number;
            b.task = list.task;
            b.checked = list.checked;
            b.taskOffset = list.offset;
            list.first = false;
        } else if (kind == DocumentBlockKind::paragraph && quotes)
            b.kind = DocumentBlockKind::quote;
        b.sourceBegin = source.size();
    }
    static int enterBlock(MD_BLOCKTYPE type, void *detail, void *data) {
        auto &p = *static_cast<Parser *>(data);
        if (p.blocks.size() > 262144 || p.lists.size() > 256)
            return 1;
        switch (type) {
        case MD_BLOCK_QUOTE:
            ++p.quotes;
            break;
        case MD_BLOCK_UL:
            p.lists.push_back({0});
            break;
        case MD_BLOCK_OL:
            p.lists.push_back({static_cast<MD_BLOCK_OL_DETAIL *>(detail)->start});
            break;
        case MD_BLOCK_LI: {
            auto &l = p.lists.back();
            auto &d = *static_cast<MD_BLOCK_LI_DETAIL *>(detail);
            l.first = true;
            l.task = d.is_task != 0;
            l.checked = d.task_mark != ' ';
            l.offset = d.task_mark_offset;
            p.start(DocumentBlockKind::paragraph);
            break;
        }
        case MD_BLOCK_P:
            if (p.active != std::string::npos && p.blocks[p.active].kind == DocumentBlockKind::list &&
                p.blocks[p.active].inlines.empty())
                break;
            p.start(DocumentBlockKind::paragraph);
            break;
        case MD_BLOCK_H:
            p.start(DocumentBlockKind::heading);
            p.blocks.back().level = int(static_cast<MD_BLOCK_H_DETAIL *>(detail)->level);
            break;
        case MD_BLOCK_HR:
            p.start(DocumentBlockKind::rule);
            break;
        case MD_BLOCK_CODE:
            p.start(DocumentBlockKind::code);
            p.blocks.back().language = attribute(static_cast<MD_BLOCK_CODE_DETAIL *>(detail)->lang);
            break;
        case MD_BLOCK_HTML:
            p.start(DocumentBlockKind::paragraph);
            break;
        case MD_BLOCK_TABLE:
            p.start(DocumentBlockKind::table);
            p.table = p.active;
            break;
        case MD_BLOCK_TR:
            p.blocks[p.table].cells.emplace_back();
            p.blocks[p.table].rows.emplace_back();
            break;
        case MD_BLOCK_TH:
        case MD_BLOCK_TD: {
            auto &b = p.blocks[p.table];
            b.cells.back().emplace_back();
            b.rows.back().emplace_back();
            if (type == MD_BLOCK_TH)
                b.alignments.push_back(int(static_cast<MD_BLOCK_TD_DETAIL *>(detail)->align));
            break;
        }
        default:
            break;
        }
        return 0;
    }
    static int leaveBlock(MD_BLOCKTYPE type, void *, void *data) {
        auto &p = *static_cast<Parser *>(data);
        if (type == MD_BLOCK_QUOTE)
            --p.quotes;
        else if (type == MD_BLOCK_UL || type == MD_BLOCK_OL)
            p.lists.pop_back();
        else if (type == MD_BLOCK_LI && !p.lists.empty() && p.lists.back().number)
            ++p.lists.back().number;
        else if (type == MD_BLOCK_CODE) {
            auto &b = p.blocks[p.active];
            b.code.setText(b.text, codeLanguageFromName(b.language));
        } else if (type == MD_BLOCK_H) {
            auto &b = p.blocks[p.active];
            std::string slug;
            for (unsigned char c : b.text) {
                if (c >= 128 || std::isalnum(c) || c == '_' || c == '-')
                    slug += char(c < 128 ? std::tolower(c) : c);
                else if (std::isspace(c) && !slug.empty() && slug.back() != '-')
                    slug += '-';
            }
            if (!slug.empty() && slug.back() == '-')
                slug.pop_back();
            if (slug.empty())
                slug = "section";
            auto &n = p.slugs[slug];
            b.anchor = slug + (n ? "-" + std::to_string(n) : "");
            ++n;
        } else if (type == MD_BLOCK_TABLE)
            p.table = std::string::npos;
        return 0;
    }
    static int enterSpan(MD_SPANTYPE type, void *detail, void *data) {
        auto &p = *static_cast<Parser *>(data);
        p.stack.push_back({p.style, p.target, p.spoiler, p.image});
        switch (type) {
        case MD_SPAN_STRONG:
            p.style |= documentBold;
            break;
        case MD_SPAN_EM:
            p.style |= documentItalic;
            break;
        case MD_SPAN_CODE:
            p.style |= documentMono;
            break;
        case MD_SPAN_DEL:
            p.style |= documentStrike;
            break;
        case MD_SPAN_U:
            p.style |= documentUnderline;
            break;
        case MD_SPAN_MARK:
            p.style |= documentHighlight;
            break;
        case MD_SPAN_SPOILER:
            p.style |= documentSpoiler;
            p.spoiler = p.nextSpoiler++;
            break;
        case MD_SPAN_A: {
            auto &d = *static_cast<MD_SPAN_A_DETAIL *>(detail);
            p.target = attribute(d.href);
            if (!d.is_autolink)
                p.destination(d.href, p.target, false);
            break;
        }
        case MD_SPAN_IMG:
            if (!p.image) {
                p.imageRun = {};
                p.imageRun.kind = DocumentInlineKind::image;
                auto &d = *static_cast<MD_SPAN_IMG_DETAIL *>(detail);
                p.imageRun.target = attribute(d.src);
                p.destination(d.src, p.imageRun.target, true);
                p.imageRun.title = attribute(d.title);
                p.image = true;
            }
            break;
        default:
            break;
        }
        return 0;
    }
    static int leaveSpan(MD_SPANTYPE type, void *, void *data) {
        auto &p = *static_cast<Parser *>(data);
        auto saved = p.stack.back();
        p.stack.pop_back();
        if (type == MD_SPAN_IMG && !saved.image && p.active != std::string::npos) {
            auto &r = p.imageRun;
            const auto pipe = r.text.rfind('|');
            if (pipe != std::string::npos) {
                char *end = nullptr;
                const float w = std::strtof(r.text.c_str() + pipe + 1, &end);
                float h = 0;
                if (end && *end == 'x')
                    h = std::strtof(end + 1, &end);
                if (end && !*end && std::isfinite(w) && w > 0 && w <= 16384 && std::isfinite(h) && h >= 0 &&
                    h <= 16384) {
                    r.width = w;
                    r.height = h;
                    r.text.resize(pipe);
                }
            }
            p.runs().push_back(r);
        }
        p.style = saved.style;
        p.target = saved.target;
        p.spoiler = saved.spoiler;
        p.image = saved.image;
        return 0;
    }
    static int text(MD_TEXTTYPE type, const char *text, MD_SIZE length, void *data) {
        auto &p = *static_cast<Parser *>(data);
        if (p.active == std::string::npos)
            return 0;
        auto &b = p.blocks[p.active];
        std::string value(text, length);
        const auto ptr = reinterpret_cast<std::uintptr_t>(text),
                   start = reinterpret_cast<std::uintptr_t>(p.source.data());
        const auto begin =
            ptr >= start && ptr <= start + p.source.size() ? std::size_t(ptr - start) : b.sourceEnd;
        const auto end = std::min(p.source.size(), begin + length);
        b.sourceBegin = std::min(b.sourceBegin, begin);
        b.sourceEnd = std::max(b.sourceEnd, end);
        if (b.kind == DocumentBlockKind::code) {
            b.text += value;
            return 0;
        }
        if (type == MD_TEXT_HTML) {
            static const std::unordered_map<std::string, unsigned> tags{
                {"u", documentUnderline}, {"mark", documentHighlight}, {"kbd", documentKeyboard},
                {"b", documentBold},      {"strong", documentBold},    {"em", documentItalic},
                {"i", documentItalic},    {"s", documentStrike},       {"del", documentStrike}};
            if (value == "<br>" || value == "<br/>" || value == "<br />")
                value = "\n";
            else {
                bool close = value.size() > 2 && value[1] == '/';
                const auto name = value.substr(close ? 2 : 1, value.size() - (close ? 3 : 2));
                auto it = tags.find(name);
                if (it != tags.end()) {
                    if (close)
                        p.htmlStyle &= ~it->second;
                    else
                        p.htmlStyle |= it->second;
                    return 0;
                }
            }
        }
        if (type == MD_TEXT_ENTITY)
            value = entity(value);
        if (type == MD_TEXT_NULLCHAR)
            value = "\xef\xbf\xbd";
        if (type == MD_TEXT_BR || type == MD_TEXT_SOFTBR)
            value = "\n";
        if (p.image) {
            p.imageRun.text += value;
            p.imageRun.sourceBegin = std::min(p.imageRun.sourceBegin ? p.imageRun.sourceBegin : begin, begin);
            p.imageRun.sourceEnd = end;
            return 0;
        }
        DocumentInline run;
        run.text = value;
        run.target = p.target;
        run.style = p.style | p.htmlStyle;
        run.sourceBegin = begin;
        run.sourceEnd = end;
        run.spoiler = p.spoiler;
        run.kind = !run.target.empty()            ? DocumentInlineKind::link
                   : (run.style & documentMono)   ? DocumentInlineKind::code
                   : (run.style & documentBold)   ? DocumentInlineKind::strong
                   : (run.style & documentItalic) ? DocumentInlineKind::emphasis
                                                  : DocumentInlineKind::text;
        if (type == MD_TEXT_SOFTBR)
            run.kind = DocumentInlineKind::softBreak;
        p.runs().push_back(std::move(run));
        b.text += value;
        if (p.table == p.active && !b.rows.empty() && !b.rows.back().empty())
            b.rows.back().back() += value;
        return 0;
    }
};
} // namespace
namespace detail {
// Shared by the native clipboard converter; not part of the public API.
std::string decodeDocumentEntity(const std::string &text) {
    return entity(text);
}
} // namespace detail
bool Document::setMarkdown(const std::string &text) {
    if (text == text_)
        return false;
    if (text.size() > 32 * 1024 * 1024)
        throw std::length_error("Document exceeds 32 MiB");
    Parser p{text};
    MD_PARSER parser{};
    parser.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS |
                   MD_FLAG_PERMISSIVEURLAUTOLINKS | MD_FLAG_SPOILERS | MD_FLAG_HIGHLIGHT |
                   MD_FLAG_NOHTMLBLOCKS;
    parser.enter_block = Parser::enterBlock;
    parser.leave_block = Parser::leaveBlock;
    parser.enter_span = Parser::enterSpan;
    parser.leave_span = Parser::leaveSpan;
    parser.text = Parser::text;
    if (md_parse(text.data(), MD_SIZE(text.size()), &parser, &p) != 0)
        throw std::runtime_error("Markdown parse failed or document nesting limit exceeded");
    text_ = text;
    blocks_ = std::move(p.blocks);
    destinations_ = std::move(p.destinations);
    ++revision_;
    return true;
}
std::string Document::plainText() const {
    std::string out;
    for (const auto &b : blocks_) {
        if (!out.empty())
            out += '\n';
        if (b.kind == DocumentBlockKind::table) {
            for (const auto &r : b.rows) {
                for (std::size_t i = 0; i < r.size(); ++i) {
                    if (i)
                        out += '\t';
                    out += r[i];
                }
                out += '\n';
            }
        } else
            out += b.text;
    }
    return out;
}
DocumentMediaKind documentMediaKind(const std::string &source) {
    auto path = source.substr(0, source.find_first_of("?#"));
    for (auto &c : path)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    const auto dot = path.rfind('.');
    const auto ext = dot == std::string::npos ? "" : path.substr(dot);
    if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".webm" || ext == ".ogv" || ext == ".m4v")
        return DocumentMediaKind::video;
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg" || ext == ".oga" ||
        ext == ".opus" || ext == ".m4a" || ext == ".aac")
        return DocumentMediaKind::audio;
    return DocumentMediaKind::image;
}
std::string documentHtml(const Document &doc, const std::string &title) {
    auto inlineHtml = [](const std::vector<DocumentInline> &runs) {
        std::string result;
        for (const auto &r : runs) {
            std::string value = escaped(r.text);
            for (std::size_t at = 0; (at = value.find('\n', at)) != std::string::npos; at += 5)
                value.replace(at, 1, "<br>\n");
            if (r.kind == DocumentInlineKind::image) {
                const auto kind = documentMediaKind(r.target);
                const char *tag = kind == DocumentMediaKind::video   ? "video"
                                  : kind == DocumentMediaKind::audio ? "audio"
                                                                     : "img";
                value = "<" + std::string(tag) + " src=\"" + escaped(r.target) + "\"" +
                        (r.width > 0 ? " width=\"" + std::to_string(int(r.width)) + "\"" : "") +
                        (kind == DocumentMediaKind::image ? " alt=\"" + escaped(r.text) + "\">"
                                                          : " controls></" + std::string(tag) + ">");
            }
            if (r.style & documentMono)
                value = "<code>" + value + "</code>";
            if (r.style & documentBold)
                value = "<strong>" + value + "</strong>";
            if (r.style & documentItalic)
                value = "<em>" + value + "</em>";
            if (r.style & documentUnderline)
                value = "<u>" + value + "</u>";
            if (r.style & documentStrike)
                value = "<s>" + value + "</s>";
            if (r.style & documentHighlight)
                value = "<mark>" + value + "</mark>";
            if (r.style & documentKeyboard)
                value = "<kbd>" + value + "</kbd>";
            if (r.style & documentSpoiler)
                value = "<details><summary>Spoiler</summary>" + value + "</details>";
            if (!r.target.empty() && r.kind != DocumentInlineKind::image) {
                auto url = r.target;
                // Exported HTML must not turn a host-handled unsafe link into executable content.
                auto colon = url.find(':');
                auto scheme = url.substr(0, colon);
                for (char &c : scheme)
                    c = char(std::tolower(static_cast<unsigned char>(c)));
                if (colon == std::string::npos || scheme == "https" || scheme == "http" || scheme == "mailto")
                    value = "<a href=\"" + escaped(url) + "\">" + value + "</a>";
            }
            result += value;
        }
        return result;
    };
    std::string out = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
                      "content=\"width=device-width\"><title>" +
                      escaped(title) + "</title></head><body>";
    for (const auto &b : doc.blocks()) {
        for (int q = 0; q < b.quoteDepth; ++q)
            out += "<blockquote>";
        auto value = inlineHtml(b.inlines);
        switch (b.kind) {
        case DocumentBlockKind::heading: {
            auto tag = "h" + std::to_string(b.level);
            out += "<" + tag + " id=\"" + escaped(b.anchor) + "\">" + value + "</" + tag + ">";
            break;
        }
        case DocumentBlockKind::code:
            out += "<pre><code>" + escaped(b.text) + "</code></pre>";
            break;
        case DocumentBlockKind::rule:
            out += "<hr>";
            break;
        case DocumentBlockKind::table:
            out += "<table>";
            for (std::size_t row = 0; row < b.cells.size(); ++row) {
                out += "<tr>";
                for (const auto &cell : b.cells[row]) {
                    auto tag = row ? "td" : "th";
                    out += "<" + std::string(tag) + ">" + inlineHtml(cell) + "</" + tag + ">";
                }
                out += "</tr>";
            }
            out += "</table>";
            break;
        default:
            out += "<p>" +
                   (b.task ? std::string(b.checked ? "&#9745; " : "&#9744; ")
                    : b.kind == DocumentBlockKind::list
                        ? (b.number ? std::to_string(b.number) + ". " : "&#8226; ")
                        : "") +
                   value + "</p>";
        }
        for (int q = 0; q < b.quoteDepth; ++q)
            out += "</blockquote>";
    }
    return out + "</body></html>";
}
} // namespace cyui

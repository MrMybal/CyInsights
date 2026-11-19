// Original Verse lexical scanner. No UEFN SDK or parser dependency.
#pragma once
#include "cyui/modules/Code.h"
#include <array>
namespace cyui { namespace detail {
inline constexpr std::string_view verseKeywords =
    "and block branch break case class defer do else enum false for if in interface "
    "is let live loop module not of or over race ref return rush set spawn struct "
    "sync then true using var when where yield";
inline constexpr std::string_view verseTypes =
    "any array char char8 comparable float int logic map message option rational "
    "string tuple type void weak_map";
inline constexpr std::string_view verseSpecifiers =
    "abstract allocates castable concrete constructor converges computes decides "
    "epic_internal final final_super immediate initializable internal intrinsic "
    "localizes native native_callable no_rollback open override persistable private "
    "protected public reads scoped suspends transacts unique varies writes";
constexpr bool verseWord(std::string_view words, std::string_view word) {
    if (word.empty()) return false;
    for (std::size_t at = 0; at < words.size();) {
        auto end = words.find(' ', at);
        if (end == words.npos) end = words.size();
        if (words.substr(at, end - at) == word) return true;
        at = end + 1;
    }
    return false;
}
constexpr bool verseIdentifier(char c) {
    return static_cast<unsigned char>(c) >= 128 || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
constexpr bool verseDigit(char c) { return c >= '0' && c <= '9'; }
constexpr bool verseHex(char c) {
    return verseDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
constexpr std::size_t verseSpace(std::string_view text, std::size_t at) {
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) ++at;
    return at;
}
constexpr TokenRole verseBuiltinWord(std::string_view word) {
    return verseWord(verseKeywords, word) ? TokenRole::keyword :
           verseWord(verseTypes, word) ? TokenRole::type : TokenRole::text;
}
// Emit borrows source byte ranges. The same scanner is evaluated by compile-time
// regression assertions, without generating or launching a native test program.
template <class Lookup, class Emit>
constexpr void scanVerse(std::string_view text, Lookup lookup, Emit emit) {
    struct Frame { char quote = 0; std::size_t braces = 0; };
    std::array<Frame, 64> frames{};
    std::size_t top = 0, at = 0;
    auto token = [&](std::size_t first, std::size_t last, TokenRole role) {
        if (last > first) emit(first, last - first, role);
    };
    auto starts = [&](std::size_t pos, std::string_view word) {
        return text.substr(pos, word.size()) == word;
    };
    auto newlineEnd = [&](std::size_t pos) {
        while (pos < text.size() && text[pos] != '\r' && text[pos] != '\n') ++pos;
        if (pos < text.size() && text[pos++] == '\r' && pos < text.size() && text[pos] == '\n') ++pos;
        return pos;
    };
    while (at < text.size()) {
        const auto first = at;
        if (frames[top].quote) {
            const auto quote = frames[top].quote;
            while (at < text.size()) {
                const auto c = text[at++];
                if (c == '\\' && at < text.size()) {
                    // Skip an escaped complete UTF-8 code point, including braces.
                    ++at;
                    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xc0) == 0x80) ++at;
                } else if (c == quote) { --top; break; }
                else if (quote == '"' && c == '{') {
                    if (top + 1 == frames.size()) { token(first, text.size(), TokenRole::string); return; }
                    frames[++top] = {0, 1}; break;
                }
            }
            token(first, at, TokenRole::string); continue;
        }
        if (starts(at, "<#>")) {
            // The following lines must be at least four columns beyond this
            // line's indentation. Dedenting restores ordinary Verse coloring.
            const auto line = at ? text.find_last_of("\r\n", at - 1) : text.npos;
            const auto begin = line == text.npos ? 0 : line + 1;
            std::size_t indent = 0;
            for (auto pos = begin; pos < at && (text[pos] == ' ' || text[pos] == '\t'); ++pos)
                indent += text[pos] == '\t' ? 4 - indent % 4 : 1;
            at = newlineEnd(at + 3);
            while (at < text.size()) {
                auto pos = at; std::size_t columns = 0;
                while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
                    columns += text[pos] == '\t' ? 4 - columns % 4 : 1; ++pos;
                }
                if (pos < text.size() && text[pos] != '\r' && text[pos] != '\n' && columns < indent + 4) break;
                at = newlineEnd(pos);
            }
            token(first, at, TokenRole::comment); continue;
        }
        if (starts(at, "<#")) {
            at += 2; std::size_t depth = 1;
            while (at < text.size() && depth) {
                if (starts(at, "<#")) { ++depth; at += 2; }
                else if (starts(at, "#>")) { --depth; at += 2; }
                else ++at;
            }
            token(first, at, TokenRole::comment); continue;
        }
        if (text[at] == '#') {
            while (at < text.size() && text[at] != '\r' && text[at] != '\n') ++at;
            token(first, at, TokenRole::comment); continue;
        }
        if (text[at] == '"' || text[at] == '\'') {
            if (top + 1 == frames.size()) { token(first, text.size(), TokenRole::string); return; }
            frames[++top] = {text[at++], 0};
            token(first, at, TokenRole::string); continue;
        }
        if (text[at] == '@' && at + 1 < text.size() && verseIdentifier(text[at + 1])) {
            at += 2; while (at < text.size() && verseIdentifier(text[at])) ++at;
            token(first, at, TokenRole::preprocessor); continue;
        }
        if (verseDigit(text[at])) {
            if (starts(at, "0x") || starts(at, "0X")) {
                at += 2; while (at < text.size() && verseHex(text[at])) ++at;
            } else {
                while (at < text.size() && verseDigit(text[at])) ++at;
                if (at + 1 < text.size() && text[at] == '.' && verseDigit(text[at + 1])) {
                    at += 2; while (at < text.size() && verseDigit(text[at])) ++at;
                }
                if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
                    auto end = at + 1;
                    if (end < text.size() && (text[end] == '+' || text[end] == '-')) ++end;
                    if (end < text.size() && verseDigit(text[end])) {
                        at = end + 1; while (at < text.size() && verseDigit(text[at])) ++at;
                    }
                }
            }
            token(first, at, TokenRole::number); continue;
        }
        if (verseIdentifier(text[at])) {
            ++at; while (at < text.size() && verseIdentifier(text[at])) ++at;
            const auto word = text.substr(first, at - first);
            auto role = lookup(word);
            const auto next = verseSpace(text, at);
            auto prev = first;
            while (prev && (text[prev - 1] == ' ' || text[prev - 1] == '\t')) --prev;
            if (role == TokenRole::text && prev && text[prev - 1] == '<' &&
                next < text.size() && text[next] == '>' && verseWord(verseSpecifiers, word)) role = TokenRole::keyword;
            auto call = next;
            // Access/effect modifiers can precede a declaration's argument list.
            while (call < text.size() && text[call] == '<' && call - at < 256) {
                const auto name = verseSpace(text, call + 1);
                auto end = name;
                while (end < text.size() && verseIdentifier(text[end])) ++end;
                const auto close = verseSpace(text, end);
                if (close >= text.size() || text[close] != '>' || !verseWord(verseSpecifiers, text.substr(name, end - name))) break;
                call = verseSpace(text, close + 1);
            }
            if (role == TokenRole::text && call < text.size() && text[call] == '(') role = TokenRole::function;
            token(first, at, role); continue;
        }
        if (text[at] == '{' && frames[top].braces) ++frames[top].braces;
        if (text[at] == '}' && frames[top].braces && --frames[top].braces == 0) {
            --top; ++at; token(first, at, TokenRole::string); continue;
        }
        ++at; token(first, at, TokenRole::text);
    }
}
} }

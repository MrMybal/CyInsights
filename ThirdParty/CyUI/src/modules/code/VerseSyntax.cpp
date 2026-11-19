#include "CodeSyntaxInternal.h"
#include "VerseSyntax.h"
#include <unordered_set>
namespace cyui { namespace detail {
std::vector<CodeToken> verseTokens(std::string_view text, const LanguageProfile &profile) {
    // Empty plugin word sets inherit Verse; nonempty sets replace that family.
    std::array<std::unordered_set<std::string_view>, 3> words;
    for (std::size_t index = 0; index < words.size() && index < profile.wordLists.size(); ++index) {
        const auto &list = profile.wordLists[index];
        for (std::size_t at = 0; at < list.size();) {
            const auto end = list.find(' ', at);
            const auto count = (end == list.npos ? list.size() : end) - at;
            if (count) words[index].insert(std::string_view(list).substr(at, count));
            if (end == list.npos) break;
            at = end + 1;
        }
    }
    auto contains = [&](std::size_t family, std::string_view word, std::string_view fallback) {
        return words[family].empty() ? verseWord(fallback, word) : words[family].count(word) != 0;
    };
    auto lookup = [&](std::string_view word) {
        if (contains(0, word, verseKeywords)) return TokenRole::keyword;
        if (contains(1, word, verseTypes)) return TokenRole::type;
        if (contains(2, word, {})) return TokenRole::function;
        return TokenRole::text;
    };
    std::vector<CodeToken> result;
    scanVerse(text, lookup, [&](std::size_t start, std::size_t length, TokenRole role) {
        if (!result.empty() && result.back().role == role && result.back().start + result.back().length == start)
            result.back().length += length;
        else result.push_back({start, length, role});
    });
    return result;
}
// Actual production scanner assertions: compile only, no executable fixture.
constexpr bool verseRole(std::string_view source, std::string_view word, TokenRole expected) {
    const auto offset = source.find(word);
    bool valid = offset != source.npos, found = false;
    std::size_t end = 0;
    scanVerse(source, verseBuiltinWord, [&](std::size_t start, std::size_t length, TokenRole role) {
        valid = valid && start == end && length && start <= source.size() && length <= source.size() - start;
        end = start + length;
        if (start <= offset && offset < end) found = role == expected;
    });
    return valid && found && end == source.size();
}
static_assert(verseRole("var Score:int = 10", "var", TokenRole::keyword));
static_assert(verseRole("var Score:int = 10", "int", TokenRole::type));
static_assert(verseRole("var Score:int = 10", "10", TokenRole::number));
static_assert(verseRole("Value := 0xFF + 1.25e-2", "0xFF", TokenRole::number));
static_assert(verseRole("Value := 0xFF + 1.25e-2", "1.25e-2", TokenRole::number));
static_assert(verseRole("for (Index := 1..10) {}", "..", TokenRole::text));
static_assert(verseRole("# var int\nreturn", "var", TokenRole::comment));
static_assert(verseRole("# var int\r\nreturn", "return", TokenRole::keyword));
static_assert(verseRole("<# outer <# inner #> still comment #> var", "still", TokenRole::comment));
static_assert(verseRole("<# outer <# inner #> still comment #> var", "var", TokenRole::keyword));
static_assert(verseRole("<# unfinished\nvar Score:int", "Score", TokenRole::comment));
static_assert(verseRole("<#>\r\n    var hidden:int\r\nvar Visible:int", "hidden", TokenRole::comment));
static_assert(verseRole("<#>\r\n    var hidden:int\r\nvar Visible:int", "Visible", TokenRole::text));
static_assert(verseRole("    <#>\n        hidden\n    return", "return", TokenRole::keyword));
static_assert(verseRole("@editable\nScore:int = 0", "@editable", TokenRole::preprocessor));
static_assert(verseRole("OnBegin<override>()<suspends>:void", "override", TokenRole::keyword));
static_assert(verseRole("OnBegin<override>()<suspends>:void", "suspends", TokenRole::keyword));
static_assert(verseRole("Print(\"hello\")", "Print", TokenRole::function));
static_assert(verseRole("Print(\"# not a comment\")", "not", TokenRole::string));
static_assert(verseRole("Print(\"Score {Score + 1}\")", "Score +", TokenRole::text));
static_assert(verseRole("Print(\"Score {Score + 1}\")", "1", TokenRole::number));
static_assert(verseRole("\"{Print(\"nested\")} tail\"", "nested", TokenRole::string));
static_assert(verseRole("\"{Print(\"nested\")} tail\"", "tail", TokenRole::string));
static_assert(verseRole("\"{array{1, 2}} tail\"", "tail", TokenRole::string));
static_assert(verseRole("\"escaped \\{var} \\\" still string\"", "var", TokenRole::string));
static_assert(verseRole("'{' # next", "{", TokenRole::string));
static_assert(verseRole("\"{ <# nested #> 42 }\"", "nested", TokenRole::comment));
static_assert(verseRole("var Caf\xc3\xa9:int # UTF-8 is preserved", "Caf\xc3\xa9", TokenRole::text));
static_assert(verseRole("OnBegin<override>()<suspends>:void", "OnBegin", TokenRole::function));
static_assert(verseRole("private = 1", "private", TokenRole::text));
static_assert(verseRole("\"unfinished <# not a comment", "not", TokenRole::string));
static_assert(verseRole("Print(\"\xc3\xa9 {true}\")", "true", TokenRole::keyword));
} }

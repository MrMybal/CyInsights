#include "cyui/modules/Spreadsheet.h"
#include "ModuleInternal.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <set>

namespace cyui {
namespace {
SpreadsheetValue error(const char *s) {
    SpreadsheetValue v;
    v.empty = false;
    v.error = s;
    return v;
}
SpreadsheetValue number(double n) {
    if (!std::isfinite(n))
        return error("#NUM!");
    SpreadsheetValue v;
    v.number = n;
    v.numeric = true;
    v.empty = false;
    return v;
}
SpreadsheetValue text(std::string s) {
    SpreadsheetValue v;
    v.empty = s.empty();
    v.text = std::move(s);
    return v;
}
bool numeric(const std::string &s, double &n) {
    if (s.empty())
        return false;
    const char *begin = s.data();
    const char *end = begin + s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;
    if (begin < end && *begin == '+')
        ++begin;
    const auto parsed = std::from_chars(begin, end, n);
    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(n);
}
std::string upper(std::string s) {
    for (auto &c : s)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string unquote(std::string s) {
    if (s.size() > 1 && s.front() == '\'' && s.back() == '\'') {
        s = s.substr(1, s.size() - 2);
        std::size_t at = 0;
        while ((at = s.find("''", at)) != std::string::npos)
            s.replace(at++, 2, "'");
    }
    return s;
}
std::size_t sheetIndex(const Workbook &book, const std::string &name) {
    for (std::size_t i = 0; i < book.sheets.size(); ++i)
        if (upper(book.sheets[i].name) == upper(unquote(name)))
            return i;
    return ~std::size_t(0);
}
using Values = std::vector<SpreadsheetValue>;
struct Evaluator;
struct Formula {
    Evaluator &eval;
    std::size_t sheet;
    const std::string &source;
    std::size_t at = 1, depth = 0;
    void ws() {
        while (at < source.size() && std::isspace(static_cast<unsigned char>(source[at])))
            ++at;
    }
    bool take(char c) {
        ws();
        if (at < source.size() && source[at] == c) {
            ++at;
            return true;
        }
        return false;
    }
    SpreadsheetValue scalar(const Values &v) {
        return v.size() == 1 ? v[0] : error("#VALUE!");
    }
    double val(const SpreadsheetValue &v) {
        double n = 0;
        if (v.numeric)
            return v.number;
        if (v.empty)
            return 0;
        if (numeric(v.text, n))
            return n;
        throw std::string("#VALUE!");
    }
    Values expression(int level = 0);
    Values primary();
    std::string name() {
        ws();
        std::string s;
        if (at < source.size() && source[at] == '\'') {
            s += source[at++];
            while (at < source.size()) {
                char c = source[at++];
                s += c;
                if (c == '\'') {
                    if (at < source.size() && source[at] == '\'')
                        s += source[at++];
                    else
                        break;
                }
            }
            return s;
        }
        while (at < source.size() && (std::isalnum(static_cast<unsigned char>(source[at])) ||
                                      source[at] == '_' || source[at] == '.' || source[at] == '$'))
            s += source[at++];
        return s;
    }
};
struct Evaluator {
    const Workbook &book;
    std::vector<std::vector<SpreadsheetValue>> values;
    std::vector<std::vector<unsigned char>> states;
    std::size_t operations = 0, stack = 0;
    explicit Evaluator(const Workbook &b) : book(b) {
        if (b.sheets.size() > 64)
            throw std::length_error("Workbook sheet limit");
        std::size_t count = 0;
        for (const auto &s : b.sheets) {
            count += s.grid.rows() * s.grid.columns();
            if (count > 1000000)
                throw std::length_error("Workbook cell limit");
            values.emplace_back(s.grid.rows() * s.grid.columns());
            states.emplace_back(values.back().size(), static_cast<unsigned char>(0));
        }
    }
    SpreadsheetValue cell(std::size_t sheet, GridAddress address) {
        if (++operations > 4000000)
            return error("#LIMIT!");
        if (sheet >= book.sheets.size())
            return error("#REF!");
        const auto &m = book.sheets[sheet].grid;
        if (address.row >= m.rows() || address.column >= m.columns())
            return {};
        auto index = address.row * m.columns() + address.column;
        if (states[sheet][index] == 2)
            return values[sheet][index];
        if (states[sheet][index] == 1)
            return error("#CYCLE!");
        if (stack > 64)
            return error("#LIMIT!");
        states[sheet][index] = 1;
        ++stack;
        SpreadsheetValue value;
        const auto &input = m.cell(address.row, address.column);
        const auto &raw = input.text;
        if (input.literal)
            value = text(raw);
        else if (raw.size() > 8192 && raw.front() == '=')
            value = error("#LIMIT!");
        else if (!raw.empty() && raw.front() == '=') {
            try {
                Formula parser{*this, sheet, raw};
                auto parsed = parser.expression();
                parser.ws();
                value = parser.at == raw.size() ? parser.scalar(parsed) : error("#VALUE!");
            } catch (const std::string &e) {
                value = error(e.c_str());
            }
        } else {
            double n = 0;
            value = numeric(raw, n) ? number(n) : text(!raw.empty() && raw[0] == '\'' ? raw.substr(1) : raw);
        }
        --stack;
        states[sheet][index] = 2;
        return values[sheet][index] = value;
    }
};
Values Formula::primary() {
    ws();
    if (++depth > 64)
        throw std::string("#LIMIT!");
    struct Guard {
        std::size_t &n;
        ~Guard() {
            --n;
        }
    } guard{depth};
    if (take('(')) {
        auto result = expression();
        if (!take(')'))
            throw std::string("#VALUE!");
        return result;
    }
    if (take('+'))
        return {number(val(scalar(primary())))};
    if (take('-')) {
        auto v = scalar(primary());
        if (!v.error.empty())
            return {v};
        return {number(-val(v))};
    }
    if (at >= source.size())
        throw std::string("#VALUE!");
    if (source[at] == '\"') {
        ++at;
        std::string s;
        bool closed = false;
        while (at < source.size()) {
            char c = source[at++];
            if (c == '\"') {
                if (at < source.size() && source[at] == '\"') {
                    s += '\"';
                    ++at;
                } else {
                    closed = true;
                    break;
                }
            } else
                s += c;
        }
        if (!closed)
            throw std::string("#VALUE!");
        return {text(s)};
    }
    if (std::isdigit(static_cast<unsigned char>(source[at])) || source[at] == '.') {
        double n = 0;
        auto parsed = std::from_chars(source.data() + at, source.data() + source.size(), n);
        if (parsed.ec == std::errc::result_out_of_range)
            throw std::string("#NUM!");
        if (parsed.ec != std::errc{})
            throw std::string("#VALUE!");
        at = std::size_t(parsed.ptr - source.data());
        auto v = number(n);
        if (take('%'))
            v = number(v.number / 100);
        return {v};
    }
    if (source.compare(at, 5, "#REF!") == 0) {
        at += 5;
        return {error("#REF!")};
    }
    auto identifier = name();
    if (identifier.empty())
        throw std::string("#VALUE!");
    auto function = upper(identifier);
    if (take('(')) {
        std::vector<Values> args;
        if (!take(')')) {
            do {
                args.push_back(expression());
            } while (take(',') || take(';'));
            if (!take(')'))
                throw std::string("#VALUE!");
        }
        if (function == "IF") {
            if (args.size() < 2 || args.size() > 3)
                return {error("#VALUE!")};
            auto test = scalar(args[0]);
            if (!test.error.empty())
                return {test};
            return val(test) != 0 ? args[1] : args.size() == 3 ? args[2] : Values{number(0)};
        }
        if (function != "SUM" && function != "AVERAGE" && function != "MIN" && function != "MAX" &&
            function != "COUNT" && function != "COUNTA" && function != "ABS" && function != "ROUND")
            return {error("#NAME?")};
        std::vector<double> numbers;
        std::size_t nonempty = 0;
        for (const auto &arg : args)
            for (const auto &v : arg) {
                if (!v.error.empty())
                    return {v};
                if (!v.empty)
                    ++nonempty;
                if (v.numeric)
                    numbers.push_back(v.number);
                else if (arg.size() == 1 && !v.empty) {
                    double n = 0;
                    if (numeric(v.text, n))
                        numbers.push_back(n);
                }
            }
        if (function == "COUNT")
            return {number(double(numbers.size()))};
        if (function == "COUNTA")
            return {number(double(nonempty))};
        if (function == "SUM" || function == "AVERAGE" || function == "MIN" || function == "MAX") {
            double sum = 0;
            for (double n : numbers)
                sum += n;
            if (function == "SUM")
                return {number(sum)};
            if (numbers.empty())
                return {function == "AVERAGE" ? error("#DIV/0!") : number(0)};
            if (function == "AVERAGE")
                return {number(sum / numbers.size())};
            return {number(function == "MIN" ? *std::min_element(numbers.begin(), numbers.end())
                                             : *std::max_element(numbers.begin(), numbers.end()))};
        }
        if (function == "ABS" && args.size() == 1)
            return {number(std::abs(val(scalar(args[0]))))};
        if (function == "ROUND" && args.size() == 2) {
            double digits = val(scalar(args[1]));
            if (std::abs(digits) > 15)
                return {error("#NUM!")};
            double scale = std::pow(10, std::trunc(digits));
            return {number(std::round(val(scalar(args[0])) * scale) / scale)};
        }
        return {error("#NAME?")};
    }
    if (function == "TRUE")
        return {number(1)};
    if (function == "FALSE")
        return {number(0)};
    std::size_t target = sheet;
    if (take('!')) {
        target = sheetIndex(eval.book, identifier);
        identifier = name();
    }
    GridAddress a, b;
    if (!gridParseAddress(identifier, a))
        return {error("#NAME?")};
    if (take(':')) {
        auto last = name();
        if (take('!'))
            last = name();
        if (!gridParseAddress(last, b))
            return {error("#REF!")};
        GridRange range{a, b};
        range = range.normalized();
        if (range.last.row - range.first.row > 100000 || range.last.column - range.first.column > 256)
            return {error("#LIMIT!")};
        Values out;
        for (std::size_t r = range.first.row; r <= range.last.row; ++r)
            for (std::size_t c = range.first.column; c <= range.last.column; ++c) {
                if (out.size() >= 1000000)
                    return {error("#LIMIT!")};
                out.push_back(eval.cell(target, {r, c}));
            }
        return out;
    }
    return {eval.cell(target, a)};
}
Values Formula::expression(int level) {
    if (++depth > 128)
        throw std::string("#LIMIT!");
    struct Guard {
        std::size_t &n;
        ~Guard() {
            --n;
        }
    } guard{depth};
    if (level == 5) {
        auto result = primary();
        while (take('%')) {
            auto value = scalar(result);
            if (!value.error.empty())
                return {value};
            result = {number(val(value) / 100)};
        }
        return result;
    }
    auto left = expression(level + 1);
    while (true) {
        ws();
        std::string op;
        if (at < source.size()) {
            char c = source[at];
            if (level == 0 && (c == '=' || c == '<' || c == '>')) {
                op += c;
                if (at + 1 < source.size() && (source[at + 1] == '=' || (c == '<' && source[at + 1] == '>')))
                    op += source[at + 1];
            }
            if (level == 1 && c == '&')
                op = "&";
            if (level == 2 && (c == '+' || c == '-'))
                op = c;
            if (level == 3 && (c == '*' || c == '/'))
                op = c;
            if (level == 4 && c == '^')
                op = c;
        }
        if (op.empty())
            break;
        at += op.size();
        auto right = expression(level == 4 ? level : level + 1);
        auto a = scalar(left), b = scalar(right);
        if (!a.error.empty()) {
            left = {a};
            continue;
        }
        if (!b.error.empty()) {
            left = {b};
            continue;
        }
        if (op == "&")
            left = {text(a.display() + b.display())};
        else if (level == 0) {
            int cmp;
            if (a.numeric && b.numeric)
                cmp = a.number < b.number ? -1 : a.number > b.number ? 1 : 0;
            else
                cmp = a.display().compare(b.display());
            left = {number(op == "="    ? cmp == 0
                           : op == "<>" ? cmp != 0
                           : op == "<"  ? cmp < 0
                           : op == ">"  ? cmp > 0
                           : op == "<=" ? cmp <= 0
                                        : cmp >= 0)};
        } else {
            double x = val(a), y = val(b);
            left = {op == "+"   ? number(x + y)
                    : op == "-" ? number(x - y)
                    : op == "*" ? number(x * y)
                    : op == "/" ? (y == 0 ? error("#DIV/0!") : number(x / y))
                                : number(std::pow(x, y))};
        }
    }
    return left;
}
const std::regex
    reference(R"((?:(?:'((?:[^']|'')+)'|([A-Za-z_][A-Za-z0-9_ ]*))!)?(\$?[A-Za-z]{1,3}\$?[1-9][0-9]{0,6}))");
std::string qualifier(const std::cmatch &match) {
    return match[1].matched ? "'" + match[1].str() + "'" : match[2].str();
}
template <class Fn> std::string references(const std::string &source, Fn replace) {
    std::string out;
    std::size_t at = 0;
    std::size_t rangeEnd = std::string::npos;
    std::string scope;
    bool string = false;
    while (at < source.size()) {
        if (source[at] == '\"') {
            out += source[at++];
            if (string && at < source.size() && source[at] == '\"') {
                out += source[at++];
                continue;
            }
            string = !string;
            continue;
        }
        if (!string && (source[at] == '[' || source[at] == ']'))
            return source; // External workbook and structured references are preserved.
        std::cmatch match;
        if (!string &&
            (at == 0 ||
             (!std::isalnum(static_cast<unsigned char>(source[at - 1])) && source[at - 1] != '_')) &&
            std::regex_search(source.c_str() + at, match, reference,
                              std::regex_constants::match_continuous)) {
            std::size_t end = at + match.length();
            if (end == source.size() || (!std::isalnum(static_cast<unsigned char>(source[end])) &&
                                         source[end] != '_' && source[end] != '(')) {
                // An unqualified second endpoint belongs to the first endpoint's sheet.
                if (at != rangeEnd || match[1].matched || match[2].matched)
                    scope = qualifier(match);
                out += replace(match, scope);
                auto colon = source.find_first_not_of(" \t\r\n", end);
                rangeEnd = colon != std::string::npos && source[colon] == ':'
                               ? source.find_first_not_of(" \t\r\n", colon + 1)
                               : std::string::npos;
                at = end;
                continue;
            }
        }
        out += source[at++];
    }
    return out;
}
template <class Fn> void rewriteFormulas(Workbook &book, Fn replace) {
    for (std::size_t sheet = 0; sheet < book.sheets.size(); ++sheet) {
        auto &m = book.sheets[sheet].grid;
        auto cells = m.cells();
        bool changed = false;
        for (auto &cell : cells)
            if (!cell.literal && !cell.text.empty() && cell.text[0] == '=') {
                auto formula = references(cell.text, [&](const std::cmatch &match, const std::string &scope) {
                    return replace(sheet, match, scope);
                });
                if (formula != cell.text) {
                    cell.text = std::move(formula);
                    changed = true;
                }
            }
        if (changed)
            m.assign(m.rows(), m.columns(), cells);
    }
}
void shiftReferences(Workbook &book, std::size_t target, int op, std::size_t at, std::size_t count) {
    rewriteFormulas(book, [&](std::size_t sheet, const std::cmatch &match, const std::string &scope) {
        const auto refSheet = scope.empty() ? sheet : sheetIndex(book, scope);
        if (refSheet != target)
            return match.str();
        auto ref = match[3].str();
        GridAddress addr;
        if (!gridParseAddress(ref, addr))
            return match.str();
        auto &index = std::abs(op) == 1 ? addr.row : addr.column;
        if (index < at)
            return match.str();
        if (op < 0 && index - at < count)
            return std::string("#REF!");
        index = op > 0 ? index + count : index - count;
        if (addr.row >= 1048576 || addr.column >= 16384)
            return std::string("#REF!");
        const bool ac = ref.front() == '$', ar = ref.find('$', 1) != std::string::npos;
        auto changed = (ac ? "$" : "") + gridColumnName(addr.column) + (ar ? "$" : "") +
                       std::to_string(addr.row + 1);
        return match.str().substr(0, match.length() - ref.size()) + changed;
    });
}
void remember(SpreadsheetState &s, Workbook before) {
    s.undo.push_back(std::move(before));
    s.redo.clear();
    while (s.undo.size() > 32)
        s.undo.erase(s.undo.begin());
    std::size_t cells = 0;
    for (auto i = s.undo.rbegin(); i != s.undo.rend(); ++i) {
        for (const auto &sheet : i->sheets)
            cells += sheet.grid.rows() * sheet.grid.columns();
        if (cells > 500000) {
            s.undo.erase(s.undo.begin(), i.base());
            break;
        }
    }
}
} // namespace
std::string SpreadsheetValue::display(int decimals) const {
    if (!error.empty())
        return error;
    if (!numeric)
        return text;
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    if (decimals >= 0)
        stream << std::fixed << std::setprecision(std::clamp(decimals, 0, 15));
    else
        stream << std::setprecision(15);
    stream << number;
    return stream.str();
}
std::vector<std::vector<SpreadsheetValue>> evaluateWorkbook(const Workbook &b) {
    Evaluator eval(b);
    for (std::size_t s = 0; s < b.sheets.size(); ++s)
        for (std::size_t r = 0; r < b.sheets[s].grid.rows(); ++r)
            for (std::size_t c = 0; c < b.sheets[s].grid.columns(); ++c)
                eval.cell(s, {r, c});
    return eval.values;
}
bool spreadsheetStructure(Workbook &b, std::size_t sheet, int op, std::size_t at, std::size_t count) {
    if (sheet >= b.sheets.size() || !count || op < -2 || op > 2 || op == 0)
        return false;
    auto next = b;
    auto &m = next.sheets[sheet].grid;
    bool changed = op == 1    ? m.insertRows(at, count)
                   : op == 2  ? m.insertColumns(at, count)
                   : op == -1 ? m.removeRows(at, count)
                              : m.removeColumns(at, count);
    if (!changed)
        return false;
    shiftReferences(next, sheet, op, at, count);
    b = std::move(next);
    return true;
}
bool spreadsheetRenameSheet(Workbook &book, std::size_t sheet, const std::string &name) {
    if (sheet >= book.sheets.size() || name.empty() || name.size() > 31 ||
        name.find_first_of("[]:*?/\\") != std::string::npos || name.front() == '\'' || name.back() == '\'' ||
        name == book.sheets[sheet].name)
        return false;
    const auto existing = sheetIndex(book, name);
    if (existing < book.sheets.size() && existing != sheet)
        return false;
    auto next = book;
    const auto old = upper(next.sheets[sheet].name);
    std::string escaped = "'";
    for (char c : name) {
        escaped += c;
        if (c == '\'')
            escaped += c;
    }
    escaped += "'!";
    rewriteFormulas(next, [&](std::size_t, const std::cmatch &m, const std::string &scope) {
        if ((m[1].matched || m[2].matched) && upper(unquote(scope)) == old)
            return escaped + m[3].str();
        return m.str();
    });
    next.sheets[sheet].name = name;
    book = std::move(next);
    return true;
}
std::string spreadsheetFormulaFromODF(const std::string &s) {
    auto at = s.find(":=");
    std::string out = "=";
    std::string input = at == std::string::npos ? (s.empty()     ? ""
                                                   : s[0] == '=' ? s.substr(1)
                                                                 : s)
                                                : s.substr(at + 2);
    bool str = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '\"')
            str = !str;
        if (!str && c == '[') {
            auto end = input.find(']', i);
            if (end == std::string::npos)
                return "=#REF!";
            auto ref = input.substr(i + 1, end - i - 1);
            std::string cleaned;
            bool quoted = false;
            for (std::size_t p = 0; p < ref.size(); ++p) {
                char ch = ref[p];
                if (ch == '\'')
                    quoted = !quoted;
                if (ch == '.' && !quoted) {
                    if (p == 0 || ref[p - 1] == ':')
                        continue;
                    cleaned += '!';
                } else
                    cleaned += ch;
            }
            if (!cleaned.empty() && cleaned.front() == '$' && cleaned.find('!') != std::string::npos)
                cleaned.erase(0, 1);
            out += cleaned;
            i = end;
        } else
            out += !str && c == ';' ? ',' : c;
    }
    return out;
}
std::string spreadsheetFormulaToODF(const std::string &s) {
    if (s.empty() || s[0] != '=')
        return s;
    auto converted = references(s.substr(1), [](const std::cmatch &m, const std::string &) {
        auto name = qualifier(m);
        return "[" + (name.empty() ? "." : name + ".") + m[3].str() + "]";
    });
    std::size_t at = 0;
    while ((at = converted.find("]:[", at)) != std::string::npos)
        converted.replace(at, 3, ":");
    bool string = false;
    for (char &c : converted) {
        if (c == '\"')
            string = !string;
        if (c == ',' && !string)
            c = ';';
    }
    return "of:=" + converted;
}
std::string spreadsheetTranslateFormula(const std::string &formula, std::int64_t rows, std::int64_t columns) {
    if (formula.empty() || formula[0] != '=')
        return formula;
    return references(formula, [&](const std::cmatch &match, const std::string &) {
        auto ref = match[3].str();
        GridAddress a;
        if (!gridParseAddress(ref, a))
            return match.str();
        const bool absoluteColumn = ref.front() == '$', absoluteRow = ref.find('$', 1) != std::string::npos;
        const auto row = std::int64_t(a.row) + (absoluteRow ? 0 : rows),
                   column = std::int64_t(a.column) + (absoluteColumn ? 0 : columns);
        if (row < 0 || column < 0 || row >= 1048576 || column >= 16384)
            return std::string("#REF!");
        return match.str().substr(0, match.length() - ref.size()) + (absoluteColumn ? "$" : "") +
               gridColumnName(std::size_t(column)) + (absoluteRow ? "$" : "") + std::to_string(row + 1);
    });
}
bool spreadsheetUndo(Workbook &b, SpreadsheetState &s) {
    if (s.undo.empty())
        return false;
    s.redo.push_back(std::move(b));
    b = std::move(s.undo.back());
    s.undo.pop_back();
    s.grid.editing = false;
    s.formulaSheet = ~std::size_t(0);
    s.revisions.clear();
    return true;
}
bool spreadsheetRedo(Workbook &b, SpreadsheetState &s) {
    if (s.redo.empty())
        return false;
    s.undo.push_back(std::move(b));
    b = std::move(s.redo.back());
    s.redo.pop_back();
    s.grid.editing = false;
    s.formulaSheet = ~std::size_t(0);
    s.revisions.clear();
    return true;
}
bool spreadsheet(const char *id, SpreadsheetState &s, Workbook &book, ImVec2 size,
                 const SpreadsheetOptions &o) {
    ScopedID scope{id};
    if (book.sheets.empty())
        book.sheets.push_back({});
    s.sheet = std::min(s.sheet, book.sheets.size() - 1);
    if (size.x <= 0)
        size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0)
        size.y = dp(480);
    ImGui::BeginChild("##spreadsheet", size);
    bool changed = false;
    std::unique_ptr<Workbook> before;
    auto backup = [&] {
        if (!before)
            before.reset(new Workbook(book));
    };
    auto rollback = [&](const char *message) {
        book = std::move(*before);
        s.sheet = std::min(s.sheet, book.sheets.size() - 1);
        s.grid = {};
        s.grid.error = message;
        s.revisions.clear();
        s.formulaSheet = ~std::size_t(0);
    };
    auto action = [&](auto fn) {
        backup();
        try {
            fn();
            changed = true;
        } catch (const std::exception &e) {
            s.grid.error = e.what();
        }
    };
    if (o.toolbar) {
        ImGui::BeginDisabled(o.readOnly);
        if (ghostButton(trLabel("Add sheet").c_str()) && book.sheets.size() < 64)
            action([&] {
                SpreadsheetSheet sheet;
                sheet.name = "Sheet" + std::to_string(book.sheets.size() + 1);
                while (sheetIndex(book, sheet.name) != ~std::size_t(0))
                    sheet.name += "_";
                book.sheets.push_back(std::move(sheet));
                s.sheet = book.sheets.size() - 1;
                s.requestedSheet = s.sheet;
                s.grid = {};
            });
        ImGui::SameLine();
        if (ghostButton(trLabel("Undo").c_str()))
            changed |= spreadsheetUndo(book, s);
        ImGui::SameLine();
        if (ghostButton(trLabel("Redo").c_str()))
            changed |= spreadsheetRedo(book, s);
        ImGui::EndDisabled();
    }
    if (ImGui::BeginTabBar("##sheets")) {
        for (std::size_t n = 0; n < book.sheets.size(); ++n) {
            ScopedID sheetKey{int(n)};
            bool open = true;
            if (ImGui::BeginTabItem((book.sheets[n].name + "###sheet").c_str(),
                                    o.readOnly || book.sheets.size() == 1 ? nullptr : &open,
                                    s.requestedSheet == n ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (s.sheet != n) {
                    s.sheet = n;
                    s.grid = {};
                    s.formulaActive = false;
                }
                ImGui::EndTabItem();
            }
            if (!open) {
                action([&] { book.sheets.erase(book.sheets.begin() + n); });
                s.sheet = std::min(s.sheet, book.sheets.size() - 1);
                s.grid = {};
                break;
            }
            if (ImGui::BeginPopupContextItem("##sheetName")) {
                if (!o.readOnly) {
                    detail::inputText(trLabel("Sheet name").c_str(), s.sheetName);
                    if (primaryButton(trLabel("Rename").c_str())) {
                        backup();
                        try {
                            if (spreadsheetRenameSheet(book, n, s.sheetName)) {
                                changed = true;
                                ImGui::CloseCurrentPopup();
                            }
                        } catch (const std::exception &error) {
                            s.grid.error = error.what();
                        }
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndTabBar();
    }
    if (s.requestedSheet < book.sheets.size()) {
        s.sheet = s.requestedSheet;
        s.requestedSheet = ~std::size_t(0);
        s.grid = {};
        s.formulaActive = false;
    }
    auto &model = book.sheets[s.sheet].grid;
    s.grid.current.row = std::min(s.grid.current.row, model.rows() - 1);
    s.grid.current.column = std::min(s.grid.current.column, model.columns() - 1);
    if ((!s.formulaActive && !(s.formulaCell == s.grid.current)) || s.formulaSheet != s.sheet) {
        s.formulaCell = s.grid.current;
        s.formulaSheet = s.sheet;
        s.formula = model.cell(s.formulaCell.row, s.formulaCell.column).text;
    }
    textDim("%s", gridAddressName(s.grid.current).c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-dp(1));
    ImGui::BeginDisabled(o.readOnly);
    bool submit = detail::inputText("##formula", s.formula,
                                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoUndoRedo);
    s.formulaActive = ImGui::IsItemActive();
    if (submit || ImGui::IsItemDeactivatedAfterEdit())
        action([&] { model.setText(s.formulaCell.row, s.formulaCell.column, s.formula); });
    ImGui::EndDisabled();
    std::vector<std::uint64_t> revisions;
    for (const auto &sh : book.sheets)
        revisions.push_back(sh.grid.revision());
    if (s.revisions != revisions) {
        try {
            s.values = evaluateWorkbook(book);
        } catch (const std::exception &error) {
            s.values.clear();
            s.grid.error = error.what();
        }
        s.revisions = revisions;
    }
    GridOptions gridOptions;
    gridOptions.readOnly = o.readOnly;
    gridOptions.toolbar = o.toolbar;
    gridOptions.history = false;
    gridOptions.clipboard = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && s.grid.hasSelection &&
        !s.grid.editing && !s.formulaActive) {
        const auto &io = ImGui::GetIO();
        if (io.KeyCtrl || io.KeySuper) {
            if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                auto range = s.grid.selection();
                s.copyOrigin = range.first;
                s.copyRows = range.last.row - range.first.row + 1;
                s.copyColumns = range.last.column - range.first.column + 1;
                s.copiedText = model.copy(range);
                s.copiedCells.clear();
                for (std::size_t row = range.first.row; row <= range.last.row; ++row)
                    for (std::size_t column = range.first.column; column <= range.last.column; ++column)
                        s.copiedCells.push_back(model.cell(row, column));
                ImGui::SetClipboardText(s.copiedText.c_str());
            }
            if (!o.readOnly && ImGui::IsKeyPressed(ImGuiKey_V)) {
                const char *clipboard = ImGui::GetClipboardText();
                if (clipboard)
                    action([&] {
                        auto destination = s.grid.selection().first;
                        if (s.copyRows && s.copyColumns &&
                            s.copiedCells.size() == s.copyRows * s.copyColumns && s.copiedText == clipboard) {
                            const auto rows = std::max(model.rows(), destination.row + s.copyRows),
                                       columns =
                                           std::max(model.columns(), destination.column + s.copyColumns);
                            if (rows > 100000 || columns > 256 || rows > 1000000 / columns)
                                throw std::length_error("Clipboard worksheet limit");
                            std::vector<GridCell> cells(rows * columns);
                            for (std::size_t r = 0; r < model.rows(); ++r)
                                for (std::size_t c = 0; c < model.columns(); ++c)
                                    cells[r * columns + c] = model.cell(r, c);
                            for (std::size_t r = 0; r < s.copyRows; ++r)
                                for (std::size_t c = 0; c < s.copyColumns; ++c) {
                                    auto cell = s.copiedCells[r * s.copyColumns + c];
                                    if (!cell.literal)
                                        cell.text = spreadsheetTranslateFormula(
                                            cell.text,
                                            std::int64_t(destination.row) - std::int64_t(s.copyOrigin.row),
                                            std::int64_t(destination.column) -
                                                std::int64_t(s.copyOrigin.column));
                                    cells[(r + destination.row) * columns + c + destination.column] =
                                        std::move(cell);
                                }
                            model.assign(rows, columns, cells);
                        } else
                            model.paste(destination, clipboard);
                        s.formulaSheet = ~std::size_t(0);
                    });
            }
        }
    }
    gridOptions.beforeChange = backup;
    gridOptions.display = [&](std::size_t r, std::size_t c, const GridCell &cell) {
        return s.sheet < s.values.size() && r * model.columns() + c < s.values[s.sheet].size()
                   ? s.values[s.sheet][r * model.columns() + c].display(cell.decimals)
                   : cell.text;
    };
    const auto edit = dataGrid("##data", s.grid, model, ImGui::GetContentRegionAvail(), gridOptions);
    if (edit.changed) {
        changed = true;
        if (edit.structure) {
            try {
                shiftReferences(book, s.sheet, edit.structure, edit.at, edit.count);
            } catch (const std::exception &error) {
                rollback(error.what());
                ImGui::EndChild();
                return false;
            }
        }
        s.formulaSheet = ~std::size_t(0);
    }
    if (!o.readOnly && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !s.grid.editing) {
        const auto &io = ImGui::GetIO();
        if (io.KeyCtrl || io.KeySuper) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z))
                changed |= io.KeyShift ? spreadsheetRedo(book, s) : spreadsheetUndo(book, s);
            if (ImGui::IsKeyPressed(ImGuiKey_Y))
                changed |= spreadsheetRedo(book, s);
        }
    }
    if (before && changed) {
        std::size_t cells = 0;
        for (const auto &sheet : book.sheets)
            cells += sheet.grid.rows() * sheet.grid.columns();
        if (cells > 1000000 || book.sheets.size() > 64) {
            rollback("Workbook cell/sheet limit");
            ImGui::EndChild();
            return false;
        }
        remember(s, std::move(*before));
        for (auto &sh : book.sheets)
            sh.grid.clearHistory();
        s.revisions.clear();
    }
    ImGui::EndChild();
    return changed;
}
} // namespace cyui

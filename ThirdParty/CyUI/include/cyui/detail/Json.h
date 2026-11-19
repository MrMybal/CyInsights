#pragma once
// Internal bounded JSON codec shared by optional modules. Numbers keep their
// decimal representation so IDs and signed 64-bit properties never pass through double.
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cyui::detail {
struct Json {
    enum Kind { null, boolean, number, string, array, object } kind = null;
    std::string text;
    std::vector<Json> items;
    std::map<std::string, Json> fields;
    static Json str(std::string s) {
        Json j;
        j.kind = string;
        j.text = std::move(s);
        return j;
    }
    static Json num(std::string s) {
        Json j;
        j.kind = number;
        j.text = std::move(s);
        return j;
    }
    static Json num(long double n) {
        if (!std::isfinite(n))
            throw std::invalid_argument("Non-finite number");
        std::ostringstream s;
        s.imbue(std::locale::classic());
        s << std::setprecision(21) << n;
        return num(s.str());
    }
    static Json num(double n) {
        if (!std::isfinite(n))
            throw std::invalid_argument("Non-finite number");
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), n);
        if (result.ec != std::errc())
            throw std::invalid_argument("Number is out of range");
        return num(std::string(buffer, result.ptr));
    }
    static Json num(float n) {
        if (!std::isfinite(n))
            throw std::invalid_argument("Non-finite number");
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), n);
        if (result.ec != std::errc())
            throw std::invalid_argument("Number is out of range");
        return num(std::string(buffer, result.ptr));
    }
    static Json flag(bool b) {
        Json j;
        j.kind = boolean;
        j.text = b ? "true" : "false";
        return j;
    }
    static Json list() {
        Json j;
        j.kind = array;
        return j;
    }
    static Json obj() {
        Json j;
        j.kind = object;
        return j;
    }
    const Json &get(const std::string &key) const {
        auto i = fields.find(key);
        if (kind != object || i == fields.end())
            throw std::invalid_argument("Missing JSON field: " + key);
        return i->second;
    }
    std::string str() const {
        if (kind != string)
            throw std::invalid_argument("Expected JSON string");
        return text;
    }
    bool flag() const {
        if (kind != boolean)
            throw std::invalid_argument("Expected JSON boolean");
        return text == "true";
    }
    long double num() const {
        if (kind != number)
            throw std::invalid_argument("Expected JSON number");
        std::istringstream s(text);
        s.imbue(std::locale::classic());
        long double n = 0;
        s >> n;
        if (!s || !std::isfinite(n))
            throw std::invalid_argument("Invalid number");
        return n;
    }
    template <class T> T integer() const {
        if (kind != number)
            throw std::invalid_argument("Expected JSON integer");
        T n{};
        auto r = std::from_chars(text.data(), text.data() + text.size(), n);
        if (r.ec != std::errc() || r.ptr != text.data() + text.size())
            throw std::invalid_argument("Invalid integer");
        return n;
    }
};
inline std::string jsonQuote(std::string_view s) {
    std::string out = "\"";
    const char *hex = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += char(c);
    }
    return out + '"';
}
inline std::string jsonDump(const Json &j) {
    if (j.kind == Json::string)
        return jsonQuote(j.text);
    if (j.kind == Json::null)
        return "null";
    if (j.kind == Json::number || j.kind == Json::boolean)
        return j.text;
    std::string out = j.kind == Json::array ? "[" : "{";
    bool first = true;
    auto add = [&](const std::string &s) {
        if (!first)
            out += ',';
        first = false;
        out += s;
    };
    if (j.kind == Json::array)
        for (const auto &v : j.items)
            add(jsonDump(v));
    else
        for (const auto &v : j.fields)
            add(jsonQuote(v.first) + ":" + jsonDump(v.second));
    return out + (j.kind == Json::array ? "]" : "}");
}
class JsonParser {
    std::string_view s;
    std::size_t at = 0, nodes = 0;
    [[noreturn]] void fail() const {
        throw std::invalid_argument("Invalid JSON");
    }
    void space() {
        while (at < s.size() && (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t'))
            ++at;
    }
    bool take(char c) {
        space();
        if (at < s.size() && s[at] == c) {
            ++at;
            return true;
        }
        return false;
    }
    unsigned hex() {
        if (at + 4 > s.size())
            fail();
        unsigned n = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s[at++];
            unsigned d = c >= '0' && c <= '9'   ? unsigned(c - '0')
                         : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
                         : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10)
                                                : 99;
            if (d > 15)
                fail();
            n = n * 16 + d;
        }
        return n;
    }
    void utf8(std::string &out, unsigned c) {
        if (c < 128)
            out += char(c);
        else if (c < 2048) {
            out += char(192 | (c >> 6));
            out += char(128 | (c & 63));
        } else if (c < 65536) {
            out += char(224 | (c >> 12));
            out += char(128 | ((c >> 6) & 63));
            out += char(128 | (c & 63));
        } else {
            out += char(240 | (c >> 18));
            out += char(128 | ((c >> 12) & 63));
            out += char(128 | ((c >> 6) & 63));
            out += char(128 | (c & 63));
        }
    }
    std::string string() {
        if (!take('"'))
            fail();
        std::string out;
        while (at < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[at++]);
            if (c == '"')
                return out;
            if (c < 32)
                fail();
            if (c != '\\') {
                out += char(c);
                continue;
            }
            if (at == s.size())
                fail();
            char e = s[at++];
            switch (e) {
            case '"':
            case '\\':
            case '/':
                out += e;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                unsigned u = hex();
                if (u >= 0xd800 && u <= 0xdbff) {
                    if (at + 2 > s.size() || s[at++] != '\\' || s[at++] != 'u')
                        fail();
                    unsigned lo = hex();
                    if (lo < 0xdc00 || lo > 0xdfff)
                        fail();
                    u = 0x10000 + ((u - 0xd800) << 10) + lo - 0xdc00;
                } else if (u >= 0xdc00 && u <= 0xdfff)
                    fail();
                utf8(out, u);
                break;
            }
            default:
                fail();
            }
        }
        fail();
    }
    Json value(int depth) {
        if (depth > 32 || ++nodes > 65536)
            throw std::invalid_argument("JSON budget exceeded");
        space();
        if (at == s.size())
            fail();
        if (s[at] == '"')
            return Json::str(string());
        if (take('{')) {
            auto j = Json::obj();
            if (take('}'))
                return j;
            do {
                auto key = string();
                if (!take(':'))
                    fail();
                if (!j.fields.emplace(key, value(depth + 1)).second)
                    fail();
            } while (take(','));
            if (!take('}'))
                fail();
            return j;
        }
        if (take('[')) {
            auto j = Json::list();
            if (take(']'))
                return j;
            do {
                j.items.push_back(value(depth + 1));
            } while (take(','));
            if (!take(']'))
                fail();
            return j;
        }
        for (auto token : {"true", "false", "null"}) {
            std::string_view t(token);
            if (s.substr(at, t.size()) == t) {
                at += t.size();
                if (t == "null")
                    return {};
                return Json::flag(t == "true");
            }
        }
        auto start = at;
        if (s[at] == '-')
            ++at;
        if (at == s.size())
            fail();
        if (s[at] == '0')
            ++at;
        else {
            if (s[at] < '1' || s[at] > '9')
                fail();
            while (at < s.size() && s[at] >= '0' && s[at] <= '9')
                ++at;
        }
        if (at < s.size() && s[at] == '.') {
            auto begin = ++at;
            while (at < s.size() && s[at] >= '0' && s[at] <= '9')
                ++at;
            if (at == begin)
                fail();
        }
        if (at < s.size() && (s[at] == 'e' || s[at] == 'E')) {
            ++at;
            if (at < s.size() && (s[at] == '+' || s[at] == '-'))
                ++at;
            auto begin = at;
            while (at < s.size() && s[at] >= '0' && s[at] <= '9')
                ++at;
            if (at == begin)
                fail();
        }
        return Json::num(std::string(s.substr(start, at - start)));
    }

  public:
    explicit JsonParser(std::string_view text) : s(text) {}
    Json run() {
        if (s.size() > 1024 * 1024)
            throw std::invalid_argument("JSON exceeds 1 MiB");
        auto j = value(0);
        space();
        if (at != s.size())
            fail();
        return j;
    }
};
inline Json jsonParse(std::string_view s) {
    return JsonParser(s).run();
}
} // namespace cyui::detail

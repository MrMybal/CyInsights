#include "cyui/Numeric.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace cyui {
namespace {
class Expression {
    std::string_view text;
    std::size_t at = 0;
    int depth = 0;
    long double original;
    void space() {
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at])))
            ++at;
    }
    bool take(char c) {
        space();
        if (at < text.size() && text[at] == c) {
            ++at;
            return true;
        }
        return false;
    }
    [[noreturn]] void fail(const char *error) const {
        throw std::invalid_argument(error);
    }
    long double finite(long double n) {
        if (!std::isfinite(n))
            fail("Non-finite result");
        return n;
    }
    long double sum() {
        auto n = product();
        for (;;) {
            if (take('+'))
                n = finite(n + product());
            else if (take('-'))
                n = finite(n - product());
            else
                return n;
        }
    }
    long double product() {
        auto n = unary();
        for (;;) {
            if (take('*'))
                n = finite(n * unary());
            else if (take('/')) {
                auto b = unary();
                if (b == 0)
                    fail("Division by zero");
                n = finite(n / b);
            } else if (take('%')) {
                auto b = unary();
                if (b == 0)
                    fail("Division by zero");
                n = finite(std::fmod(n, b));
            } else
                return n;
        }
    }
    long double unary() {
        if (++depth > 32)
            fail("Expression is too deeply nested");
        long double n;
        if (take('+'))
            n = unary();
        else if (take('-'))
            n = -unary();
        else {
            n = primary();
            if (take('^'))
                n = finite(std::pow(n, unary()));
        }
        --depth;
        return n;
    }
    long double primary() {
        if (take('(')) {
            auto n = sum();
            if (!take(')'))
                fail("Expected closing parenthesis");
            return n;
        }
        space();
        auto start = at;
        if (at < text.size() && std::isalpha(static_cast<unsigned char>(text[at]))) {
            while (at < text.size() && std::isalpha(static_cast<unsigned char>(text[at])))
                ++at;
            std::string name(text.substr(start, at - start));
            for (char &c : name)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            if (name == "value")
                return original;
            if (name == "pi")
                return std::acos(-1.L);
            if (name == "e")
                return std::exp(1.L);
            if (!take('('))
                fail("Unknown identifier");
            std::vector<long double> args{sum()};
            while (take(',')) {
                if (args.size() >= 3)
                    fail("Too many arguments");
                args.push_back(sum());
            }
            if (!take(')'))
                fail("Expected closing parenthesis");
            if (args.size() == 1) {
                auto a = args[0];
                if (name == "abs")
                    return std::abs(a);
                if (name == "sqrt")
                    return finite(std::sqrt(a));
                if (name == "round")
                    return std::round(a);
                if (name == "floor")
                    return std::floor(a);
                if (name == "ceil")
                    return std::ceil(a);
            }
            if (args.size() == 2 && name == "min")
                return std::min(args[0], args[1]);
            if (args.size() == 2 && name == "max")
                return std::max(args[0], args[1]);
            if (args.size() == 3 && name == "clamp" && args[1] <= args[2])
                return std::clamp(args[0], args[1], args[2]);
            fail("Unknown function or invalid arguments");
        }
        bool digits = false;
        while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) {
            ++at;
            digits = true;
        }
        if (at < text.size() && text[at] == '.') {
            ++at;
            while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) {
                ++at;
                digits = true;
            }
        }
        if (!digits)
            fail("Expected a number or Value");
        if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
            ++at;
            if (at < text.size() && (text[at] == '+' || text[at] == '-'))
                ++at;
            auto exponent = at;
            while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])))
                ++at;
            if (at == exponent)
                fail("Invalid exponent");
        }
        std::istringstream input(std::string(text.substr(start, at - start)));
        input.imbue(std::locale::classic());
        long double n = 0;
        input >> n;
        if (!input)
            fail("Number is out of range");
        if (at < text.size() && (text[at] == 'k' || text[at] == 'K')) {
            ++at;
            n *= 1000;
        }
        return finite(n);
    }

  public:
    Expression(std::string_view s, long double value) : text(s), original(value) {}
    long double run() {
        if (text.size() > 4096)
            fail("Expression is too long");
        auto n = sum();
        space();
        if (at != text.size())
            fail("Unexpected input");
        return finite(n);
    }
};
} // namespace
NumericExpressionResult evaluateNumericExpression(std::string_view expression, long double value) {
    try {
        return {true, Expression(expression, value).run(), {}};
    } catch (const std::invalid_argument &error) {
        return {false, 0, error.what()};
    }
}
} // namespace cyui

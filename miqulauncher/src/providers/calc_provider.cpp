#include "calc_provider.hpp"
#include <miqutoolkit/system/app_manager.hpp>
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

namespace miqu {

namespace {

enum class TokenType {
    Number,
    Plus,
    Minus,
    Multiply,
    Divide,
    Modulo,
    Power,
    LParen,
    RParen,
    Identifier,
    End
};

struct Token {
    TokenType type;
    double value = 0.0;
    std::string text;
};

class Lexer {
public:
    explicit Lexer(std::string input) : m_input(std::move(input)), m_pos(0) {}

    Token next_token() {
        skip_whitespace();
        if (m_pos >= m_input.size()) {
            return {TokenType::End, 0.0, ""};
        }

        char c = m_input[m_pos];

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            return read_number();
        }

        m_pos++;
        switch (c) {
            case '+': return {TokenType::Plus, 0.0, "+"};
            case '-': return {TokenType::Minus, 0.0, "-"};
            case '*': return {TokenType::Multiply, 0.0, "*"};
            case '/': return {TokenType::Divide, 0.0, "/"};
            case '%': return {TokenType::Modulo, 0.0, "%"};
            case '^': return {TokenType::Power, 0.0, "^"};
            case '(': return {TokenType::LParen, 0.0, "("};
            case ')': return {TokenType::RParen, 0.0, ")"};
            default: break;
        }

        if (std::isalpha(static_cast<unsigned char>(c))) {
            m_pos--;
            return read_identifier();
        }

        return {TokenType::End, 0.0, ""};
    }

private:
    void skip_whitespace() {
        while (m_pos < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_pos]))) {
            m_pos++;
        }
    }

    Token read_number() {
        size_t start = m_pos;
        bool has_dot = false;
        while (m_pos < m_input.size()) {
            char c = m_input[m_pos];
            if (std::isdigit(static_cast<unsigned char>(c))) {
                m_pos++;
            } else if (c == '.' && !has_dot) {
                has_dot = true;
                m_pos++;
            } else {
                break;
            }
        }
        std::string num_str = m_input.substr(start, m_pos - start);
        double val = 0.0;
        try {
            val = std::stod(num_str);
        } catch (...) {
            return {TokenType::End, 0.0, ""};
        }
        return {TokenType::Number, val, num_str};
    }

    Token read_identifier() {
        size_t start = m_pos;
        while (m_pos < m_input.size() && std::isalpha(static_cast<unsigned char>(m_input[m_pos]))) {
            m_pos++;
        }
        std::string id = m_input.substr(start, m_pos - start);
        std::transform(id.begin(), id.end(), id.begin(), ::tolower);
        return {TokenType::Identifier, 0.0, id};
    }

    std::string m_input;
    size_t m_pos;
};

class Parser {
public:
    explicit Parser(Lexer lexer) : m_lexer(std::move(lexer)) {
        advance();
    }

    bool parse(double& result) {
        try {
            result = parse_expression();
            return m_current.type == TokenType::End;
        } catch (...) {
            return false;
        }
    }

private:
    void advance() {
        m_current = m_lexer.next_token();
    }

    double parse_expression() {
        double val = parse_term();
        while (m_current.type == TokenType::Plus || m_current.type == TokenType::Minus) {
            TokenType op = m_current.type;
            advance();
            double right = parse_term();
            if (op == TokenType::Plus) val += right;
            else val -= right;
        }
        return val;
    }

    double parse_term() {
        double val = parse_factor();
        while (m_current.type == TokenType::Multiply || m_current.type == TokenType::Divide || m_current.type == TokenType::Modulo) {
            TokenType op = m_current.type;
            advance();
            double right = parse_factor();
            if (op == TokenType::Multiply) {
                val *= right;
            } else if (op == TokenType::Divide) {
                if (std::abs(right) < 1e-12) throw std::runtime_error("div0");
                val /= right;
            } else {
                if (std::abs(right) < 1e-12) throw std::runtime_error("mod0");
                val = std::fmod(val, right);
            }
        }
        return val;
    }

    double parse_factor() {
        double val = parse_unary();
        if (m_current.type == TokenType::Power) {
            advance();
            double right = parse_factor();
            val = std::pow(val, right);
        }
        return val;
    }

    double parse_unary() {
        if (m_current.type == TokenType::Plus) {
            advance();
            return parse_unary();
        }
        if (m_current.type == TokenType::Minus) {
            advance();
            return -parse_unary();
        }
        return parse_primary();
    }

    double parse_primary() {
        if (m_current.type == TokenType::Number) {
            double v = m_current.value;
            advance();
            if (m_current.type == TokenType::Modulo) {
                // Suffix percentage, e.g. 15% -> 0.15
                advance();
                v /= 100.0;
            }
            return v;
        }

        if (m_current.type == TokenType::Identifier) {
            std::string func = m_current.text;
            advance();
            if (func == "pi") return M_PI;
            if (func == "e") return M_E;

            if (m_current.type != TokenType::LParen) {
                throw std::runtime_error("expected lparen");
            }
            advance();
            double arg = parse_expression();
            if (m_current.type != TokenType::RParen) {
                throw std::runtime_error("expected rparen");
            }
            advance();

            if (func == "sqrt") {
                if (arg < 0) throw std::runtime_error("sqrt neg");
                return std::sqrt(arg);
            }
            if (func == "cbrt") return std::cbrt(arg);
            if (func == "abs") return std::abs(arg);
            if (func == "sin") return std::sin(arg);
            if (func == "cos") return std::cos(arg);
            if (func == "tan") return std::tan(arg);
            if (func == "log" || func == "ln") {
                if (arg <= 0) throw std::runtime_error("log neg");
                return std::log(arg);
            }
            if (func == "log10") {
                if (arg <= 0) throw std::runtime_error("log neg");
                return std::log10(arg);
            }
            if (func == "exp") return std::exp(arg);
            if (func == "ceil") return std::ceil(arg);
            if (func == "floor") return std::floor(arg);
            if (func == "round") return std::round(arg);

            throw std::runtime_error("unknown func");
        }

        if (m_current.type == TokenType::LParen) {
            advance();
            double v = parse_expression();
            if (m_current.type != TokenType::RParen) {
                throw std::runtime_error("missing rparen");
            }
            advance();
            return v;
        }

        throw std::runtime_error("syntax error");
    }

    Lexer m_lexer;
    Token m_current;
};

} // namespace

bool CalcProvider::is_math_query(const std::string& query) {
    if (query.empty()) return false;
    if (query[0] == '=') return true;

    bool has_digit = false;
    bool has_operator = false;

    for (size_t i = 0; i < query.size(); ++i) {
        char c = query[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            has_digit = true;
        } else if (c == '+' || c == '*' || c == '/' || c == '^' || c == '%' || c == '(' || c == ')') {
            has_operator = true;
        } else if (c == '-') {
            if (i > 0) has_operator = true;
        }
    }

    if (!has_digit) return false;

    // Check for math keywords
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("sqrt") != std::string::npos ||
        lower.find("sin") != std::string::npos ||
        lower.find("cos") != std::string::npos ||
        lower.find("tan") != std::string::npos ||
        lower.find("abs") != std::string::npos ||
        lower.find("log") != std::string::npos ||
        lower.find("pi") != std::string::npos) {
        return true;
    }

    return has_operator;
}

std::string CalcProvider::format_number(double val) {
    if (std::isnan(val)) return "NaN";
    if (std::isinf(val)) return val > 0 ? "Infinity" : "-Infinity";

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << val;
    std::string str = ss.str();

    // Strip trailing zeros after decimal point
    size_t dot = str.find('.');
    if (dot != std::string::npos) {
        while (str.back() == '0') {
            str.pop_back();
        }
        if (str.back() == '.') {
            str.pop_back();
        }
    }
    return str;
}

std::optional<LauncherItem> CalcProvider::evaluate(const std::string& query) {
    if (!is_math_query(query)) {
        return std::nullopt;
    }

    std::string expr = query;
    if (!expr.empty() && expr[0] == '=') {
        expr = expr.substr(1);
    }

    // Trim whitespace
    size_t first = expr.find_first_not_of(" \t");
    if (first == std::string::npos) return std::nullopt;
    size_t last = expr.find_last_not_of(" \t");
    expr = expr.substr(first, last - first + 1);

    Lexer lexer(expr);
    Parser parser(std::move(lexer));
    double result = 0.0;
    if (!parser.parse(result)) {
        return std::nullopt;
    }

    std::string formatted = format_number(result);

    LauncherItem item;
    item.id = "calc:" + formatted;
    item.title = "= " + formatted;
    item.subtitle = expr + " (Press Enter to copy to clipboard)";
    item.icon_name = "accessories-calculator";
    item.badge = "Calc";
    item.category = ItemCategory::Calc;
    item.return_value = formatted;
    return item;
}

void CalcProvider::activate(const LauncherItem& item) {
    if (item.return_value.empty()) return;

    // Copy result to clipboard via wl-copy or xclip
    std::string copy_cmd = "printf '%s' " + item.return_value + " | (wl-copy 2>/dev/null || xclip -selection clipboard 2>/dev/null)";
    AppManager::launch_command(copy_cmd);
}

} // namespace miqu

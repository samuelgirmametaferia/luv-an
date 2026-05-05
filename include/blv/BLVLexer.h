#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <unordered_map>

namespace blv {

enum class TokenKind {
    // Literals
    IDENT, STRING, INT, FLOAT, BOOL_TRUE, BOOL_FALSE,

    // Keywords
    LET, IF, ELSE, EXTERN, FUNC, VERIFY, RETURN,

    // Symbols
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, DOT, SEMI, COLON, ARROW, EQ, EQEQ, NEQ,
    LT, GT, LTE, GTE, PLUS, MINUS, STAR, SLASH, BANG,
    AND, OR, PIPE,

    // Intrinsics (@ prefixed)
    AT_REGION, AT_MAP, AT_RESERVE, AT_COMBINE,
    AT_LOAD, AT_RESOLVE, AT_RELOC, AT_PATCH, AT_INSPECT,
    AT_POLICY, AT_ENFORCE, AT_FILTER,
    AT_BUILD, AT_EMIT, AT_LINK, AT_DOWNLOAD, AT_EXISTS,
    AT_EXEC, AT_ALLOC, AT_READ, AT_APPEND, AT_SLICE, AT_SIZEOF,
    AT_TARGET, AT_HOST, AT_PRINTLN, AT_EXIT,
    AT_CLI,

    // Special
    NEWLINE, EOF_TOK, INVALID
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int col;
    Token() : kind(TokenKind::INVALID), line(0), col(0) {}
    Token(TokenKind k, std::string t, int l, int c) : kind(k), text(std::move(t)), line(l), col(c) {}
};

class BLVLexer {
public:
    BLVLexer(const std::string& source, const std::string& filename = "<input>")
        : src_(source), pos_(0), line_(1), col_(1), filename_(filename) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < src_.size()) {
            skipWhitespace();
            if (pos_ >= src_.size()) break;

            char c = peek();

            // Comments
            if (c == '#') {
                skipLineComment();
                continue;
            }

            // Newline
            if (c == '\n') {
                tokens.emplace_back(TokenKind::NEWLINE, "\\n", line_, col_);
                advance();
                continue;
            }

            // Strings
            if (c == '"') {
                tokens.push_back(lexString());
                continue;
            }

            // Numbers
            if (isdigit(c)) {
                tokens.push_back(lexNumber());
                continue;
            }

            // @ intrinsics
            if (c == '@') {
                tokens.push_back(lexIntrinsic());
                continue;
            }

            // Identifiers and keywords
            if (isalpha(c) || c == '_') {
                tokens.push_back(lexIdentOrKeyword());
                continue;
            }

            // Operators & punctuation
            tokens.push_back(lexSymbol());
        }
        tokens.emplace_back(TokenKind::EOF_TOK, "", line_, col_);
        return tokens;
    }

private:
    std::string src_;
    size_t pos_;
    int line_, col_;
    std::string filename_;

    char peek() { return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char peek2() { return (pos_ + 1) < src_.size() ? src_[pos_ + 1] : '\0'; }
    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { line_++; col_ = 1; } else { col_++; }
        return c;
    }

    void skipWhitespace() {
        while (pos_ < src_.size() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) advance();
    }

    void skipLineComment() {
        while (pos_ < src_.size() && peek() != '\n') advance();
    }

    Token lexString() {
        int sl = line_, sc = col_;
        advance(); // consume "
        std::string s;
        while (pos_ < src_.size() && peek() != '"') {
            if (peek() == '\\') {
                advance();
                char esc = advance();
                switch (esc) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case 'e': s += '\x1b'; break;
                    case '\\': s += '\\'; break;
                    case '"': s += '"'; break;
                    default: s += esc; break;
                }
            } else {
                s += advance();
            }
        }
        if (pos_ < src_.size()) advance(); // consume closing "
        return {TokenKind::STRING, s, sl, sc};
    }

    Token lexNumber() {
        int sl = line_, sc = col_;
        std::string n;
        bool isFloat = false;
        while (pos_ < src_.size() && (isdigit(peek()) || peek() == '.')) {
            if (peek() == '.') {
                if (isFloat) break;
                isFloat = true;
            }
            n += advance();
        }
        // Hex support
        if (n == "0" && pos_ < src_.size() && (peek() == 'x' || peek() == 'X')) {
            n += advance();
            while (pos_ < src_.size() && isxdigit(peek())) n += advance();
        }
        return {isFloat ? TokenKind::FLOAT : TokenKind::INT, n, sl, sc};
    }

    Token lexIntrinsic() {
        int sl = line_, sc = col_;
        advance(); // consume @
        std::string name;
        while (pos_ < src_.size() && (isalnum(peek()) || peek() == '_')) {
            name += advance();
        }
        // Map to known intrinsics
        static const std::unordered_map<std::string, TokenKind> intrinsics = {
            {"region", TokenKind::AT_REGION}, {"map", TokenKind::AT_MAP},
            {"reserve", TokenKind::AT_RESERVE}, {"combine", TokenKind::AT_COMBINE},
            {"load", TokenKind::AT_LOAD}, {"resolve", TokenKind::AT_RESOLVE},
            {"reloc", TokenKind::AT_RELOC}, {"patch", TokenKind::AT_PATCH},
            {"inspect", TokenKind::AT_INSPECT}, {"policy", TokenKind::AT_POLICY},
            {"enforce", TokenKind::AT_ENFORCE}, {"filter", TokenKind::AT_FILTER},
            {"build", TokenKind::AT_BUILD}, {"emit", TokenKind::AT_EMIT},
            {"link", TokenKind::AT_LINK}, {"download", TokenKind::AT_DOWNLOAD},
            {"exists", TokenKind::AT_EXISTS}, {"exec", TokenKind::AT_EXEC},
            {"alloc", TokenKind::AT_ALLOC}, {"read", TokenKind::AT_READ},
            {"append", TokenKind::AT_APPEND}, {"slice", TokenKind::AT_SLICE},
            {"sizeof", TokenKind::AT_SIZEOF},
            {"target", TokenKind::AT_TARGET}, {"host", TokenKind::AT_HOST},
            {"println", TokenKind::AT_PRINTLN}, {"exit", TokenKind::AT_EXIT},
            {"cli", TokenKind::AT_CLI},
        };
        auto it = intrinsics.find(name);
        if (it != intrinsics.end()) return {it->second, "@" + name, sl, sc};
        return {TokenKind::IDENT, "@" + name, sl, sc}; // Unknown @ treated as ident
    }

    Token lexIdentOrKeyword() {
        int sl = line_, sc = col_;
        std::string id;
        while (pos_ < src_.size() && (isalnum(peek()) || peek() == '_')) {
            id += advance();
        }
        if (id == "let") return {TokenKind::LET, id, sl, sc};
        if (id == "if") return {TokenKind::IF, id, sl, sc};
        if (id == "else") return {TokenKind::ELSE, id, sl, sc};
        if (id == "extern") return {TokenKind::EXTERN, id, sl, sc};
        if (id == "func") return {TokenKind::FUNC, id, sl, sc};
        if (id == "verify") return {TokenKind::VERIFY, id, sl, sc};
        if (id == "return") return {TokenKind::RETURN, id, sl, sc};
        if (id == "true") return {TokenKind::BOOL_TRUE, id, sl, sc};
        if (id == "false") return {TokenKind::BOOL_FALSE, id, sl, sc};
        return {TokenKind::IDENT, id, sl, sc};
    }

    Token lexSymbol() {
        int sl = line_, sc = col_;
        char c = advance();
        switch (c) {
            case '(': return {TokenKind::LPAREN, "(", sl, sc};
            case ')': return {TokenKind::RPAREN, ")", sl, sc};
            case '{': return {TokenKind::LBRACE, "{", sl, sc};
            case '}': return {TokenKind::RBRACE, "}", sl, sc};
            case '[': return {TokenKind::LBRACKET, "[", sl, sc};
            case ']': return {TokenKind::RBRACKET, "]", sl, sc};
            case ',': return {TokenKind::COMMA, ",", sl, sc};
            case '.': return {TokenKind::DOT, ".", sl, sc};
            case ';': return {TokenKind::SEMI, ";", sl, sc};
            case ':': return {TokenKind::COLON, ":", sl, sc};
            case '+': return {TokenKind::PLUS, "+", sl, sc};
            case '*': return {TokenKind::STAR, "*", sl, sc};
            case '/': return {TokenKind::SLASH, "/", sl, sc};
            case '|':
                if (peek() == '|') { advance(); return {TokenKind::OR, "||", sl, sc}; }
                return {TokenKind::PIPE, "|", sl, sc};
            case '&':
                if (peek() == '&') { advance(); return {TokenKind::AND, "&&", sl, sc}; }
                return {TokenKind::INVALID, "&", sl, sc};
            case '=':
                if (peek() == '=') { advance(); return {TokenKind::EQEQ, "==", sl, sc}; }
                return {TokenKind::EQ, "=", sl, sc};
            case '!':
                if (peek() == '=') { advance(); return {TokenKind::NEQ, "!=", sl, sc}; }
                return {TokenKind::BANG, "!", sl, sc};
            case '<':
                if (peek() == '=') { advance(); return {TokenKind::LTE, "<=", sl, sc}; }
                return {TokenKind::LT, "<", sl, sc};
            case '>':
                if (peek() == '=') { advance(); return {TokenKind::GTE, ">=", sl, sc}; }
                return {TokenKind::GT, ">", sl, sc};
            case '-':
                if (peek() == '>') { advance(); return {TokenKind::ARROW, "->", sl, sc}; }
                return {TokenKind::MINUS, "-", sl, sc};
            default:
                return {TokenKind::INVALID, std::string(1, c), sl, sc};
        }
    }
};

} // namespace blv

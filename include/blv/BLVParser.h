#pragma once
#include "BLVLexer.h"
#include <memory>
#include <variant>
#include <unordered_map>

namespace blv {

// ─────────────────────────────────────────────────────────
//  AST Node Types
// ─────────────────────────────────────────────────────────

struct ASTNode {
    int line = 0, col = 0;
    virtual ~ASTNode() = default;
};

// ── Expressions ──
struct Expr : ASTNode {};

struct StringLit : Expr { std::string value; };
struct IntLit : Expr { int64_t value; };
struct FloatLit : Expr { double value; };
struct BoolLit : Expr { bool value; };
struct Ident : Expr { std::string name; };

struct BinaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> left, right;
};

struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> operand;
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct DotExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string member;
};

struct IntrinsicExpr : Expr {
    std::string name; // e.g. "region", "load", "build"
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::string> flags; // for @build -target wasm32 etc.
};

struct CliExpr : Expr {
    std::string method; // "get" or "has"
    std::string key;
};

struct PolicyRule : Expr {
    std::string rule; // e.g. "deny.sys_calls"
};

// ── Statements ──
struct Stmt : ASTNode {};

struct LetStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> value;
};

struct VerifyStmt : Stmt {
    std::unique_ptr<Expr> condition;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> thenBody;
    std::vector<std::unique_ptr<Stmt>> elseBody;
};

struct ExternStmt : Stmt {
    std::string libPath;
    std::string funcName;
    std::vector<std::string> params;
    std::string returnType;
};

struct Block : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

struct Program : ASTNode {
    std::vector<std::unique_ptr<Stmt>> statements;
};

// ─────────────────────────────────────────────────────────
//  Parser
// ─────────────────────────────────────────────────────────
class BLVParser {
public:
    BLVParser(std::vector<Token> tokens, const std::string& filename = "<input>")
        : tokens_(std::move(tokens)), pos_(0), filename_(filename) {}

    std::unique_ptr<Program> parse() {
        auto prog = std::make_unique<Program>();
        skipNewlines();
        while (!atEnd()) {
            auto stmt = parseStatement();
            if (stmt) prog->statements.push_back(std::move(stmt));
            skipNewlines();
        }
        return prog;
    }

private:
    std::vector<Token> tokens_;
    size_t pos_;
    std::string filename_;

    bool isIntrinsicToken(TokenKind k) const {
        switch (k) {
            case TokenKind::AT_REGION:
            case TokenKind::AT_MAP:
            case TokenKind::AT_RESERVE:
            case TokenKind::AT_COMBINE:
            case TokenKind::AT_LOAD:
            case TokenKind::AT_RESOLVE:
            case TokenKind::AT_RELOC:
            case TokenKind::AT_PATCH:
            case TokenKind::AT_INSPECT:
            case TokenKind::AT_POLICY:
            case TokenKind::AT_ENFORCE:
            case TokenKind::AT_FILTER:
            case TokenKind::AT_BUILD:
            case TokenKind::AT_EMIT:
            case TokenKind::AT_LINK:
            case TokenKind::AT_DOWNLOAD:
            case TokenKind::AT_EXISTS:
            case TokenKind::AT_EXEC:
            case TokenKind::AT_ALLOC:
            case TokenKind::AT_READ:
            case TokenKind::AT_APPEND:
            case TokenKind::AT_SLICE:
            case TokenKind::AT_SIZEOF:
            case TokenKind::AT_TARGET:
            case TokenKind::AT_HOST:
            case TokenKind::AT_PRINTLN:
            case TokenKind::AT_EXIT:
                return true;
            default:
                return false;
        }
    }

    // ── Token helpers ──
    const Token& cur() { return tokens_[pos_]; }
    bool atEnd() { return pos_ >= tokens_.size() || cur().kind == TokenKind::EOF_TOK; }
    
    bool check(TokenKind k) { return !atEnd() && cur().kind == k; }
    
    bool match(TokenKind k) {
        if (check(k)) { pos_++; return true; }
        return false;
    }

    Token consume(TokenKind k, const std::string& msg) {
        if (check(k)) { Token t = cur(); pos_++; return t; }
        error(msg);
        return {};
    }

    void skipNewlines() {
        while (!atEnd() && cur().kind == TokenKind::NEWLINE) pos_++;
    }

    void error(const std::string& msg) {
        auto& t = cur();
        std::cerr << "\033[1;31m[BLV Error]\033[0m " << filename_ << ":" << t.line << ":" << t.col
                  << " — " << msg << " (got '" << t.text << "')" << std::endl;
    }

    // ── Statement parsing ──
    std::unique_ptr<Stmt> parseStatement() {
        skipNewlines();
        if (atEnd()) return nullptr;

        if (check(TokenKind::LET)) return parseLetStmt();
        if (check(TokenKind::VERIFY)) return parseVerifyStmt();
        if (check(TokenKind::IF)) return parseIfStmt();
        if (check(TokenKind::EXTERN)) return parseExternStmt();

        // Otherwise treat as expression statement
        auto expr = parseExpr();
        if (!expr) return nullptr;
        auto s = std::make_unique<ExprStmt>();
        s->expr = std::move(expr);
        s->line = s->expr->line;
        s->col = s->expr->col;
        return s;
    }

    std::unique_ptr<Stmt> parseLetStmt() {
        auto s = std::make_unique<LetStmt>();
        s->line = cur().line; s->col = cur().col;
        consume(TokenKind::LET, "expected 'let'");
        Token name = consume(TokenKind::IDENT, "expected variable name");
        s->name = name.text;
        consume(TokenKind::EQ, "expected '='");
        s->value = parseExpr();
        return s;
    }

    std::unique_ptr<Stmt> parseVerifyStmt() {
        auto s = std::make_unique<VerifyStmt>();
        s->line = cur().line; s->col = cur().col;
        consume(TokenKind::VERIFY, "expected 'verify'");
        s->condition = parseExpr();
        return s;
    }

    std::unique_ptr<Stmt> parseIfStmt() {
        auto s = std::make_unique<IfStmt>();
        s->line = cur().line; s->col = cur().col;
        consume(TokenKind::IF, "expected 'if'");
        consume(TokenKind::LPAREN, "expected '('");
        s->condition = parseExpr();
        consume(TokenKind::RPAREN, "expected ')'");
        consume(TokenKind::LBRACE, "expected '{'");
        skipNewlines();
        while (!atEnd() && !check(TokenKind::RBRACE)) {
            auto stmt = parseStatement();
            if (stmt) s->thenBody.push_back(std::move(stmt));
            skipNewlines();
        }
        consume(TokenKind::RBRACE, "expected '}'");
        skipNewlines();
        if (match(TokenKind::ELSE)) {
            consume(TokenKind::LBRACE, "expected '{'");
            skipNewlines();
            while (!atEnd() && !check(TokenKind::RBRACE)) {
                auto stmt = parseStatement();
                if (stmt) s->elseBody.push_back(std::move(stmt));
                skipNewlines();
            }
            consume(TokenKind::RBRACE, "expected '}'");
        }
        return s;
    }

    std::unique_ptr<Stmt> parseExternStmt() {
        auto s = std::make_unique<ExternStmt>();
        s->line = cur().line; s->col = cur().col;
        consume(TokenKind::EXTERN, "expected 'extern'");
        Token lib = consume(TokenKind::STRING, "expected library path string");
        s->libPath = lib.text;
        consume(TokenKind::FUNC, "expected 'func'");
        Token fn = consume(TokenKind::IDENT, "expected function name");
        s->funcName = fn.text;
        consume(TokenKind::LPAREN, "expected '('");
        while (!atEnd() && !check(TokenKind::RPAREN)) {
            Token p = consume(TokenKind::IDENT, "expected parameter name");
            s->params.push_back(p.text);
            if (!check(TokenKind::RPAREN)) consume(TokenKind::COMMA, "expected ','");
        }
        consume(TokenKind::RPAREN, "expected ')'");
        if (match(TokenKind::ARROW)) {
            Token rt = consume(TokenKind::IDENT, "expected return type");
            s->returnType = rt.text;
        }
        return s;
    }

    // ── Expression parsing (precedence climbing) ──
    std::unique_ptr<Expr> parseExpr() { return parseOr(); }

    std::unique_ptr<Expr> parseOr() {
        auto left = parseAnd();
        while (check(TokenKind::OR)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseAnd();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseAnd() {
        auto left = parseEquality();
        while (check(TokenKind::AND)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseEquality();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseEquality() {
        auto left = parseComparison();
        while (check(TokenKind::EQEQ) || check(TokenKind::NEQ)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseComparison();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseComparison() {
        auto left = parseAdditive();
        while (check(TokenKind::LT) || check(TokenKind::GT) ||
               check(TokenKind::LTE) || check(TokenKind::GTE)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseAdditive();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseAdditive() {
        auto left = parseMultiplicative();
        while (check(TokenKind::PLUS) || check(TokenKind::MINUS)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseMultiplicative();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseMultiplicative() {
        auto left = parseUnary();
        while (check(TokenKind::STAR) || check(TokenKind::SLASH)) {
            auto e = std::make_unique<BinaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->left = std::move(left);
            e->right = parseUnary();
            left = std::move(e);
        }
        return left;
    }

    std::unique_ptr<Expr> parseUnary() {
        if (check(TokenKind::BANG) || check(TokenKind::MINUS)) {
            auto e = std::make_unique<UnaryExpr>();
            e->line = cur().line; e->col = cur().col;
            e->op = cur().text; pos_++;
            e->operand = parseUnary();
            return e;
        }
        return parsePostfix();
    }

    std::unique_ptr<Expr> parsePostfix() {
        auto expr = parsePrimary();
        while (true) {
            if (check(TokenKind::DOT)) {
                pos_++;
                Token member = consume(TokenKind::IDENT, "expected member name after '.'");
                if (check(TokenKind::LPAREN)) {
                    // method call: obj.method(args)
                    pos_++;
                    auto call = std::make_unique<CallExpr>();
                    call->line = member.line; call->col = member.col;
                    auto dot = std::make_unique<DotExpr>();
                    dot->object = std::move(expr);
                    dot->member = member.text;
                    call->callee = std::move(dot);
                    while (!atEnd() && !check(TokenKind::RPAREN)) {
                        call->args.push_back(parseExpr());
                        if (!check(TokenKind::RPAREN)) consume(TokenKind::COMMA, "expected ','");
                    }
                    consume(TokenKind::RPAREN, "expected ')'");
                    expr = std::move(call);
                } else {
                    auto d = std::make_unique<DotExpr>();
                    d->line = member.line; d->col = member.col;
                    d->object = std::move(expr);
                    d->member = member.text;
                    expr = std::move(d);
                }
            } else if (check(TokenKind::LPAREN)) {
                pos_++;
                auto call = std::make_unique<CallExpr>();
                call->line = expr->line; call->col = expr->col;
                call->callee = std::move(expr);
                while (!atEnd() && !check(TokenKind::RPAREN)) {
                    call->args.push_back(parseExpr());
                    if (!check(TokenKind::RPAREN)) consume(TokenKind::COMMA, "expected ','");
                }
                consume(TokenKind::RPAREN, "expected ')'");
                expr = std::move(call);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> parsePrimary() {
        if (atEnd()) { error("unexpected end of input"); return nullptr; }
        auto& t = cur();

        // Intrinsics
        if (isIntrinsicToken(t.kind)) {
            return parseIntrinsicExpr();
        }

        // @cli special
        if (t.kind == TokenKind::AT_CLI) {
            return parseCliExpr();
        }

        // Literals
        if (t.kind == TokenKind::STRING) {
            auto e = std::make_unique<StringLit>();
            e->value = t.text; e->line = t.line; e->col = t.col;
            pos_++;
            return e;
        }
        if (t.kind == TokenKind::INT) {
            auto e = std::make_unique<IntLit>();
            e->line = t.line; e->col = t.col;
            // Handle hex
            if (t.text.size() > 2 && t.text[1] == 'x') {
                e->value = std::stoll(t.text, nullptr, 16);
            } else {
                e->value = std::stoll(t.text);
            }
            pos_++;
            return e;
        }
        if (t.kind == TokenKind::FLOAT) {
            auto e = std::make_unique<FloatLit>();
            e->value = std::stod(t.text); e->line = t.line; e->col = t.col;
            pos_++;
            return e;
        }
        if (t.kind == TokenKind::BOOL_TRUE || t.kind == TokenKind::BOOL_FALSE) {
            auto e = std::make_unique<BoolLit>();
            e->value = (t.kind == TokenKind::BOOL_TRUE); e->line = t.line; e->col = t.col;
            pos_++;
            return e;
        }

        // Identifier
        if (t.kind == TokenKind::IDENT) {
            auto e = std::make_unique<Ident>();
            e->name = t.text; e->line = t.line; e->col = t.col;
            pos_++;
            return e;
        }

        // Grouping
        if (t.kind == TokenKind::LPAREN) {
            pos_++;
            auto e = parseExpr();
            consume(TokenKind::RPAREN, "expected ')'");
            return e;
        }

        error("unexpected token");
        pos_++;
        return nullptr;
    }

    std::unique_ptr<Expr> parseIntrinsicExpr() {
        auto e = std::make_unique<IntrinsicExpr>();
        e->line = cur().line; e->col = cur().col;
        std::string fullName = cur().text; // e.g. "@load"
        e->name = fullName.substr(1);      // strip @
        pos_++;

        // Parse flags (e.g. -target, -debug, -opt=speed, -o)
        while (!atEnd() && check(TokenKind::MINUS)) {
            pos_++; // consume -
            Token flag = consume(TokenKind::IDENT, "expected flag name after '-'");
            std::string flagStr = "-" + flag.text;
            // Check for =value
            if (check(TokenKind::EQ)) {
                pos_++;
                flagStr += "=" + cur().text;
                pos_++;
            }
            // Check for following value without =
            else if (!atEnd() && !check(TokenKind::NEWLINE) && !check(TokenKind::COMMA) &&
                     !check(TokenKind::RPAREN) && !check(TokenKind::RBRACE) &&
                     cur().kind != TokenKind::MINUS &&
                     !isIntrinsicToken(cur().kind)) {
                // If next is an ident, string, or int, it's a flag value
                if (check(TokenKind::IDENT) || check(TokenKind::STRING) || check(TokenKind::INT)) {
                    flagStr += " " + cur().text;
                    pos_++;
                }
            }
            e->flags.push_back(flagStr);
        }

        // Parse arguments if ( follows
        if (check(TokenKind::LPAREN)) {
            pos_++;
            skipNewlines();
            while (!atEnd() && !check(TokenKind::RPAREN)) {
                e->args.push_back(parseExpr());
                skipNewlines();
                if (!check(TokenKind::RPAREN)) {
                    consume(TokenKind::COMMA, "expected ','");
                    skipNewlines();
                }
            }
            consume(TokenKind::RPAREN, "expected ')'");
        }
        // If no parens but followed by expressions (for @build src style)
        else if (!atEnd() && !check(TokenKind::NEWLINE) && !check(TokenKind::EOF_TOK) &&
                 !check(TokenKind::RBRACE)) {
            // Consume remaining args on the line
            while (!atEnd() && !check(TokenKind::NEWLINE) && !check(TokenKind::EOF_TOK) &&
                   !check(TokenKind::RBRACE)) {
                e->args.push_back(parseExpr());
                if (check(TokenKind::COMMA)) pos_++;
            }
        }

        return e;
    }

    std::unique_ptr<Expr> parseCliExpr() {
        auto e = std::make_unique<CliExpr>();
        e->line = cur().line; e->col = cur().col;
        pos_++; // consume @cli
        consume(TokenKind::DOT, "expected '.' after @cli");
        Token method = consume(TokenKind::IDENT, "expected method name (get/has)");
        e->method = method.text;
        consume(TokenKind::LPAREN, "expected '('");
        Token key = consume(TokenKind::STRING, "expected key string");
        e->key = key.text;
        consume(TokenKind::RPAREN, "expected ')'");
        return e;
    }
};

} // namespace blv

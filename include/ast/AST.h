#pragma once
#include <vector>
#include <string>
#include <memory>
#include <llvm/IR/Value.h>

namespace luv {

class ASTVisitor;

// ─────────────────────────────────────────────────────────
//  Visibility enumeration for AST-level tracking
// ─────────────────────────────────────────────────────────
enum class ASTVisibility {
    DEFAULT,   // public by default
    PUBLIC,
    PRIVATE
};

// ─────────────────────────────────────────────────────────
//  Base node types
// ─────────────────────────────────────────────────────────
class Node {
public:
    virtual ~Node() = default;
    virtual llvm::Value* accept(ASTVisitor& visitor) = 0;
};

class Expr : public Node {
public:
    std::string semanticType; // Used for type resolution in CodeGen
};
class Stmt : public Node {};

// ─────────────────────────────────────────────────────────
//  Expression nodes
// ─────────────────────────────────────────────────────────
class IntExpr : public Expr {
public:
    std::string value;
    IntExpr(std::string v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class FloatExpr : public Expr {
public:
    std::string value;
    FloatExpr(std::string v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;
    StringExpr(std::string v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class CharExpr : public Expr {
public:
    char value;
    CharExpr(char v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class BoolExpr : public Expr {
public:
    bool value;
    BoolExpr(bool v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class NullExpr : public Expr {
public:
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class VarExpr : public Expr {
public:
    std::string name;
    VarExpr(std::string n) : name(n) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class BinaryExpr : public Expr {
public:
    Expr *left, *right;
    std::string op;
    BinaryExpr(Expr* l, std::string o, Expr* r)
        : left(l), op(o), right(r) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class UnaryExpr : public Expr {
public:
    std::string op;
    Expr* expr;
    UnaryExpr(std::string o, Expr* e) : op(o), expr(e) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<Expr*> args;
    CallExpr(std::string c, std::vector<Expr*> a)
        : callee(c), args(std::move(a)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class GenericCallExpr : public Expr {
public:
    std::string callee;
    std::vector<std::string> typeArgs;
    std::vector<Expr*> args;
    GenericCallExpr(std::string c, std::vector<std::string> ta, std::vector<Expr*> a)
        : callee(c), typeArgs(std::move(ta)), args(std::move(a)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StructInstExpr : public Expr {
public:
    std::string structName;
    std::vector<std::pair<std::string, Expr*>> fields;
    StructInstExpr(std::string n, std::vector<std::pair<std::string, Expr*>> f)
        : structName(std::move(n)), fields(std::move(f)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class IntrinsicCallExpr : public Expr {
public:
    std::string callee;
    std::vector<Expr*> args;
    IntrinsicCallExpr(std::string c, std::vector<Expr*> a)
        : callee(c), args(std::move(a)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class AsmExpr : public Expr {
public:
    std::string code;
    AsmExpr(std::string c) : code(std::move(c)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class MethodCallExpr : public Expr {
public:
    Expr* object;
    std::string methodName;
    std::vector<Expr*> args;
    MethodCallExpr(Expr* obj, std::string name, std::vector<Expr*> a)
        : object(obj), methodName(name), args(std::move(a)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class SuperCallExpr : public Expr {
public:
    std::string methodName;
    std::vector<Expr*> args;
    SuperCallExpr(std::string name, std::vector<Expr*> a)
        : methodName(name), args(std::move(a)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class PropertyExpr : public Expr {
public:
    Expr* object;
    std::string propertyName;
    PropertyExpr(Expr* obj, std::string name)
        : object(obj), propertyName(name) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class CastExpr : public Expr {
public:
    Expr* expr;
    std::string targetType;
    bool isUnsafe;
    CastExpr(Expr* e, std::string t, bool u)
        : expr(e), targetType(std::move(t)), isUnsafe(u) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class TernaryExpr : public Expr {
public:
    Expr *condition, *thenExpr, *elseExpr;
    TernaryExpr(Expr* c, Expr* t, Expr* e)
        : condition(c), thenExpr(t), elseExpr(e) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ComparisonChainExpr : public Expr {
public:
    std::vector<Expr*> operands;
    std::vector<std::string> operators;
    ComparisonChainExpr(std::vector<Expr*> opnds, std::vector<std::string> ops)
        : operands(std::move(opnds)), operators(std::move(ops)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StmtExpr : public Expr {
public:
    Stmt* stmt;
    StmtExpr(Stmt* s) : stmt(s) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StringInterpolationExpr : public Expr {
public:
    std::vector<Expr*> parts;
    StringInterpolationExpr(std::vector<Expr*> p) : parts(std::move(p)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class Pattern : public Node {
public:
    virtual ~Pattern() = default;
    virtual llvm::Value* accept(ASTVisitor& visitor) = 0;
    std::string semanticType;
};

class IdentifierPattern : public Pattern {
public:
    std::string name;
    IdentifierPattern(std::string n) : name(std::move(n)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class TuplePattern : public Pattern {
public:
    std::vector<Pattern*> elements;
    TuplePattern(std::vector<Pattern*> e) : elements(std::move(e)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StructPattern : public Pattern {
public:
    std::string structName;
    std::vector<std::pair<std::string, Pattern*>> fields;
    StructPattern(std::string n, std::vector<std::pair<std::string, Pattern*>> f)
        : structName(std::move(n)), fields(std::move(f)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class VariantPattern : public Pattern {
public:
    std::string variantName;
    std::vector<Pattern*> elements;
    VariantPattern(std::string n, std::vector<Pattern*> e)
        : variantName(std::move(n)), elements(std::move(e)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class WildcardPattern : public Pattern {
public:
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class LiteralPattern : public Pattern {
public:
    Expr* literal;
    LiteralPattern(Expr* l) : literal(l) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

//  Statement nodes
// ─────────────────────────────────────────────────────────
class Block : public Stmt {
public:
    std::vector<Stmt*> statements;
    Block(std::vector<Stmt*> s) : statements(std::move(s)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class VarDecl : public Stmt {
public:
    Pattern* pattern;
    std::string type;
    bool isMutable;
    bool isConst;
    bool isDynamic;
    std::vector<std::string> attributes; // For @SoA, @repr, ![...], etc.
    Expr* init;
    VarDecl(Pattern* p, std::string t, bool m, bool c, bool d, std::vector<std::string> attrs, Expr* i)
        : pattern(p), type(std::move(t)), isMutable(m), isConst(c), isDynamic(d), attributes(std::move(attrs)), init(i) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class Assignment : public Stmt {
public:
    std::vector<Expr*> targets;
    Expr* value;
    std::string op;
    Assignment(std::vector<Expr*> t, Expr* v, std::string o = "=")
        : targets(std::move(t)), value(v), op(std::move(o)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct MatchCase {
    Pattern* pattern = nullptr; // If null, it's the default '_' case
    Expr* resultExpr = nullptr;
    Block* resultBlock = nullptr;
};

class MatchExpr : public Expr {
public:
    Expr* value;
    std::vector<MatchCase> cases;
    MatchExpr(Expr* v, std::vector<MatchCase> c)
        : value(v), cases(std::move(c)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct EfExpr {
    Expr* cond;
    Block* block;
};

class IfExpr : public Expr {
public:
    std::vector<std::string> attributes;
    Expr* cond;
    Block* thenBlock;
    std::vector<EfExpr> efs;
    Block* elseBlock;
    IfExpr(Expr* c, Block* t, std::vector<EfExpr> e, Block* el, std::vector<std::string> attrs = {})
        : attributes(std::move(attrs)), cond(c), thenBlock(t), efs(std::move(e)), elseBlock(el) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class IndexExpr : public Expr {
public:
    Expr* target;
    Expr* index;
    IndexExpr(Expr* t, Expr* i) : target(t), index(i) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class PostfixExpr : public Expr {
public:
    Expr* expr;
    std::string op;
    PostfixExpr(Expr* e, std::string o) : expr(e), op(std::move(o)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class WhileExpr : public Expr {
public:
    std::vector<std::string> attributes;
    Expr* cond;
    Block* body;
    WhileExpr(Expr* c, Block* b, std::vector<std::string> attrs = {})
        : attributes(std::move(attrs)), cond(c), body(b) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ForRangeExpr : public Expr {
public:
    std::vector<std::string> attributes;
    Pattern* pattern;
    bool isDynamic;
    Expr *start, *end;
    bool inclusive;
    Block* body;
    ForRangeExpr(Pattern* p, bool dyn, Expr* s, Expr* e, bool i, Block* b, std::vector<std::string> attrs = {})
        : attributes(std::move(attrs)), pattern(p), isDynamic(dyn), start(s), end(e), inclusive(i), body(b) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ForCStyleExpr : public Expr {
public:
    std::vector<std::string> attributes;
    Stmt* init;
    Expr* cond;
    Stmt* step;
    Block* body;
    ForCStyleExpr(Stmt* i, Expr* c, Stmt* s, Block* b, std::vector<std::string> attrs = {})
        : attributes(std::move(attrs)), init(i), cond(c), step(s), body(b) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ForInExpr : public Expr {
public:
    std::vector<std::string> attributes;
    Pattern* pattern;
    bool isDynamic;
    Expr* iterable;
    Block* body;
    ForInExpr(Pattern* p, bool dyn, Expr* iter, Block* b, std::vector<std::string> attrs = {})
        : attributes(std::move(attrs)), pattern(p), isDynamic(dyn), iterable(iter), body(b) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ReturnStmt : public Stmt {
public:
    Expr* value;
    ReturnStmt(Expr* v) : value(v) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class BreakStmt : public Stmt {
public:
    std::string label;
    BreakStmt(std::string l = "") : label(std::move(l)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ContinueStmt : public Stmt {
public:
    std::string label;
    ContinueStmt(std::string l = "") : label(std::move(l)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ExprStmt : public Stmt {
public:
    Expr* expr;
    ExprStmt(Expr* e) : expr(e) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct StructField {
    std::string name;
    std::string type;
};

struct EnumVariant {
    std::string name;
    std::vector<std::string> types;
};

class EnumDecl : public Stmt {
public:
    std::string name;
    std::vector<EnumVariant> variants;
    std::vector<std::string> attributes;
    ASTVisibility visibility = ASTVisibility::DEFAULT;
    EnumDecl(std::string n, std::vector<EnumVariant> v, std::vector<std::string> attrs = {}, ASTVisibility vis = ASTVisibility::DEFAULT)
        : name(std::move(n)), variants(std::move(v)), attributes(std::move(attrs)), visibility(vis) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class StructDecl : public Stmt {
public:
    std::string name;
    std::vector<std::string> attributes;
    std::vector<StructField> fields;
    std::vector<Stmt*> nestedDecls;
    ASTVisibility visibility = ASTVisibility::DEFAULT;
    StructDecl(std::string n, std::vector<StructField> f, std::vector<Stmt*> nd = {}, std::vector<std::string> attrs = {}, ASTVisibility vis = ASTVisibility::DEFAULT)
        : name(std::move(n)), fields(std::move(f)), nestedDecls(std::move(nd)), attributes(std::move(attrs)), visibility(vis) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct Param {
    std::string name;
    std::string type;
    bool isDynamic;
    bool isMutable;
    std::vector<std::string> attributes;
    Expr* defaultVal = nullptr;
};

class FuncDecl : public Stmt {
public:
    std::string boundStruct;
    std::string name;
    std::vector<std::string> typeParams;
    std::vector<Param> params;
    std::string returnType;
    Block* body;
    std::vector<std::string> attributes;
    ASTVisibility visibility = ASTVisibility::DEFAULT;
    bool isOverride = false;
    bool isStatic = false;
    FuncDecl(std::string bs, std::string n, std::vector<std::string> tp, std::vector<Param> p, std::string rt, Block* b,
             std::vector<std::string> attrs = {}, ASTVisibility vis = ASTVisibility::DEFAULT)
        : boundStruct(std::move(bs)), name(std::move(n)), typeParams(std::move(tp)), params(std::move(p)), returnType(std::move(rt)), body(b), 
          attributes(std::move(attrs)), visibility(vis) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ExternDecl : public Stmt {
public:
    std::string abi;
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::vector<std::string> attributes;
    ASTVisibility visibility = ASTVisibility::DEFAULT;
    ExternDecl(std::string abiStr, std::string n, std::vector<Param> p, std::string rt, std::vector<std::string> attrs = {}, ASTVisibility vis = ASTVisibility::DEFAULT)
        : abi(abiStr), name(n), params(std::move(p)), returnType(rt), attributes(std::move(attrs)), visibility(vis) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct ClassField {
    std::string name;
    std::string type;
    bool isPrivate;
    std::vector<std::string> attributes;
};

class ClassDecl : public Stmt {
public:
    std::string name;
    bool isAbstract = false;
    std::vector<std::string> baseAndInterfaces;
    std::vector<ClassField> fields;
    std::vector<FuncDecl*> methods;
    std::vector<Stmt*> nestedDecls;
    std::vector<std::string> attributes;
    ASTVisibility visibility = ASTVisibility::DEFAULT;
    
    ClassDecl(std::string n, bool abstract_, std::vector<std::string> bases, std::vector<ClassField> f, std::vector<FuncDecl*> m, std::vector<Stmt*> nd = {}, std::vector<std::string> attrs = {}, ASTVisibility vis = ASTVisibility::DEFAULT)
        : name(std::move(n)), isAbstract(abstract_), baseAndInterfaces(std::move(bases)), fields(std::move(f)), methods(std::move(m)), nestedDecls(std::move(nd)), attributes(std::move(attrs)), visibility(vis) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

struct InterfaceMethod {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::vector<std::string> attributes;
};

class InterfaceDecl : public Stmt {
public:
    std::string name;
    std::vector<InterfaceMethod> methods;
    std::vector<std::string> attributes;
    
    InterfaceDecl(std::string n, std::vector<InterfaceMethod> m, std::vector<std::string> attrs = {})
        : name(std::move(n)), methods(std::move(m)), attributes(std::move(attrs)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ModuleDeclStmt : public Stmt {
public:
    std::string name;
    ModuleDeclStmt(std::string n) : name(std::move(n)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class UseStmt : public Stmt {
public:
    enum TargetKind { SINGLE, SET, ALL_PUBLIC, ALL_PRIVATE, PATH };
    TargetKind targetKind;
    std::vector<std::string> names;
    std::vector<std::string> modulePath;
    UseStmt(TargetKind k, std::vector<std::string> n, std::vector<std::string> p)
        : targetKind(k), names(std::move(n)), modulePath(std::move(p)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class Program : public Node {
public:
    std::string moduleName;
    std::vector<UseStmt*> useStatements;
    std::vector<Stmt*> statements;
    Program(std::string name, std::vector<UseStmt*> uses, std::vector<Stmt*> stmts)
        : moduleName(std::move(name)), useStatements(std::move(uses)), statements(std::move(stmts)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ArrayExpr : public Expr {
public:
    std::vector<Expr*> elements;
    ArrayExpr(std::vector<Expr*> e) : elements(std::move(e)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ArrayRepeatExpr : public Expr {
public:
    Expr *value, *count;
    ArrayRepeatExpr(Expr* v, Expr* c) : value(v), count(c) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class ArrayCompExpr : public Expr {
public:
    Expr* expr;
    std::string varName;
    Expr* iterable;
    Expr* rangeEnd;
    bool inclusive;
    Expr* step;
    ArrayCompExpr(Expr* e, std::string n, Expr* it, Expr* re = nullptr, bool inc = false, Expr* s = nullptr)
        : expr(e), varName(std::move(n)), iterable(it), rangeEnd(re), inclusive(inc), step(s) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class TupleExpr : public Expr {
public:
    std::vector<Expr*> elements;
    TupleExpr(std::vector<Expr*> e) : elements(std::move(e)) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

class SliceExpr : public Expr {
public:
    Expr *target, *start, *end, *step;
    bool inclusive;
    SliceExpr(Expr* t, Expr* s, Expr* e, bool inc, Expr* st = nullptr)
        : target(t), start(s), end(e), inclusive(inc), step(st) {}
    llvm::Value* accept(ASTVisitor& visitor) override;
};

// ─────────────────────────────────────────────────────────
//  ASTVisitor: base class for analyzing/walking the tree
// ─────────────────────────────────────────────────────────
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual llvm::Value* visit(IntExpr& node) = 0;
    virtual llvm::Value* visit(FloatExpr& node) = 0;
    virtual llvm::Value* visit(StringExpr& node) = 0;
    virtual llvm::Value* visit(CharExpr& node) = 0;
    virtual llvm::Value* visit(BoolExpr& node) = 0;
    virtual llvm::Value* visit(NullExpr& node) = 0;
    virtual llvm::Value* visit(VarExpr& node) = 0;
    virtual llvm::Value* visit(BinaryExpr& node) = 0;
    virtual llvm::Value* visit(IdentifierPattern& node) = 0;
    virtual llvm::Value* visit(TuplePattern& node) = 0;
    virtual llvm::Value* visit(StructPattern& node) = 0;
    virtual llvm::Value* visit(VariantPattern& node) = 0;
    virtual llvm::Value* visit(WildcardPattern& node) = 0;
    virtual llvm::Value* visit(LiteralPattern& node) = 0;
    virtual llvm::Value* visit(EnumDecl& node) = 0;
    virtual llvm::Value* visit(UnaryExpr& node) = 0;
    virtual llvm::Value* visit(CallExpr& node) = 0;
    virtual llvm::Value* visit(GenericCallExpr& node) = 0;
    virtual llvm::Value* visit(StructInstExpr& node) = 0;
    virtual llvm::Value* visit(IntrinsicCallExpr& node) = 0;
    virtual llvm::Value* visit(AsmExpr& node) = 0;
    virtual llvm::Value* visit(MethodCallExpr& node) = 0;
    virtual llvm::Value* visit(SuperCallExpr& node) = 0;
    virtual llvm::Value* visit(PropertyExpr& node) = 0;
    virtual llvm::Value* visit(CastExpr& node) = 0;
    virtual llvm::Value* visit(TernaryExpr& node) = 0;
    virtual llvm::Value* visit(ComparisonChainExpr& node) = 0;
    virtual llvm::Value* visit(StmtExpr& node) = 0;
    virtual llvm::Value* visit(StringInterpolationExpr& node) = 0;
    virtual llvm::Value* visit(Block& node) = 0;
    virtual llvm::Value* visit(MatchExpr& node) = 0;
    virtual llvm::Value* visit(VarDecl& node) = 0;
    virtual llvm::Value* visit(Assignment& node) = 0;
    virtual llvm::Value* visit(IfExpr& node) = 0;
    virtual llvm::Value* visit(WhileExpr& node) = 0;
    virtual llvm::Value* visit(ForRangeExpr& node) = 0;
    virtual llvm::Value* visit(ForCStyleExpr& node) = 0;
    virtual llvm::Value* visit(ForInExpr& node) = 0;
    virtual llvm::Value* visit(ReturnStmt& node) = 0;
    virtual llvm::Value* visit(BreakStmt& node) = 0;
    virtual llvm::Value* visit(ContinueStmt& node) = 0;
    virtual llvm::Value* visit(ExprStmt& node) = 0;
    virtual llvm::Value* visit(FuncDecl& node) = 0;
    virtual llvm::Value* visit(StructDecl& node) = 0;
    virtual llvm::Value* visit(ClassDecl& node) = 0;
    virtual llvm::Value* visit(InterfaceDecl& node) = 0;
    virtual llvm::Value* visit(ExternDecl& node) = 0;
    virtual llvm::Value* visit(ModuleDeclStmt& node) = 0;
    virtual llvm::Value* visit(IndexExpr& node) = 0;
    virtual llvm::Value* visit(PostfixExpr& node) = 0;
    virtual llvm::Value* visit(UseStmt& node) = 0;
    virtual llvm::Value* visit(Program& node) = 0;
    virtual llvm::Value* visit(ArrayExpr& node) = 0;
    virtual llvm::Value* visit(ArrayRepeatExpr& node) = 0;
    virtual llvm::Value* visit(ArrayCompExpr& node) = 0;
    virtual llvm::Value* visit(TupleExpr& node) = 0;
    virtual llvm::Value* visit(SliceExpr& node) = 0;
};

} // namespace luv

#pragma once
#include "ast/AST.h"
#include "LuvError.h"
#include "ModuleResolver.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <algorithm>

namespace luv {

// ─────────────────────────────────────────────────────────
//  SymbolInfo: what we know about a declared symbol
// ─────────────────────────────────────────────────────────
struct SymbolInfo {
    enum Kind { VAR, FUNC, PARAM, IMPORTED_VAR, IMPORTED_FUNC, BUILTIN_FUNC, STRUCT, CLASS, INTERFACE, ENUM };
    Kind kind;
    std::string type;      // known type or "" for inferred/dyn
    bool isMutable = true;
    bool isConst = false;
    bool isDynamic = false;
    bool isUsed = false;   // for unused-variable warnings
    int declLine = 0;
    int declCol = 0;
    int paramCount = -1;   // for functions: total param count
    int minParamCount = -1; // for functions: minimum required args
    bool isAssigned = false; // set to true if variable is ever the target of an assignment
};

// ─────────────────────────────────────────────────────────
//  Scope: a lexical scope for symbol lookup
// ─────────────────────────────────────────────────────────
class Scope {
public:
    Scope(Scope* parent = nullptr) : parent_(parent) {}

    bool define(const std::string& name, SymbolInfo info) {
        if (symbols_.count(name)) return false; // already defined in this scope
        symbols_[name] = std::move(info);
        return true;
    }

    SymbolInfo* lookup(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) return &it->second;
        return parent_ ? parent_->lookup(name) : nullptr;
    }

    SymbolInfo* lookupLocal(const std::string& name) {
        auto it = symbols_.find(name);
        return it != symbols_.end() ? &it->second : nullptr;
    }

    Scope* parent() const { return parent_; }

    const std::map<std::string, SymbolInfo>& symbols() const { return symbols_; }
    std::map<std::string, SymbolInfo>& symbols() { return symbols_; }

private:
    Scope* parent_;
    std::map<std::string, SymbolInfo> symbols_;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer(const std::string& file, const ModuleResolver* resolver = nullptr,
                     const std::string& moduleName = "")
        : file_(file), resolver_(resolver), moduleName_(moduleName) {
        if (resolver_) {
            const ModuleInfo* mod = resolver_->getModule(moduleName_);
            isLibraryModule_ = mod && !mod->exports.empty();
        }
    }

    bool analyze(Program& prog) {
        pushScope();
        registerBuiltins();
        registerImports(prog);

        for (auto& stmt : prog.statements) {
            registerTopLevel(stmt);
        }

        bool hasMain = false;
        bool hasScript = false;
        for (auto& stmt : prog.statements) {
            if (auto* fn = dynamic_cast<FuncDecl*>(stmt)) {
                if (fn->name == "main" && fn->boundStruct.empty()) hasMain = true;
            } else if (!dynamic_cast<StructDecl*>(stmt) && 
                       !dynamic_cast<ClassDecl*>(stmt) && 
                       !dynamic_cast<InterfaceDecl*>(stmt) && 
                       !dynamic_cast<EnumDecl*>(stmt) &&
                       !dynamic_cast<ExternDecl*>(stmt) && 
                       !dynamic_cast<ModuleDeclStmt*>(stmt) &&
                       !dynamic_cast<UseStmt*>(stmt) &&
                       !dynamic_cast<VarDecl*>(stmt)) {
                hasScript = true;
            }
        }
        if (hasMain && hasScript) {
            LuvError::error(ErrorKind::INTERNAL_ERROR, 
                "Ambiguous entry point: file contains both top-level scripting code and a 'main' function.", 
                file_, 0, 0);
        }

        for (auto& stmt : prog.statements) {
            analyzeStmt(stmt);
        }

        emitUnusedWarnings();
        popScope();
        return !LuvError::instance().hasErrors();
    }

private:
    std::string file_;
    const ModuleResolver* resolver_;
    std::string moduleName_;
    std::stack<std::unique_ptr<Scope>> scopes_;
    bool insideFunction_ = false;
    std::string currentFunctionName_;
    bool currentFuncHasReturn_ = false;
    bool isLibraryModule_ = false; 
    std::string currentClass_ = ""; 
    std::stack<std::vector<std::string>> loopLabels_; 
    std::map<std::string, ClassDecl*> classDecls;
    std::map<std::string, InterfaceDecl*> interfaceDecls;
    std::map<std::string, EnumDecl*> enumDecls;
    std::map<std::string, std::vector<std::string>> vtableLayouts; 
    std::set<std::string> defines_;
    std::set<std::string> excludes_;

public:
    void addDefine(const std::string& d) { defines_.insert(d); }
    void addExclude(const std::string& e) { excludes_.insert(e); }

private:
    void pushScope() {
        Scope* parent = scopes_.empty() ? nullptr : scopes_.top().get();
        scopes_.push(std::make_unique<Scope>(parent));
    }

    void popScope() {
        scopes_.pop();
    }

    Scope& currentScope() {
        return *scopes_.top();
    }

    void registerBuiltins() {
        currentScope().define("printf", {SymbolInfo::BUILTIN_FUNC, "int", false, false, false, true, 0, 0, -1});
        currentScope().define("puts", {SymbolInfo::BUILTIN_FUNC, "int", false, false, false, true, 0, 0, 1});
        currentScope().define("println", {SymbolInfo::BUILTIN_FUNC, "void", false, false, false, true, 0, 0, -1});
        currentScope().define("print", {SymbolInfo::BUILTIN_FUNC, "void", false, false, false, true, 0, 0, -1});
        currentScope().define("__arg__", {SymbolInfo::VAR, "[string]", false, true, false, true, 0, 0});
        currentScope().define("__name__", {SymbolInfo::VAR, "string", false, true, false, true, 0, 0});
    }

    void registerImports(const Program& prog) {
        if (!resolver_) return;
        for (const auto& use : prog.useStatements) {
            std::string targetModName;
            for (size_t i = 0; i < use->modulePath.size(); ++i) {
                if (i > 0) targetModName += "::";
                targetModName += use->modulePath[i];
            }

            const ModuleInfo* targetMod = resolver_->getModule(targetModName);
            if (!targetMod) continue;

            ImportRequest req;
            req.sourceFile = file_;
            req.modulePath = use->modulePath;
            switch (use->targetKind) {
                case UseStmt::SINGLE:      req.targetKind = ImportRequest::SINGLE; break;
                case UseStmt::SET:         req.targetKind = ImportRequest::SET; break;
                case UseStmt::ALL_PUBLIC:  req.targetKind = ImportRequest::ALL_PUBLIC; break;
                case UseStmt::ALL_PRIVATE: req.targetKind = ImportRequest::ALL_PRIVATE; break;
                case UseStmt::PATH:        req.targetKind = ImportRequest::PATH; break;
            }
            for (const auto& sym : use->symbols) req.names.push_back(sym.original);

            auto importedSymbols = resolver_->getImportedSymbols(moduleName_, req);
            for (const auto& sym : importedSymbols) {
                SymbolInfo info;
                info.kind = (sym.kind == ExportedSymbol::FUNCTION) ? SymbolInfo::IMPORTED_FUNC : SymbolInfo::IMPORTED_VAR;
                info.type = "";
                info.isMutable = false;
                info.isConst = false;
                info.isDynamic = false;
                info.isUsed = false;
                info.declLine = 0;
                info.declCol = 0;
                info.paramCount = -1;

                if (sym.kind == ExportedSymbol::FUNCTION && targetMod->program) {
                    for (const auto& s : targetMod->program->statements) {
                        auto* fn = dynamic_cast<FuncDecl*>(s);
                        if (fn && fn->name == sym.name) {
                            info.paramCount = (int)fn->params.size();
                            int minP = 0;
                            for (const auto& p : fn->params) if (!p.defaultVal) minP++;
                            info.minParamCount = minP;
                            break;
                        }
                    }
                }
                
                std::string localName = sym.name;
                for (const auto& alias : use->symbols) {
                    if (alias.original == sym.name && !alias.alias.empty()) {
                        localName = alias.alias;
                        break;
                    }
                }
                currentScope().define(localName, info);
            }
        }
    }

    void registerTopLevel(Stmt* stmt) {
        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) {
            if (!func->boundStruct.empty()) func->name = func->boundStruct + "_" + func->name;
            SymbolInfo info;
            info.kind = SymbolInfo::FUNC;
            info.type = func->returnType;
            info.paramCount = (int)func->params.size();
            int minP = 0;
            for (const auto& p : func->params) if (!p.defaultVal) minP++;
            info.minParamCount = minP;
            info.isUsed = (func->name == "main") || isLibraryModule_;
            if (!currentScope().define(func->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Function '" + func->name + "' is already defined", file_, 0, 0);
            }
        } else if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
            analyzePattern(var->pattern, var->isMutable, var->isConst, var->isDynamic, true);
        } else if (auto* ext = dynamic_cast<ExternDecl*>(stmt)) {
            SymbolInfo info;
            info.kind = SymbolInfo::FUNC;
            info.type = ext->returnType;
            info.paramCount = -1; 
            info.isUsed = true;
            SymbolInfo* existing = currentScope().lookupLocal(ext->name);
            if (existing && existing->kind == SymbolInfo::BUILTIN_FUNC) *existing = info;
            else if (!currentScope().define(ext->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Extern function '" + ext->name + "' conflicts with existing definition", file_, 0, 0);
            }
        } else if (auto* str = dynamic_cast<StructDecl*>(stmt)) {
            SymbolInfo info;
            info.kind = SymbolInfo::STRUCT;
            info.isUsed = true;
            if (!currentScope().define(str->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Struct '" + str->name + "' already defined", file_, 0, 0);
            }
        } else if (auto* enm = dynamic_cast<EnumDecl*>(stmt)) {
            enumDecls[enm->name] = enm;
            SymbolInfo info;
            info.kind = SymbolInfo::ENUM;
            info.isUsed = true;
            if (!currentScope().define(enm->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Enum '" + enm->name + "' already defined", file_, 0, 0);
            }
        } else if (auto* cls = dynamic_cast<ClassDecl*>(stmt)) {
            classDecls[cls->name] = cls;
            std::vector<std::string> layout;
            if (!cls->baseAndInterfaces.empty()) {
                std::string base = cls->baseAndInterfaces[0];
                if (vtableLayouts.count(base)) layout = vtableLayouts[base];
            }
            for (auto* method : cls->methods) {
                if (method->isStatic) continue;
                std::string methodName = method->name;
                size_t underscore = methodName.find("_");
                if (underscore != std::string::npos) methodName = methodName.substr(underscore + 1);
                bool found = false;
                for (const auto& existing : layout) if (existing == methodName) { found = true; break; }
                if (!found) layout.push_back(methodName);
            }
            vtableLayouts[cls->name] = layout;
            SymbolInfo info;
            info.kind = SymbolInfo::CLASS;
            info.isUsed = true;
            if (!currentScope().define(cls->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Class '" + cls->name + "' already defined", file_, 0, 0);
            }
            for (auto& method : cls->methods) registerTopLevel(method);
        } else if (auto* iface = dynamic_cast<InterfaceDecl*>(stmt)) {
            interfaceDecls[iface->name] = iface;
            SymbolInfo info;
            info.kind = SymbolInfo::INTERFACE;
            info.isUsed = true;
            if (!currentScope().define(iface->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Interface '" + iface->name + "' already defined", file_, 0, 0);
            }
        }
    }

    void analyzeStmt(Stmt* stmt) {
        if (!stmt) return;
        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) analyzeFunc(func);
        else if (auto* var = dynamic_cast<VarDecl*>(stmt)) analyzeVarDecl(var);
        else if (auto* assign = dynamic_cast<Assignment*>(stmt)) analyzeAssignment(assign);
        else if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) analyzeReturn(ret);
        else if (auto* brk = dynamic_cast<BreakStmt*>(stmt)) analyzeBreak(brk);
        else if (auto* cont = dynamic_cast<ContinueStmt*>(stmt)) analyzeContinue(cont);
        else if (auto* cls = dynamic_cast<ClassDecl*>(stmt)) {
            std::string oldClass = currentClass_;
            currentClass_ = cls->name;
            for (auto& method : cls->methods) analyzeStmt(method);
            currentClass_ = oldClass;
        } else if (auto* exprS = dynamic_cast<ExprStmt*>(stmt)) analyzeExpr(exprS->expr);
        else if (auto* block = dynamic_cast<Block*>(stmt)) analyzeBlock(block);
        else if (auto* enm = dynamic_cast<EnumDecl*>(stmt)) {}
    }

    void analyzeFunc(FuncDecl* func) {
        bool oldInsideFunc = insideFunction_;
        std::string oldFuncName = currentFunctionName_;
        bool oldHasReturn = currentFuncHasReturn_;
        insideFunction_ = true;
        currentFunctionName_ = func->name;
        currentFuncHasReturn_ = false;
        pushScope();
        for (const auto& param : func->params) {
            analyzePattern(param.pattern, param.isMutable, false, param.isDynamic, true);
        }
        if (func->body) for (auto& stmt : func->body->statements) analyzeStmt(stmt);
        popScope();
        insideFunction_ = oldInsideFunc;
        currentFunctionName_ = oldFuncName;
        currentFuncHasReturn_ = oldHasReturn;
    }

    void analyzePattern(Pattern* p, bool isMut, bool isConst, bool isDyn, bool isUsed = false) {
        if (!p) return;
        if (auto* vp = dynamic_cast<VarPattern*>(p)) {
            SymbolInfo info;
            info.kind = SymbolInfo::VAR;
            info.isMutable = isMut;
            info.isConst = isConst;
            info.isDynamic = isDyn;
            info.isUsed = isUsed;
            if (!currentScope().define(vp->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION, "Variable '" + vp->name + "' already defined", file_, 0, 0);
            }
        } else if (auto* tp = dynamic_cast<TuplePattern*>(p)) {
            for (auto* sub : tp->patterns) analyzePattern(sub, isMut, isConst, isDyn, isUsed);
        } else if (auto* sp = dynamic_cast<StructPattern*>(p)) {
            for (auto& f : sp->fields) analyzePattern(f.second, isMut, isConst, isDyn, isUsed);
        } else if (auto* ap = dynamic_cast<ArrayPattern*>(p)) {
            for (auto* sub : ap->patterns) analyzePattern(sub, isMut, isConst, isDyn, isUsed);
        }
    }

    void analyzeVarDecl(VarDecl* var) {
        if (var->init) analyzeExpr(var->init);
        if (insideFunction_) {
            analyzePattern(var->pattern, var->isMutable, var->isConst, var->isDynamic, false);
        }
    }

    void analyzeAssignment(Assignment* assign) {
        for (auto* t : assign->targets) analyzeExpr(t);
        analyzeExpr(assign->value);
    }

    void analyzeIfExpr(IfExpr* ifExpr) {
        analyzeExpr(ifExpr->cond);
        if (ifExpr->thenBlock) analyzeBlock(ifExpr->thenBlock);
        for (auto& ef : ifExpr->efs) {
            analyzeExpr(ef.cond);
            if (ef.block) analyzeBlock(ef.block);
        }
        if (ifExpr->elseBlock) analyzeBlock(ifExpr->elseBlock);
    }

    void analyzeWhileExpr(WhileExpr* whileExpr) {
        analyzeExpr(whileExpr->cond);
        loopLabels_.push(extractLabels(whileExpr->attributes));
        if (whileExpr->body) analyzeBlock(whileExpr->body);
        loopLabels_.pop();
    }

    std::vector<std::string> extractLabels(const std::vector<Expr*>& attrs) {
        std::vector<std::string> labels;
        for (auto* attr : attrs) {
            if (auto* var = dynamic_cast<VarExpr*>(attr)) labels.push_back(var->name);
        }
        return labels;
    }

    void analyzeForRangeExpr(ForRangeExpr* forRangeExpr) {
        analyzeExpr(forRangeExpr->start);
        analyzeExpr(forRangeExpr->end);
        loopLabels_.push(extractLabels(forRangeExpr->attributes));
        pushScope();
        analyzePattern(forRangeExpr->pattern, true, false, forRangeExpr->isDynamic, true);
        if (forRangeExpr->body) for (auto& stmt : forRangeExpr->body->statements) analyzeStmt(stmt);
        popScope();
        loopLabels_.pop();
    }

    void analyzeForCStyleExpr(ForCStyleExpr* forC) {
        loopLabels_.push(extractLabels(forC->attributes));
        pushScope();
        analyzeStmt(forC->init);
        analyzeExpr(forC->cond);
        analyzeStmt(forC->step);
        if (forC->body) for (auto& stmt : forC->body->statements) analyzeStmt(stmt);
        popScope();
        loopLabels_.pop();
    }

    void analyzeForInExpr(ForInExpr* forInExpr) {
        analyzeExpr(forInExpr->iterable);
        loopLabels_.push(extractLabels(forInExpr->attributes));
        pushScope();
        analyzePattern(forInExpr->pattern, true, false, forInExpr->isDynamic, true);
        if (forInExpr->body) for (auto& stmt : forInExpr->body->statements) analyzeStmt(stmt);
        popScope();
        loopLabels_.pop();
    }

    void analyzeBreak(BreakStmt* b) {
        if (loopLabels_.empty()) {
            LuvError::error(ErrorKind::SYNTAX_ERROR, "'break' outside of a loop", file_, 0, 0);
            return;
        }
        if (!b->label.empty()) {
            bool found = false;
            std::stack<std::vector<std::string>> copy = loopLabels_;
            while (!copy.empty()) {
                for (const auto& l : copy.top()) if (l == b->label) { found = true; break; }
                if (found) break;
                copy.pop();
            }
            if (!found) LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined loop label '" + b->label + "'", file_, 0, 0);
        }
    }

    void analyzeContinue(ContinueStmt* c) {
        if (loopLabels_.empty()) {
            LuvError::error(ErrorKind::SYNTAX_ERROR, "'continue' outside of a loop", file_, 0, 0);
            return;
        }
        if (!c->label.empty()) {
            bool found = false;
            std::stack<std::vector<std::string>> copy = loopLabels_;
            while (!copy.empty()) {
                for (const auto& l : copy.top()) if (l == c->label) { found = true; break; }
                if (found) break;
                copy.pop();
            }
            if (!found) LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined loop label '" + c->label + "'", file_, 0, 0);
        }
    }

    void analyzeReturn(ReturnStmt* ret) {
        if (!insideFunction_) LuvError::error(ErrorKind::SYNTAX_ERROR, "'return' outside of a function", file_, 0, 0);
        currentFuncHasReturn_ = true;
        if (ret->value) analyzeExpr(ret->value);
    }

    void analyzeBlock(Block* block) {
        pushScope();
        for (auto& stmt : block->statements) analyzeStmt(stmt);
        popScope();
    }

    void analyzeExpr(Expr* expr) {
        if (!expr) return;
        if (auto* var = dynamic_cast<VarExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(var->name);
            if (!sym) LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined variable '" + var->name + "'", file_, 0, 0);
            else {
                sym->isUsed = true;
                var->semanticType = (sym->kind == SymbolInfo::CLASS) ? var->name : sym->type;
            }
        } else if (auto* call = dynamic_cast<CallExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(call->callee);
            if (!sym) LuvError::error(ErrorKind::UNDEFINED_FUNCTION, "Undefined function/class '" + call->callee + "'", file_, 0, 0);
            else {
                sym->isUsed = true;
                call->semanticType = (sym->kind == SymbolInfo::CLASS) ? call->callee : sym->type;
            }
            for (auto& arg : call->args) analyzeExpr(arg);
        } else if (auto* matchExpr = dynamic_cast<MatchExpr*>(expr)) {
            analyzeExpr(matchExpr->value);
            for (auto& case_ : matchExpr->cases) {
                pushScope();
                analyzePattern(case_.pattern, false, false, false, true);
                if (case_.resultExpr) analyzeExpr(case_.resultExpr);
                if (case_.resultBlock) analyzeBlock(case_.resultBlock);
                popScope();
            }
        } else if (auto* ifExpr = dynamic_cast<IfExpr*>(expr)) analyzeIfExpr(ifExpr);
        else if (auto* whileExpr = dynamic_cast<WhileExpr*>(expr)) analyzeWhileExpr(whileExpr);
        else if (auto* forRangeExpr = dynamic_cast<ForRangeExpr*>(expr)) analyzeForRangeExpr(forRangeExpr);
        else if (auto* forCStyleExpr = dynamic_cast<ForCStyleExpr*>(expr)) analyzeForCStyleExpr(forCStyleExpr);
        else if (auto* forInExpr = dynamic_cast<ForInExpr*>(expr)) analyzeForInExpr(forInExpr);
        else if (auto* sExpr = dynamic_cast<StmtExpr*>(expr)) analyzeStmt(sExpr->stmt);
        else if (auto* arr = dynamic_cast<ArrayExpr*>(expr)) {
            for (auto& el : arr->elements) analyzeExpr(el);
        } else if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
            analyzeExpr(bin->left);
            analyzeExpr(bin->right);
        }
        if (auto* ilit = dynamic_cast<IntExpr*>(expr)) ilit->semanticType = "int";
        else if (auto* flit = dynamic_cast<FloatExpr*>(expr)) flit->semanticType = "float";
        else if (auto* slit = dynamic_cast<StringExpr*>(expr)) slit->semanticType = "string";
        else if (auto* blit = dynamic_cast<BoolExpr*>(expr)) blit->semanticType = "bool";
    }

    void emitUnusedWarnings() {
        for (auto& [name, sym] : currentScope().symbols()) {
            if (!sym.isUsed && sym.kind == SymbolInfo::VAR) LuvError::warn(ErrorKind::UNDEFINED_VARIABLE, "Variable '" + name + "' is declared but never used", file_, sym.declLine, sym.declCol);
            if (!sym.isUsed && sym.kind == SymbolInfo::FUNC && name != "main") LuvError::warn(ErrorKind::UNDEFINED_FUNCTION, "Function '" + name + "' is defined but never called", file_, sym.declLine, sym.declCol);
        }
    }
};

} // namespace luv

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
    enum Kind { VAR, FUNC, PARAM, IMPORTED_VAR, IMPORTED_FUNC, BUILTIN_FUNC, STRUCT, CLASS, INTERFACE };
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

// ─────────────────────────────────────────────────────────
//  SemanticAnalyzer: walks the AST and validates correctness
//
//  Runs AFTER parsing and BEFORE code generation.
//  Reports all errors via LuvError. If any errors are found,
//  the compiler will halt before reaching LLVM.
//
//  Checks performed:
//    1. Symbol resolution (every variable/function used must be declared)
//    2. Const reassignment detection
//    3. Function call arity checking
//    4. Import visibility enforcement (pub/priv)
//    5. Duplicate definition detection
//    6. Unused variable warnings (with suggestions)
//    7. Return statement validation
// ─────────────────────────────────────────────────────────
class SemanticAnalyzer {
public:
    SemanticAnalyzer(const std::string& file, const ModuleResolver* resolver = nullptr,
                     const std::string& moduleName = "")
        : file_(file), resolver_(resolver), moduleName_(moduleName) {
        // Check if this module has exports (i.e. it's a library, not entry point)
        if (resolver_) {
            const ModuleInfo* mod = resolver_->getModule(moduleName_);
            isLibraryModule_ = mod && !mod->exports.empty();
        }
    }

    // ── Main entry point ──
    // Returns true if the program is semantically valid
    bool analyze(Program& prog) {
        // Set up root scope
        pushScope();

        // Register builtins
        registerBuiltins();

        // Register imported symbols
        registerImports(prog);

        // First pass: register all top-level declarations (functions + vars)
        // so forward references work
        for (auto& stmt : prog.statements) {
            registerTopLevel(stmt);
        }

        // Check for main vs script exclusivity
        bool hasMain = false;
        bool hasScript = false;
        for (auto& stmt : prog.statements) {
            if (auto* fn = dynamic_cast<FuncDecl*>(stmt)) {
                if (fn->name == "main" && fn->boundStruct.empty()) hasMain = true;
            } else if (!dynamic_cast<StructDecl*>(stmt) &&
                       !dynamic_cast<EnumDecl*>(stmt) &&
                       !dynamic_cast<ClassDecl*>(stmt) &&
                       !dynamic_cast<InterfaceDecl*>(stmt) &&
                       !dynamic_cast<ExternDecl*>(stmt) &&
                       !dynamic_cast<ModuleDeclStmt*>(stmt) &&
                       !dynamic_cast<UseStmt*>(stmt) &&
                       !dynamic_cast<VarDecl*>(stmt)) {
                hasScript = true;
            }        }
        if (hasMain && hasScript) {
            LuvError::error(ErrorKind::INTERNAL_ERROR, 
                "Ambiguous entry point: file contains both top-level scripting code and a 'main' function. "
                "Please use either scripting mode (top-level code) or traditional mode (main function), not both.", 
                file_, 0, 0);
        }

        // Second pass: analyze bodies
        for (auto& stmt : prog.statements) {
            analyzeStmt(stmt);
        }

        // Emit unused variable warnings
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
    bool isLibraryModule_ = false;  // true if this module has exports (suppress unused warnings)
    std::string currentClass_ = ""; // Empty if not in a class method
    std::string currentContainer_ = ""; // Prefix for nested types
    std::stack<std::vector<std::string>> loopLabels_; // Stack of labels for nested loops
    std::map<std::string, ClassDecl*> classDecls;
    std::map<std::string, StructDecl*> structDecls;
    std::map<std::string, EnumDecl*> enumDecls;
    std::map<std::string, InterfaceDecl*> interfaceDecls;
    std::map<std::string, std::vector<std::string>> vtableLayouts; // class -> method names in order
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
        // printf is always available (variadic)
        currentScope().define("printf", {
            SymbolInfo::BUILTIN_FUNC, "int", false, false, false, true, 0, 0, -1
        });
        // Add other builtins as needed
        currentScope().define("puts", {
            SymbolInfo::BUILTIN_FUNC, "int", false, false, false, true, 0, 0, 1
        });
        currentScope().define("println", {
            SymbolInfo::BUILTIN_FUNC, "void", false, false, false, true, 0, 0, -1
        });
        currentScope().define("print", {
            SymbolInfo::BUILTIN_FUNC, "void", false, false, false, true, 0, 0, -1
        });

        // Scripting special variables
        currentScope().define("__arg__", {
            SymbolInfo::VAR, "[string]", false, true, false, true, 0, 0
        });
        currentScope().define("__name__", {
            SymbolInfo::VAR, "string", false, true, false, true, 0, 0
        });
    }

    // ── Register imported symbols ──
    void registerImports(const Program& prog) {
        if (!resolver_) return;

        for (const auto& use : prog.useStatements) {
            // Resolve which module this use points to
            std::string targetModName;
            if (use->modulePath.size() == 1) {
                targetModName = use->modulePath[0];
            } else {
                targetModName = use->modulePath[0];
                for (size_t i = 1; i < use->modulePath.size(); ++i) {
                    targetModName += "::" + use->modulePath[i];
                }
            }

            const ModuleInfo* targetMod = resolver_->getModule(targetModName);
            if (!targetMod) continue; // Module not found — already reported by ModuleResolver

            // Build the import request
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
            req.names = use->names;

            auto importedSymbols = resolver_->getImportedSymbols(moduleName_, req);
            for (const auto& sym : importedSymbols) {
                SymbolInfo info;
                info.kind = (sym.kind == ExportedSymbol::FUNCTION)
                    ? SymbolInfo::IMPORTED_FUNC
                    : SymbolInfo::IMPORTED_VAR;
                info.type = "";
                info.isMutable = false;
                info.isConst = false;
                info.isDynamic = false;
                info.isUsed = false; // will be marked when referenced
                info.declLine = 0;
                info.declCol = 0;
                info.paramCount = -1; // unknown from export metadata

                // Look up param count from the actual AST if possible
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

                currentScope().define(sym.name, info);
            }
        }
    }

    void registerTopLevel(Stmt* stmt) {
        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) {
            std::string name = func->name;
            static std::set<std::string> ops = {"+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">="};
            if (ops.count(name)) name = "operator" + name;

            if (!func->boundStruct.empty()) {
                func->name = func->boundStruct + "_" + name;
            } else {
                func->name = name;
            }
            SymbolInfo info;
            info.kind = SymbolInfo::FUNC;
            info.type = func->returnType;
            info.paramCount = (int)func->params.size();
            int minP = 0;
            for (const auto& p : func->params) if (!p.defaultVal) minP++;
            info.minParamCount = minP;
            // main is always used; exported functions are used by importers
            info.isUsed = (func->name == "main") || isLibraryModule_;
            info.declLine = 0;

            if (!currentScope().define(func->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Function '" + func->name + "' is already defined",
                    file_, 0, 0,
                    "Rename one of the definitions or remove the duplicate.");
            }
        } else if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
            // For now, only handle simple identifier patterns at top level
            if (auto* ipat = dynamic_cast<IdentifierPattern*>(var->pattern)) {
                SymbolInfo info;
                info.kind = SymbolInfo::VAR;
                info.type = var->type;
                info.isMutable = var->isMutable;
                info.isConst = var->isConst;
                info.isDynamic = var->isDynamic;
                info.isUsed = isLibraryModule_;

                if (!currentScope().define(ipat->name, info)) {
                    LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                        "Variable '" + ipat->name + "' is already defined in this scope",
                        file_, 0, 0,
                        "Use a different name or remove the duplicate declaration.");
                }
            }
        } else if (auto* ext = dynamic_cast<ExternDecl*>(stmt)) {
            SymbolInfo info;
            info.kind = SymbolInfo::FUNC;
            info.type = ext->returnType;
            info.paramCount = -1; // Disable arity check for externs (variadics support)
            info.isUsed = true; // Externs are implicitly used
            info.declLine = 0;

            // Externs are allowed to override builtins (e.g. puts, printf)
            SymbolInfo* existing = currentScope().lookupLocal(ext->name);
            if (existing && existing->kind == SymbolInfo::BUILTIN_FUNC) {
                // Silently upgrade the builtin to the user's extern signature
                *existing = info;
            } else if (!currentScope().define(ext->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Extern function '" + ext->name + "' conflicts with existing definition",
                    file_, 0, 0,
                    "Rename one of the definitions or remove the duplicate.");
            }
        } else if (auto* str = dynamic_cast<StructDecl*>(stmt)) {
            structDecls[str->name] = str;
            SymbolInfo info;
            info.kind = SymbolInfo::STRUCT;
            info.isUsed = true;
            if (!currentScope().define(str->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Struct '" + str->name + "' already defined in this scope",
                    file_, 0, 0);
            }
            std::string oldScope = currentContainer_;
            currentContainer_ = currentContainer_.empty() ? str->name : currentContainer_ + "_" + str->name;
            for (auto* nd : str->nestedDecls) {
                 if (auto* nsd = dynamic_cast<StructDecl*>(nd)) nsd->name = currentContainer_ + "_" + nsd->name;
                 else if (auto* ned = dynamic_cast<EnumDecl*>(nd)) ned->name = currentContainer_ + "_" + ned->name;
                 registerTopLevel(nd);
            }
            currentContainer_ = oldScope;
        } else if (auto* ed = dynamic_cast<EnumDecl*>(stmt)) {
            enumDecls[ed->name] = ed;
            SymbolInfo info;
            info.kind = SymbolInfo::VAR; // Enum type acts like a type/var
            info.type = ed->name;
            info.isUsed = true;
            currentScope().define(ed->name, info);
            
            // Register variants as functions or constants
            for (auto& v : ed->variants) {
                SymbolInfo vinfo;
                vinfo.kind = SymbolInfo::FUNC;
                vinfo.type = ed->name;
                vinfo.paramCount = (int)v.types.size();
                vinfo.isUsed = true;
                currentScope().define(v.name, vinfo);
            }
        } else if (auto* cls = dynamic_cast<ClassDecl*>(stmt)) {
            classDecls[cls->name] = cls;
            // ...
            for (auto& method : cls->methods) {
                registerTopLevel(method);
            }
            std::string oldScope = currentContainer_;
            currentContainer_ = currentContainer_.empty() ? cls->name : currentContainer_ + "_" + cls->name;
            for (auto* nd : cls->nestedDecls) {
                 if (auto* nsd = dynamic_cast<StructDecl*>(nd)) nsd->name = currentContainer_ + "_" + nsd->name;
                 else if (auto* ned = dynamic_cast<EnumDecl*>(nd)) ned->name = currentContainer_ + "_" + ned->name;
                 registerTopLevel(nd);
            }
            currentContainer_ = oldScope;
        } else if (auto* iface = dynamic_cast<InterfaceDecl*>(stmt)) {
            interfaceDecls[iface->name] = iface;
            SymbolInfo info;
            info.kind = SymbolInfo::INTERFACE;
            info.isUsed = true;
            if (!currentScope().define(iface->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Interface '" + iface->name + "' already defined in this scope",
                    file_, 0, 0);
            }
        }
    }

    std::string resolveType(const std::string& name) {
        if (name.empty() || name == "int" || name == "string" || name == "float" || name == "bool" || name == "void") return name;
        if (name.back() == '?') return resolveType(name.substr(0, name.size() - 1)) + "?";
        
        // Check current container context
        if (!currentContainer_.empty()) {
            std::string nested = currentContainer_ + "_" + name;
            if (structDecls.count(nested) || classDecls.count(nested) || enumDecls.count(nested)) return nested;
        }
        
        // Check global scope
        if (structDecls.count(name) || classDecls.count(name) || enumDecls.count(name)) return name;
        
        return name;
    }

    // ── Analyze statements ──
    void analyzeStmt(Stmt* stmt) {
        if (!stmt) return;

        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) {
            func->returnType = resolveType(func->returnType);
            for (auto& p : func->params) p.type = resolveType(p.type);
            analyzeFunc(func);
        } else if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
            var->type = resolveType(var->type);
            analyzeVarDecl(var);
        } else if (auto* assign = dynamic_cast<Assignment*>(stmt)) {
            analyzeAssignment(assign);
        } else if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
            analyzeReturn(ret);
        } else if (auto* brk = dynamic_cast<BreakStmt*>(stmt)) {
            analyzeBreak(brk);
        } else if (auto* cont = dynamic_cast<ContinueStmt*>(stmt)) {
            analyzeContinue(cont);
        } else if (auto* cls = dynamic_cast<ClassDecl*>(stmt)) {
            std::string oldClass = currentClass_;
            currentClass_ = cls->name;
            std::string oldContainer = currentContainer_;
            currentContainer_ = currentContainer_.empty() ? cls->name : currentContainer_ + "_" + cls->name;
            
            for (auto& f : cls->fields) f.type = resolveType(f.type);
            
            // Check interface implementations
            for (const auto& base : cls->baseAndInterfaces) {
                if (interfaceDecls.count(base)) {
                    InterfaceDecl* iface = interfaceDecls[base];
                    for (const auto& imethod : iface->methods) {
                        bool found = false;
                        for (const auto& method : cls->methods) {
                            if (method->name == cls->name + "_" + imethod.name) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            LuvError::error(ErrorKind::TYPE_ERROR,
                                "Class '" + cls->name + "' does not implement interface method '" + imethod.name + "'",
                                file_, 0, 0);
                        }
                    }
                }
            }
            
            for (auto& method : cls->methods) {
                // Check override
                if (method->isOverride) {
                    bool foundInBase = false;
                    std::string searchName = method->name.substr(cls->name.length() + 1);
                    std::string searchClass = cls->baseAndInterfaces.empty() ? "" : cls->baseAndInterfaces[0];
                    while (!searchClass.empty() && classDecls.count(searchClass)) {
                        ClassDecl* baseDecl = classDecls[searchClass];
                        for (const auto& baseMethod : baseDecl->methods) {
                            if (baseMethod->name == searchClass + "_" + searchName) {
                                foundInBase = true;
                                break;
                            }
                        }
                        if (foundInBase) break;
                        searchClass = baseDecl->baseAndInterfaces.empty() ? "" : baseDecl->baseAndInterfaces[0];
                    }
                    if (!foundInBase) {
                        LuvError::error(ErrorKind::TYPE_ERROR,
                            "Method '" + method->name + "' is marked 'override' but no base class method matches",
                            file_, 0, 0);
                    }
                }
                analyzeStmt(method);
            }
            for (auto* nd : cls->nestedDecls) analyzeStmt(nd);
            currentContainer_ = oldContainer;
            currentClass_ = oldClass;
        } else if (auto* str = dynamic_cast<StructDecl*>(stmt)) {
            std::string oldContainer = currentContainer_;
            currentContainer_ = currentContainer_.empty() ? str->name : currentContainer_ + "_" + str->name;
            for (auto& f : str->fields) f.type = resolveType(f.type);
            for (auto* nd : str->nestedDecls) analyzeStmt(nd);
            currentContainer_ = oldContainer;
        } else if (auto* exprS = dynamic_cast<ExprStmt*>(stmt)) {
            analyzeExpr(exprS->expr);
        } else if (auto* block = dynamic_cast<Block*>(stmt)) {
            analyzeBlock(block);
        }
    }

    // ── Function analysis ──
    void analyzeFunc(FuncDecl* func) {
        bool oldInsideFunc = insideFunction_;
        std::string oldFuncName = currentFunctionName_;
        bool oldHasReturn = currentFuncHasReturn_;

        insideFunction_ = true;
        currentFunctionName_ = func->name;
        currentFuncHasReturn_ = false;

        pushScope();

        // Register parameters
        for (const auto& param : func->params) {
            SymbolInfo info;
            info.kind = SymbolInfo::PARAM;
            info.type = param.type;
            info.isMutable = param.isMutable;
            info.isDynamic = param.isDynamic;
            // 'self' is always implicitly used in class methods
            info.isUsed = (param.name == "self");

            if (!currentScope().define(param.name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Parameter '" + param.name + "' shadows another parameter",
                    file_, 0, 0,
                    "Use unique names for function parameters.");
            }
        }

        // Allow implicit self inside instance methods when omitted in source.
        if (!currentScope().lookupLocal("self") && (!func->isStatic || !func->boundStruct.empty())) {
            SymbolInfo selfInfo;
            selfInfo.kind = SymbolInfo::PARAM;
            selfInfo.type = func->boundStruct.empty() ? (currentClass_.empty() ? "dyn" : currentClass_) : func->boundStruct;
            selfInfo.isMutable = true;
            selfInfo.isDynamic = selfInfo.type == "dyn";
            selfInfo.isUsed = true;
            currentScope().define("self", selfInfo);
        }

        // Analyze the body
        if (func->body) {
            for (auto& stmt : func->body->statements) {
                analyzeStmt(stmt);
            }
        }

        // Check unused parameters (only warn, don't error)
        for (const auto& [name, sym] : currentScope().symbols()) {
            if (sym.kind == SymbolInfo::PARAM && !sym.isUsed) {
                LuvError::warn(ErrorKind::UNDEFINED_VARIABLE,
                    "Parameter '" + name + "' is declared but never used in function '" +
                    func->name + "'",
                    file_, sym.declLine, sym.declCol);
            }
        }

        popScope();

        insideFunction_ = oldInsideFunc;
        currentFunctionName_ = oldFuncName;
        currentFuncHasReturn_ = oldHasReturn;
    }

    void analyzePattern(Pattern* pat, const std::string& type, bool isMutable, bool isConst, bool isDynamic) {
        if (!pat) return;
        pat->semanticType = type;

        if (auto* ipat = dynamic_cast<IdentifierPattern*>(pat)) {
            if (ipat->name == "_" || ipat->name == "ignore") return;
            SymbolInfo info;
            info.kind = SymbolInfo::VAR;
            info.type = type;
            info.isMutable = isMutable;
            info.isConst = isConst;
            info.isDynamic = isDynamic;
            info.isUsed = false;
            if (!currentScope().define(ipat->name, info)) {
                LuvError::error(ErrorKind::DUPLICATE_DEFINITION,
                    "Variable '" + ipat->name + "' is already defined in this scope",
                    file_, 0, 0);
            }
        } else if (auto* tpat = dynamic_cast<TuplePattern*>(pat)) {
            for (auto* el : tpat->elements) {
                analyzePattern(el, "", isMutable, isConst, isDynamic);
            }
        } else if (auto* spat = dynamic_cast<StructPattern*>(pat)) {
            spat->structName = resolveType(spat->structName);
            if (!spat->structName.empty() && structDecls.count(spat->structName)) {
                auto* sd = structDecls[spat->structName];
                for (auto& f : spat->fields) {
                    std::string fType = "";
                    for (const auto& sdf : sd->fields) if (sdf.name == f.first) { fType = sdf.type; break; }
                    analyzePattern(f.second, fType, isMutable, isConst, isDynamic);
                }
            }
        } else if (auto* vpat = dynamic_cast<VariantPattern*>(pat)) {
            // Variant name might be Shape_Circle etc. if it's already mangled, but usually it's just Circle
            // We'd need to find which enum it belongs to.
            for (auto* el : vpat->elements) {
                analyzePattern(el, "", isMutable, isConst, isDynamic);
            }
        } else if (auto* lpat = dynamic_cast<LiteralPattern*>(pat)) {
            analyzeExpr(lpat->literal);
        }
    }

    // ── Variable declaration analysis ──
    void analyzeVarDecl(VarDecl* var) {
        // Analyze initializer first for inference
        std::string inferredType = var->type;
        if (var->init) {
            analyzeExpr(var->init);
            if (inferredType.empty()) {
                inferredType = var->init->semanticType;
            }
            
            // Null safety check
            if (var->init->semanticType == "nen") {
                if (!inferredType.empty() && inferredType != "nen" && inferredType.back() != '?') {
                    LuvError::error(ErrorKind::TYPE_ERROR,
                        "Cannot assign 'nen' to non-optional type '" + inferredType + "'",
                        file_, 0, 0,
                        "Use an optional type (e.g., '" + inferredType + "?') or provide a non-null value.");
                }
            }
        }
        var->type = inferredType;

        // If inside a function, define in current scope
        if (insideFunction_) {
            analyzePattern(var->pattern, inferredType, var->isMutable, var->isConst, var->isDynamic);
        }
    }

    // ── Assignment analysis ──
    void analyzeAssignment(Assignment* assign) {
        if (assign->targets.empty()) return;
        Expr* targetExpr = assign->targets[0];
        
        // Analyze the value expression first to get its type
        analyzeExpr(assign->value);

        if (auto* varExpr = dynamic_cast<VarExpr*>(targetExpr)) {
            SymbolInfo* sym = currentScope().lookup(varExpr->name);
            if (sym) {
                // Check const reassignment
                if (sym->isConst) {
                    LuvError::error(ErrorKind::CONST_REASSIGNMENT,
                        "Cannot reassign constant variable '" + varExpr->name + "'",
                        file_, 0, 0,
                        "Use 'mut' instead of 'const' if you need to reassign this variable.");
                }
                
                // Null safety check
                if (assign->value->semanticType == "nen") {
                    if (!sym->type.empty() && sym->type != "nen" && sym->type.back() != '?') {
                        LuvError::error(ErrorKind::TYPE_ERROR,
                            "Cannot assign 'nen' to non-optional type '" + sym->type + "' of variable '" + varExpr->name + "'",
                            file_, 0, 0);
                    }
                }

                sym->isUsed = true;
                sym->isAssigned = true;
            } else {
                // Implicit declaration (Luv allows `x = 42` without var keyword)
                SymbolInfo info;
                info.kind = SymbolInfo::VAR;
                info.type = assign->value->semanticType;
                info.isMutable = true;
                info.isDynamic = false;
                info.isUsed = true;
                currentScope().define(varExpr->name, info);
            }
        } else if (auto* propExpr = dynamic_cast<PropertyExpr*>(targetExpr)) {
            analyzeExpr(propExpr->object);
        }

        // Infer/propagate assignment type for implicit and untyped variables.
        if (auto* varExpr = dynamic_cast<VarExpr*>(targetExpr)) {
            SymbolInfo* sym = currentScope().lookup(varExpr->name);
            if (sym && sym->type.empty() && assign->value) {
                sym->type = assign->value->semanticType;
            }
        }
    }

    // ── If/ef/else analysis ──
    void analyzeIfExpr(IfExpr* ifExpr) {
        analyzeExpr(ifExpr->cond);
        if (ifExpr->thenBlock) analyzeBlock(ifExpr->thenBlock);
        for (auto& ef : ifExpr->efs) {
            analyzeExpr(ef.cond);
            if (ef.block) analyzeBlock(ef.block);
        }
        if (ifExpr->elseBlock) analyzeBlock(ifExpr->elseBlock);

        // Infer semanticType from branches
        std::string commonType;
        auto checkBlock = [&](Block* b) {
            if (b && !b->statements.empty()) {
                if (auto* es = dynamic_cast<ExprStmt*>(b->statements.back())) {
                    if (commonType.empty()) commonType = es->expr->semanticType;
                    else if (commonType != es->expr->semanticType) commonType = "dyn";
                }
            }
        };
        checkBlock(ifExpr->thenBlock);
        for (auto& ef : ifExpr->efs) checkBlock(ef.block);
        checkBlock(ifExpr->elseBlock);
        ifExpr->semanticType = commonType.empty() ? "void" : commonType;
    }

    void analyzeMatchExpr(MatchExpr* match) {
        analyzeExpr(match->value);
        std::string commonType;
        for (auto& case_ : match->cases) {
            pushScope();
            if (case_.pattern) {
                analyzePattern(case_.pattern, match->value->semanticType, false, false, false);
            }
            
            if (case_.resultExpr) {
                analyzeExpr(case_.resultExpr);
                if (commonType.empty()) commonType = case_.resultExpr->semanticType;
                else if (commonType != case_.resultExpr->semanticType) commonType = "dyn";
            }
            if (case_.resultBlock) {
                for (auto& stmt : case_.resultBlock->statements) {
                    analyzeStmt(stmt);
                }
                if (!case_.resultBlock->statements.empty()) {
                    if (auto* es = dynamic_cast<ExprStmt*>(case_.resultBlock->statements.back())) {
                        if (commonType.empty()) commonType = es->expr->semanticType;
                        else if (commonType != es->expr->semanticType) commonType = "dyn";
                    }
                }
            }
            popScope();
        }
        match->semanticType = commonType.empty() ? "void" : commonType;
    }

    void analyzeArrayCompExpr(ArrayCompExpr* ac) {
        analyzeExpr(ac->iterable);
        if (ac->rangeEnd) analyzeExpr(ac->rangeEnd);
        if (ac->step) analyzeExpr(ac->step);
        
        pushScope();
        std::string loopVarType = "int";
        if (!ac->rangeEnd) {
            std::string iterType = ac->iterable->semanticType;
            if (iterType.size() >= 2 && iterType.front() == '[' && iterType.back() == ']') {
                loopVarType = iterType.substr(1, iterType.size() - 2);
            }
        }
        currentScope().define(ac->varName, {SymbolInfo::VAR, loopVarType, false});
        analyzeExpr(ac->expr);
        ac->semanticType = "[" + ac->expr->semanticType + "]";
        popScope();
    }

    // ── While expression analysis ──
    void analyzeWhileExpr(WhileExpr* whileExpr) {
        analyzeExpr(whileExpr->cond);
        loopLabels_.push(whileExpr->attributes);
        if (whileExpr->body) analyzeBlock(whileExpr->body);
        loopLabels_.pop();
    }

    // ── For range expression analysis ──
    void analyzeForRangeExpr(ForRangeExpr* forRangeExpr) {
        analyzeExpr(forRangeExpr->start);
        analyzeExpr(forRangeExpr->end);

        loopLabels_.push(forRangeExpr->attributes);
        pushScope();
        analyzePattern(forRangeExpr->pattern, "int", true, false, forRangeExpr->isDynamic);

        if (forRangeExpr->body) {
            for (auto& stmt : forRangeExpr->body->statements) {
                analyzeStmt(stmt);
            }
        }
        popScope();
        loopLabels_.pop();
    }

    // ── C-style for expression analysis ──
    void analyzeForCStyleExpr(ForCStyleExpr* forC) {
        loopLabels_.push(forC->attributes);
        pushScope();
        analyzeStmt(forC->init);
        analyzeExpr(forC->cond);
        analyzeStmt(forC->step);
        if (forC->body) {
            for (auto& stmt : forC->body->statements) {
                analyzeStmt(stmt);
            }
        }
        popScope();
        loopLabels_.pop();
    }

    // ── For-in expression analysis ──
    void analyzeForInExpr(ForInExpr* forInExpr) {
        analyzeExpr(forInExpr->iterable);

        loopLabels_.push(forInExpr->attributes);
        pushScope();
        analyzePattern(forInExpr->pattern, "", true, false, forInExpr->isDynamic);

        if (forInExpr->body) {
            for (auto& stmt : forInExpr->body->statements) {
                analyzeStmt(stmt);
            }
        }
        popScope();
        loopLabels_.pop();
    }

    void analyzeBreak(BreakStmt* b) {
        if (loopLabels_.empty()) {
            LuvError::error(ErrorKind::SYNTAX_ERROR, "'break' statement outside of a loop", file_, 0, 0);
            return;
        }
        if (!b->label.empty()) {
            bool found = false;
            // Check all nested loops in the stack
            std::stack<std::vector<std::string>> copy = loopLabels_;
            while (!copy.empty()) {
                for (const auto& l : copy.top()) {
                    if (l == b->label) { found = true; break; }
                }
                if (found) break;
                copy.pop();
            }
            if (!found) {
                LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined loop label '" + b->label + "'", file_, 0, 0);
            }
        }
    }

    void analyzeContinue(ContinueStmt* c) {
        if (loopLabels_.empty()) {
            LuvError::error(ErrorKind::SYNTAX_ERROR, "'continue' statement outside of a loop", file_, 0, 0);
            return;
        }
        if (!c->label.empty()) {
            bool found = false;
            std::stack<std::vector<std::string>> copy = loopLabels_;
            while (!copy.empty()) {
                for (const auto& l : copy.top()) {
                    if (l == c->label) { found = true; break; }
                }
                if (found) break;
                copy.pop();
            }
            if (!found) {
                LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined loop label '" + c->label + "'", file_, 0, 0);
            }
        }
    }

    // ── Return analysis ──
    void analyzeReturn(ReturnStmt* ret) {
        if (!insideFunction_) {
            LuvError::error(ErrorKind::SYNTAX_ERROR,
                "'return' statement outside of a function",
                file_, 0, 0,
                "Return statements can only appear inside function bodies.");
        }
        currentFuncHasReturn_ = true;
        if (ret->value) {
            analyzeExpr(ret->value);
        }
    }

    // ── Block analysis ──
    void analyzeBlock(Block* block) {
        pushScope();
        for (auto& stmt : block->statements) {
            analyzeStmt(stmt);
        }
        popScope();
    }

    // ── Expression analysis ──
    void analyzeExpr(Expr* expr) {
        if (!expr) return;

        if (auto* var = dynamic_cast<VarExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(var->name);
            if (!sym) {
                if (var->name == "self" && !currentClass_.empty()) {
                    var->semanticType = currentClass_;
                    return;
                }
                // Try to provide a helpful suggestion
                std::string suggestion = findSimilarSymbol(var->name);
                std::string hint;
                if (!suggestion.empty()) {
                    hint = "Did you mean '" + suggestion + "'?";
                } else {
                    hint = "Declare it with: " + var->name + " = <value>, or import it from a module.";
                }
                LuvError::error(ErrorKind::UNDEFINED_VARIABLE,
                    "Undefined variable '" + var->name + "'",
                    file_, 0, 0, hint);
            } else {
                sym->isUsed = true;
                if (sym->kind == SymbolInfo::CLASS) {
                    var->semanticType = var->name;
                } else {
                    var->semanticType = sym->type;
                }
            }
        } else if (auto* call = dynamic_cast<CallExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(call->callee);
            if (!sym) {
                std::string suggestion = findSimilarSymbol(call->callee, true);
                std::string hint;
                if (!suggestion.empty()) {
                    hint = "Did you mean '" + suggestion + "()'?";
                } else {
                    hint = "Define the function or import it using: use " +
                           call->callee + " from <module>";
                }
                LuvError::error(ErrorKind::UNDEFINED_FUNCTION,
                    "Undefined function/class '" + call->callee + "'",
                    file_, 0, 0, hint);
            } else {
                sym->isUsed = true;
                
                if (sym->kind == SymbolInfo::CLASS) {
                    if (classDecls.count(call->callee) && classDecls[call->callee]->isAbstract) {
                        LuvError::error(ErrorKind::TYPE_ERROR,
                            "Cannot instantiate abstract class '" + call->callee + "'",
                            file_, 0, 0);
                    }
                    call->semanticType = call->callee;
                } else {
                    call->semanticType = sym->type;
                }

                // Check arity
                if (sym->paramCount >= 0) {
                    if ((int)call->args.size() < sym->minParamCount || (int)call->args.size() > sym->paramCount) {
                        std::string expected;
                        if (sym->minParamCount == sym->paramCount) expected = std::to_string(sym->paramCount);
                        else expected = std::to_string(sym->minParamCount) + " to " + std::to_string(sym->paramCount);

                        LuvError::error(ErrorKind::TYPE_MISMATCH,
                            "Function '" + call->callee + "' expects " + expected + " argument" +
                            (sym->paramCount == 1 ? "" : "s") + ", but " +
                            std::to_string(call->args.size()) + " " +
                            (call->args.size() == 1 ? "was" : "were") + " provided",
                            file_, 0, 0,
                            "Check the function signature and adjust your call.");
                    }
                }
            }

            // Analyze each argument
            for (auto& arg : call->args) {
                analyzeExpr(arg);
            }
        } else if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
            analyzeExpr(unary->expr);
        } else if (auto* gcall = dynamic_cast<GenericCallExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(gcall->callee);
            if (!sym) {
                LuvError::error(ErrorKind::UNDEFINED_FUNCTION,
                    "Undefined generic function '" + gcall->callee + "'",
                    file_, 0, 0);
            } else {
                sym->isUsed = true;
            }
            for (auto& arg : gcall->args) {
                analyzeExpr(arg);
            }
        } else if (auto* intrinsic = dynamic_cast<IntrinsicCallExpr*>(expr)) {
            for (auto& arg : intrinsic->args) {
                analyzeExpr(arg);
            }
            if (intrinsic->callee == "println" || intrinsic->callee == "print") {
                intrinsic->semanticType = "void";
            } else if (intrinsic->callee == "sizeof" || intrinsic->callee == "popcount") {
                intrinsic->semanticType = "int";
            }
        } else if (auto* cast = dynamic_cast<CastExpr*>(expr)) {
            analyzeExpr(cast->expr);
            cast->semanticType = cast->targetType;
        } else if (auto* prop = dynamic_cast<PropertyExpr*>(expr)) {
            analyzeExpr(prop->object);
            std::string objType = prop->object->semanticType;
            if (!objType.empty() && structDecls.count(objType)) {
                auto* sd = structDecls[objType];
                for (const auto& f : sd->fields) {
                    if (f.name == prop->propertyName) {
                        prop->semanticType = f.type;
                        return;
                    }
                }
            }
            if (!objType.empty() && classDecls.count(objType)) {
                std::string searchClass = objType;
                while (!searchClass.empty() && classDecls.count(searchClass)) {
                    auto* cd = classDecls[searchClass];
                    for (const auto& f : cd->fields) {
                        if (f.name == prop->propertyName) {
                            prop->semanticType = f.type;
                            return;
                        }
                    }
                    searchClass = cd->baseAndInterfaces.empty() ? "" : cd->baseAndInterfaces[0];
                }
            }
        } else if (auto* meth = dynamic_cast<MethodCallExpr*>(expr)) {
            analyzeExpr(meth->object);
            for (auto& arg : meth->args) analyzeExpr(arg);

            // Built-in string method return types.
            if (meth->object && meth->object->semanticType == "string") {
                if (meth->methodName == "length" || meth->methodName == "indexOf") {
                    meth->semanticType = "int";
                } else if (meth->methodName == "contains" ||
                           meth->methodName == "startsWith" ||
                           meth->methodName == "endsWith") {
                    meth->semanticType = "bool";
                } else {
                    meth->semanticType = "string";
                }
            } else if (meth->methodName == "toString") {
                meth->semanticType = "string";
            }

            // Enforce private method access at compile time
            std::string objType;
            if (auto* varObj = dynamic_cast<VarExpr*>(meth->object)) {
                SymbolInfo* objSym = currentScope().lookup(varObj->name);
                if (objSym) objType = objSym->type;
            }
            if (!objType.empty() && classDecls.count(objType)) {
                // Walk inheritance chain to find the method
                std::string searchClass = objType;
                while (!searchClass.empty() && classDecls.count(searchClass)) {
                    ClassDecl* cls = classDecls[searchClass];
                    for (auto& m : cls->methods) {
                        if (m->name == searchClass + "_" + meth->methodName) {
                            if (m->visibility == ASTVisibility::PRIVATE && searchClass != currentClass_) {
                                // Check if currentClass_ is a subclass of searchClass
                                bool canAccess = false;
                                std::string walk = currentClass_;
                                while (!walk.empty() && classDecls.count(walk)) {
                                    if (walk == searchClass) { canAccess = true; break; }
                                    ClassDecl* wc = classDecls[walk];
                                    walk = wc->baseAndInterfaces.empty() ? "" : wc->baseAndInterfaces[0];
                                }
                                if (!canAccess) {
                                    LuvError::error(ErrorKind::TYPE_ERROR,
                                        "Cannot access private method '" + meth->methodName +
                                        "' of class '" + searchClass + "'",
                                        file_, 0, 0,
                                        "Private methods can only be called from within the class or its subclasses.");
                                }
                            }
                            goto method_found;
                        }
                    }
                    searchClass = cls->baseAndInterfaces.empty() ? "" : cls->baseAndInterfaces[0];
                }
                method_found:;
            }
        } else if (auto* superCall = dynamic_cast<SuperCallExpr*>(expr)) {
            // Validate super calls
            if (currentClass_.empty()) {
                LuvError::error(ErrorKind::TYPE_ERROR,
                    "'super' can only be used inside class methods",
                    file_, 0, 0);
            } else if (!classDecls.count(currentClass_) ||
                       classDecls[currentClass_]->baseAndInterfaces.empty()) {
                LuvError::error(ErrorKind::TYPE_ERROR,
                    "Class '" + currentClass_ + "' has no superclass to call 'super' on",
                    file_, 0, 0);
            }
            for (auto& arg : superCall->args) analyzeExpr(arg);
        } else if (auto* interp = dynamic_cast<StringInterpolationExpr*>(expr)) {
            for (auto& part : interp->parts) {
                analyzeExpr(part);
            }
            interp->semanticType = "string";
        } else if (auto* sInst = dynamic_cast<StructInstExpr*>(expr)) {
            SymbolInfo* sym = currentScope().lookup(sInst->structName);
            if (!sym || sym->kind != SymbolInfo::STRUCT) {
                LuvError::error(ErrorKind::UNDEFINED_VARIABLE,
                    "Undefined struct '" + sInst->structName + "'",
                    file_, 0, 0);
            } else {
                sym->isUsed = true;
            }
            for (auto& f : sInst->fields) {
                analyzeExpr(f.second);
            }
            sInst->semanticType = sInst->structName;
        } else if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
            analyzeExpr(index->target);
            analyzeExpr(index->index);
            if (index->target->semanticType.size() > 2 && index->target->semanticType[0] == '[') {
                index->semanticType = index->target->semanticType.substr(1, index->target->semanticType.size() - 2);
            }
        } else if (auto* postfix = dynamic_cast<PostfixExpr*>(expr)) {
            analyzeExpr(postfix->expr);
        } else if (auto* matchExpr = dynamic_cast<MatchExpr*>(expr)) {
            analyzeMatchExpr(matchExpr);
        } else if (auto* ternary = dynamic_cast<TernaryExpr*>(expr)) {
            analyzeExpr(ternary->condition);
            analyzeExpr(ternary->thenExpr);
            analyzeExpr(ternary->elseExpr);
            ternary->semanticType = ternary->thenExpr->semanticType;
        } else if (auto* chain = dynamic_cast<ComparisonChainExpr*>(expr)) {
            for (auto* opnd : chain->operands) analyzeExpr(opnd);
            chain->semanticType = "bool";
        } else if (auto* ifExpr = dynamic_cast<IfExpr*>(expr)) {
            analyzeIfExpr(ifExpr);
        } else if (auto* whileExpr = dynamic_cast<WhileExpr*>(expr)) {
            analyzeWhileExpr(whileExpr);
        } else if (auto* forRangeExpr = dynamic_cast<ForRangeExpr*>(expr)) {
            analyzeForRangeExpr(forRangeExpr);
        } else if (auto* forCStyleExpr = dynamic_cast<ForCStyleExpr*>(expr)) {
            analyzeForCStyleExpr(forCStyleExpr);
        } else if (auto* forInExpr = dynamic_cast<ForInExpr*>(expr)) {
            analyzeForInExpr(forInExpr);
        } else if (auto* sExpr = dynamic_cast<StmtExpr*>(expr)) {
            analyzeStmt(sExpr->stmt);
        } else if (auto* arr = dynamic_cast<ArrayExpr*>(expr)) {
            std::string commonType;
            for (auto& el : arr->elements) {
                analyzeExpr(el);
                if (commonType.empty()) commonType = el->semanticType;
                else if (commonType != el->semanticType) commonType = "dyn";
            }
            if (commonType.empty()) commonType = "dyn";
            arr->semanticType = "[" + commonType + "]";
        } else if (auto* arep = dynamic_cast<ArrayRepeatExpr*>(expr)) {
            analyzeExpr(arep->value);
            analyzeExpr(arep->count);
            arep->semanticType = "[" + arep->value->semanticType + "]";
        } else if (auto* tup = dynamic_cast<TupleExpr*>(expr)) {
            std::string t = "(";
            for (size_t i = 0; i < tup->elements.size(); ++i) {
                analyzeExpr(tup->elements[i]);
                t += tup->elements[i]->semanticType;
                if (i < tup->elements.size() - 1) t += ",";
            }
            t += ")";
            tup->semanticType = t;
        } else if (auto* sl = dynamic_cast<SliceExpr*>(expr)) {
            analyzeExpr(sl->target);
            if (sl->start) analyzeExpr(sl->start);
            if (sl->end) analyzeExpr(sl->end);
            if (sl->step) analyzeExpr(sl->step);
            sl->semanticType = sl->target->semanticType;
        } else if (auto* ac = dynamic_cast<ArrayCompExpr*>(expr)) {
            analyzeArrayCompExpr(ac);
        } else if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
            analyzeExpr(bin->left);
            analyzeExpr(bin->right);
            
            // Check for operator overloading
            std::string leftType = bin->left->semanticType;
            if (!leftType.empty() && (classDecls.count(leftType) || structDecls.count(leftType))) {
                std::string opName = "operator" + bin->op;
                SymbolInfo* opSym = currentScope().lookup(leftType + "_" + opName);
                if (opSym) {
                    bin->semanticType = opSym->type;
                    return;
                }
            }

            // Creative inference: infer missing parameter types from usage
            if (bin->left->semanticType.empty() && !bin->right->semanticType.empty()) {
                if (auto* var = dynamic_cast<VarExpr*>(bin->left)) {
                    SymbolInfo* sym = currentScope().lookup(var->name);
                    if (sym && sym->kind == SymbolInfo::PARAM && sym->type.empty()) {
                        sym->type = bin->right->semanticType;
                        var->semanticType = sym->type;
                    }
                }
            } else if (bin->right->semanticType.empty() && !bin->left->semanticType.empty()) {
                if (auto* var = dynamic_cast<VarExpr*>(bin->right)) {
                    SymbolInfo* sym = currentScope().lookup(var->name);
                    if (sym && sym->kind == SymbolInfo::PARAM && sym->type.empty()) {
                        sym->type = bin->left->semanticType;
                        var->semanticType = sym->type;
                    }
                }
            }
            
            // Compile-time check for division by zero
            if (bin->op == "/" || bin->op == "%") {
                if (auto* literal = dynamic_cast<IntExpr*>(bin->right)) {
                    if (literal->value == "0") {
                        LuvError::error(ErrorKind::TYPE_ERROR,
                            "Compile-time error: Division or modulo by zero",
                            file_, 0, 0,
                            "Adjust your logic to ensure the divisor is non-zero.");
                    }
                }
            }

            // Basic expression type propagation.
            if (bin->op == "+" &&
                (bin->left->semanticType == "string" || bin->right->semanticType == "string")) {
                bin->semanticType = "string";
            } else if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || bin->op == ">" ||
                       bin->op == "<=" || bin->op == ">=" || bin->op == "&&" || bin->op == "||" ||
                       bin->op == "and" || bin->op == "or") {
                bin->semanticType = "bool";
            } else if (!bin->left->semanticType.empty()) {
                bin->semanticType = bin->left->semanticType;
            } else {
                bin->semanticType = bin->right->semanticType;
            }
        }
        
        // Literals (IntExpr, FloatExpr, etc.) and AsmExpr are always valid
        if (auto* ilit = dynamic_cast<IntExpr*>(expr)) {
            ilit->semanticType = "int";
        } else if (auto* flit = dynamic_cast<FloatExpr*>(expr)) {
            flit->semanticType = "float";
        } else if (auto* slit = dynamic_cast<StringExpr*>(expr)) {
            slit->semanticType = "string";
        } else if (auto* blit = dynamic_cast<BoolExpr*>(expr)) {
            blit->semanticType = "bool";
        } else if (auto* nlit = dynamic_cast<NullExpr*>(expr)) {
            nlit->semanticType = "nen";
        }
    }

    // ── Fuzzy matching for "did you mean?" suggestions ──
    // Prefers symbols of the same kind (variable vs function)
    std::string findSimilarSymbol(const std::string& name, bool isCall = false) const {
        std::string best;
        int bestDist = 999;
        bool bestKindMatch = false;

        // Collect all symbols from all scopes
        const Scope* scope = scopes_.empty() ? nullptr : scopes_.top().get();
        while (scope) {
            for (const auto& [sym, info] : scope->symbols()) {
                int dist = levenshtein(name, sym);
                if (dist > 3 || dist == 0) continue;

                // Prefer symbols of matching kind
                bool kindMatch = isCall
                    ? (info.kind == SymbolInfo::FUNC || info.kind == SymbolInfo::IMPORTED_FUNC || info.kind == SymbolInfo::BUILTIN_FUNC)
                    : (info.kind == SymbolInfo::VAR || info.kind == SymbolInfo::IMPORTED_VAR || info.kind == SymbolInfo::PARAM);

                // Better if: closer distance, or same distance but matching kind
                if (dist < bestDist || (dist == bestDist && kindMatch && !bestKindMatch)) {
                    bestDist = dist;
                    best = sym;
                    bestKindMatch = kindMatch;
                }
            }
            scope = scope->parent();
        }
        return best;
    }

    // ── Levenshtein distance for typo detection ──
    static int levenshtein(const std::string& a, const std::string& b) {
        int m = (int)a.size(), n = (int)b.size();
        std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
        for (int i = 0; i <= m; ++i) dp[i][0] = i;
        for (int j = 0; j <= n; ++j) dp[0][j] = j;
        for (int i = 1; i <= m; ++i) {
            for (int j = 1; j <= n; ++j) {
                int cost = (a[i-1] == b[j-1]) ? 0 : 1;
                dp[i][j] = std::min({dp[i-1][j] + 1, dp[i][j-1] + 1, dp[i-1][j-1] + cost});
            }
        }
        return dp[m][n];
    }

    // ── Emit warnings for unused variables ──
    void emitUnusedWarnings() {
        for (auto& [name, sym] : currentScope().symbols()) {
            if (!sym.isUsed && sym.kind == SymbolInfo::VAR) {
                LuvError::warn(ErrorKind::UNDEFINED_VARIABLE,
                    "Variable '" + name + "' is declared but never used",
                    file_, sym.declLine, sym.declCol);
            } else if (sym.isUsed && sym.kind == SymbolInfo::VAR && sym.isMutable && !sym.isAssigned && !sym.isConst) {
                // Optimization: if a variable is used but never reassigned, mark it as const
                sym.isConst = true;
                sym.isMutable = false;
                LuvError::warn(ErrorKind::CODE_OPTIMIZATION,
                    "Variable '" + name + "' is never reassigned; marking as 'const' for optimization. " +
                    "Explicitly declare as 'const " + name + "' to avoid this warning.",
                    file_, sym.declLine, sym.declCol);
            }
            if (!sym.isUsed && sym.kind == SymbolInfo::FUNC && name != "main") {
                LuvError::warn(ErrorKind::UNDEFINED_FUNCTION,
                    "Function '" + name + "' is defined but never called",
                    file_, sym.declLine, sym.declCol);
            }
            if (!sym.isUsed &&
                (sym.kind == SymbolInfo::IMPORTED_FUNC || sym.kind == SymbolInfo::IMPORTED_VAR)) {
                LuvError::warn(ErrorKind::IMPORT_ERROR,
                    "Imported symbol '" + name + "' is never used",
                    file_, sym.declLine, sym.declCol);
            }
        }
    }
};

} // namespace luv

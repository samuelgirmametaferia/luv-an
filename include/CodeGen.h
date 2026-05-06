#pragma once
#include "ast/AST.h"
#include "LuvError.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include <set>

namespace luv {

class CodeGen : public ASTVisitor {
public:
    CodeGen(llvm::LLVMContext& ctx, const std::string& modName) 
        : context(ctx), builder(ctx), module(std::make_unique<llvm::Module>(modName, ctx)) {}

    llvm::Module& getModule() { return *module; }
    std::unique_ptr<llvm::Module> takeModule() { return std::move(module); }
    llvm::LLVMContext& getContext() { return context; }

    // Register an external function from an imported module
    void registerExternalFunction(const std::string& name, llvm::FunctionType* type) {
        if (!module->getFunction(name)) {
            llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, *module);
        }
    }

    // Register an external global from an imported module
    void registerExternalGlobal(const std::string& name, llvm::Type* type) {
        if (!module->getGlobalVariable(name)) {
            new llvm::GlobalVariable(*module, type, false,
                llvm::GlobalValue::ExternalLinkage, nullptr, name);
        }
    }

    // Track which symbols are imported into this module
    void addImportedSymbol(const std::string& name) {
        importedSymbols_.insert(name);
    }

    bool isImported(const std::string& name) const {
        return importedSymbols_.count(name) > 0;
    }

    // ── Visitor overrides ──
    llvm::Value* visit(IntExpr& node) override;
    llvm::Value* visit(FloatExpr& node) override;
    llvm::Value* visit(StringExpr& node) override;
    llvm::Value* visit(CharExpr& node) override;
    llvm::Value* visit(BoolExpr& node) override;
    llvm::Value* visit(NullExpr& node) override;
    llvm::Value* visit(VarExpr& node) override;
    llvm::Value* visit(BinaryExpr& node) override;
    llvm::Value* visit(IdentifierPattern& node) override;
    llvm::Value* visit(TuplePattern& node) override;
    llvm::Value* visit(StructPattern& node) override;
    llvm::Value* visit(VariantPattern& node) override;
    llvm::Value* visit(WildcardPattern& node) override;
    llvm::Value* visit(LiteralPattern& node) override;
    llvm::Value* visit(EnumDecl& node) override;
    llvm::Value* visit(UnaryExpr& node) override;
    llvm::Value* visit(CallExpr& node) override;
    llvm::Value* visit(GenericCallExpr& node) override;
    llvm::Value* visit(StructInstExpr& node) override;
    llvm::Value* visit(IntrinsicCallExpr& node) override;
    llvm::Value* visit(AsmExpr& node) override;
    llvm::Value* visit(MethodCallExpr& node) override;
    llvm::Value* visit(SuperCallExpr& node) override;
    llvm::Value* visit(PropertyExpr& node) override;
    llvm::Value* visit(CastExpr& node) override;
    llvm::Value* visit(TernaryExpr& node) override;
    llvm::Value* visit(ComparisonChainExpr& node) override;
    llvm::Value* visit(StmtExpr& node) override;
    llvm::Value* visit(StringInterpolationExpr& node) override;
    llvm::Value* visit(Block& node) override;
    llvm::Value* visit(MatchExpr& node) override;
    llvm::Value* visit(VarDecl& node) override;
    llvm::Value* visit(Assignment& node) override;
    llvm::Value* visit(IfExpr& node) override;
    llvm::Value* visit(WhileExpr& node) override;
    llvm::Value* visit(ForRangeExpr& node) override;
    llvm::Value* visit(ForCStyleExpr& node) override;
    llvm::Value* visit(ForInExpr& node) override;
    llvm::Value* visit(ReturnStmt& node) override;
    llvm::Value* visit(BreakStmt& node) override;
    llvm::Value* visit(ContinueStmt& node) override;
    llvm::Value* visit(ExprStmt& node) override;
    llvm::Value* visit(FuncDecl& node) override;
    llvm::Value* visit(StructDecl& node) override;
    llvm::Value* visit(ClassDecl& node) override;
    llvm::Value* visit(InterfaceDecl& node) override;
    llvm::Value* visit(ExternDecl& node) override;
    llvm::Value* visit(ModuleDeclStmt& node) override;
    llvm::Value* visit(IndexExpr& node) override;
    llvm::Value* visit(PostfixExpr& node) override;
    llvm::Value* visit(UseStmt& node) override;
    llvm::Value* visit(Program& node) override;
    llvm::Value* visit(ArrayExpr& node) override;
    llvm::Value* visit(ArrayRepeatExpr& node) override;
    llvm::Value* visit(ArrayCompExpr& node) override;
    llvm::Value* visit(TupleExpr& node) override;
    llvm::Value* visit(SliceExpr& node) override;

private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;

    struct VarInfo {
        llvm::Value* ptr;
        llvm::Type* type;
        bool isMut;
    };
    std::map<std::string, VarInfo> namedValues;
    std::map<std::string, VarDecl*> delayedVars;
    std::map<std::string, std::vector<bool>> funcMutParams;
    std::map<std::string, std::string> mangledNames;
    std::map<std::string, FuncDecl*> functionDecls;
    std::map<std::string, FuncDecl*> genericFunctions;
    std::map<std::string, std::string> typeSubstitutions;
    std::map<std::string, llvm::StructType*> structTypes;
    std::map<std::string, StructDecl*> structDecls;
    std::map<std::string, llvm::StructType*> enumTypes;
    std::map<std::string, EnumDecl*> enumDecls;
    std::map<std::string, llvm::StructType*> classTypes;
    std::map<std::string, ClassDecl*> classDecls;
    std::map<std::string, std::vector<std::string>> vtableLayouts;
    std::map<std::string, llvm::StructType*> vtableTypes;
    std::set<std::string> interfaceNames;
    std::map<std::string, InterfaceDecl*> interfaceDecls;
    std::map<std::string, std::vector<std::string>> interfaceLayouts;
    std::map<std::string, std::string> varSemanticTypes; // var name -> type name
    std::vector<std::string> classContextStack;
    
    struct LoopBlocks {
        llvm::BasicBlock* continueBB;
        llvm::BasicBlock* exitBB;
        std::vector<std::string> labels;
    };
    std::vector<LoopBlocks> loopStack;

    std::set<std::string> defines_;
    std::set<std::string> excludes_;
    std::set<std::string> importedSymbols_;

    public:
    void addDefine(const std::string& d) { defines_.insert(d); }
    void addExclude(const std::string& e) { excludes_.insert(e); }

    llvm::Type* getType(const std::string& typeName);
    std::string getTypeNameFromLLVM(llvm::Type* t);
    llvm::Value* generateToString(llvm::Value* val);
    llvm::Function* getOrCreateStringConcat();
    llvm::Value* toBool(llvm::Value* val);
    void generateNullCheck(llvm::Value* val, const std::string& msg);
    void generateDivByZeroCheck(llvm::Value* val);
    void generateBoundsCheck(llvm::Value* index, llvm::Value* length);
    llvm::Value* generatePrint(const std::vector<Expr*>& args, bool newline);

    llvm::Value* lastValue = nullptr;
    llvm::Value* blockLastValue = nullptr;
    llvm::Type* currentReturnType = nullptr;
    llvm::Function* scriptInitFunc = nullptr;
    void generateMainWrapper();
    bool generatePatternMatch(llvm::Value* val, Pattern* pat, llvm::BasicBlock* successBB, llvm::BasicBlock* failBB);
    void generatePatternDestructuring(llvm::Value* val, Pattern* pat, bool isMut, const std::string& baseType);
};

} // namespace luv

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "antlr4-runtime.h"
#include "LuvLexer.h"
#include "LuvParser.h"
#include "LuvVisitorImpl.h"
#include "Arena.h"
#include "CodeGen.h"
#include "LuvErrorListener.h"
#include "LuvError.h"
#include "ModuleResolver.h"
#include "SemanticAnalyzer.h"
#include "rss/RSSPipeline.h"
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Linker/Linker.h>

using namespace antlr4;
using namespace luv;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────
//  CLI help
// ─────────────────────────────────────────────────────────
void printHelp() {
    std::cout << "\033[1;35m" << "Luv Programming Language CLI" << "\033[0m" << std::endl;
    std::cout << "Usage: luvc [options] <source_file>" << std::endl;
    std::cout << "\033[1;36m" << "Options:" << "\033[0m" << std::endl;
    std::cout << "  -o <file>      Specify output object file (default: output.o)" << std::endl;
    std::cout << "  --lex          Print lexed tokens" << std::endl;
    std::cout << "  --parse        Print parse tree" << std::endl;
    std::cout << "  --ir           Print generated LLVM IR" << std::endl;
    std::cout << "  --asm          Emit assembly instead of object file" << std::endl;
    std::cout << "  --dep-graph    Print dependency graph" << std::endl;
    std::cout << "  --rss-dump     Print RSS SMIR dump" << std::endl;
    std::cout << "  --rss-json     Print RSS SMIR JSON snapshot" << std::endl;
    std::cout << "  --help         Show this help menu" << std::endl;
}

// ─────────────────────────────────────────────────────────
//  Block comment preprocessor
//
//  Strips /* ... */ comments (with full nesting support).
//  Preserves line/column numbers by replacing comment chars
//  with whitespace so ANTLR's error locations stay accurate.
//  Warns (does NOT fatal) for any unclosed block comment.
// ─────────────────────────────────────────────────────────
std::string preprocessSource(const std::string& src, const std::string& filePath) {
    std::string out;
    out.reserve(src.size());

    size_t i = 0;
    size_t len = src.size();
    int depth = 0;
    int openLine = 0;    // line where outermost /* was opened
    int openCol = 0;     // column where outermost /* was opened
    int line = 1;
    int col = 1;

    auto advance = [&](size_t steps = 1) {
        for (size_t s = 0; s < steps; ++s) {
            if (i < len) {
                if (src[i] == '\n') { ++line; col = 1; }
                else { ++col; }
                ++i;
            }
        }
    };

    while (i < len) {
        // ── Inside a block comment ──
        if (depth > 0) {
            // Nested open
            if (i + 1 < len && src[i] == '/' && src[i+1] == '*') {
                ++depth;
                out += "  ";
                advance(2);
                continue;
            }
            // Close
            if (i + 1 < len && src[i] == '*' && src[i+1] == '/') {
                --depth;
                out += (src[i] == '\n') ? "\n" : " ";
                out += (src[i+1] == '\n') ? "\n" : " ";
                advance(2);
                continue;
            }
            // Blank out chars but preserve newlines
            out += (src[i] == '\n') ? '\n' : ' ';
            advance();
            continue;
        }

        // ── Check for line comment // ──
        if (i + 1 < len && src[i] == '/' && src[i+1] == '/') {
            while (i < len && src[i] != '\n') {
                out += src[i];
                advance();
            }
            continue;
        }

        // ── Check for block comment open /* ──
        if (i + 1 < len && src[i] == '/' && src[i+1] == '*') {
            openLine = line; openCol = col;
            ++depth;
            out += "  ";
            advance(2);
            continue;
        }

        // ── Skip line comments # ──
        if (src[i] == '#') {
            while (i < len && src[i] != '\n') {
                out += src[i];
                advance();
            }
            continue;
        }

        // ── String literals "" (multiline) ──
        if (src[i] == '"') {
            int strOpenLine = line;
            int strOpenCol = col;
            out += src[i]; advance(); // opening "
            while (i < len && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < len) {
                    out += src[i]; advance(); // backslash
                    out += src[i]; advance(); // escaped char
                    continue;
                }
                out += src[i]; advance();
            }
            if (i >= len) {
                std::cerr << "\033[1;31m[error]\033[0m "
                          << filePath << ":" << strOpenLine << ":" << strOpenCol
                          << ": unclosed string literal — you didn't close the string that started here\n";
                return out;
            }
            out += src[i]; advance(); // closing "
            continue;
        }

        // ── Backtick strings `` (multiline) ──
        if (src[i] == '`') {
            int strOpenLine = line;
            int strOpenCol = col;
            out += src[i]; advance(); // opening `
            while (i < len && src[i] != '`') {
                if (src[i] == '\\' && i + 1 < len) {
                    out += src[i]; advance();
                    out += src[i]; advance();
                    continue;
                }
                out += src[i]; advance();
            }
            if (i >= len) {
                std::cerr << "\033[1;31m[error]\033[0m "
                          << filePath << ":" << strOpenLine << ":" << strOpenCol
                          << ": unclosed backtick string — you didn't close the string that started here\n";
                return out;
            }
            out += src[i]; advance(); // closing `
            continue;
        }

        // ── Char literals '' (single line only) ──
        if (src[i] == '\'') {
            int strOpenLine = line;
            int strOpenCol = col;
            out += src[i]; advance(); // opening '
            while (i < len && src[i] != '\'' && src[i] != '\n') {
                if (src[i] == '\\' && i + 1 < len) {
                    out += src[i]; advance();
                    out += src[i]; advance();
                    continue;
                }
                out += src[i]; advance();
            }
            if (i >= len || src[i] == '\n') {
                std::cerr << "\033[1;31m[error]\033[0m "
                          << filePath << ":" << strOpenLine << ":" << strOpenCol
                          << ": unclosed character literal — you didn't close the char that started here\n";
                if (i < len) { out += src[i]; advance(); } // preserve the newline
                continue;
            }
            out += src[i]; advance(); // closing '
            continue;
        }

        // ── Normal code — pass through ──
        out += src[i];
        advance();
    }

    // Unclosed comment warning
    if (depth > 0) {
        std::cerr << "\033[1;33m[warning]\033[0m "
                  << filePath << ":" << openLine << ":" << openCol
                  << ": unclosed block comment '/*' — comment extends to end of file\n";
    }

    return out;
}

// ─────────────────────────────────────────────────────────
//  Parse a single .lv file into a Program AST
// ─────────────────────────────────────────────────────────
struct ParseResult {
    Program* program = nullptr;
    bool success = false;
};

ParseResult parseFile(const std::string& filePath, Arena& arena, bool printLex, bool printParse) {
    ParseResult result;

    std::ifstream stream(filePath);
    if (!stream) {
        LuvError::error(ErrorKind::FILE_NOT_FOUND,
            "Could not open file: " + filePath, filePath, 0, 0,
            "Check that the file exists and you have read permissions.");
        return result;
    }

    // Read file content and pre-process block comments
    std::string rawSource((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    std::string processedSource = preprocessSource(rawSource, filePath);

    LuvError::instance().setCurrentFile(filePath);

    ANTLRInputStream input(processedSource);
    LuvLexer lexer(&input);
    LuvErrorListener errorListener;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errorListener);

    CommonTokenStream tokens(&lexer);

    if (printLex) {
        tokens.fill();
        std::cout << "\033[1;36m── Tokens: " << filePath << " ──\033[0m" << std::endl;
        for (auto token : tokens.getTokens()) {
            std::cout << token->toString() << std::endl;
        }
    }

    LuvParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errorListener);

    LuvParser::ProgramContext* tree = parser.program();

    if (printParse) {
        std::cout << "\033[1;36m── Parse tree: " << filePath << " ──\033[0m" << std::endl;
        std::cout << tree->toStringTree(&parser) << std::endl;
    }

    LuvVisitorImpl visitor(arena);
    visitor.setCurrentFile(filePath);
    Node* programNode = std::any_cast<Node*>(visitor.visitProgram(tree));
    result.program = dynamic_cast<Program*>(programNode);
    result.success = true;

    return result;
}

// ─────────────────────────────────────────────────────────
//  Extract exports from a parsed Program AST
// ─────────────────────────────────────────────────────────
std::vector<ExportedSymbol> extractExports(const Program& prog) {
    std::vector<ExportedSymbol> exports;
    for (auto* stmt : prog.statements) {
        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) {
            Visibility vis = Visibility::PUBLIC; // default
            if (func->visibility == ASTVisibility::PRIVATE) {
                vis = Visibility::PRIVATE;
            }
            exports.push_back({func->name, vis, ExportedSymbol::FUNCTION});
        } else if (auto* var = dynamic_cast<VarDecl*>(stmt)) {
            // Top-level variables are public by default
            if (auto* ipat = dynamic_cast<IdentifierPattern*>(var->pattern)) {
                exports.push_back({ipat->name, Visibility::PUBLIC, ExportedSymbol::VARIABLE});
            }
        }
    }
    return exports;
}

// ─────────────────────────────────────────────────────────
//  Extract import requests from a parsed Program AST
// ─────────────────────────────────────────────────────────
std::vector<ImportRequest> extractImports(const Program& prog, const std::string& file) {
    std::vector<ImportRequest> imports;
    for (auto* use : prog.useStatements) {
        ImportRequest req;
        req.sourceFile = file;
        req.modulePath = use->modulePath;

        switch (use->targetKind) {
            case UseStmt::SINGLE:      req.targetKind = ImportRequest::SINGLE; break;
            case UseStmt::SET:         req.targetKind = ImportRequest::SET; break;
            case UseStmt::ALL_PUBLIC:  req.targetKind = ImportRequest::ALL_PUBLIC; break;
            case UseStmt::ALL_PRIVATE: req.targetKind = ImportRequest::ALL_PRIVATE; break;
            case UseStmt::PATH:        req.targetKind = ImportRequest::PATH; break;
        }
        req.names = use->names;

        imports.push_back(std::move(req));
    }
    return imports;
}

// ─────────────────────────────────────────────────────────
//  Recursively discover and parse all dependent modules
// ─────────────────────────────────────────────────────────
void discoverModules(ModuleResolver& resolver, Arena& arena, const std::string& moduleName,
                     const std::string& filePath, bool printLex, bool printParse,
                     std::set<std::string>& discovered) {
    if (discovered.count(moduleName)) return;
    discovered.insert(moduleName);

    auto result = parseFile(filePath, arena, printLex, printParse);
    if (!result.success) return;

    // If the program has a module declaration, use that name
    std::string effectiveName = result.program->moduleName.empty()
        ? moduleName
        : result.program->moduleName;

    // Extract exports and imports
    auto exports = extractExports(*result.program);
    auto imports = extractImports(*result.program, filePath);

    // Register module
    resolver.registerModule(effectiveName, filePath, result.program);
    resolver.addExports(effectiveName, exports);
    resolver.addImports(effectiveName, imports);

    // Recursively discover dependencies
    for (const auto& imp : imports) {
        std::string depFilePath = resolver.resolveModulePath(imp.modulePath);
        if (depFilePath.empty()) {
            LuvError::error(ErrorKind::MODULE_FILE_NOT_FOUND,
                "Cannot find module file for: " +
                    [&]() { std::string s; for (size_t i = 0; i < imp.modulePath.size(); ++i) {
                        if (i) s += "::"; s += imp.modulePath[i]; } return s; }(),
                imp.sourceFile, imp.line, imp.column,
                "Make sure the module file exists in the project directory or search path.");
            continue;
        }
        std::string depName = resolver.moduleNameFromFile(depFilePath);
        discoverModules(resolver, arena, depName, depFilePath, printLex, printParse, discovered);
    }
}

// ─────────────────────────────────────────────────────────
//  Print dependency graph
// ─────────────────────────────────────────────────────────
void printDepGraph(const ModuleResolver& resolver) {
    std::cout << "\033[1;36m── Dependency Graph ──\033[0m" << std::endl;
    for (const auto& [name, mod] : resolver.allModules()) {
        std::cout << "  \033[1;37m" << name << "\033[0m (" << mod.filePath << ")" << std::endl;
        if (mod.dependencies.empty()) {
            std::cout << "    └── (no dependencies)" << std::endl;
        } else {
            size_t i = 0;
            for (const auto& dep : mod.dependencies) {
                bool last = (++i == mod.dependencies.size());
                std::cout << "    " << (last ? "└── " : "├── ")
                          << "\033[1;33m" << dep << "\033[0m" << std::endl;
            }
        }
        // Show exports
        if (!mod.exports.empty()) {
            std::cout << "    \033[1;32mexports:\033[0m";
            for (const auto& exp : mod.exports) {
                std::cout << " " << exp.name;
                if (exp.visibility == Visibility::PRIVATE) std::cout << "(priv)";
            }
            std::cout << std::endl;
        }
    }
}

// ─────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────
int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string sourceFile;
    std::string outputFile = "output.o";
    bool printLex = false;
    bool printParse = false;
    bool printIR = false;
    bool printAsm = false;
    bool printDeps = false;
    bool rssDump = false;
    bool rssJson = false;

    std::string targetTripleStr = llvm::sys::getDefaultTargetTriple();
    std::vector<std::string> defines;
    std::set<std::string> excludes;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { printHelp(); return 0; }
        else if (arg == "--lex") { printLex = true; }
        else if (arg == "--parse") { printParse = true; }
        else if (arg == "--ir") { printIR = true; }
        else if (arg == "--asm") { printAsm = true; }
        else if (arg == "--dep-graph") { printDeps = true; }
        else if (arg == "--rss-dump") { rssDump = true; }
        else if (arg == "--rss-json") { rssJson = true; }
        else if (arg == "--target" && i + 1 < argc) { targetTripleStr = argv[++i]; }
        else if (arg == "--define" && i + 1 < argc) { 
            std::string d = argv[++i];
            size_t start = 0, end;
            while ((end = d.find_first_of(" ,", start)) != std::string::npos) {
                if (end > start) defines.push_back(d.substr(start, end - start));
                start = end + 1;
            }
            if (start < d.size()) defines.push_back(d.substr(start));
        }
        else if (arg == "--exclude" && i + 1 < argc) {
            std::string e = argv[++i];
            size_t start = 0, end;
            while ((end = e.find_first_of(" ,", start)) != std::string::npos) {
                if (end > start) excludes.insert(e.substr(start, end - start));
                start = end + 1;
            }
            if (start < e.size()) excludes.insert(e.substr(start));
        }
        else if (arg == "-o" && i + 1 < argc) { outputFile = argv[++i]; }
        else if (arg[0] != '-') { sourceFile = arg; }
    }

    if (sourceFile.empty()) {
        LuvError::fatal(ErrorKind::FILE_NOT_FOUND,
            "No source file provided.", "", 0, 0,
            "Usage: luvc [options] <source_file>");
    }

    if (!fs::exists(sourceFile)) {
        LuvError::fatal(ErrorKind::FILE_NOT_FOUND,
            "File not found: " + sourceFile, sourceFile, 0, 0);
    }

    // ── Set up module resolver ──
    std::string projectRoot = fs::canonical(fs::absolute(fs::path(sourceFile)).parent_path()).string();
    ModuleResolver resolver(projectRoot);
    Arena arena;

    // ── Check for _imp_.lv ──
    std::string impFile = resolver.findImpFile();
    std::set<std::string> discovered;

    if (!impFile.empty()) {
        std::string impModName = resolver.moduleNameFromFile(impFile);
        discoverModules(resolver, arena, impModName, impFile, printLex, printParse, discovered);
    }

    // ── Parse entry file and discover all dependencies ──
    std::string entryModName = resolver.moduleNameFromFile(fs::canonical(fs::absolute(sourceFile)).string());
    discoverModules(resolver, arena, entryModName,
                    fs::canonical(fs::absolute(sourceFile)).string(),
                    printLex, printParse, discovered);

    LuvError::instance().checkAndBail("parsing");

    // ── Build dependency graph ──
    resolver.buildDependencyGraph();

    if (printDeps) {
        printDepGraph(resolver);
    }

    // ── Detect circular dependencies ──
    if (resolver.detectCircularDependencies()) {
        return 1; // fatal already printed
    }

    // ── Validate imports ──
    resolver.validateImports();
    LuvError::instance().checkAndBail("import resolution");

    // ── Get compilation order ──
    auto compilationOrder = resolver.getCompilationOrder();

    // ── Only compile modules that are actually used ──
    auto usedModules = resolver.getUsedModules(entryModName);
    std::set<std::string> usedSet(usedModules.begin(), usedModules.end());

    // Always include _imp_ if it exists
    if (!impFile.empty()) {
        std::string impModName = resolver.moduleNameFromFile(impFile);
        usedSet.insert(impModName);
    }

    // ── Semantic analysis for each module ──
    for (const auto& modName : compilationOrder) {
        if (!usedSet.count(modName)) continue;
        auto* modInfo = resolver.getModule(modName);
        if (!modInfo || !modInfo->program) continue;

        SemanticAnalyzer sema(modInfo->filePath, &resolver, modName);
        for (const auto& d : defines) sema.addDefine(d);
        for (const auto& e : excludes) sema.addExclude(e);
        sema.analyze(*modInfo->program);
    }
    LuvError::instance().checkAndBail("semantic analysis");

    // ── RSS analysis pipeline (always on) ──
    luv::rss::RSSPipeline rssPipeline;
    luv::rss::PipelineConfig rssConfig;
    rssConfig.seed = 0x525353ull;
    rssConfig.deterministic = true;

    luv::rss::ProbabilisticProfile rssProfile;
    luv::rss::HardwareProfile rssHardware;

    std::string aggregatedSmirDump;
    std::string aggregatedSmirJson;
    bool anySmirVerifyFailed = false;

    // ── RSS analysis pipeline (always on) ──
    for (const auto& modName : compilationOrder) {
        if (!usedSet.count(modName)) continue;
        auto* modInfo = resolver.getModule(modName);
        if (!modInfo || !modInfo->program) continue;

        const auto rssResult = rssPipeline.run(*modInfo->program, modName, {}, rssProfile, rssHardware, rssConfig);
        if (!rssResult.smirVerifier.ok) anySmirVerifyFailed = true;

        if (rssDump) {
            aggregatedSmirDump += "\n\033[1;36m── RSS SMIR: " + modName + " ──\033[0m\n";
            aggregatedSmirDump += rssResult.smirDebugDump;
        }
        if (rssJson) {
            aggregatedSmirJson += "\n\033[1;36m── RSS SMIR JSON: " + modName + " ──\033[0m\n";
            aggregatedSmirJson += rssResult.smirSnapshot;
        }
    }

    if (rssDump) {
        std::cout << aggregatedSmirDump << std::endl;
    }
    if (rssJson) {
        std::cout << aggregatedSmirJson << std::endl;
    }

    if (anySmirVerifyFailed) {
        LuvError::fatal(ErrorKind::CODEGEN_ERROR, "RSS SMIR verification failed.");
    }

    // ── CodeGen for each module in order ──
    llvm::LLVMContext sharedContext;
    std::vector<std::unique_ptr<llvm::Module>> llvmModules;

    // Initialize LLVM targets once
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    llvm::Triple TargetTriple(targetTripleStr);
    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
        LuvError::fatal(ErrorKind::LLVM_ERROR, "LLVM target lookup failed: " + Error);
    }

    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Reloc::PIC_;
    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    for (const auto& modName : compilationOrder) {
        if (!usedSet.count(modName)) continue;

        auto* modInfo = resolver.getModule(modName);
        if (!modInfo || !modInfo->program) continue;

        CodeGen codegen(sharedContext, modName);
        for (const auto& d : defines) codegen.addDefine(d);
        for (const auto& e : excludes) codegen.addExclude(e);
        codegen.getModule().setTargetTriple(TargetTriple);
        codegen.getModule().setDataLayout(TargetMachine->createDataLayout());
        codegen.getModule().setModuleIdentifier(modName);

        // Register printf for all modules
        std::vector<llvm::Type*> printfArgs;
        printfArgs.push_back(llvm::PointerType::get(codegen.getContext(), 0));
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(codegen.getContext()), printfArgs, true);
        llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", &codegen.getModule());

        // Register external functions from imported modules
        for (const auto& imp : modInfo->imports) {
            auto importedSymbols = resolver.getImportedSymbols(modName, imp);
            for (const auto& sym : importedSymbols) {
                codegen.addImportedSymbol(sym.name);
                if (sym.kind == ExportedSymbol::FUNCTION) {
                    // Find the actual function in the source module to get its type
                    std::string srcModName;
                    if (imp.targetKind == ImportRequest::PATH) {
                        srcModName = imp.modulePath.size() > 1 ? imp.modulePath[0] : imp.modulePath[0];
                    } else {
                        srcModName = imp.modulePath[0];
                        for (size_t i = 1; i < imp.modulePath.size(); ++i) {
                            srcModName += "::" + imp.modulePath[i];
                        }
                    }
                    // We'll declare it as an external with a generic signature
                    // The linker will resolve the actual types
                    if (!codegen.getModule().getFunction(sym.name)) {
                        // Default to a void() function — actual type resolved at link time
                        auto* voidFT = llvm::FunctionType::get(
                            llvm::PointerType::get(codegen.getContext(), 0), {}, false);
                        llvm::Function::Create(voidFT, llvm::Function::ExternalLinkage,
                            sym.name, &codegen.getModule());
                    }
                } else if (sym.kind == ExportedSymbol::VARIABLE) {
                    if (!codegen.getModule().getGlobalVariable(sym.name)) {
                        new llvm::GlobalVariable(
                            codegen.getModule(),
                            llvm::Type::getInt8Ty(codegen.getContext()), // Opaque pointers handle any type
                            false,
                            llvm::GlobalValue::ExternalLinkage,
                            nullptr,
                            sym.name
                        );
                    }
                }
            }
        }

        modInfo->program->accept(codegen);
        modInfo->codegenDone = true;

        if (printIR) {
            std::cout << "\033[1;36m── IR: " << modName << " ──\033[0m" << std::endl;
            codegen.getModule().print(llvm::outs(), nullptr);
        }

        // Transfer module ownership
        llvmModules.push_back(codegen.takeModule());
    }

    LuvError::instance().checkAndBail("code generation");

    // ── Link all modules together ──
    if (llvmModules.empty()) {
        LuvError::fatal(ErrorKind::CODEGEN_ERROR, "No modules to compile.");
    }

    auto& mainModule = llvmModules[0];
    for (size_t i = 1; i < llvmModules.size(); ++i) {
        if (llvm::Linker::linkModules(*mainModule, std::move(llvmModules[i]))) {
            LuvError::fatal(ErrorKind::LLVM_ERROR,
                "Failed to link module #" + std::to_string(i));
        }
    }

    // ── Emit object file ──
    std::error_code EC;
    llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);
    if (EC) {
        LuvError::fatal(ErrorKind::FILE_READ_ERROR,
            "Could not open output file: " + EC.message());
    }

    llvm::legacy::PassManager pass;
    auto FileType = printAsm ? llvm::CodeGenFileType::AssemblyFile : llvm::CodeGenFileType::ObjectFile;
    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        LuvError::fatal(ErrorKind::LLVM_ERROR,
            "Target machine can't emit a file of this type");
    }

    pass.run(*mainModule);
    dest.flush();

    std::cout << "\033[1;32mSuccessfully compiled " << sourceFile << " to " << outputFile << "\033[0m" << std::endl;

    LuvError::instance().printSummary();
    return 0;
}

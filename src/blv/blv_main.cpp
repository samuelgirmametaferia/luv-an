#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "blv/BLVLexer.h"
#include "blv/BLVParser.h"
#include "blv/BLVEngine.h"
#include "cli/DefaultBLV.h"

void printBLVHelp() {
    std::cout << "\033[1;35mBLV — Luv Build Engine\033[0m v1.1\n"
              << "Usage: blv [options] [script.blv] [-- args...]\n\n"
              << "\033[1;36mOptions:\033[0m\n"
              << "  --default    Use the built-in default Luv build orchestration\n"
              << "  --help       Show this help\n"
              << "  --lex        Print tokens\n"
              << "  --parse      Print AST (debug)\n\n"
              << "\033[1;36mExample:\033[0m\n"
              << "  blv --default -- --src=main.lv --app=my_app\n"
              << "  blv build.blv -- --base=0x1000 -g\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) { printBLVHelp(); return 0; }

    std::string scriptFile;
    bool useDefault = false;
    bool showLex = false, showParse = false;
    int scriptArgStart = argc; // Where -- starts

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help") { printBLVHelp(); return 0; }
        if (a == "--default") { useDefault = true; continue; }
        if (a == "--lex") { showLex = true; continue; }
        if (a == "--parse") { showParse = true; continue; }
        if (a == "--") { scriptArgStart = i + 1; break; }
        if (a[0] != '-' && scriptFile.empty()) { scriptFile = a; continue; }
    }

    std::string source;
    std::string scriptPath;

    if (useDefault) {
        source = DEFAULT_BLV_CONTENT;
        scriptPath = "internal://default.blv";
    } else {
        if (scriptFile.empty()) {
            std::cerr << "\033[1;31m[BLV]\033[0m No script file provided and --default not specified.\n";
            return 1;
        }
        if (!std::filesystem::exists(scriptFile)) {
            std::cerr << "\033[1;31m[BLV]\033[0m File not found: " << scriptFile << "\n";
            return 1;
        }
        scriptPath = scriptFile;
        std::ifstream f(scriptFile);
        std::stringstream buf;
        buf << f.rdbuf();
        source = buf.str();
    }

    // Lex
    blv::BLVLexer lexer(source, scriptPath);
    auto tokens = lexer.tokenize();

    if (showLex) {
        std::cout << "\033[1;36m── BLV Tokens ──\033[0m\n";
        for (auto& t : tokens) {
            if (t.kind == blv::TokenKind::NEWLINE || t.kind == blv::TokenKind::EOF_TOK) continue;
            std::cout << "  " << t.line << ":" << t.col << "  " << t.text << "\n";
        }
    }

    // Parse
    blv::BLVParser parser(std::move(tokens), scriptPath);
    auto program = parser.parse();

    if (!program) {
        std::cerr << "\033[1;31m[BLV]\033[0m Parsing failed. Aborting.\n";
        return 1;
    }

    if (showParse) {
        std::cout << "\033[1;36m── BLV AST ──\033[0m\n";
        std::cout << "  " << program->statements.size() << " top-level statements\n";
    }

    // Run
    std::string projectRoot;
    if (useDefault) {
        projectRoot = std::filesystem::current_path().string();
    } else {
        projectRoot = std::filesystem::canonical(std::filesystem::absolute(std::filesystem::path(scriptFile)).parent_path()).string();
    }
    
    // Build argv for the script (everything after --)
    int scriptArgc = argc - scriptArgStart;
    const char** scriptArgv = argv + scriptArgStart;

    // Use /proc/self/exe for reliable self-location
    std::string execDir;
    if (std::filesystem::exists("/proc/self/exe")) {
        execDir = std::filesystem::canonical("/proc/self/exe").parent_path().string();
    } else {
        execDir = std::filesystem::canonical(std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path()).string();
    }

    blv::BLVEngine engine(projectRoot, scriptArgc, scriptArgv, execDir);
    int result = engine.run(*program);

    if (result == 0) {
        std::cout << "\033[1;32m[BLV]\033[0m Orchestration successful.\n";
    } else {
        std::cerr << "\033[1;31m[BLV]\033[0m Orchestration failed.\n";
    }
    return result;
}

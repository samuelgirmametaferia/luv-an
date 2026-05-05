#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

#include "cli/DefaultBLV.h"

namespace fs = std::filesystem;

void printBanner() {
    std::cout << "\033[1;35m"
              << " █    █  █ █   █\n"
              << " █    █  █  █ █ \n"
              << " █▄▄▄ ▀▄▄▀   █  \n"
              << "\033[0m\033[1;36m LUV PACKAGE MANAGER & ORCHESTRATOR \033[0m\n"
              << "\033[1;30m ──────────────────────────────────\033[0m\n\n";
}

void printHelp() {
    printBanner();
    std::cout << "Usage: luv <command> [args]\n\n"
              << "\033[1;36mCore Commands:\033[0m\n"
              << "  \033[1;32mnew\033[0m <type> <name>   Initialize a new project\n"
              << "  \033[1;32mbuild\033[0m <file.lv>     Build the specified file (or current project)\n"
              << "  \033[1;32mrun\033[0m <file.lv>       Compile, link, and execute instantly\n"
              << "  \033[1;32mclean\033[0m               Nuke the build artifacts\n"
              << "  \033[1;32minit\033[0m                Initialize BLV in current directory\n\n"
              << "\033[1;36mDiagnostic Flags (passed to luvc):\033[0m\n"
              << "  --lex               Dump lexed token stream\n"
              << "  --parse             Dump AST parse tree\n"
              << "  --ir                Dump LLVM IR\n"
              << "  --asm               Dump assembly code\n"
              << "  --dep-graph         Dump dependency graph\n\n";
}

void createProject(const std::string& type, const std::string& name) {
    if (fs::exists(name) && name != ".") {
        std::cerr << "\033[1;31m[!] Error:\033[0m Directory '" << name << "' already exists.\n";
        return;
    }
    if (name != ".") fs::create_directory(name);
    fs::create_directories(name + "/src");
    fs::create_directories(name + "/build");

    // src/main.lv
    std::string mainPath = name + "/src/main.lv";
    if (!fs::exists(mainPath)) {
        std::ofstream mainFile(mainPath);
        mainFile << "fn main(args: [string]) {\n"
                 << "    println(\"Hello from Luv!\")\n"
                 << "    println(\"Received \", args.len, \" arguments.\")\n"
                 << "}\n";
        mainFile.close();
    }

    // build.blv
    std::string blvPath = name + "/build.blv";
    if (!fs::exists(blvPath)) {
        std::ofstream blvFile(blvPath);
        blvFile << DEFAULT_BLV_CONTENT;
        blvFile.close();
    }

    std::cout << "\033[1;32m[+] Created " << type << " project '" << name << "' successfully.\033[0m\n";
    if (name != ".") std::cout << "  \033[1;35mcd\033[0m " << name << "\n";
    std::cout << "  \033[1;35mluv\033[0m run src/main.lv\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string cmd = argv[1];
    std::string luvExecDir;
    if (fs::exists("/proc/self/exe")) {
        luvExecDir = fs::canonical("/proc/self/exe").parent_path().string();
    } else {
        luvExecDir = fs::canonical(fs::absolute(fs::path(argv[0])).parent_path()).string();
    }
    
    if (cmd == "new") {
        printBanner();
        if (argc < 4) {
            std::cerr << "Usage: luv new <template> <name>\n";
            return 1;
        }
        createProject(argv[2], argv[3]);
        return 0;
    } 
    else if (cmd == "init") {
        printBanner();
        createProject("console", ".");
        return 0;
    }
    else if (cmd == "clean") {
        printBanner();
        if (fs::exists("build")) {
            fs::remove_all("build");
            fs::create_directory("build");
            std::cout << "\033[1;32m[+] Successfully nuked build artifacts.\033[0m\n";
        } else {
            std::cout << "\033[1;33m[*] Build directory already clean.\033[0m\n";
        }
        return 0;
    }

    // Commands that involve BLV (build, run)
    if (cmd == "build" || cmd == "run") {
        printBanner();
        
        std::string sourceFile = "";
        std::string blvArgs = "";
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] != '-' && sourceFile.empty()) {
                sourceFile = arg;
            } else {
                blvArgs += " " + arg;
            }
        }

        std::string blvPath = "build.blv";
        bool isDefault = false;
        if (!fs::exists(blvPath)) {
            blvPath = "--default";
            isDefault = true;
        }

        // Auto-inject --src if not already present
        if (!sourceFile.empty()) {
            blvArgs = " --src=" + sourceFile + blvArgs;
        } else if (fs::exists("src/main.lv")) {
            blvArgs = " --src=src/main.lv" + blvArgs;
        }

        std::string blvCmd = luvExecDir + "/blv " + blvPath + " --" + blvArgs;
        std::cout << "\033[1;34m[*] Engaging BLV Engine (" << (isDefault ? "built-in" : "build.blv") << ")...\033[0m\n";
        
        int buildResult = system(blvCmd.c_str());

        if (buildResult != 0) {
            std::cerr << "\033[1;31m[!] Build failed.\033[0m\n";
            return 1;
        }

        if (cmd == "run") {
            // Find executable
            std::string appName = "luv_app";
            // Check for --app= override in blvArgs
            size_t appPos = blvArgs.find("--app=");
            if (appPos != std::string::npos) {
                size_t start = appPos + 6;
                size_t end = blvArgs.find(" ", start);
                if (end == std::string::npos) end = blvArgs.length();
                appName = blvArgs.substr(start, end - start);
            }

            std::string execPath = "./build/" + appName;
            if (!fs::exists(execPath) && fs::exists(execPath + ".exe")) execPath += ".exe";

            if (fs::exists(execPath)) {
                std::cout << "\n\033[1;32m>> Running Artifact:\033[0m " << execPath << "\n";
                std::cout << "\033[1;30m──────────────────────────────────\033[0m\n";
                int r = system(execPath.c_str());
                std::cout << "\033[1;30m──────────────────────────────────\033[0m\n";
                // system() return value is platform dependent, WEXITSTATUS is for POSIX
#ifdef _WIN32
                int exitCode = r;
#else
                int exitCode = WEXITSTATUS(r);
#endif
                std::cout << "\033[1;32m[luv] Process exited with code " << exitCode << "\033[0m\n";
                return exitCode;
            } else {
                std::cerr << "\033[1;31m[!] Error: Could not locate built binary at " << execPath << "\033[0m\n";
                return 1;
            }
        }
        return 0;
    }

    // Fallback: Proxy to luvc
    std::string luvcCmd = luvExecDir + "/luvc";
    for (int i = 1; i < argc; ++i) {
        luvcCmd += " ";
        luvcCmd += argv[i];
    }
    return system(luvcCmd.c_str());
}

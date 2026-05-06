#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <filesystem>
#include <functional>
#include "ast/AST.h"
#include "LuvError.h"

namespace luv {

// ─────────────────────────────────────────────────────────
//  Symbol visibility
// ─────────────────────────────────────────────────────────
enum class Visibility {
    PUBLIC,
    PRIVATE
};

// ─────────────────────────────────────────────────────────
//  An exported symbol from a module
// ─────────────────────────────────────────────────────────
struct ExportedSymbol {
    std::string name;
    Visibility visibility;
    enum Kind { FUNCTION, VARIABLE, MODULE } kind;
};

// ─────────────────────────────────────────────────────────
//  Describes what a single `use` statement requests
// ─────────────────────────────────────────────────────────
struct ImportRequest {
    enum TargetKind {
        SINGLE,         // use X from Y
        SET,            // use {A, B} from Y
        ALL_PUBLIC,     // use * from Y
        ALL_PRIVATE,    // use @ from Y
        PATH            // use X::Y (last element is target)
    };

    TargetKind targetKind;
    std::vector<std::string> names;       // SINGLE/SET: symbol names
    std::vector<std::string> modulePath;  // module path segments
    std::string sourceFile;               // file containing this use stmt
    int line = 0;
    int column = 0;
};

// ─────────────────────────────────────────────────────────
//  Per-file module information
// ─────────────────────────────────────────────────────────
struct ModuleInfo {
    std::string name;                             // module name
    std::string filePath;                         // absolute path to .lv file
    std::vector<ImportRequest> imports;            // use statements
    std::vector<ExportedSymbol> exports;           // symbols defined here
    std::set<std::string> dependencies;            // module names we depend on
    bool parsed = false;
    bool codegenDone = false;
    Program* program = nullptr;              // AST

    // Which symbols are actually used from this module (for tree shaking)
    std::set<std::string> usedSymbols;
};

// ─────────────────────────────────────────────────────────
//  ModuleResolver: manages multi-file compilation
// ─────────────────────────────────────────────────────────
class ModuleResolver {
public:
    // Constructor takes the project root directory
    explicit ModuleResolver(const std::string& projectRoot)
        : projectRoot_(std::filesystem::canonical(projectRoot).string()) {
        searchPaths_.push_back(projectRoot_);
    }

    // Add additional search paths for module resolution
    void addSearchPath(const std::string& path) {
        if (std::filesystem::exists(path)) {
            searchPaths_.push_back(std::filesystem::canonical(path).string());
        }
    }

    // Register a parsed module
    void registerModule(const std::string& name, const std::string& filePath,
                        Program* program) {
        if (modules_.count(name)) {
            // Allow re-registration of same file (e.g. _imp_.lv re-parsed)
            if (modules_[name].filePath != filePath) {
                LuvError::error(ErrorKind::DUPLICATE_MODULE,
                    "Module '" + name + "' is already defined in " + modules_[name].filePath,
                    filePath, 1, 0,
                    "Each module name must be unique. Use a different module name.");
                return;
            }
        }
        ModuleInfo info;
        info.name = name;
        info.filePath = filePath;
        info.parsed = true;
        info.program = program;
        modules_[name] = std::move(info);
    }

    // Register imports for a module
    void addImports(const std::string& moduleName,
                    const std::vector<ImportRequest>& imports) {
        if (!modules_.count(moduleName)) return;
        modules_[moduleName].imports = imports;
    }

    // Register exports for a module
    void addExports(const std::string& moduleName,
                    const std::vector<ExportedSymbol>& exports) {
        if (!modules_.count(moduleName)) return;
        modules_[moduleName].exports = exports;
    }

    // Resolve a module path to a file path
    // Module path like ["std", "io"] -> search for std/io.lv
    std::string resolveModulePath(const std::vector<std::string>& path) const {
        namespace fs = std::filesystem;

        // Build relative path from segments
        std::string relPath;
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) relPath += "/";
            relPath += path[i];
        }

        // Search in all search paths
        for (const auto& searchDir : searchPaths_) {
            // Try: searchDir/relPath.lv
            fs::path candidate = fs::path(searchDir) / (relPath + ".lv");
            if (fs::exists(candidate)) {
                return fs::canonical(candidate).string();
            }

            // Try: searchDir/relPath/_imp_.lv (directory module)
            candidate = fs::path(searchDir) / relPath / "_imp_.lv";
            if (fs::exists(candidate)) {
                return fs::canonical(candidate).string();
            }

            // Try: searchDir/relPath/relPath.lv (e.g. math/math.lv)
            if (path.size() == 1) {
                candidate = fs::path(searchDir) / relPath / (relPath + ".lv");
                if (fs::exists(candidate)) {
                    return fs::canonical(candidate).string();
                }
            }
        }

        return ""; // Not found
    }

    // Derive module name from a file path
    std::string moduleNameFromFile(const std::string& filePath) const {
        namespace fs = std::filesystem;
        fs::path p(filePath);
        std::string stem = p.stem().string();
        if (stem == "_imp_") {
            // Use parent directory name
            stem = p.parent_path().filename().string();
        }
        return stem;
    }

    // Build the dependency graph
    void buildDependencyGraph() {
        for (auto& [name, mod] : modules_) {
            mod.dependencies.clear();
            for (const auto& imp : mod.imports) {
                std::string depName;
                if (imp.targetKind == ImportRequest::PATH) {
                    // For path-style imports, the module is everything except last
                    if (imp.modulePath.size() > 1) {
                        depName = imp.modulePath[0];
                        for (size_t i = 1; i < imp.modulePath.size() - 1; ++i) {
                            depName += "::" + imp.modulePath[i];
                        }
                    } else {
                        depName = imp.modulePath[0];
                    }
                } else {
                    // For from-style imports, join the module path
                    depName = imp.modulePath[0];
                    for (size_t i = 1; i < imp.modulePath.size(); ++i) {
                        depName += "::" + imp.modulePath[i];
                    }
                }

                if (depName != name) { // Don't self-depend
                    mod.dependencies.insert(depName);

                    // Track which symbols are used
                    if (modules_.count(depName)) {
                        if (imp.targetKind == ImportRequest::ALL_PUBLIC ||
                            imp.targetKind == ImportRequest::ALL_PRIVATE) {
                            // Mark all symbols as used
                            for (const auto& exp : modules_[depName].exports) {
                                modules_[depName].usedSymbols.insert(exp.name);
                            }
                        } else {
                            for (const auto& symName : imp.names) {
                                modules_[depName].usedSymbols.insert(symName);
                            }
                        }
                    }
                }
            }
        }
    }

    // Detect circular dependencies — returns true if cycles found
    bool detectCircularDependencies() {
        enum State { WHITE, GRAY, BLACK };
        std::map<std::string, State> color;
        std::vector<std::string> path;
        bool hasCycle = false;

        for (const auto& [name, _] : modules_) {
            color[name] = WHITE;
        }

        std::function<void(const std::string&)> dfs = [&](const std::string& u) {
            if (hasCycle) return;
            color[u] = GRAY;
            path.push_back(u);

            if (modules_.count(u)) {
                for (const auto& dep : modules_[u].dependencies) {
                    if (!color.count(dep)) continue; // Unknown module, skip
                    if (color[dep] == GRAY) {
                        // Found cycle! Build the cycle string
                        hasCycle = true;
                        std::string cycle;
                        bool inCycle = false;
                        for (const auto& p : path) {
                            if (p == dep) inCycle = true;
                            if (inCycle) {
                                if (!cycle.empty()) cycle += " -> ";
                                cycle += p;
                            }
                        }
                        cycle += " -> " + dep;
                        LuvError::fatal(ErrorKind::CIRCULAR_DEPENDENCY,
                            "Circular dependency detected: " + cycle,
                            modules_[u].filePath, 1, 0,
                            "Break the cycle by restructuring your modules or using forward declarations.");
                        return;
                    }
                    if (color[dep] == WHITE) {
                        dfs(dep);
                    }
                }
            }

            path.pop_back();
            color[u] = BLACK;
        };

        for (const auto& [name, _] : modules_) {
            if (color[name] == WHITE) {
                dfs(name);
                if (hasCycle) return true;
            }
        }
        return false;
    }

    // Get topological compilation order (dependencies first)
    std::vector<std::string> getCompilationOrder() {
        std::vector<std::string> order;
        std::set<std::string> visited;

        std::function<void(const std::string&)> topoSort = [&](const std::string& name) {
            if (visited.count(name)) return;
            visited.insert(name);

            if (modules_.count(name)) {
                for (const auto& dep : modules_[name].dependencies) {
                    if (modules_.count(dep)) {
                        topoSort(dep);
                    }
                }
            }
            order.push_back(name);
        };

        for (const auto& [name, _] : modules_) {
            topoSort(name);
        }

        return order;
    }

    // Validate imports: check that all imported symbols actually exist
    void validateImports() {
        for (const auto& [name, mod] : modules_) {
            for (const auto& imp : mod.imports) {
                // Resolve the module name from the path
                std::string targetModName;
                if (imp.targetKind == ImportRequest::PATH) {
                    if (imp.modulePath.size() > 1) {
                        targetModName = imp.modulePath[0];
                    } else {
                        targetModName = imp.modulePath[0];
                    }
                } else {
                    targetModName = imp.modulePath[0];
                    for (size_t i = 1; i < imp.modulePath.size(); ++i) {
                        targetModName += "::" + imp.modulePath[i];
                    }
                }

                // Check module exists
                if (!modules_.count(targetModName)) {
                    // Try with just first segment
                    if (!modules_.count(imp.modulePath[0])) {
                        LuvError::error(ErrorKind::MODULE_NOT_FOUND,
                            "Module '" + targetModName + "' not found",
                            imp.sourceFile, imp.line, imp.column,
                            "Make sure the module file exists and the path is correct.");
                        continue;
                    }
                    targetModName = imp.modulePath[0];
                }

                const auto& targetMod = modules_.at(targetModName);

                // For SINGLE and SET imports, check each symbol exists
                if (imp.targetKind == ImportRequest::SINGLE ||
                    imp.targetKind == ImportRequest::SET ||
                    imp.targetKind == ImportRequest::PATH) {
                    for (const auto& symName : imp.names) {
                        bool found = false;
                        bool isPrivate = false;
                        for (const auto& exp : targetMod.exports) {
                            if (exp.name == symName) {
                                found = true;
                                isPrivate = (exp.visibility == Visibility::PRIVATE);
                                break;
                            }
                        }
                        if (!found) {
                            LuvError::error(ErrorKind::SYMBOL_NOT_FOUND,
                                "Symbol '" + symName + "' not found in module '" + targetModName + "'",
                                imp.sourceFile, imp.line, imp.column,
                                "Check available exports from '" + targetModName + "'.");
                        } else if (isPrivate && imp.targetKind != ImportRequest::ALL_PRIVATE) {
                            LuvError::error(ErrorKind::PRIVATE_ACCESS,
                                "Symbol '" + symName + "' is private in module '" + targetModName + "'",
                                imp.sourceFile, imp.line, imp.column,
                                "Use 'use @ from " + targetModName + "' to access private symbols.");
                        }
                    }
                }

                // For ALL_PUBLIC, check that private symbols are not leaked
                if (imp.targetKind == ImportRequest::ALL_PUBLIC) {
                    // Filtered at codegen time — no symbol-level check needed here
                }
            }
        }
    }

    // Check for _imp_.lv in project root
    std::string findImpFile() const {
        namespace fs = std::filesystem;
        fs::path impPath = fs::path(projectRoot_) / "_imp_.lv";
        if (fs::exists(impPath)) {
            return fs::canonical(impPath).string();
        }
        return "";
    }

    // Get list of symbols that should be imported into a module
    std::vector<ExportedSymbol> getImportedSymbols(const std::string& moduleName,
                                                    const ImportRequest& imp) const {
        std::vector<ExportedSymbol> result;

        std::string targetModName;
        if (imp.targetKind == ImportRequest::PATH) {
            targetModName = imp.modulePath.size() > 1 ? imp.modulePath[0] : imp.modulePath[0];
        } else {
            targetModName = imp.modulePath[0];
            for (size_t i = 1; i < imp.modulePath.size(); ++i) {
                targetModName += "::" + imp.modulePath[i];
            }
        }

        if (!modules_.count(targetModName)) return result;
        const auto& targetMod = modules_.at(targetModName);

        switch (imp.targetKind) {
            case ImportRequest::ALL_PUBLIC:
                for (const auto& exp : targetMod.exports) {
                    if (exp.visibility == Visibility::PUBLIC) {
                        result.push_back(exp);
                    }
                }
                break;
            case ImportRequest::ALL_PRIVATE:
                for (const auto& exp : targetMod.exports) {
                    result.push_back(exp); // Everything, even private
                }
                break;
            case ImportRequest::SINGLE:
            case ImportRequest::SET:
            case ImportRequest::PATH:
                for (const auto& symName : imp.names) {
                    for (const auto& exp : targetMod.exports) {
                        if (exp.name == symName) {
                            result.push_back(exp);
                            break;
                        }
                    }
                }
                break;
        }

        return result;
    }

    // Get only modules that are actually used (for dead code elimination)
    std::vector<std::string> getUsedModules(const std::string& entryModule) const {
        std::vector<std::string> result;
        std::set<std::string> visited;

        std::function<void(const std::string&)> collect = [&](const std::string& name) {
            if (visited.count(name)) return;
            visited.insert(name);

            if (modules_.count(name)) {
                for (const auto& dep : modules_.at(name).dependencies) {
                    collect(dep);
                }
            }
            result.push_back(name);
        };

        collect(entryModule);
        return result;
    }

    // Access module info
    ModuleInfo* getModule(const std::string& name) {
        auto it = modules_.find(name);
        return it != modules_.end() ? &it->second : nullptr;
    }

    const ModuleInfo* getModule(const std::string& name) const {
        auto it = modules_.find(name);
        return it != modules_.end() ? &it->second : nullptr;
    }

    const std::map<std::string, ModuleInfo>& allModules() const { return modules_; }

    const std::string& projectRoot() const { return projectRoot_; }

private:
    std::string projectRoot_;
    std::vector<std::string> searchPaths_;
    std::map<std::string, ModuleInfo> modules_;
};

} // namespace luv

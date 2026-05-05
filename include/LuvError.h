#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <functional>

namespace luv {

// ─────────────────────────────────────────────────────────
//  Error severity levels
// ─────────────────────────────────────────────────────────
enum class ErrorSeverity {
    NOTE,
    WARNING,
    ERROR,
    FATAL
};

// ─────────────────────────────────────────────────────────
//  Error categories
// ─────────────────────────────────────────────────────────
enum class ErrorKind {
    // Syntax
    SYNTAX_ERROR,
    UNEXPECTED_TOKEN,

    // Type system
    TYPE_ERROR,
    TYPE_MISMATCH,
    UNKNOWN_TYPE,

    // Module / import system
    MODULE_NOT_FOUND,
    MODULE_FILE_NOT_FOUND,
    CIRCULAR_DEPENDENCY,
    DUPLICATE_MODULE,
    IMPORT_ERROR,
    SYMBOL_NOT_FOUND,
    SYMBOL_NOT_EXPORTED,
    DUPLICATE_SYMBOL,
    PRIVATE_ACCESS,

    // Variable / function
    UNDEFINED_VARIABLE,
    UNDEFINED_FUNCTION,
    DUPLICATE_DEFINITION,
    CONST_REASSIGNMENT,

    // File I/O
    FILE_NOT_FOUND,
    FILE_READ_ERROR,

    // CodeGen
    CODEGEN_ERROR,
    LLVM_ERROR,

    // Internal
    INTERNAL_ERROR,

    // Optimization
    CODE_OPTIMIZATION
};

// ─────────────────────────────────────────────────────────
//  Source location tracking
// ─────────────────────────────────────────────────────────
struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;

    SourceLocation() = default;
    SourceLocation(const std::string& f, int l, int c) : file(f), line(l), column(c) {}

    bool valid() const { return !file.empty() && line > 0; }
};

// ─────────────────────────────────────────────────────────
//  A single diagnostic record
// ─────────────────────────────────────────────────────────
struct Diagnostic {
    ErrorSeverity severity;
    ErrorKind kind;
    std::string message;
    SourceLocation location;
    std::string hint;       // optional advice
    std::string context;    // optional source context line
};

// ─────────────────────────────────────────────────────────
//  LuvError: central error reporting engine
// ─────────────────────────────────────────────────────────
class LuvError {
public:
    // ── Singleton access ──
    static LuvError& instance() {
        static LuvError inst;
        return inst;
    }

    // ── Configuration ──
    void setCurrentFile(const std::string& file) { currentFile_ = file; }
    const std::string& getCurrentFile() const { return currentFile_; }

    // ── Reporting helpers ──
    static void note(const std::string& msg,
                     const std::string& file = "", int line = 0, int col = 0) {
        instance().report(ErrorSeverity::NOTE, ErrorKind::INTERNAL_ERROR, msg, file, line, col);
    }

    static void warn(ErrorKind kind, const std::string& msg,
                     const std::string& file = "", int line = 0, int col = 0) {
        instance().report(ErrorSeverity::WARNING, kind, msg, file, line, col);
    }

    static void error(ErrorKind kind, const std::string& msg,
                      const std::string& file = "", int line = 0, int col = 0,
                      const std::string& hint = "") {
        instance().report(ErrorSeverity::ERROR, kind, msg, file, line, col, hint);
    }

    static void fatal(ErrorKind kind, const std::string& msg,
                      const std::string& file = "", int line = 0, int col = 0,
                      const std::string& hint = "") {
        instance().report(ErrorSeverity::FATAL, kind, msg, file, line, col, hint);
        // FATAL always terminates
        instance().printSummary();
        std::exit(1);
    }

    // ── Low-level report ──
    void report(ErrorSeverity severity, ErrorKind kind, const std::string& msg,
                const std::string& file = "", int line = 0, int col = 0,
                const std::string& hint = "") {
        Diagnostic diag;
        diag.severity = severity;
        diag.kind = kind;
        diag.message = msg;
        diag.location = SourceLocation(file.empty() ? currentFile_ : file, line, col);
        diag.hint = hint;

        // Try to grab source context line
        if (diag.location.valid()) {
            diag.context = readSourceLine(diag.location.file, diag.location.line);
        }

        diagnostics_.push_back(diag);
        printDiagnostic(diag);

        if (severity == ErrorSeverity::ERROR) errorCount_++;
        if (severity == ErrorSeverity::WARNING) warningCount_++;
    }

    // ── Query state ──
    bool hasErrors() const { return errorCount_ > 0; }
    int errorCount() const { return errorCount_; }
    int warningCount() const { return warningCount_; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // ── Print summary ──
    void printSummary() const {
        if (errorCount_ == 0 && warningCount_ == 0) return;
        std::cerr << "\n";
        if (errorCount_ > 0)
            std::cerr << "\033[1;31m" << errorCount_ << " error"
                      << (errorCount_ > 1 ? "s" : "") << "\033[0m";
        if (errorCount_ > 0 && warningCount_ > 0)
            std::cerr << ", ";
        if (warningCount_ > 0)
            std::cerr << "\033[1;33m" << warningCount_ << " warning"
                      << (warningCount_ > 1 ? "s" : "") << "\033[0m";
        std::cerr << " generated." << std::endl;
    }

    // ── Reset for reuse ──
    void reset() {
        diagnostics_.clear();
        errorCount_ = 0;
        warningCount_ = 0;
        currentFile_.clear();
    }

    // ── Convenience: check and bail if errors ──
    void checkAndBail(const std::string& phase) {
        if (hasErrors()) {
            std::cerr << "\033[1;31mCompilation aborted during " << phase
                      << " phase.\033[0m" << std::endl;
            printSummary();
            std::exit(1);
        }
    }

    // ── Kind to human-readable string ──
    static std::string kindString(ErrorKind kind) {
        switch (kind) {
            case ErrorKind::SYNTAX_ERROR:         return "SyntaxError";
            case ErrorKind::UNEXPECTED_TOKEN:     return "UnexpectedToken";
            case ErrorKind::TYPE_ERROR:           return "TypeError";
            case ErrorKind::TYPE_MISMATCH:        return "TypeMismatch";
            case ErrorKind::UNKNOWN_TYPE:         return "UnknownType";
            case ErrorKind::MODULE_NOT_FOUND:     return "ModuleNotFound";
            case ErrorKind::MODULE_FILE_NOT_FOUND:return "ModuleFileNotFound";
            case ErrorKind::CIRCULAR_DEPENDENCY:  return "CircularDependency";
            case ErrorKind::DUPLICATE_MODULE:     return "DuplicateModule";
            case ErrorKind::IMPORT_ERROR:         return "ImportError";
            case ErrorKind::SYMBOL_NOT_FOUND:     return "SymbolNotFound";
            case ErrorKind::SYMBOL_NOT_EXPORTED:  return "SymbolNotExported";
            case ErrorKind::DUPLICATE_SYMBOL:     return "DuplicateSymbol";
            case ErrorKind::PRIVATE_ACCESS:       return "PrivateAccess";
            case ErrorKind::UNDEFINED_VARIABLE:   return "UndefinedVariable";
            case ErrorKind::UNDEFINED_FUNCTION:   return "UndefinedFunction";
            case ErrorKind::DUPLICATE_DEFINITION: return "DuplicateDefinition";
            case ErrorKind::CONST_REASSIGNMENT:   return "ConstReassignment";
            case ErrorKind::FILE_NOT_FOUND:       return "FileNotFound";
            case ErrorKind::FILE_READ_ERROR:      return "FileReadError";
            case ErrorKind::CODEGEN_ERROR:        return "CodeGenError";
            case ErrorKind::LLVM_ERROR:           return "LLVMError";
            case ErrorKind::INTERNAL_ERROR:       return "InternalError";
            case ErrorKind::CODE_OPTIMIZATION:    return "Optimization";
        }
        return "UnknownError";
    }

private:
    LuvError() = default;

    std::vector<Diagnostic> diagnostics_;
    int errorCount_ = 0;
    int warningCount_ = 0;
    std::string currentFile_;

    // ── Read a single line from a source file ──
    static std::string readSourceLine(const std::string& file, int line) {
        std::ifstream in(file);
        if (!in.is_open()) return "";
        std::string text;
        for (int i = 0; i < line && std::getline(in, text); ++i) {}
        return text;
    }

    // ── Pretty-print a diagnostic ──
    void printDiagnostic(const Diagnostic& diag) const {
        // Severity color
        const char* color = "\033[0m";
        const char* label = "note";
        switch (diag.severity) {
            case ErrorSeverity::NOTE:    color = "\033[1;36m"; label = "note";    break;
            case ErrorSeverity::WARNING: color = "\033[1;33m"; label = "warning"; break;
            case ErrorSeverity::ERROR:   color = "\033[1;31m"; label = "error";   break;
            case ErrorSeverity::FATAL:   color = "\033[1;31m"; label = "fatal";   break;
        }

        // Location
        std::cerr << "\033[1;37m";
        if (diag.location.valid()) {
            std::cerr << diag.location.file << ":" << diag.location.line
                      << ":" << diag.location.column << ": ";
        }

        // Severity + kind
        std::cerr << color << label << "[" << kindString(diag.kind) << "]: "
                  << "\033[0m\033[1;37m" << diag.message << "\033[0m" << std::endl;

        // Source context
        if (!diag.context.empty()) {
            std::cerr << "  \033[0;37m" << diag.location.line << " | \033[0m"
                      << diag.context << std::endl;
            if (diag.location.column > 0) {
                std::string lineNum = std::to_string(diag.location.line);
                std::string padding(lineNum.size() + 3 + (diag.location.column - 1), ' ');
                std::cerr << padding << color << "^" << "\033[0m" << std::endl;
            }
        }

        // Hint
        if (!diag.hint.empty()) {
            std::cerr << "  \033[1;34mhint: " << diag.hint << "\033[0m" << std::endl;
        }
    }
};

// ─────────────────────────────────────────────────────────
//  LuvException: throwable error for control flow
// ─────────────────────────────────────────────────────────
class LuvException : public std::runtime_error {
public:
    ErrorKind kind;
    SourceLocation location;

    LuvException(ErrorKind k, const std::string& msg,
                 const std::string& file = "", int line = 0, int col = 0)
        : std::runtime_error(msg), kind(k), location(file, line, col) {}
};

} // namespace luv

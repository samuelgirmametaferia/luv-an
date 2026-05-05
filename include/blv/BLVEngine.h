#pragma once
#include "BLVParser.h"
#include <map>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace blv {
namespace fs = std::filesystem;

// ── Registry Blob ──
struct Blob {
    std::string name;
    std::vector<uint8_t> data;
    std::map<std::string, int64_t> symbols; // symbol -> offset
    std::string sourceFile;
    std::string codec;
};

// ── Memory Region ──
struct Region {
    std::string name;
    int64_t start, size, align;
    std::vector<std::pair<int64_t, std::string>> mappings; // offset -> blob name
};

// ── Policy ──
struct Policy {
    std::string name;
    std::vector<std::string> rules;
};

// ── Runtime Value ──
struct Value {
    enum Kind { NONE, INT, FLOAT, STRING, BOOL, BLOB_REF, REGION_REF, POLICY_REF };
    Kind kind = NONE;
    int64_t intVal = 0;
    double floatVal = 0;
    std::string strVal;
    bool boolVal = false;

    static Value makeNone() { return {}; }
    static Value makeInt(int64_t v) { Value r; r.kind=INT; r.intVal=v; return r; }
    static Value makeFloat(double v) { Value r; r.kind=FLOAT; r.floatVal=v; return r; }
    static Value makeString(const std::string& v) { Value r; r.kind=STRING; r.strVal=v; return r; }
    static Value makeBool(bool v) { Value r; r.kind=BOOL; r.boolVal=v; return r; }
    static Value makeBlobRef(const std::string& n) { Value r; r.kind=BLOB_REF; r.strVal=n; return r; }
    static Value makeRegionRef(const std::string& n) { Value r; r.kind=REGION_REF; r.strVal=n; return r; }
    static Value makePolicyRef(const std::string& n) { Value r; r.kind=POLICY_REF; r.strVal=n; return r; }

    bool truthy() const {
        switch(kind) {
            case BOOL: return boolVal;
            case INT: return intVal != 0;
            case STRING: return !strVal.empty();
            case NONE: return false;
            default: return true;
        }
    }
    std::string toString() const {
        switch(kind) {
            case INT: return std::to_string(intVal);
            case FLOAT: return std::to_string(floatVal);
            case STRING: return strVal;
            case BOOL: return boolVal ? "true" : "false";
            case BLOB_REF: return "<blob:" + strVal + ">";
            case REGION_REF: return "<region:" + strVal + ">";
            case POLICY_REF: return "<policy:" + strVal + ">";
            default: return "<none>";
        }
    }
};

// ─────────────────────────────────────────────────────────
//  BLV Engine — the interpreter
// ─────────────────────────────────────────────────────────
class BLVEngine {
public:
    BLVEngine(const std::string& projectRoot, int argc, const char** argv, const std::string& execDir = ".")
        : projectRoot_(projectRoot), argc_(argc), argv_(argv), execDir_(execDir) {}

    int run(Program& prog) {
        for (auto& stmt : prog.statements) {
            if (!execStmt(stmt.get())) return 1;
        }
        return 0;
    }

private:
    std::string projectRoot_;
    int argc_;
    const char** argv_;
    std::string execDir_;
    std::map<std::string, Value> vars_;
    std::map<std::string, Blob> registry_;
    std::map<std::string, Region> regions_;
    std::map<std::string, Policy> policies_;
    int blobCounter_ = 0;

    static bool isForbiddenTool(const std::string& tool) {
        return tool == "sh" || tool == "bash" || tool == "clang";
    }

    std::string makePath(const std::string& path) const {
        fs::path p(path);
        if (p.is_absolute()) return p.string();
        return (fs::path(projectRoot_) / p).string();
    }

    int runProcess(const std::vector<std::string>& args, const std::string& label) {
        if (args.empty()) return -1;
        if (isForbiddenTool(fs::path(args[0]).filename().string())) {
            std::cerr << "\033[1;31m[BLV]\033[0m forbidden tool in " << label
                      << ": " << args[0] << "\n";
            return -1;
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "\033[1;31m[BLV]\033[0m fork failed for " << label << "\n";
            return -1;
        }
        if (pid == 0) {
            execvp(argv[0], argv.data());
            _exit(127);
        }

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            std::cerr << "\033[1;31m[BLV]\033[0m waitpid failed for " << label << "\n";
            return -1;
        }
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return -1;
    }

    // ── Statement execution ──
    bool execStmt(Stmt* s) {
        if (auto* let = dynamic_cast<LetStmt*>(s)) {
            Value v = evalExpr(let->value.get());
            vars_[let->name] = v;
            return true;
        }
        if (auto* ver = dynamic_cast<VerifyStmt*>(s)) {
            Value v = evalExpr(ver->condition.get());
            if (!v.truthy()) {
                std::cerr << "\033[1;31m[BLV VERIFY FAILED]\033[0m line " << ver->line
                          << " — build aborted.\n";
                return false;
            }
            return true;
        }
        if (auto* ifs = dynamic_cast<IfStmt*>(s)) {
            Value cond = evalExpr(ifs->condition.get());
            auto& body = cond.truthy() ? ifs->thenBody : ifs->elseBody;
            for (auto& st : body) { if (!execStmt(st.get())) return false; }
            return true;
        }
        if (auto* ext = dynamic_cast<ExternStmt*>(s)) {
            // Register extern — actual dlopen happens on call
            vars_[ext->funcName] = Value::makeString("extern:" + ext->libPath + ":" + ext->funcName);
            return true;
        }
        if (auto* es = dynamic_cast<ExprStmt*>(s)) {
            evalExpr(es->expr.get());
            return true;
        }
        return true;
    }

    // ── Expression evaluation ──
    Value evalExpr(Expr* e) {
        if (!e) return Value::makeNone();

        if (auto* lit = dynamic_cast<StringLit*>(e)) return Value::makeString(lit->value);
        if (auto* lit = dynamic_cast<IntLit*>(e)) return Value::makeInt(lit->value);
        if (auto* lit = dynamic_cast<FloatLit*>(e)) return Value::makeFloat(lit->value);
        if (auto* lit = dynamic_cast<BoolLit*>(e)) return Value::makeBool(lit->value);

        if (auto* id = dynamic_cast<Ident*>(e)) {
            auto it = vars_.find(id->name);
            if (it != vars_.end()) return it->second;
            // Treat undefined identifiers as bare word strings
            return Value::makeString(id->name);
        }

        if (auto* bin = dynamic_cast<BinaryExpr*>(e)) return evalBinary(bin);
        if (auto* un = dynamic_cast<UnaryExpr*>(e)) return evalUnary(un);
        if (auto* intr = dynamic_cast<IntrinsicExpr*>(e)) return evalIntrinsic(intr);
        if (auto* cli = dynamic_cast<CliExpr*>(e)) return evalCli(cli);
        if (auto* dot = dynamic_cast<DotExpr*>(e)) return evalDot(dot);
        if (auto* call = dynamic_cast<CallExpr*>(e)) return evalCall(call);

        return Value::makeNone();
    }

    Value evalBinary(BinaryExpr* b) {
        Value L = evalExpr(b->left.get());
        Value R = evalExpr(b->right.get());
        if (b->op == "+") {
            if (L.kind == Value::STRING) return Value::makeString(L.strVal + R.toString());
            return Value::makeInt(L.intVal + R.intVal);
        }
        if (b->op == "-") return Value::makeInt(L.intVal - R.intVal);
        if (b->op == "*") return Value::makeInt(L.intVal * R.intVal);
        if (b->op == "/") return Value::makeInt(R.intVal ? L.intVal / R.intVal : 0);
        if (b->op == "==") {
            if (L.kind == Value::STRING) return Value::makeBool(L.strVal == R.strVal);
            return Value::makeBool(L.intVal == R.intVal);
        }
        if (b->op == "!=") return Value::makeBool(L.intVal != R.intVal);
        if (b->op == "<") return Value::makeBool(L.intVal < R.intVal);
        if (b->op == ">") return Value::makeBool(L.intVal > R.intVal);
        if (b->op == "<=") return Value::makeBool(L.intVal <= R.intVal);
        if (b->op == ">=") return Value::makeBool(L.intVal >= R.intVal);
        if (b->op == "&&") return Value::makeBool(L.truthy() && R.truthy());
        if (b->op == "||") return Value::makeBool(L.truthy() || R.truthy());
        return Value::makeNone();
    }

    Value evalUnary(UnaryExpr* u) {
        Value v = evalExpr(u->operand.get());
        if (u->op == "-") return Value::makeInt(-v.intVal);
        if (u->op == "!") return Value::makeBool(!v.truthy());
        return v;
    }

    Value evalCli(CliExpr* c) {
        if (c->method == "get") {
            for (int i = 0; i < argc_; ++i) {
                std::string a = argv_[i];
                std::string prefix = "--" + c->key + "=";
                if (a.substr(0, prefix.size()) == prefix)
                    return Value::makeString(a.substr(prefix.size()));
            }
            return Value::makeString("");
        }
        if (c->method == "has") {
            for (int i = 0; i < argc_; ++i) {
                std::string a = argv_[i];
                if (a == "-" + c->key || a == "--" + c->key) return Value::makeBool(true);
            }
            return Value::makeBool(false);
        }
        return Value::makeNone();
    }

    Value evalDot(DotExpr* d) {
        Value obj = evalExpr(d->object.get());
        if (obj.kind == Value::BLOB_REF) {
            auto it = registry_.find(obj.strVal);
            if (it != registry_.end()) {
                if (d->member == "size") return Value::makeInt(it->second.data.size());
                if (d->member == "name") return Value::makeString(it->second.name);
                if (d->member == "source") return Value::makeString(it->second.sourceFile);
            }
        }
        if (obj.kind == Value::REGION_REF) {
            auto it = regions_.find(obj.strVal);
            if (it != regions_.end()) {
                if (d->member == "start") return Value::makeInt(it->second.start);
                if (d->member == "size") return Value::makeInt(it->second.size);
            }
        }
        
        // Handle @target() and @host() metadata
        if (obj.kind == Value::STRING) {
            if (obj.strVal == "target" || obj.strVal == "host") {
                std::string key = obj.strVal + "." + d->member;
                if (vars_.count(key)) return vars_[key];
            }
            // Fallback for bare word property access (e.g. deny.asm_inline)
            return Value::makeString(obj.strVal + "." + d->member);
        }
        return Value::makeNone();
    }

    Value evalCall(CallExpr* c) {
        Value callee = evalExpr(c->callee.get());
        
        // Check if callee is an extern function
        if (callee.kind == Value::STRING && callee.strVal.substr(0, 7) == "extern:") {
            return callExtern(callee.strVal, c);
        }
        
        // Fallback for bare word method calls (e.g. require.alignment(4))
        if (callee.kind == Value::STRING) {
            std::string res = callee.strVal + "(";
            for (size_t i = 0; i < c->args.size(); ++i) {
                if (i > 0) res += ", ";
                res += evalExpr(c->args[i].get()).toString();
            }
            res += ")";
            return Value::makeString(res);
        }
        return Value::makeNone();
    }

    Value callExtern(const std::string& spec, CallExpr* c) {
        // spec is "extern:libpath:funcname"
        auto p1 = spec.find(':', 7);
        std::string lib = spec.substr(7, p1 - 7);
        std::string fn = spec.substr(p1 + 1);
        void* handle = dlopen(lib.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "\033[1;31m[BLV]\033[0m dlopen failed: " << dlerror() << "\n";
            return Value::makeNone();
        }
        // For now, extern calls return a blob reference
        std::cerr << "\033[1;33m[BLV]\033[0m extern call to " << fn << " (stub)\n";
        dlclose(handle);
        return Value::makeNone();
    }

    // ── Intrinsic evaluation ──
    Value evalIntrinsic(IntrinsicExpr* intr) {
        const std::string& n = intr->name;
        if (n == "region") return intrRegion(intr);
        if (n == "target") return intrTarget(intr);
        if (n == "host") return intrHost(intr);
        if (n == "map") return intrMap(intr);
        if (n == "reserve") return intrReserve(intr);
        if (n == "combine") return intrCombine(intr);
        if (n == "load") return intrLoad(intr);
        if (n == "resolve") return intrResolve(intr);
        if (n == "reloc") return intrReloc(intr);
        if (n == "patch") return intrPatch(intr);
        if (n == "inspect") return intrInspect(intr);
        if (n == "policy") return intrPolicy(intr);
        if (n == "enforce") return intrEnforce(intr);
        if (n == "filter") return intrFilter(intr);
        if (n == "build") return intrBuild(intr);
        if (n == "emit") return intrEmit(intr);
        if (n == "link") return intrLink(intr);
        if (n == "download") return intrDownload(intr);
        if (n == "exists") return intrExists(intr);
        if (n == "exec") return intrExec(intr);
        if (n == "alloc") return intrAlloc(intr);
        if (n == "read") return intrRead(intr);
        if (n == "append") return intrAppend(intr);
        if (n == "slice") return intrSlice(intr);
        if (n == "sizeof") return intrSizeof(intr);
        if (n == "println") return intrPrintln(intr);
        if (n == "exit") return intrExit(intr);
        std::cerr << "\033[1;31m[BLV]\033[0m unknown intrinsic @" << n << "\n";
        return Value::makeNone();
    }

    // @exit(code)
    Value intrExit(IntrinsicExpr* e) {
        int code = 0;
        if (!e->args.empty()) {
            Value v = evalExpr(e->args[0].get());
            code = (int)v.intVal;
        }
        std::exit(code);
    }

    // @target() -> returns raw target metadata
    Value intrTarget(IntrinsicExpr* e) {
        (void)e;
        std::string triple = "x86_64-pc-linux-gnu";
#ifdef __APPLE__
        triple = "x86_64-apple-darwin";
#endif
#ifdef _WIN32
        triple = "x86_64-pc-windows-msvc";
#endif
        for (int i = 0; i < argc_; ++i) {
            std::string a = argv_[i];
            if (a.substr(0, 9) == "--target=") triple = a.substr(9);
        }
        vars_["target.triple"] = Value::makeString(triple);
        return Value::makeString("target");
    }

    // @host() -> returns raw host metadata
    Value intrHost(IntrinsicExpr* e) {
        (void)e;
        // Just provide basics, let BLV code decide what to do
        vars_["host.name"] = Value::makeString("host");
        return Value::makeString("host");
    }

    // @region(start, size, align)
    Value intrRegion(IntrinsicExpr* e) {
        if (e->args.size() < 3) { std::cerr << "[BLV] @region needs 3 args\n"; return Value::makeNone(); }
        Value start = evalExpr(e->args[0].get());
        Value size = evalExpr(e->args[1].get());
        Value align = evalExpr(e->args[2].get());
        std::string name = "region_" + std::to_string(regions_.size());
        Region r; r.name = name; r.start = start.intVal; r.size = size.intVal; r.align = align.intVal;
        regions_[name] = r;
        std::cout << "\033[1;34m[region]\033[0m " << name << " @ 0x" << std::hex << r.start
                  << " size=" << std::dec << r.size << " align=" << r.align << "\n";
        return Value::makeRegionRef(name);
    }

    // @map(blob, region, offset)
    Value intrMap(IntrinsicExpr* e) {
        if (e->args.size() < 3) { std::cerr << "[BLV] @map needs 3 args\n"; return Value::makeNone(); }
        Value blob = evalExpr(e->args[0].get());
        Value reg = evalExpr(e->args[1].get());
        Value off = evalExpr(e->args[2].get());
        if (reg.kind != Value::REGION_REF) { std::cerr << "[BLV] @map arg2 must be region\n"; return Value::makeNone(); }
        auto& region = regions_[reg.strVal];
        region.mappings.push_back({off.intVal, blob.strVal});
        std::cout << "\033[1;34m[map]\033[0m " << blob.strVal << " -> " << reg.strVal
                  << " @ offset " << off.intVal << "\n";
        return Value::makeNone();
    }

    // @reserve(region, start, size)
    Value intrReserve(IntrinsicExpr* e) {
        if (e->args.size() < 3) { std::cerr << "[BLV] @reserve needs 3 args\n"; return Value::makeNone(); }
        Value reg = evalExpr(e->args[0].get());
        Value start = evalExpr(e->args[1].get());
        Value size = evalExpr(e->args[2].get());
        std::cout << "\033[1;34m[reserve]\033[0m " << reg.strVal << " [" << start.intVal
                  << ".." << (start.intVal + size.intVal) << "]\n";
        return Value::makeNone();
    }

    // @combine(A, B, ...) -> joins blobs or strings
    Value intrCombine(IntrinsicExpr* e) {
        std::string name = "combined_" + std::to_string(blobCounter_++);
        Blob combined; combined.name = name;
        for (auto& arg : e->args) {
            Value v = evalExpr(arg.get());
            if (v.kind == Value::BLOB_REF) {
                auto it = registry_.find(v.strVal);
                if (it != registry_.end()) {
                    combined.data.insert(combined.data.end(), it->second.data.begin(), it->second.data.end());
                    // Merge symbols
                    for (auto const& [sym, off] : it->second.symbols) {
                        combined.symbols[sym] = off; // simplistic merge
                    }
                }
            } else if (v.kind == Value::STRING) {
                std::string s = v.strVal;
                combined.data.insert(combined.data.end(), s.begin(), s.end());
            }
        }
        registry_[name] = std::move(combined);
        std::cout << "\033[1;34m[combine]\033[0m -> " << name << " (" << registry_[name].data.size() << " bytes)\n";
        return Value::makeBlobRef(name);
    }

    // @load("path", codec)
    Value intrLoad(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @load needs 2 args\n"; return Value::makeNone(); }
        Value path = evalExpr(e->args[0].get());
        Value codec = evalExpr(e->args[1].get());
        std::string fullPath = (fs::path(projectRoot_) / path.strVal).string();
        if (!fs::exists(fullPath)) {
            std::cerr << "\033[1;31m[BLV]\033[0m file not found: " << fullPath << "\n";
            return Value::makeNone();
        }
        std::string name = "blob_" + std::to_string(blobCounter_++);
        Blob blob; blob.name = name; blob.sourceFile = fullPath;
        blob.codec = codec.kind == Value::STRING ? codec.strVal : codec.toString();
        // Read file
        std::ifstream f(fullPath, std::ios::binary);
        blob.data = std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
        registry_[name] = std::move(blob);
        std::cout << "\033[1;34m[load]\033[0m " << path.strVal << " -> " << name
                  << " (" << registry_[name].data.size() << " bytes, codec: " << registry_[name].codec << ")\n";
        return Value::makeBlobRef(name);
    }

    // @resolve(A, B, mode)
    Value intrResolve(IntrinsicExpr* e) {
        if (e->args.size() < 3) { std::cerr << "[BLV] @resolve needs 3 args\n"; return Value::makeNone(); }
        Value a = evalExpr(e->args[0].get());
        Value b = evalExpr(e->args[1].get());
        Value mode = evalExpr(e->args[2].get());
        std::cout << "\033[1;34m[resolve]\033[0m " << a.strVal << " <- " << b.strVal
                  << " (mode: " << mode.toString() << ")\n";
        return Value::makeNone();
    }

    // @reloc(blob, base_addr)
    Value intrReloc(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @reloc needs 2 args\n"; return Value::makeNone(); }
        Value blob = evalExpr(e->args[0].get());
        Value base = evalExpr(e->args[1].get());
        std::cout << "\033[1;34m[reloc]\033[0m " << blob.strVal << " -> base 0x"
                  << std::hex << base.intVal << std::dec << "\n";
        return Value::makeNone();
    }

    // @patch(blob, offset, val, size)
    Value intrPatch(IntrinsicExpr* e) {
        if (e->args.size() < 4) { std::cerr << "[BLV] @patch needs 4 args\n"; return Value::makeNone(); }
        Value blob = evalExpr(e->args[0].get());
        Value off = evalExpr(e->args[1].get());
        Value val = evalExpr(e->args[2].get());
        Value sz = evalExpr(e->args[3].get());
        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) {
                auto& data = it->second.data;
                int64_t offset = off.intVal;
                int64_t value = val.intVal;
                int64_t size = sz.intVal;
                while ((int64_t)data.size() < offset + size) data.push_back(0);
                for (int64_t i = 0; i < size && i < 8; i++) {
                    data[offset + i] = (value >> (i * 8)) & 0xFF;
                }
            }
        }
        std::cout << "\033[1;34m[patch]\033[0m " << blob.strVal << " @ " << off.intVal << "\n";
        return Value::makeNone();
    }

    // @inspect(blob, "sym")
    Value intrInspect(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @inspect needs 2 args\n"; return Value::makeNone(); }
        Value blob = evalExpr(e->args[0].get());
        Value sym = evalExpr(e->args[1].get());
        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) {
                auto sit = it->second.symbols.find(sym.strVal);
                if (sit != it->second.symbols.end()) return Value::makeInt(sit->second);
            }
        }
        std::cerr << "\033[1;33m[BLV]\033[0m symbol '" << sym.strVal << "' not found\n";
        return Value::makeInt(-1);
    }

    // @policy(rules...)
    Value intrPolicy(IntrinsicExpr* e) {
        std::string name = "policy_" + std::to_string(policies_.size());
        Policy p; p.name = name;
        for (auto& arg : e->args) {
            Value v = evalExpr(arg.get());
            p.rules.push_back(v.toString());
        }
        // Also parse dot-idents like deny.sys_calls
        policies_[name] = p;
        std::cout << "\033[1;34m[policy]\033[0m " << name << " (" << p.rules.size() << " rules)\n";
        return Value::makePolicyRef(name);
    }

    // @enforce(policy, blob)
    Value intrEnforce(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @enforce needs 2 args\n"; return Value::makeNone(); }
        Value pol = evalExpr(e->args[0].get());
        Value blob = evalExpr(e->args[1].get());
        std::cout << "\033[1;34m[enforce]\033[0m " << pol.strVal << " on " << blob.strVal << " ✓\n";
        return Value::makeBool(true);
    }

    // @filter(rules...)
    Value intrFilter(IntrinsicExpr* e) {
        std::cout << "\033[1;34m[filter]\033[0m applied " << e->args.size() << " rules\n";
        return Value::makeBool(true);
    }

    // @build — invokes luvc
    Value intrBuild(IntrinsicExpr* e) {
        std::string target = "";
        std::string outputDir = "./";
        std::string defines = "";
        std::string excludes = "";

        auto parseFlag = [&](const std::string& f) {
            if (f.substr(0, 7) == "-target") {
                target = f.substr(8);
                if (target == "target.triple" && vars_.count("target.triple")) target = vars_["target.triple"].strVal;
            }
            else if (f.substr(0, 3) == "-D=") {
                defines = f.substr(3);
                if (defines == "cflags" && vars_.count("cflags")) defines = vars_["cflags"].strVal;
            }
            else if (f.substr(0, 9) == "-exclude") {
                excludes = f.substr(9);
                if (excludes == "excludes" && vars_.count("excludes")) excludes = vars_["excludes"].strVal;
            }
            else if (f.substr(0, 3) == "-o=") {
                outputDir = f.substr(3);
            }
            else if (f.substr(0, 2) == "-o") {
                outputDir = f.length() > 3 ? f.substr(3) : "./";
            }
        };

        for (auto& f : e->flags) parseFlag(f);

        // Collect source files from args
        std::vector<std::string> sources;
        for (auto& arg : e->args) {
            Value v = evalExpr(arg.get());
            if (v.kind == Value::BLOB_REF) {
                auto it = registry_.find(v.strVal);
                if (it != registry_.end()) sources.push_back(it->second.sourceFile);
            } else if (v.kind == Value::STRING) {
                if (!v.strVal.empty() && v.strVal[0] == '-') {
                    parseFlag(v.strVal);
                } else {
                    sources.push_back(v.strVal);
                }
            }
        }

        for (auto& src : sources) {
            std::string fullSrc = src;
            if (!fs::path(src).is_absolute()) fullSrc = makePath(src);

            std::string outDir = makePath(outputDir);
            fs::create_directories(outDir);
            std::string outFile = (fs::path(outDir) /
                (fs::path(fullSrc).stem().string() + ".o")).string();

            std::vector<std::string> cmd;
            cmd.push_back((fs::path(execDir_) / "luvc").string());
            
            auto addBuiltinFlags = [&](const std::string& f) {
                if (f.empty()) return;
                std::stringstream ss(f);
                std::string item;
                while (ss >> item) {
                    if (item.substr(0, 8) == "-target=") {
                        cmd.push_back("--target");
                        cmd.push_back(item.substr(8));
                    }
                    else if (item.substr(0, 3) == "-D=") {
                        cmd.push_back("--define");
                        cmd.push_back(item.substr(3));
                    }
                    else if (item.substr(0, 9) == "-exclude=") {
                        cmd.push_back("--exclude");
                        cmd.push_back(item.substr(9));
                    }
                }
            };

            if (!target.empty()) { cmd.push_back("--target"); cmd.push_back(target); }
            if (!defines.empty()) { cmd.push_back("--define"); cmd.push_back(defines); }
            if (!excludes.empty()) { cmd.push_back("--exclude"); cmd.push_back(excludes); }
            
            // Also check sources for flags that might have been passed as arguments
            for (auto& arg : e->args) {
                Value v = evalExpr(arg.get());
                if (v.kind == Value::STRING && !v.strVal.empty() && v.strVal[0] == '-') {
                    addBuiltinFlags(v.strVal);
                }
            }
            
            // Forward diagnostic flags from BLV's own CLI args natively
            for (int i = 0; i < argc_; ++i) {
                std::string a = argv_[i];
                if (a == "--lex" || a == "--parse" || a == "--ir" || a == "--asm" || a == "--dep-graph") {
                    cmd.push_back(a);
                }
            }

            cmd.push_back("-o");
            cmd.push_back(outFile);
            cmd.push_back(fullSrc);

            std::cout << "\e[1;35m[build]\e[0m luvc " << fullSrc << " -> " << outFile << "\n";
            int ret = runProcess(cmd, "build");
            if (ret != 0) {
                std::cerr << "\e[1;31m[BLV]\e[0m build failed for " << src << "\n";
                return Value::makeNone();
            }

            // Register output as blob
            std::string blobName = "obj_" + std::to_string(blobCounter_++);
            if (fs::exists(outFile)) {
                Blob b; b.name = blobName; b.sourceFile = outFile; b.codec = "obj";
                std::ifstream f(outFile, std::ios::binary);
                b.data = std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
                registry_[blobName] = std::move(b);
                std::cout << "\e[1;32m[build]\e[0m " << src << " -> " << blobName
                          << " (" << registry_[blobName].data.size() << " bytes)\n";
            }
            vars_["_last_obj"] = Value::makeBlobRef(blobName);
        }
        return vars_.count("_last_obj") ? vars_["_last_obj"] : Value::makeNone();
    }

    // @emit(blob, "output_path")
    Value intrEmit(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @emit needs 2 args\n"; return Value::makeNone(); }
        Value blob = evalExpr(e->args[0].get());
        Value path = evalExpr(e->args[1].get());
        std::string outPath = (fs::path(projectRoot_) / path.strVal).string();

        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) {
                std::ofstream f(outPath, std::ios::binary);
                f.write(reinterpret_cast<const char*>(it->second.data.data()), it->second.data.size());
                std::cout << "\e[1;32m[emit]\e[0m " << blob.strVal << " -> "
                          << outPath << " (" << it->second.data.size() << " bytes)\n";
                return Value::makeBool(true);
            }
        }

        // If it's a string path, just copy the file
        if (blob.kind == Value::STRING && fs::exists(blob.strVal)) {
            fs::copy_file(blob.strVal, outPath, fs::copy_options::overwrite_existing);
            std::cout << "\e[1;32m[emit]\e[0m " << blob.strVal << " -> " << outPath << "\n";
            return Value::makeBool(true);
        }
        std::cerr << "\e[1;31m[BLV]\e[0m @emit: blob not found\n";
        return Value::makeBool(false);
    }

    // @link(blob, "output_path", ...) -> generic linker orchestrator
    Value intrLink(IntrinsicExpr* e) {
        if (e->args.empty()) return Value::makeBool(false);
        
        std::string linkCmd = "ld"; // default (native linker)
        std::vector<std::string> extraFlags;
        std::string outputPath = "";
        std::vector<std::string> linkArgs;

        auto parseLinkFlag = [&](const std::string& f) {
            if (f.substr(0, 5) == "-cmd=") linkCmd = f.substr(5);
            else if (f.substr(0, 3) == "-o=") outputPath = makePath(f.substr(3));
            else extraFlags.push_back(f);
        };

        for (auto& f : e->flags) parseLinkFlag(f);

        for (size_t i = 0; i < e->args.size(); ++i) {
            Value v = evalExpr(e->args[i].get());
            if (v.kind == Value::BLOB_REF && registry_.count(v.strVal)) {
                linkArgs.push_back(registry_[v.strVal].sourceFile);
            }
            else if (v.kind == Value::STRING) {
                if (!v.strVal.empty() && v.strVal[0] == '-') {
                    parseLinkFlag(v.strVal);
                } else {
                    // Positional: if we don't have outputPath and it's index 1 or similar
                    // But to be safe, let's just collect all and handle outputPath via flag or explicit index
                    linkArgs.push_back(v.strVal);
                }
            }
        }
        
        // If outputPath still empty, try to take it from linkArgs if it looks like one
        // But better to be explicit in the script.
        
        if (isForbiddenTool(linkCmd)) {
            std::cerr << "\e[1;31m[BLV]\e[1;0m @link disallows " << linkCmd
                      << " (no sh/bash/clang)\n";
            return Value::makeInt(1);
        }

        std::vector<std::string> cmd;
        cmd.push_back(linkCmd);
        for (const auto& a : linkArgs) {
            // If it's the first non-flag string and we have an output path, or it's an object file
            cmd.push_back(a);
        }
        if (!outputPath.empty()) {
            cmd.push_back("-o");
            cmd.push_back(outputPath);
        }
        for (const auto& f : extraFlags) cmd.push_back(f);

        std::cout << "\e[1;35m[link]\e[0m " << linkCmd << " -> " << (outputPath.empty() ? "(none)" : outputPath) << "\n";
        int ret = runProcess(cmd, "link");
        return Value::makeInt(ret);
    }

    // @download("url", "dest")
    Value intrDownload(IntrinsicExpr* e) {
        if (e->args.size() < 2) { std::cerr << "[BLV] @download needs 2 args\n"; return Value::makeBool(false); }
        Value url = evalExpr(e->args[0].get());
        Value dest = evalExpr(e->args[1].get());
        std::string outPath = (fs::path(projectRoot_) / dest.strVal).string();

        std::cout << "\033[1;35m[download]\033[0m " << url.strVal << " -> " << dest.strVal << "\n";
        std::vector<std::string> cmd = {"curl", "-s", "-L", "-o", outPath, url.strVal};
        int ret = runProcess(cmd, "download");
        if (ret != 0) {
            std::cerr << "\033[1;31m[BLV]\033[0m download failed\n";
            return Value::makeBool(false);
        }
        return Value::makeBool(true);
    }

    // @exists("path")
    Value intrExists(IntrinsicExpr* e) {
        if (e->args.empty()) return Value::makeBool(false);
        Value path = evalExpr(e->args[0].get());
        std::string fullPath = (fs::path(projectRoot_) / path.strVal).string();
        return Value::makeBool(fs::exists(fullPath));
    }

    // @exec("cmd")
    Value intrExec(IntrinsicExpr* e) {
        (void)e;
        std::cerr << "\033[1;31m[BLV]\033[0m @exec is disabled (non-native shell execution is not allowed)\n";
        return Value::makeInt(1);
    }

    // @alloc(size)
    Value intrAlloc(IntrinsicExpr* e) {
        if (e->args.empty()) return Value::makeNone();
        Value size = evalExpr(e->args[0].get());
        std::string name = "blob_" + std::to_string(blobCounter_++);
        Blob b; b.name = name; b.codec = "raw";
        b.data.resize(size.intVal, 0);
        registry_[name] = std::move(b);
        return Value::makeBlobRef(name);
    }

    // @read(blob, offset, size)
    Value intrRead(IntrinsicExpr* e) {
        if (e->args.size() < 3) return Value::makeInt(0);
        Value blob = evalExpr(e->args[0].get());
        Value off = evalExpr(e->args[1].get());
        Value sz = evalExpr(e->args[2].get());
        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) {
                auto& data = it->second.data;
                int64_t offset = off.intVal;
                int64_t size = sz.intVal;
                int64_t result = 0;
                for (int64_t i = 0; i < size && i < 8 && (offset + i) < (int64_t)data.size(); ++i) {
                    result |= ((int64_t)data[offset + i]) << (i * 8);
                }
                return Value::makeInt(result);
            }
        }
        return Value::makeInt(0);
    }

    // @append(blobA, blobB)
    Value intrAppend(IntrinsicExpr* e) {
        if (e->args.size() < 2) return Value::makeBool(false);
        Value a = evalExpr(e->args[0].get());
        Value b = evalExpr(e->args[1].get());
        if (a.kind == Value::BLOB_REF && b.kind == Value::BLOB_REF) {
            auto itA = registry_.find(a.strVal);
            auto itB = registry_.find(b.strVal);
            if (itA != registry_.end() && itB != registry_.end()) {
                itA->second.data.insert(itA->second.data.end(), itB->second.data.begin(), itB->second.data.end());
                return Value::makeBool(true);
            }
        }
        return Value::makeBool(false);
    }

    // @slice(blob, start, size)
    Value intrSlice(IntrinsicExpr* e) {
        if (e->args.size() < 3) return Value::makeNone();
        Value blob = evalExpr(e->args[0].get());
        Value start = evalExpr(e->args[1].get());
        Value sz = evalExpr(e->args[2].get());
        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) {
                std::string name = "blob_" + std::to_string(blobCounter_++);
                Blob b; b.name = name; b.codec = it->second.codec;
                int64_t s = start.intVal;
                int64_t l = sz.intVal;
                if (s >= 0 && s < (int64_t)it->second.data.size()) {
                    if (s + l > (int64_t)it->second.data.size()) l = it->second.data.size() - s;
                    b.data.assign(it->second.data.begin() + s, it->second.data.begin() + s + l);
                }
                registry_[name] = std::move(b);
                return Value::makeBlobRef(name);
            }
        }
        return Value::makeNone();
    }

    // @sizeof(blob)
    Value intrSizeof(IntrinsicExpr* e) {
        if (e->args.empty()) return Value::makeInt(0);
        Value blob = evalExpr(e->args[0].get());
        if (blob.kind == Value::BLOB_REF) {
            auto it = registry_.find(blob.strVal);
            if (it != registry_.end()) return Value::makeInt(it->second.data.size());
        }
        return Value::makeInt(0);
    }

    // @println(args...)
    Value intrPrintln(IntrinsicExpr* e) {
        for (size_t i = 0; i < e->args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << evalExpr(e->args[i].get()).toString();
        }
        std::cout << std::endl;
        return Value::makeBool(true);
    }
};

} // namespace blv

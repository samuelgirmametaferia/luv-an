#include "rss/RSSPipeline.h"
#include "rss/MetaprogrammingEngine.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace luv::rss {

namespace {

static std::string smirStateToString(SMIRStateKind s) {
    switch (s) {
        case SMIRStateKind::CLEAN: return "clean";
        case SMIRStateKind::DIRTY: return "dirty";
        case SMIRStateKind::BORROWED: return "borrowed";
        case SMIRStateKind::MOVED: return "moved";
        case SMIRStateKind::ESCAPED: return "escaped";
        case SMIRStateKind::FREED: return "freed";
        default: return "unknown";
    }
}

static std::string smirOpToString(SMIRMemoryOpKind k) {
    switch (k) {
        case SMIRMemoryOpKind::READ: return "read";
        case SMIRMemoryOpKind::WRITE: return "write";
        case SMIRMemoryOpKind::MUTATE: return "mutate";
        case SMIRMemoryOpKind::ALLOCATE: return "allocate";
        case SMIRMemoryOpKind::FREE: return "free";
        case SMIRMemoryOpKind::BORROW: return "borrow";
        case SMIRMemoryOpKind::MOVE: return "move";
        case SMIRMemoryOpKind::ESCAPE: return "escape";
        case SMIRMemoryOpKind::CALL_BOUNDARY: return "call-boundary";
        default: return "unknown";
    }
}

static std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (const char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)c;
                else out << c;
        }
    }
    return out.str();
}

static SMIRMemoryOpKind mapMfgKind(const std::string& opKind) {
    if (opKind == "READ") return SMIRMemoryOpKind::READ;
    if (opKind == "MUTATE") return SMIRMemoryOpKind::MUTATE;
    if (opKind == "WRITE") return SMIRMemoryOpKind::WRITE;
    if (opKind == "ALLOCATE") return SMIRMemoryOpKind::ALLOCATE;
    if (opKind == "FREE") return SMIRMemoryOpKind::FREE;
    if (opKind == "BORROW") return SMIRMemoryOpKind::BORROW;
    if (opKind == "MOVE") return SMIRMemoryOpKind::MOVE;
    if (opKind == "ESCAPE") return SMIRMemoryOpKind::ESCAPE;
    return SMIRMemoryOpKind::UNKNOWN;
}

static SMIRStateTransition inferStateTransition(SMIRMemoryOpKind kind) {
    SMIRStateTransition t;
    switch (kind) {
        case SMIRMemoryOpKind::READ:
            t.from = SMIRStateKind::CLEAN;
            t.to = SMIRStateKind::CLEAN;
            t.reason = "read-only access";
            break;
        case SMIRMemoryOpKind::WRITE:
        case SMIRMemoryOpKind::MUTATE:
            t.from = SMIRStateKind::CLEAN;
            t.to = SMIRStateKind::DIRTY;
            t.reason = "mutation observed";
            break;
        case SMIRMemoryOpKind::ALLOCATE:
            t.from = SMIRStateKind::UNKNOWN;
            t.to = SMIRStateKind::CLEAN;
            t.reason = "allocation initializes memory";
            break;
        case SMIRMemoryOpKind::FREE:
            t.from = SMIRStateKind::DIRTY;
            t.to = SMIRStateKind::FREED;
            t.reason = "resource released";
            break;
        case SMIRMemoryOpKind::BORROW:
            t.from = SMIRStateKind::CLEAN;
            t.to = SMIRStateKind::BORROWED;
            t.reason = "borrowed alias";
            break;
        case SMIRMemoryOpKind::MOVE:
            t.from = SMIRStateKind::CLEAN;
            t.to = SMIRStateKind::MOVED;
            t.reason = "ownership transferred";
            break;
        case SMIRMemoryOpKind::ESCAPE:
        case SMIRMemoryOpKind::CALL_BOUNDARY:
            t.from = SMIRStateKind::CLEAN;
            t.to = SMIRStateKind::ESCAPED;
            t.reason = "cross-boundary transfer";
            break;
        default:
            t.reason = "unknown op";
            break;
    }
    return t;
}

static std::string fallbackRefFor(const std::string& fn, const std::string& node) {
    return "fallback://" + fn + "/" + node;
}

static std::vector<SMIROp> emitSMIR(const std::vector<MFGOp>& mfg,
                                    const ProbabilisticProfile& profile,
                                    const PipelineConfig& config,
                                    const RSSPipeline::SMIRStreamSink& sink = nullptr) {
    std::vector<SMIROp> out;
    out.reserve(mfg.size());
    for (size_t i = 0; i < mfg.size(); ++i) {
        const auto& op = mfg[i];
        SMIROp smir;
        smir.id = op.functionName + "#smir" + std::to_string((config.seed + i) & 0xffffffffull);
        smir.functionName = op.functionName;
        smir.sourceNodeId = op.sourceNodeId;
        smir.memorySymbol = op.sourceNodeId;
        smir.opKind = mapMfgKind(op.opKind);
        smir.state = inferStateTransition(smir.opKind);
        const auto pit = profile.likelihoodBySymbol.find(op.functionName);
        smir.specialization.pathLikelihood = pit == profile.likelihoodBySymbol.end() ? 1.0 : pit->second;
        smir.specialization.assumptions = {
            "cfg.node-exists",
            "profile.seed-stable",
            "fallback.available"
        };
        smir.fallbackSkeletonRef = fallbackRefFor(op.functionName, op.sourceNodeId);
        smir.specialization.fallbackLink = smir.fallbackSkeletonRef;
        out.push_back(std::move(smir));
        if (sink) sink(out.back());
    }
    return out;
}

static SMIRVerifierReport verifySMIR(const std::vector<SMIROp>& smir) {
    SMIRVerifierReport report;
    report.ok = true;
    report.verifiedOpCount = smir.size();

    auto addIssue = [&](SMIRVerifierIssue::Severity severity,
                        const std::string& code,
                        const std::string& message,
                        const std::string& opId) {
        report.issues.push_back({severity, code, message, opId});
        if (severity == SMIRVerifierIssue::Severity::ERROR) report.ok = false;
    };

    std::set<std::string> ids;
    for (const auto& op : smir) {
        if (op.id.empty()) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.id.empty", "SMIR op must have an id", op.id);
        if (!ids.insert(op.id).second) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.id.duplicate", "Duplicate SMIR id", op.id);
        if (op.functionName.empty()) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.function.empty", "SMIR op must have function name", op.id);
        if (op.sourceNodeId.empty()) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.node.empty", "SMIR op must reference source node", op.id);
        if (op.opKind == SMIRMemoryOpKind::UNKNOWN) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.op.unknown", "SMIR op kind cannot be unknown", op.id);
        if (op.state.to == SMIRStateKind::UNKNOWN) addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.state.to.unknown", "SMIR transition must have known target state", op.id);
        if (op.state.from == SMIRStateKind::FREED && op.state.to != SMIRStateKind::FREED) {
            addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.transition.freed", "Freed state cannot transition to live state", op.id);
        }
        if (std::isnan(op.specialization.pathLikelihood) ||
            op.specialization.pathLikelihood < 0.0 || op.specialization.pathLikelihood > 1.0) {
            addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.path.invalid", "Path likelihood must be in [0,1]", op.id);
        }
        if (op.specialization.assumptions.empty()) {
            addIssue(SMIRVerifierIssue::Severity::WARNING, "smir.assumptions.empty", "No specialization assumptions recorded", op.id);
        }
        if (op.fallbackSkeletonRef.empty()) {
            addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.fallback.missing", "Missing fallback skeleton reference", op.id);
        }
        if (op.specialization.fallbackLink.empty()) {
            addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.fallback.link.missing", "Missing specialization fallback link", op.id);
        } else if (op.specialization.fallbackLink != op.fallbackSkeletonRef) {
            addIssue(SMIRVerifierIssue::Severity::ERROR, "smir.fallback.link.mismatch", "Fallback link must match fallback skeleton reference", op.id);
        }
    }
    return report;
}

static std::vector<const FuncDecl*> collectFunctions(const Program& program) {
    std::vector<const FuncDecl*> funcs;
    funcs.reserve(program.statements.size());
    for (const auto* stmt : program.statements) {
        if (const auto* fn = dynamic_cast<const FuncDecl*>(stmt)) {
            funcs.push_back(fn);
        }
    }
    return funcs;
}

static std::string stableNodeId(const std::string& fnName, size_t idx, uint64_t seed) {
    std::hash<std::string> h;
    return fnName + "#n" + std::to_string((h(fnName) ^ (seed + idx)) & 0xfffffff);
}

static CFGFunction buildFunctionCFG(const FuncDecl& fn, uint64_t seed) {
    CFGFunction out;
    out.name = fn.name;
    const auto* body = fn.body;
    if (!body) return out;

    for (size_t i = 0; i < body->statements.size(); ++i) {
        CFGNode n;
        n.id = stableNodeId(fn.name, i, seed);
        n.kind = "stmt";
        if (i + 1 < body->statements.size()) {
            n.succ.push_back(stableNodeId(fn.name, i + 1, seed));
        }
        out.nodes.push_back(std::move(n));
    }
    return out;
}

static std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

class AliasPropagationPass final : public RSSPass {
public:
    std::string name() const override { return "alias-propagation"; }
    void run(RSSContext& ctx) override {
        (*ctx.outputs)["alias-propagation"] =
            "interprocedural summaries over " + std::to_string(ctx.cfg->functions.size()) + " functions";
    }
};

class ProbabilisticSpecializationPass final : public RSSPass {
public:
    std::string name() const override { return "probabilistic-specialization"; }
    std::vector<std::string> dependsOn() const override { return {"alias-propagation"}; }
    void run(RSSContext& ctx) override {
        (*ctx.outputs)["probabilistic-specialization"] =
            "profile symbols=" + std::to_string(ctx.profile->likelihoodBySymbol.size());
    }
};

class HardwareMappingPass final : public RSSPass {
public:
    std::string name() const override { return "hardware-mapping"; }
    std::vector<std::string> dependsOn() const override { return {"probabilistic-specialization"}; }
    std::vector<std::string> invalidates() const override { return {"emission-selection"}; }
    void run(RSSContext& ctx) override {
        const auto s = std::string("mmu=") + (ctx.hardware->hasMMU ? "1" : "0") +
                       " tlb_alias=" + (ctx.hardware->hasTLBAlias ? "1" : "0") +
                       " tbi=" + (ctx.hardware->hasTBI ? "1" : "0") +
                       " huge_pages=" + (ctx.hardware->hasHugePages ? "1" : "0") +
                       " trap_assist=" + (ctx.hardware->hasTrapAssist ? "1" : "0");
        (*ctx.outputs)["hardware-mapping"] = s;
    }
};

class EmissionSelectionPass final : public RSSPass {
public:
    std::string name() const override { return "emission-selection"; }
    std::vector<std::string> dependsOn() const override { return {"hardware-mapping"}; }
    void run(RSSContext& ctx) override {
        (*ctx.outputs)["emission-selection"] = "streaming-smir";
    }
};

} // namespace

CFGModule CFGBuilder::buildFull(const Program& program, const std::string& moduleName, const PipelineConfig& cfg) const {
    CFGModule mod;
    mod.moduleName = moduleName;
    auto funcs = collectFunctions(program);
    std::sort(funcs.begin(), funcs.end(), [](const FuncDecl* a, const FuncDecl* b) {
        return a->name < b->name;
    });
    for (const auto* fn : funcs) {
        mod.functions[fn->name] = buildFunctionCFG(*fn, cfg.seed);
    }
    return mod;
}

void CFGBuilder::updateIncremental(const Program& program,
                                   CFGModule& existing,
                                   const std::set<std::string>& changedFunctions,
                                   const PipelineConfig& cfg) const {
    if (changedFunctions.empty()) return;
    auto funcs = collectFunctions(program);
    std::unordered_map<std::string, const FuncDecl*> byName;
    byName.reserve(funcs.size());
    for (const auto* f : funcs) byName[f->name] = f;

    for (const auto& name : changedFunctions) {
        const auto it = byName.find(name);
        if (it == byName.end()) {
            existing.functions.erase(name);
            continue;
        }
        existing.functions[name] = buildFunctionCFG(*it->second, cfg.seed);
    }
}

void MFGBuilder::streamFromCFG(const CFGModule& cfg, const StreamSink& sink) const {
    for (const auto& [fnName, fnCfg] : cfg.functions) {
        for (const auto& node : fnCfg.nodes) {
            sink(MFGOp{fnName, "READ", node.id});
            sink(MFGOp{fnName, "MUTATE", node.id});
        }
    }
}

void RSSPassManager::add(std::unique_ptr<RSSPass> pass) {
    passes_.push_back(std::move(pass));
}

void RSSPassManager::run(RSSContext& ctx, std::vector<std::string>* executionOrderOut) {
    std::set<std::string> completed;
    std::set<std::string> pending;
    for (const auto& p : passes_) pending.insert(p->name());

    size_t guard = 0;
    while (!pending.empty()) {
        bool progressed = false;
        for (const auto& p : passes_) {
            if (!pending.count(p->name())) continue;
            const auto deps = p->dependsOn();
            const bool ready = std::all_of(deps.begin(), deps.end(), [&](const std::string& d) {
                return completed.count(d) > 0;
            });
            if (!ready) continue;

            p->run(ctx);
            completed.insert(p->name());
            pending.erase(p->name());
            if (executionOrderOut) executionOrderOut->push_back(p->name());

            for (const auto& inv : p->invalidates()) {
                if (completed.erase(inv) > 0) pending.insert(inv);
            }
            progressed = true;
        }
        if (!progressed) {
            throw std::runtime_error("RSS pass ordering has unresolved dependencies");
        }
        if (++guard > 1024) {
            throw std::runtime_error("RSS pass manager exceeded stabilization guard");
        }
    }
}

ProbabilisticProfile RSSPipeline::loadProbabilisticProfile(const std::string& path) {
    ProbabilisticProfile p;
    std::ifstream in(path);
    if (!in) return p;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        const auto key = trim(line.substr(0, sep));
        const auto val = trim(line.substr(sep + 1));
        try {
            p.likelihoodBySymbol[key] = std::stod(val);
        } catch (...) {
        }
    }
    return p;
}

HardwareProfile RSSPipeline::loadHardwareProfile(const std::string& path) {
    HardwareProfile h;
    std::ifstream in(path);
    if (!in) return h;
    std::string line;
    auto parseBool = [](const std::string& s) {
        return s == "1" || s == "true" || s == "on" || s == "yes";
    };
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        const auto key = trim(line.substr(0, sep));
        const auto val = parseBool(trim(line.substr(sep + 1)));
        if (key == "mmu") h.hasMMU = val;
        else if (key == "tlb_alias") h.hasTLBAlias = val;
        else if (key == "tbi") h.hasTBI = val;
        else if (key == "huge_pages") h.hasHugePages = val;
        else if (key == "trap_assist") h.hasTrapAssist = val;
    }
    return h;
}

AnalysisResult RSSPipeline::run(const Program& program,
                                const std::string& moduleName,
                                const std::set<std::string>& changedFunctions,
                                const ProbabilisticProfile& profile,
                                const HardwareProfile& hardware,
                                const PipelineConfig& config) const {
    AnalysisResult result;
    CFGBuilder cfgBuilder;
    MFGBuilder mfgBuilder;

    result.cfg = cfgBuilder.buildFull(program, moduleName, config);
    cfgBuilder.updateIncremental(program, result.cfg, changedFunctions, config);

    mfgBuilder.streamFromCFG(result.cfg, [&](const MFGOp& op) {
        result.streamedMfg.push_back(op);
    });

    MetaprogrammingEngine me;
    me.execute(program, result.streamedMfg, config);

    result.smir = emitSMIR(result.streamedMfg, profile, config);
    result.smirVerifier = verifySMIR(result.smir);
    result.smirDebugDump = dumpSMIR(result.smir);
    result.smirSnapshot = snapshotSMIR(result.smir);

    RSSPassManager pm;
    pm.add(std::make_unique<AliasPropagationPass>());
    pm.add(std::make_unique<ProbabilisticSpecializationPass>());
    pm.add(std::make_unique<HardwareMappingPass>());
    pm.add(std::make_unique<EmissionSelectionPass>());

    RSSContext ctx;
    ctx.program = &program;
    ctx.cfg = &result.cfg;
    ctx.profile = &profile;
    ctx.hardware = &hardware;
    ctx.config = &config;
    ctx.outputs = &result.passOutputs;

    std::vector<std::string> order;
    pm.run(ctx, &order);
    result.passOutputs["pass-order"] = [&]() {
        std::ostringstream oss;
        for (size_t i = 0; i < order.size(); ++i) {
            if (i) oss << " -> ";
            oss << order[i];
        }
        return oss.str();
    }();

    if (config.deterministic) {
        std::mt19937_64 rng(config.seed);
        result.passOutputs["determinism-seed"] = std::to_string(rng());
    }
    result.passOutputs["smir-op-count"] = std::to_string(result.smir.size());
    result.passOutputs["smir-verify"] = result.smirVerifier.ok ? "ok" : "failed";
    result.passOutputs["smir-issues"] = std::to_string(result.smirVerifier.issues.size());
    return result;
}

std::string RSSPipeline::dumpSMIR(const std::vector<SMIROp>& smir) {
    std::ostringstream out;
    out << "SMIR dump (" << smir.size() << " ops)\n";
    for (const auto& op : smir) {
        out << "- id=" << op.id
            << " fn=" << op.functionName
            << " node=" << op.sourceNodeId
            << " op=" << smirOpToString(op.opKind)
            << " state=" << smirStateToString(op.state.from) << "->" << smirStateToString(op.state.to)
            << " p=" << op.specialization.pathLikelihood
            << " fallback=" << op.fallbackSkeletonRef
            << "\n";
    }
    return out.str();
}

std::string RSSPipeline::snapshotSMIR(const std::vector<SMIROp>& smir) {
    std::ostringstream out;
    out << "{\n  \"smir\": [\n";
    for (size_t i = 0; i < smir.size(); ++i) {
        const auto& op = smir[i];
        out << "    {\n"
            << "      \"id\": \"" << jsonEscape(op.id) << "\",\n"
            << "      \"function\": \"" << jsonEscape(op.functionName) << "\",\n"
            << "      \"sourceNode\": \"" << jsonEscape(op.sourceNodeId) << "\",\n"
            << "      \"memorySymbol\": \"" << jsonEscape(op.memorySymbol) << "\",\n"
            << "      \"opKind\": \"" << smirOpToString(op.opKind) << "\",\n"
            << "      \"state\": {\"from\": \"" << smirStateToString(op.state.from)
            << "\", \"to\": \"" << smirStateToString(op.state.to)
            << "\", \"reason\": \"" << jsonEscape(op.state.reason) << "\"},\n"
            << "      \"specialization\": {\n"
            << "        \"pathLikelihood\": " << op.specialization.pathLikelihood << ",\n"
            << "        \"assumptions\": [";
        for (size_t j = 0; j < op.specialization.assumptions.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << jsonEscape(op.specialization.assumptions[j]) << "\"";
        }
        out << "],\n"
            << "        \"fallbackLink\": \"" << jsonEscape(op.specialization.fallbackLink) << "\"\n"
            << "      },\n"
            << "      \"fallbackSkeletonRef\": \"" << jsonEscape(op.fallbackSkeletonRef) << "\"\n"
            << "    }" << (i + 1 < smir.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

} // namespace luv::rss

#pragma once

#include "ast/AST.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace luv::rss {

struct CFGNode {
    std::string id;
    std::string kind;
    std::vector<std::string> succ;
};

struct CFGFunction {
    std::string name;
    std::vector<CFGNode> nodes;
};

struct CFGModule {
    std::string moduleName;
    std::map<std::string, CFGFunction> functions;
};

struct MFGOp {
    std::string functionName;
    std::string opKind;
    std::string sourceNodeId;
};

enum class SMIRMemoryOpKind {
    READ,
    WRITE,
    MUTATE,
    ALLOCATE,
    FREE,
    BORROW,
    MOVE,
    ESCAPE,
    CALL_BOUNDARY,
    UNKNOWN
};

enum class SMIRStateKind {
    UNKNOWN,
    CLEAN,
    DIRTY,
    BORROWED,
    MOVED,
    ESCAPED,
    FREED
};

struct SMIRStateTransition {
    SMIRStateKind from = SMIRStateKind::UNKNOWN;
    SMIRStateKind to = SMIRStateKind::UNKNOWN;
    std::string reason;
};

struct SMIRSpecializationMetadata {
    double pathLikelihood = 1.0;
    std::vector<std::string> assumptions;
    std::string fallbackLink;
};

struct SMIROp {
    std::string id;
    std::string functionName;
    std::string sourceNodeId;
    std::string memorySymbol;
    SMIRMemoryOpKind opKind = SMIRMemoryOpKind::UNKNOWN;
    SMIRStateTransition state;
    SMIRSpecializationMetadata specialization;
    std::string fallbackSkeletonRef;
};

struct SMIRVerifierIssue {
    enum class Severity { ERROR, WARNING };
    Severity severity = Severity::ERROR;
    std::string code;
    std::string message;
    std::string opId;
};

struct SMIRVerifierReport {
    bool ok = true;
    size_t verifiedOpCount = 0;
    std::vector<SMIRVerifierIssue> issues;
};

struct ProbabilisticProfile {
    std::map<std::string, double> likelihoodBySymbol;
};

struct HardwareProfile {
    bool hasMMU = true;
    bool hasTLBAlias = false;
    bool hasTBI = false;
    bool hasHugePages = false;
    bool hasTrapAssist = false;
};

struct PipelineConfig {
    uint64_t seed = 0x525353ull;
    bool deterministic = true;
};

struct AnalysisResult {
    CFGModule cfg;
    std::vector<MFGOp> streamedMfg;
    std::vector<SMIROp> smir;
    SMIRVerifierReport smirVerifier;
    std::string smirDebugDump;
    std::string smirSnapshot;
    std::map<std::string, std::string> passOutputs;
};

class CFGBuilder {
public:
    CFGModule buildFull(const Program& program, const std::string& moduleName, const PipelineConfig& cfg) const;
    void updateIncremental(const Program& program,
                           CFGModule& existing,
                           const std::set<std::string>& changedFunctions,
                           const PipelineConfig& cfg) const;
};

class MFGBuilder {
public:
    using StreamSink = std::function<void(const MFGOp&)>;
    void streamFromCFG(const CFGModule& cfg, const StreamSink& sink) const;
};

struct RSSContext {
    const Program* program = nullptr;
    const CFGModule* cfg = nullptr;
    const ProbabilisticProfile* profile = nullptr;
    const HardwareProfile* hardware = nullptr;
    const PipelineConfig* config = nullptr;
    std::map<std::string, std::string>* outputs = nullptr;
};

class RSSPass {
public:
    virtual ~RSSPass() = default;
    virtual std::string name() const = 0;
    virtual std::vector<std::string> dependsOn() const { return {}; }
    virtual std::vector<std::string> invalidates() const { return {}; }
    virtual void run(RSSContext& ctx) = 0;
};

class RSSPassManager {
public:
    void add(std::unique_ptr<RSSPass> pass);
    void run(RSSContext& ctx, std::vector<std::string>* executionOrderOut = nullptr);

private:
    std::vector<std::unique_ptr<RSSPass>> passes_;
};

class RSSPipeline {
public:
    using SMIRStreamSink = std::function<void(const SMIROp&)>;

    static ProbabilisticProfile loadProbabilisticProfile(const std::string& path);
    static HardwareProfile loadHardwareProfile(const std::string& path);

    static std::string dumpSMIR(const std::vector<SMIROp>& smir);
    static std::string snapshotSMIR(const std::vector<SMIROp>& smir);

    AnalysisResult run(const Program& program,
                       const std::string& moduleName,
                       const std::set<std::string>& changedFunctions,
                       const ProbabilisticProfile& profile,
                       const HardwareProfile& hardware,
                       const PipelineConfig& config) const;
};

} // namespace luv::rss

#pragma once

#include "ast/AST.h"
#include "rss/RSSPipeline.h"
#include <string>
#include <vector>

namespace luv::rss {

class MetaprogrammingEngine {
public:
    struct MacroContext {
        const Program& program;
        const std::vector<MFGOp>& mfg;
        const PipelineConfig& config;
    };

    void execute(const Program& program,
                 std::vector<MFGOp>& mfg,
                 const PipelineConfig& config);

private:
    void processNode(Node* node, MacroContext& ctx);
    void handleIntrinsic(IntrinsicCallExpr* intrinsic, MacroContext& ctx);
};

} // namespace luv::rss

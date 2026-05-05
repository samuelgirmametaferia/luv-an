#include "rss/MetaprogrammingEngine.h"
#include "LuvError.h"
#include <iostream>

namespace luv::rss {

class MetaprogrammingVisitor : public ASTVisitor {
public:
    MetaprogrammingVisitor(MetaprogrammingEngine::MacroContext& ctx) : ctx_(ctx) {}

    llvm::Value* visit(IntExpr& node) override { return nullptr; }
    llvm::Value* visit(FloatExpr& node) override { return nullptr; }
    llvm::Value* visit(StringExpr& node) override { return nullptr; }
    llvm::Value* visit(CharExpr& node) override { return nullptr; }
    llvm::Value* visit(BoolExpr& node) override { return nullptr; }
    llvm::Value* visit(NullExpr& node) override { return nullptr; }
    llvm::Value* visit(VarExpr& node) override { return nullptr; }
    llvm::Value* visit(BinaryExpr& node) override { return nullptr; }
    llvm::Value* visit(UnaryExpr& node) override { return nullptr; }
    llvm::Value* visit(CallExpr& node) override { return nullptr; }
    llvm::Value* visit(GenericCallExpr& node) override { return nullptr; }
    llvm::Value* visit(StructInstExpr& node) override { return nullptr; }
    
    llvm::Value* visit(IntrinsicCallExpr& node) override {
        if (node.callee == "RequireUnique") {
            if (node.args.size() != 1) {
                LuvError::error(ErrorKind::TYPE_ERROR, "@RequireUnique expects exactly 1 argument", "", 0, 0);
                return nullptr;
            }
            auto* var = dynamic_cast<VarExpr*>(node.args[0]);
            if (!var) {
                LuvError::error(ErrorKind::TYPE_ERROR, "@RequireUnique argument must be a variable", "", 0, 0);
                return nullptr;
            }

            // Simple MFG query: check if there's a "UNIQUE" state for this variable in MFG
            // (Note: the current MFG is dummy, so we simulate a check)
            bool foundUnique = false;
            for (const auto& op : ctx_.mfg) {
                if (op.opKind == "ALLOCATE" && op.sourceNodeId.find(var->name) != std::string::npos) {
                    foundUnique = true;
                    break;
                }
            }

            // For now, let's just log it
            std::cout << "[Metaprogramming] Checking @RequireUnique(" << var->name << ")... " 
                      << (foundUnique ? "OK" : "NOT FOUND (DUMMY)") << std::endl;
        }
        return nullptr;
    }

    llvm::Value* visit(AsmExpr& node) override { return nullptr; }
    llvm::Value* visit(MethodCallExpr& node) override {
        node.object->accept(*this);
        for (auto* arg : node.args) arg->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(SuperCallExpr& node) override {
        for (auto* arg : node.args) arg->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(PropertyExpr& node) override {
        node.object->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(CastExpr& node) override {
        node.expr->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(TernaryExpr& node) override {
        node.condition->accept(*this);
        node.thenExpr->accept(*this);
        node.elseExpr->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ComparisonChainExpr& node) override {
        for (auto* opnd : node.operands) opnd->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(StmtExpr& node) override {
        node.stmt->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(StringInterpolationExpr& node) override {
        for (auto* p : node.parts) p->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(Block& node) override {
        for (auto* s : node.statements) s->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(MatchExpr& node) override {
        node.value->accept(*this);
        for (auto& c : node.cases) {
            if (c.pattern) c.pattern->accept(*this);
            if (c.resultExpr) c.resultExpr->accept(*this);
            if (c.resultBlock) c.resultBlock->accept(*this);
        }
        return nullptr;
    }
    llvm::Value* visit(VarDecl& node) override {
        if (node.init) node.init->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(Assignment& node) override {
        for (auto* t : node.targets) t->accept(*this);
        node.value->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(IfExpr& node) override {
        node.cond->accept(*this);
        node.thenBlock->accept(*this);
        for (auto& ef : node.efs) {
            ef.cond->accept(*this);
            ef.block->accept(*this);
        }
        if (node.elseBlock) node.elseBlock->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(WhileExpr& node) override {
        node.cond->accept(*this);
        node.body->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ForRangeExpr& node) override {
        node.start->accept(*this);
        node.end->accept(*this);
        node.body->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ForCStyleExpr& node) override {
        node.init->accept(*this);
        node.cond->accept(*this);
        node.step->accept(*this);
        node.body->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ForInExpr& node) override {
        node.iterable->accept(*this);
        node.body->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ReturnStmt& node) override {
        if (node.value) node.value->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(BreakStmt& node) override { return nullptr; }
    llvm::Value* visit(ContinueStmt& node) override { return nullptr; }
    llvm::Value* visit(ExprStmt& node) override {
        node.expr->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(FuncDecl& node) override {
        if (node.body) node.body->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(StructDecl& node) override { return nullptr; }
    llvm::Value* visit(ClassDecl& node) override {
        for (auto* m : node.methods) m->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(InterfaceDecl& node) override { return nullptr; }
    llvm::Value* visit(ExternDecl& node) override { return nullptr; }
    llvm::Value* visit(ModuleDeclStmt& node) override { return nullptr; }
    llvm::Value* visit(IndexExpr& node) override {
        node.target->accept(*this);
        node.index->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(PostfixExpr& node) override {
        node.expr->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(UseStmt& node) override { return nullptr; }
    llvm::Value* visit(Program& node) override {
        for (auto* s : node.statements) s->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ArrayExpr& node) override {
        for (auto* e : node.elements) e->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ArrayRepeatExpr& node) override {
        node.value->accept(*this);
        node.count->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(ArrayCompExpr& node) override {
        node.iterable->accept(*this);
        if (node.rangeEnd) node.rangeEnd->accept(*this);
        if (node.step) node.step->accept(*this);
        node.expr->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(TupleExpr& node) override {
        for (auto* e : node.elements) e->accept(*this);
        return nullptr;
    }
    llvm::Value* visit(SliceExpr& node) override {
        node.target->accept(*this);
        if (node.start) node.start->accept(*this);
        if (node.end) node.end->accept(*this);
        if (node.step) node.step->accept(*this);
        return nullptr;
    }

private:
    MetaprogrammingEngine::MacroContext& ctx_;
};

void MetaprogrammingEngine::execute(const Program& program,
                                    std::vector<MFGOp>& mfg,
                                    const PipelineConfig& config) {
    MacroContext ctx{program, mfg, config};
    MetaprogrammingVisitor visitor(ctx);
    const_cast<Program&>(program).accept(visitor);
}

} // namespace luv::rss

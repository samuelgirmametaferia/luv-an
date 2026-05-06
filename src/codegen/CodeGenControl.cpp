#include "CodeGen.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::visit(IfExpr& node) {
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont");
    
    llvm::Type* resTy = getType(node.semanticType);
    bool isExpr = (resTy && !resTy->isVoidTy());
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiValues;

    // Condition
    llvm::Value* cond = node.cond->accept(*this);
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", F);
    llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "next");
    
    builder.CreateCondBr(toBool(cond), thenBB, nextBB);

    // Then
    builder.SetInsertPoint(thenBB);
    llvm::Value* thenVal = node.thenBlock->accept(*this);
    if (isExpr && thenVal) phiValues.push_back({thenVal, builder.GetInsertBlock()});
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);

    // Else-ifs (efs)
    for (auto& ef : node.efs) {
        F->insert(F->end(), nextBB);
        builder.SetInsertPoint(nextBB);
        
        llvm::Value* efCond = ef.cond->accept(*this);
        llvm::BasicBlock* efThenBB = llvm::BasicBlock::Create(context, "efthen", F);
        nextBB = llvm::BasicBlock::Create(context, "efnext");
        
        builder.CreateCondBr(toBool(efCond), efThenBB, nextBB);
        
        builder.SetInsertPoint(efThenBB);
        llvm::Value* efVal = ef.block->accept(*this);
        if (isExpr && efVal) phiValues.push_back({efVal, builder.GetInsertBlock()});
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);
    }

    // Else
    F->insert(F->end(), nextBB);
    builder.SetInsertPoint(nextBB);
    if (node.elseBlock) {
        llvm::Value* elseVal = node.elseBlock->accept(*this);
        if (isExpr && elseVal) phiValues.push_back({elseVal, builder.GetInsertBlock()});
    } else if (isExpr) {
        phiValues.push_back({llvm::Constant::getNullValue(resTy), builder.GetInsertBlock()});
    }
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(mergeBB);

    F->insert(F->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
    
    if (isExpr && !phiValues.empty()) {
        llvm::PHINode* phi = builder.CreatePHI(resTy, (unsigned)phiValues.size());
        for (auto& p : phiValues) {
            llvm::Value* v = p.first;
            if (v->getType() != resTy && v->getType()->isPointerTy() && resTy->isPointerTy()) {
                v = builder.CreateBitCast(v, resTy);
            }
            phi->addIncoming(v, p.second);
        }
        return lastValue = phi;
    }
    
    return nullptr;
}

llvm::Value* CodeGen::visit(WhileExpr& node) {
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "whilecond", F);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "whilebody");
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "whilecont");

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    llvm::Value* cond = node.cond->accept(*this);
    builder.CreateCondBr(toBool(cond), bodyBB, contBB);

    F->insert(F->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);
    loopStack.push_back({condBB, contBB, {}});
    node.body->accept(*this);
    loopStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(condBB);

    F->insert(F->end(), contBB);
    builder.SetInsertPoint(contBB);
    return nullptr;
}

llvm::Value* CodeGen::visit(ForRangeExpr& node) {
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "forcond", F);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "forbody");
    llvm::BasicBlock* stepBB = llvm::BasicBlock::Create(context, "forstep");
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "forcont");

    llvm::Value* startVal = node.start->accept(*this);
    llvm::Value* endVal = node.end->accept(*this);
    
    std::string varName = "i";
    if (auto* ipat = dynamic_cast<IdentifierPattern*>(node.pattern)) {
        varName = ipat->name;
    }

    llvm::AllocaInst* alloca = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, varName);
    builder.CreateStore(startVal, alloca);
    
    auto oldNamedValues = namedValues;
    namedValues[varName] = {alloca, llvm::Type::getInt64Ty(context), true};

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    
    llvm::Value* curr = builder.CreateLoad(llvm::Type::getInt64Ty(context), alloca);
    llvm::Value* cond = node.inclusive ? builder.CreateICmpSLE(curr, endVal) : builder.CreateICmpSLT(curr, endVal);
    builder.CreateCondBr(cond, bodyBB, contBB);

    F->insert(F->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);
    loopStack.push_back({stepBB, contBB, {}});
    node.body->accept(*this);
    loopStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(stepBB);

    F->insert(F->end(), stepBB);
    builder.SetInsertPoint(stepBB);
    llvm::Value* next = builder.CreateAdd(curr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1));
    builder.CreateStore(next, alloca);
    builder.CreateBr(condBB);

    F->insert(F->end(), contBB);
    builder.SetInsertPoint(contBB);
    namedValues = oldNamedValues;
    return nullptr;
}

llvm::Value* CodeGen::visit(ForCStyleExpr& node) {
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "forcond", F);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "forbody");
    llvm::BasicBlock* stepBB = llvm::BasicBlock::Create(context, "forstep");
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "forcont");

    auto oldNamedValues = namedValues;
    if (node.init) node.init->accept(*this);

    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    if (node.cond) {
        llvm::Value* cond = node.cond->accept(*this);
        builder.CreateCondBr(toBool(cond), bodyBB, contBB);
    } else {
        builder.CreateBr(bodyBB);
    }

    F->insert(F->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);
    
    std::vector<std::string> labels;
    for (const auto& attr : node.attributes) if (!attr.empty() && attr[0] == '!') labels.push_back(attr.substr(1));
    loopStack.push_back({stepBB, contBB, labels});
    
    node.body->accept(*this);
    loopStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(stepBB);

    F->insert(F->end(), stepBB);
    builder.SetInsertPoint(stepBB);
    if (node.step) node.step->accept(*this);
    builder.CreateBr(condBB);

    F->insert(F->end(), contBB);
    builder.SetInsertPoint(contBB);
    namedValues = oldNamedValues;
    return nullptr;
}

llvm::Value* CodeGen::visit(ForInExpr& node) {
    llvm::Value* iterable = node.iterable->accept(*this);
    if (!iterable) return nullptr;

    if (iterable->getType()->isStructTy()) {
        llvm::Value* dataPtr = builder.CreateExtractValue(iterable, 0);
        llvm::Value* len = builder.CreateExtractValue(iterable, 1);
        
        llvm::Type* elemType = llvm::Type::getInt64Ty(context);
        std::string semType = node.iterable->semanticType;
        if (semType.size() >= 2 && semType.front() == '[' && semType.back() == ']') {
            elemType = getType(semType.substr(1, semType.size() - 2));
        }

        llvm::Function* F = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "forincond", F);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "forinbody");
        llvm::BasicBlock* stepBB = llvm::BasicBlock::Create(context, "forinstep");
        llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "forincont");

        llvm::AllocaInst* iPtr = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "i");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), iPtr);
        builder.CreateBr(condBB);

        builder.SetInsertPoint(condBB);
        llvm::Value* i = builder.CreateLoad(llvm::Type::getInt64Ty(context), iPtr);
        llvm::Value* cond = builder.CreateICmpSLT(i, len);
        builder.CreateCondBr(cond, bodyBB, contBB);

        F->insert(F->end(), bodyBB);
        builder.SetInsertPoint(bodyBB);

        llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, i);
        llvm::Value* element = builder.CreateLoad(elemType, elementPtr);
        
        auto oldNamedValues = namedValues;
        generatePatternDestructuring(element, node.pattern, false, "");

        std::vector<std::string> labels;
        for (const auto& attr : node.attributes) if (!attr.empty() && attr[0] == '!') labels.push_back(attr.substr(1));
        loopStack.push_back({stepBB, contBB, labels});

        node.body->accept(*this);
        loopStack.pop_back();
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateBr(stepBB);

        F->insert(F->end(), stepBB);
        builder.SetInsertPoint(stepBB);
        builder.CreateStore(builder.CreateAdd(i, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)), iPtr);
        builder.CreateBr(condBB);

        F->insert(F->end(), contBB);
        builder.SetInsertPoint(contBB);
        namedValues = oldNamedValues;
    }
    return nullptr;
}

} // namespace luv

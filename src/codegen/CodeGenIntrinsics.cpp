#include "CodeGen.h"
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::generatePrint(const std::vector<Expr*>& args, bool newline) {
    llvm::Function* printfFunc = module->getFunction("printf");
    if (!printfFunc) {
        std::vector<llvm::Type*> printfArgs;
        printfArgs.push_back(llvm::PointerType::get(context, 0));
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), printfArgs, true);
        printfFunc = llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", *module);
    }

    if (args.empty()) {
        if (newline) return lastValue = builder.CreateCall(printfFunc, {builder.CreateGlobalStringPtr("\n")});
        return nullptr;
    }

    std::vector<llvm::Value*> values;
    values.reserve(args.size());
    for (auto* arg : args) values.push_back(arg->accept(*this));

    auto promotePrintfArg = [&](llvm::Value* v) -> llvm::Value* {
        if (!v) return v;
        if (v->getType()->isIntegerTy(1)) return builder.CreateZExt(v, llvm::Type::getInt32Ty(context));
        if (v->getType()->isIntegerTy(8) || v->getType()->isIntegerTy(16)) {
            return builder.CreateSExt(v, llvm::Type::getInt32Ty(context));
        }
        if (v->getType()->isFloatTy()) return builder.CreateFPExt(v, llvm::Type::getDoubleTy(context));
        return v;
    };

    std::vector<llvm::Value*> callArgs;
    std::string fmt;

    bool firstIsFormat = values[0] && values[0]->getType()->isPointerTy();
    if (firstIsFormat) {
        callArgs.push_back(values[0]);
        for (size_t i = 1; i < values.size(); ++i) callArgs.push_back(promotePrintfArg(values[i]));
        if (newline) {
            builder.CreateCall(printfFunc, callArgs);
            return lastValue = builder.CreateCall(printfFunc, {builder.CreateGlobalStringPtr("\n")});
        }
        return lastValue = builder.CreateCall(printfFunc, callArgs);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        llvm::Value* v = values[i];
        if (!v) continue;
        if (v->getType()->isPointerTy()) fmt += "%s";
        else if (v->getType()->isFloatingPointTy()) fmt += "%f";
        else if (v->getType()->isIntegerTy(1)) fmt += "%d";
        else if (v->getType()->isIntegerTy(8)) fmt += "%c";
        else fmt += "%lld";
        if (i + 1 < values.size()) fmt += " ";
    }
    if (newline) fmt += "\n";
    callArgs.push_back(builder.CreateGlobalStringPtr(fmt));
    for (auto* v : values) callArgs.push_back(promotePrintfArg(v));
    return lastValue = builder.CreateCall(printfFunc, callArgs);
}

llvm::Value* CodeGen::visit(IntrinsicCallExpr& node) {
    if (node.callee == "println" || node.callee == "print") {
        return generatePrint(node.args, node.callee == "println");
    }

    if (node.callee == "max") {
        if (node.args.size() != 2) return nullptr;
        llvm::Value* L = node.args[0]->accept(*this);
        llvm::Value* R = node.args[1]->accept(*this);
        llvm::Value* cond = builder.CreateICmpSGT(L, R);
        return lastValue = builder.CreateSelect(cond, L, R);
    }

    if (node.callee == "min") {
        if (node.args.size() != 2) return nullptr;
        llvm::Value* L = node.args[0]->accept(*this);
        llvm::Value* R = node.args[1]->accept(*this);
        llvm::Value* cond = builder.CreateICmpSLT(L, R);
        return lastValue = builder.CreateSelect(cond, L, R);
    }

    if (node.callee == "abs") {
        if (node.args.size() != 1) return nullptr;
        llvm::Value* val = node.args[0]->accept(*this);
        if (val->getType()->isFloatingPointTy()) {
            llvm::Function* absFunc = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::fabs, {val->getType()});
            return lastValue = builder.CreateCall(absFunc, {val});
        } else {
            llvm::Value* neg = builder.CreateNeg(val);
            llvm::Value* cond = builder.CreateICmpSGE(val, llvm::ConstantInt::get(val->getType(), 0));
            return lastValue = builder.CreateSelect(cond, val, neg);
        }
    }

    if (node.callee == "sizeof") {
        if (node.args.size() != 1) return nullptr;
        llvm::Type* t = getType(node.args[0]->semanticType);
        if (t->isVoidTy()) {
             llvm::Value* v = node.args[0]->accept(*this);
             if (v) t = v->getType();
        }
        uint64_t size = module->getDataLayout().getTypeAllocSize(t);
        return lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), size);
    }

    if (node.callee == "popcount") {
        if (node.args.size() != 1) return nullptr;
        llvm::Value* val = node.args[0]->accept(*this);
        llvm::Function* popcountFunc = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::ctpop, {val->getType()});
        return lastValue = builder.CreateCall(popcountFunc, {val});
    }

    return nullptr;
}

llvm::Value* CodeGen::visit(AsmExpr& node) {
    llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), false);
    llvm::InlineAsm* IA = llvm::InlineAsm::get(FT, node.code, "=r", true);
    return lastValue = builder.CreateCall(IA);
}

llvm::Value* CodeGen::visit(CastExpr& node) {
    llvm::Value* val = node.expr->accept(*this);
    llvm::Type* destType = getType(node.targetType);
    if (!val) return nullptr;
    
    if (val->getType() == destType) return lastValue = val;
    
    if (val->getType()->isFloatingPointTy() && destType->isIntegerTy())
        return lastValue = builder.CreateFPToSI(val, destType);
    if (val->getType()->isIntegerTy() && destType->isFloatingPointTy())
        return lastValue = builder.CreateSIToFP(val, destType);
    if (val->getType()->isIntegerTy() && destType->isIntegerTy())
        return lastValue = builder.CreateIntCast(val, destType, true);
        
    return lastValue = builder.CreateBitCast(val, destType);
}

llvm::Value* CodeGen::visit(StringInterpolationExpr& node) {
    llvm::Value* acc = builder.CreateGlobalStringPtr("");
    llvm::Function* concat = getOrCreateStringConcat();
    if (!concat) return nullptr;
    for (auto* part : node.parts) {
        llvm::Value* v = part->accept(*this);
        llvm::Value* asStr = generateToString(v);
        if (!asStr) continue;
        acc = builder.CreateCall(concat, {acc, asStr});
    }
    return lastValue = acc;
}

void CodeGen::generateDivByZeroCheck(llvm::Value* val) {
    llvm::Function* F = module->getFunction("luv_div_zero_check");
    if (!F) {
        auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {llvm::Type::getInt64Ty(context)}, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "luv_div_zero_check", *module);
    }
    llvm::Value* v = val;
    if (!v->getType()->isIntegerTy(64)) v = builder.CreateIntCast(v, llvm::Type::getInt64Ty(context), true);
    builder.CreateCall(F, {v});
}

void CodeGen::generateNullCheck(llvm::Value* val, const std::string& msg) {
    llvm::Function* F = module->getFunction("luv_null_check");
    if (!F) {
        auto* i8p = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i8p, i8p}, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "luv_null_check", *module);
    }
    llvm::Value* ptr = val;
    if (!ptr->getType()->isPointerTy()) return;
    llvm::Value* cast = builder.CreateBitCast(ptr, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
    builder.CreateCall(F, {cast, builder.CreateGlobalStringPtr(msg)});
}

void CodeGen::generateBoundsCheck(llvm::Value* index, llvm::Value* length) {
    llvm::Function* F = module->getFunction("luv_bounds_check");
    if (!F) {
        auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
            {llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "luv_bounds_check", *module);
    }
    llvm::Value* i = index;
    llvm::Value* l = length;
    if (!i->getType()->isIntegerTy(64)) i = builder.CreateIntCast(i, llvm::Type::getInt64Ty(context), true);
    if (!l->getType()->isIntegerTy(64)) l = builder.CreateIntCast(l, llvm::Type::getInt64Ty(context), true);
    builder.CreateCall(F, {i, l});
}

llvm::Value* CodeGen::generateToString(llvm::Value* val) {
    if (!val) return builder.CreateGlobalStringPtr("");
    auto* i8p = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    if (val->getType()->isPointerTy()) return val;
    if (val->getType()->isIntegerTy(1)) {
        llvm::Value* cond = builder.CreateICmpNE(val, llvm::ConstantInt::get(val->getType(), 0));
        return builder.CreateSelect(cond, builder.CreateGlobalStringPtr("true"), builder.CreateGlobalStringPtr("false"));
    }

    std::string fnName;
    llvm::Type* argTy = nullptr;
    if (val->getType()->isIntegerTy(8)) { fnName = "string_from_char"; argTy = llvm::Type::getInt8Ty(context); }
    else if (val->getType()->isFloatingPointTy()) { fnName = "string_from_float"; argTy = llvm::Type::getDoubleTy(context); }
    else { fnName = "string_from_int"; argTy = llvm::Type::getInt64Ty(context); }

    llvm::Function* F = module->getFunction(fnName);
    if (!F) {
        auto* FT = llvm::FunctionType::get(i8p, {argTy}, false);
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, *module);
    }

    llvm::Value* arg = val;
    if (argTy->isIntegerTy() && !val->getType()->isIntegerTy()) arg = builder.CreateFPToSI(val, argTy);
    else if (argTy->isFloatingPointTy() && !val->getType()->isFloatingPointTy()) arg = builder.CreateSIToFP(val, argTy);
    else if (arg->getType() != argTy) arg = builder.CreateIntCast(arg, argTy, true);
    return builder.CreateCall(F, {arg});
}

llvm::Function* CodeGen::getOrCreateStringConcat() {
    llvm::Function* F = module->getFunction("string_concat");
    if (F) return F;
    auto* i8p = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    auto* FT = llvm::FunctionType::get(i8p, {i8p, i8p}, false);
    return llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "string_concat", *module);
}

} // namespace luv

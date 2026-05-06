#include "CodeGen.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <functional>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::visit(VarDecl& node) {
    llvm::Type* T = getType(node.type);
    llvm::Value* initVal = node.init ? node.init->accept(*this) : llvm::Constant::getNullValue(T);

    // Check for Class to Interface cast in VarDecl
    if (!node.type.empty() && interfaceNames.count(node.type) && initVal && initVal->getType()->isPointerTy()) {
        std::string clsName = node.init ? node.init->semanticType : "";
        if (!clsName.empty()) {
            std::string itableName = clsName + "_as_" + node.type + "_itable";
            if (auto* itableGlobal = module->getGlobalVariable(itableName, true)) {
                llvm::Value* fatPtr = llvm::UndefValue::get(T);
                fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(initVal, llvm::PointerType::get(context, 0)), 0);
                fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(itableGlobal, llvm::PointerType::get(context, 0)), 1);
                initVal = fatPtr;
            }
        }
    }

    if (!builder.GetInsertBlock()) {
        if (auto* ipat = dynamic_cast<IdentifierPattern*>(node.pattern)) {
            llvm::Constant* cInit = llvm::dyn_cast<llvm::Constant>(initVal);
            if (!cInit) cInit = llvm::Constant::getNullValue(T);
            auto* g = new llvm::GlobalVariable(*module, T, node.isConst,
                llvm::GlobalValue::InternalLinkage, cInit, ipat->name);
            varSemanticTypes[ipat->name] = node.type;
            return lastValue = g;
        }
        return nullptr;
    }

    generatePatternDestructuring(initVal, node.pattern, node.isMutable, node.type);
    return lastValue = initVal;
}

llvm::Value* CodeGen::visit(Assignment& node) {
    auto computeAssigned = [&](llvm::Value* oldVal, llvm::Value* newVal, const std::string& op, const std::string& targetType = "") -> llvm::Value* {
        if (op == "=") {
            // Check for Class to Interface cast
            if (!targetType.empty() && interfaceNames.count(targetType) && newVal->getType()->isPointerTy()) {
                std::string clsName = node.value->semanticType;
                if (clsName.empty() && newVal->getType()->isStructTy()) {
                    auto* st = llvm::cast<llvm::StructType>(newVal->getType());
                    if (st->hasName()) clsName = std::string(st->getName());
                }
                
                if (!clsName.empty()) {
                    std::string itableName = clsName + "_as_" + targetType + "_itable";
                    if (auto* itableGlobal = module->getGlobalVariable(itableName, true)) {
                        llvm::Type* fatPtrTy = getType(targetType);
                        llvm::Value* fatPtr = llvm::UndefValue::get(fatPtrTy);
                        fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(newVal, llvm::PointerType::get(context, 0)), 0);
                        fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(itableGlobal, llvm::PointerType::get(context, 0)), 1);
                        return fatPtr;
                    }
                }
            }
            return newVal;
        }
        bool isFloat = oldVal->getType()->isFloatingPointTy() || newVal->getType()->isFloatingPointTy();
        llvm::Value* L = oldVal;
        llvm::Value* R = newVal;
        if (L->getType() != R->getType()) {
            if (isFloat) {
                if (!L->getType()->isFloatingPointTy()) L = builder.CreateSIToFP(L, llvm::Type::getDoubleTy(context));
                if (!R->getType()->isFloatingPointTy()) R = builder.CreateSIToFP(R, llvm::Type::getDoubleTy(context));
            } else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                unsigned w = std::max(L->getType()->getIntegerBitWidth(), R->getType()->getIntegerBitWidth());
                llvm::Type* W = llvm::Type::getIntNTy(context, w);
                if (L->getType() != W) L = builder.CreateIntCast(L, W, true);
                if (R->getType() != W) R = builder.CreateIntCast(R, W, true);
            }
        }
        if (op == "+=") return isFloat ? builder.CreateFAdd(L, R) : builder.CreateAdd(L, R);
        if (op == "-=") return isFloat ? builder.CreateFSub(L, R) : builder.CreateSub(L, R);
        if (op == "*=") return isFloat ? builder.CreateFMul(L, R) : builder.CreateMul(L, R);
        if (op == "/=") return isFloat ? builder.CreateFDiv(L, R) : builder.CreateSDiv(L, R);
        if (op == "%=") return isFloat ? builder.CreateFRem(L, R) : builder.CreateSRem(L, R);
        if (op == "&=") return builder.CreateAnd(L, R);
        if (op == "|=") return builder.CreateOr(L, R);
        if (op == "^=") return builder.CreateXor(L, R);
        if (op == "<<=") return builder.CreateShl(L, R);
        if (op == ">>=") return builder.CreateAShr(L, R);
        return newVal;
    };

    llvm::Value* val = node.value->accept(*this);
    if (!val) return nullptr;

    std::function<void(Expr*, llvm::Value*)> performAssign = [&](Expr* target, llvm::Value* currentVal) {
        if (auto* var = dynamic_cast<VarExpr*>(target)) {
            if (namedValues.count(var->name)) {
                auto& info = namedValues[var->name];
                llvm::Value* oldVal = builder.CreateLoad(info.type, info.ptr);
                llvm::Value* assigned = computeAssigned(oldVal, currentVal, node.op, varSemanticTypes[var->name]);
                if (assigned->getType() != info.type) {
                    if (assigned->getType()->isIntegerTy() && info.type->isIntegerTy()) {
                        assigned = builder.CreateIntCast(assigned, info.type, true);
                    } else if (assigned->getType()->isFloatingPointTy() && info.type->isFloatingPointTy()) {
                        assigned = builder.CreateFPCast(assigned, info.type);
                    } else if (assigned->getType()->isPointerTy() && info.type->isPointerTy()) {
                        assigned = builder.CreateBitCast(assigned, info.type);
                    }
                }
                builder.CreateStore(assigned, info.ptr);
            } else if (auto* g = module->getGlobalVariable(var->name)) {
                llvm::Value* oldVal = builder.CreateLoad(g->getValueType(), g);
                llvm::Value* assigned = computeAssigned(oldVal, currentVal, node.op); 
                builder.CreateStore(assigned, g);
            } else {
                // Implicit declaration
                std::string semType = target->semanticType; 
                llvm::Type* T = currentVal->getType();
                if (!semType.empty()) T = getType(semType);
                varSemanticTypes[var->name] = semType;

                if (builder.GetInsertBlock()) {
                    llvm::AllocaInst* alloca = builder.CreateAlloca(T, nullptr, var->name);
                    builder.CreateStore(currentVal, alloca);
                    namedValues[var->name] = {alloca, T, true};
                } else {
                    llvm::Constant* cInit = llvm::dyn_cast<llvm::Constant>(currentVal);
                    if (!cInit) cInit = llvm::Constant::getNullValue(T);
                    new llvm::GlobalVariable(*module, T, false,
                        llvm::GlobalValue::InternalLinkage, cInit, var->name);
                }
            }
        } else if (auto* tuple = dynamic_cast<TupleExpr*>(target)) {
            if (currentVal->getType()->isStructTy()) {
                for (size_t i = 0; i < tuple->elements.size(); ++i) {
                    llvm::Value* elem = builder.CreateExtractValue(currentVal, {(unsigned)i});
                    performAssign(tuple->elements[i], elem);
                }
            }
        } else if (auto* prop = dynamic_cast<PropertyExpr*>(target)) {
            llvm::Value* obj = prop->object->accept(*this);
            if (!obj) return;
            std::string clsName = prop->object->semanticType;
            if (!classDecls.count(clsName) || !classTypes.count(clsName)) return;
            llvm::Value* clsPtr = obj;
            llvm::Type* expectedPtrTy = llvm::PointerType::get(classTypes[clsName], 0);
            if (clsPtr->getType() != expectedPtrTy && clsPtr->getType()->isPointerTy()) {
                clsPtr = builder.CreateBitCast(clsPtr, expectedPtrTy);
            }
            using FieldRef = std::pair<llvm::Value*, llvm::Type*>;
            std::function<FieldRef(const std::string&, llvm::Value*, const std::string&)> buildFieldPtr;
            buildFieldPtr = [&](const std::string& className, llvm::Value* basePtr, const std::string& fieldName) -> FieldRef {
                auto* decl = classDecls[className];
                for (size_t i = 0; i < decl->fields.size(); ++i) {
                    if (decl->fields[i].name == fieldName) {
                        llvm::Value* ptr = builder.CreateStructGEP(classTypes[className], basePtr, (unsigned)(i + 1));
                        return {ptr, getType(decl->fields[i].type)};
                    }
                }
                if (!decl->baseAndInterfaces.empty() && classDecls.count(decl->baseAndInterfaces[0])) {
                    llvm::Value* basePtrField = builder.CreateStructGEP(classTypes[className], basePtr, 0);
                    return buildFieldPtr(decl->baseAndInterfaces[0], basePtrField, fieldName);
                }
                return {nullptr, nullptr};
            };
            auto [fieldPtr, fieldTy] = buildFieldPtr(clsName, clsPtr, prop->propertyName);
            if (!fieldPtr || !fieldTy) return;
            llvm::Value* oldVal = builder.CreateLoad(fieldTy, fieldPtr);
            llvm::Value* assigned = computeAssigned(oldVal, currentVal, node.op);
            builder.CreateStore(assigned, fieldPtr);
        } else if (auto* idxExpr = dynamic_cast<IndexExpr*>(target)) {
            llvm::Value* arr = idxExpr->target->accept(*this);
            llvm::Value* idx = idxExpr->index->accept(*this);
            if (!arr || !idx) return;
            if (arr->getType()->isStructTy()) {
                llvm::Value* dataPtr = builder.CreateExtractValue(arr, 0);
                llvm::Type* elemType = llvm::Type::getInt64Ty(context);
                std::string semType = idxExpr->target->semanticType;
                if (semType.size() >= 2 && semType.front() == '[' && semType.back() == ']') {
                    elemType = getType(semType.substr(1, semType.size() - 2));
                }
                llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, idx);
                llvm::Value* oldVal = builder.CreateLoad(elemType, elementPtr);
                llvm::Value* assigned = computeAssigned(oldVal, currentVal, node.op);
                builder.CreateStore(assigned, elementPtr);
            }
        }
    };

    for (size_t i = 0; i < node.targets.size(); ++i) {
        llvm::Value* currentVal = val;
        if (node.targets.size() > 1 && val->getType()->isStructTy()) {
            llvm::StructType* ST = llvm::cast<llvm::StructType>(val->getType());
            if (i < ST->getNumElements()) {
                currentVal = builder.CreateExtractValue(val, {(unsigned)i});
            }
        }
        performAssign(node.targets[i], currentVal);
    }
    return lastValue = val;
}

llvm::Value* CodeGen::visit(ReturnStmt& node) {
    llvm::Value* val = node.value ? node.value->accept(*this) : nullptr;
    if (val && currentReturnType && val->getType() != currentReturnType) {
        if (val->getType()->isIntegerTy() && currentReturnType->isIntegerTy()) {
            val = builder.CreateIntCast(val, currentReturnType, true);
        } else if (val->getType()->isFloatingPointTy() && currentReturnType->isFloatingPointTy()) {
            val = builder.CreateFPCast(val, currentReturnType);
        } else if (val->getType()->isPointerTy() && currentReturnType->isPointerTy()) {
            val = builder.CreateBitCast(val, currentReturnType);
        }
    }
    return lastValue = val ? builder.CreateRet(val) : builder.CreateRetVoid();
}

llvm::Value* CodeGen::visit(FuncDecl& node) {
    std::vector<llvm::Type*> argTypes;
    bool needsImplicitSelf = !node.boundStruct.empty() && 
        (classDecls.count(node.boundStruct) || structDecls.count(node.boundStruct)) && 
        !node.isStatic &&
        (node.params.empty() || node.params[0].name != "self");

    if (needsImplicitSelf) {
        llvm::Type* selfTy = classDecls.count(node.boundStruct) ? 
            (llvm::Type*)llvm::PointerType::get(classTypes[node.boundStruct], 0) :
            (llvm::Type*)llvm::PointerType::get(structTypes[node.boundStruct], 0);
        argTypes.push_back(selfTy);
    }
    for (const auto& p : node.params) {
        argTypes.push_back(getType(p.type));
    }
    
    llvm::FunctionType* FT = llvm::FunctionType::get(getType(node.returnType), argTypes, false);
    llvm::Function* F = module->getFunction(node.name);
    if (!F) {
        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, node.name, *module);
    }
    functionDecls[node.name] = &node;

    if (node.body) {
        llvm::BasicBlock* BB = llvm::BasicBlock::Create(context, "entry", F);
        builder.SetInsertPoint(BB);
        auto oldNamedValues = namedValues;
        currentReturnType = getType(node.returnType);
        
        unsigned argIdx = 0;
        if (needsImplicitSelf) {
            llvm::Argument* arg = F->getArg(argIdx++);
            llvm::AllocaInst* alloca = builder.CreateAlloca(arg->getType(), nullptr, "self");
            builder.CreateStore(arg, alloca);
            namedValues["self"] = {alloca, arg->getType(), false};
        }
        for (const auto& p : node.params) {
            llvm::Argument* arg = F->getArg(argIdx++);
            llvm::AllocaInst* alloca = builder.CreateAlloca(arg->getType(), nullptr, p.name);
            builder.CreateStore(arg, alloca);
            namedValues[p.name] = {alloca, arg->getType(), true}; // params are mutable by default in Luv
            varSemanticTypes[p.name] = p.type;
        }

        node.body->accept(*this);
        
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (FT->getReturnType()->isVoidTy()) builder.CreateRetVoid();
            else if (FT->getReturnType()->isIntegerTy()) builder.CreateRet(llvm::ConstantInt::get(FT->getReturnType(), 0));
            else builder.CreateRet(llvm::Constant::getNullValue(FT->getReturnType()));
        }
        namedValues = oldNamedValues;
    }
    return lastValue = F;
}

llvm::Value* CodeGen::visit(BreakStmt& node) {
    if (loopStack.empty()) return nullptr;
    if (!node.label.empty()) {
        for (auto it = loopStack.rbegin(); it != loopStack.rend(); ++it) {
            for (const auto& l : it->labels) if (l == node.label) return lastValue = builder.CreateBr(it->exitBB);
        }
    }
    return lastValue = builder.CreateBr(loopStack.back().exitBB);
}

llvm::Value* CodeGen::visit(ContinueStmt& node) {
    if (loopStack.empty()) return nullptr;
    if (!node.label.empty()) {
        for (auto it = loopStack.rbegin(); it != loopStack.rend(); ++it) {
            for (const auto& l : it->labels) if (l == node.label) return lastValue = builder.CreateBr(it->continueBB);
        }
    }
    return lastValue = builder.CreateBr(loopStack.back().continueBB);
}

llvm::Value* CodeGen::visit(ExprStmt& node) {
    return lastValue = node.expr ? node.expr->accept(*this) : nullptr;
}

llvm::Value* CodeGen::visit(ExternDecl& node) {
    std::vector<llvm::Type*> argTypes;
    for (const auto& p : node.params) argTypes.push_back(getType(p.type));
    llvm::FunctionType* FT = llvm::FunctionType::get(getType(node.returnType), argTypes, false);
    return lastValue = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, node.name, *module);
}

} // namespace luv

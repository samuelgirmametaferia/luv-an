#include "CodeGen.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::visit(IntExpr& node) {
    return lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), std::stoll(node.value));
}

llvm::Value* CodeGen::visit(FloatExpr& node) {
    return lastValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), std::stod(node.value));
}

llvm::Value* CodeGen::visit(StringExpr& node) {
    return lastValue = builder.CreateGlobalStringPtr(node.value, "", 0, module.get());
}

llvm::Value* CodeGen::visit(CharExpr& node) {
    return lastValue = llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), node.value);
}

llvm::Value* CodeGen::visit(BoolExpr& node) {
    return lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), node.value);
}

llvm::Value* CodeGen::visit(NullExpr& node) {
    return lastValue = llvm::ConstantPointerNull::get(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
}

llvm::Value* CodeGen::visit(VarExpr& node) {
    if (namedValues.count(node.name)) {
        auto& info = namedValues[node.name];
        return lastValue = builder.CreateLoad(info.type, info.ptr, node.name);
    }
    if (node.name == "__name__") {
        return lastValue = builder.CreateGlobalStringPtr("__main__", "", 0, module.get());
    }
    if (node.name == "__arg__") {
        llvm::Type* arrTy = getType("[string]");
        llvm::Value* arr = llvm::UndefValue::get(arrTy);
        llvm::Type* strPtrTy = llvm::PointerType::get(getType("string"), 0);
        arr = builder.CreateInsertValue(arr, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(strPtrTy)), 0);
        arr = builder.CreateInsertValue(arr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), 1);
        arr = builder.CreateInsertValue(arr, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), 2);
        return lastValue = arr;
    }
    if (auto* func = module->getFunction(node.name)) return lastValue = func;
    if (auto* g = module->getGlobalVariable(node.name)) return lastValue = builder.CreateLoad(g->getValueType(), g, node.name);
    
    if (classDecls.count(node.name)) return nullptr; 

    LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Undefined variable: " + node.name);
    return lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
}

llvm::Value* CodeGen::visit(BinaryExpr& node) {
    llvm::Value* L = node.left->accept(*this);
    llvm::Value* R = node.right->accept(*this);
    if (!L || !R) return nullptr;

    // Check for operator overloading
    std::string leftType = node.left->semanticType;
    if (!leftType.empty()) {
        std::string opName = leftType + "_operator" + node.op;
        if (auto* opFunc = module->getFunction(opName)) {
            llvm::Value* selfArg = L;
            if (opFunc->getArg(0)->getType()->isPointerTy() && !L->getType()->isPointerTy()) {
                if (auto* var = dynamic_cast<VarExpr*>(node.left)) {
                    if (namedValues.count(var->name)) {
                        selfArg = namedValues[var->name].ptr;
                    }
                }
                // If not a variable, we might need a temporary alloca
                if (!selfArg->getType()->isPointerTy()) {
                    llvm::AllocaInst* tmp = builder.CreateAlloca(L->getType());
                    builder.CreateStore(L, tmp);
                    selfArg = tmp;
                }
            }
            return lastValue = builder.CreateCall(opFunc, {selfArg, R});
        }
    }

    if (node.op == "+" && L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
        llvm::Function* concat = getOrCreateStringConcat();
        if (!concat) return nullptr;
        return lastValue = builder.CreateCall(concat, {L, R});
    }

    if ((node.op == "==" || node.op == "!=") && L->getType() != R->getType()) {
        auto* nullR = llvm::dyn_cast<llvm::ConstantPointerNull>(R);
        auto* nullL = llvm::dyn_cast<llvm::ConstantPointerNull>(L);
        if (nullR && L->getType()->isStructTy()) {
            llvm::Value* maybePtr = builder.CreateExtractValue(L, 0);
            if (maybePtr->getType()->isPointerTy()) {
                L = maybePtr;
            }
        } else if (nullL && R->getType()->isStructTy()) {
            llvm::Value* maybePtr = builder.CreateExtractValue(R, 0);
            if (maybePtr->getType()->isPointerTy()) {
                R = maybePtr;
            }
        }
    }

    if (L->getType() != R->getType()) {
        if (L->getType()->isFloatingPointTy()) R = builder.CreateSIToFP(R, L->getType());
        else if (R->getType()->isFloatingPointTy()) L = builder.CreateSIToFP(L, R->getType());
        else if (L->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(R)) {
            R = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(L->getType()));
        } else if (R->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(L)) {
            L = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(R->getType()));
        }
        else if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) R = builder.CreateSExt(R, L->getType());
            else L = builder.CreateSExt(L, R->getType());
        }
    }

    bool isFloat = L->getType()->isFloatingPointTy();
    bool isInt = L->getType()->isIntegerTy();

    if (node.op == "+") {
        if (isFloat) return lastValue = builder.CreateFAdd(L, R);
        if (isInt) return lastValue = builder.CreateAdd(L, R);
        LuvError::error(ErrorKind::TYPE_ERROR, "Cannot use '+' on non-primitive types without operator overloading.");
        return nullptr;
    }
    if (node.op == "-") {
        if (isFloat) return lastValue = builder.CreateFSub(L, R);
        if (isInt) return lastValue = builder.CreateSub(L, R);
        return nullptr;
    }
    if (node.op == "*") {
        if (isFloat) return lastValue = builder.CreateFMul(L, R);
        if (isInt) return lastValue = builder.CreateMul(L, R);
        return nullptr;
    }
    if (node.op == "/") {
        if (isFloat) return lastValue = builder.CreateFDiv(L, R);
        if (isInt) {
            generateDivByZeroCheck(R);
            return lastValue = builder.CreateSDiv(L, R);
        }
        return nullptr;
    }
    if (node.op == "==") return lastValue = isFloat ? builder.CreateFCmpOEQ(L, R) : (isInt || L->getType()->isPointerTy() ? builder.CreateICmpEQ(L, R) : nullptr);
    if (node.op == "!=") return lastValue = isFloat ? builder.CreateFCmpONE(L, R) : (isInt || L->getType()->isPointerTy() ? builder.CreateICmpNE(L, R) : nullptr);
    if (node.op == "<") return lastValue = isFloat ? builder.CreateFCmpOLT(L, R) : builder.CreateICmpSLT(L, R);
    if (node.op == "<=") return lastValue = isFloat ? builder.CreateFCmpOLE(L, R) : builder.CreateICmpSLE(L, R);
    if (node.op == ">") return lastValue = isFloat ? builder.CreateFCmpOGT(L, R) : builder.CreateICmpSGT(L, R);
    if (node.op == ">=") return lastValue = isFloat ? builder.CreateFCmpOGE(L, R) : builder.CreateICmpSGE(L, R);
    if (node.op == "%") return lastValue = isFloat ? builder.CreateFRem(L, R) : builder.CreateSRem(L, R);
    if (node.op == "&") return lastValue = builder.CreateAnd(L, R);
    if (node.op == "|") return lastValue = builder.CreateOr(L, R);
    if (node.op == "^") return lastValue = builder.CreateXor(L, R);
    if (node.op == "<<") return lastValue = builder.CreateShl(L, R);
    if (node.op == ">>") return lastValue = builder.CreateAShr(L, R);
    if (node.op == "&&" || node.op == "and") return lastValue = builder.CreateAnd(toBool(L), toBool(R));
    if (node.op == "||" || node.op == "or") return lastValue = builder.CreateOr(toBool(L), toBool(R));

    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(UnaryExpr& node) {
    llvm::Value* V = node.expr->accept(*this);
    if (!V) return nullptr;
    if (node.op == "-") return lastValue = V->getType()->isFloatingPointTy() ? builder.CreateFNeg(V) : builder.CreateNeg(V);
    if (node.op == "!" || node.op == "not") return lastValue = builder.CreateNot(toBool(V));
    if (node.op == "~") return lastValue = builder.CreateNot(V);
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(TernaryExpr& node) {
    llvm::Value* cond = node.condition->accept(*this);
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", F);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "cont");
    
    builder.CreateCondBr(toBool(cond), thenBB, elseBB);
    
    builder.SetInsertPoint(thenBB);
    llvm::Value* thenVal = node.thenExpr->accept(*this);
    builder.CreateBr(mergeBB);
    thenBB = builder.GetInsertBlock();
    
    F->insert(F->end(), elseBB);
    builder.SetInsertPoint(elseBB);
    llvm::Value* elseVal = node.elseExpr->accept(*this);
    builder.CreateBr(mergeBB);
    elseBB = builder.GetInsertBlock();
    
    F->insert(F->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(thenVal->getType(), 2);
    phi->addIncoming(thenVal, thenBB);
    phi->addIncoming(elseVal, elseBB);
    return lastValue = phi;
}

llvm::Value* CodeGen::visit(ComparisonChainExpr& node) {
    if (node.operands.empty()) return lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
    if (node.operators.empty()) return lastValue = node.operands[0]->accept(*this);

    std::cout << "DEBUG: ComparisonChainExpr operators: " << node.operators.size() << " operands: " << node.operands.size() << std::endl;

    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "chain_merge");
    
    llvm::Value* L = node.operands[0]->accept(*this);
    if (!L) return nullptr;

    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiValues;

    for (size_t i = 0; i < node.operators.size(); ++i) {
        std::cout << "DEBUG: ComparisonChainExpr iteration i=" << i << std::endl;
        llvm::Value* R = node.operands[i+1]->accept(*this);
        if (!R) return nullptr;
        std::cout << "DEBUG: ComparisonChainExpr R evaluated for i=" << i << std::endl;
        
        llvm::Value* curL = L;
        llvm::Value* curR = R;

        if (curL->getType() != curR->getType()) {
            if (curL->getType()->isFloatingPointTy() && curR->getType()->isIntegerTy()) {
                curR = builder.CreateSIToFP(curR, curL->getType());
            } else if (curR->getType()->isFloatingPointTy() && curL->getType()->isIntegerTy()) {
                curL = builder.CreateSIToFP(curL, curR->getType());
            } else if (curL->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(curR)) {
                curR = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(curL->getType()));
            } else if (curR->getType()->isPointerTy() && llvm::isa<llvm::ConstantPointerNull>(curL)) {
                curL = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(curR->getType()));
            } else if (curL->getType()->isIntegerTy() && curR->getType()->isIntegerTy()) {
                unsigned w = std::max(curL->getType()->getIntegerBitWidth(), curR->getType()->getIntegerBitWidth());
                llvm::Type* W = llvm::Type::getIntNTy(context, w);
                curL = builder.CreateIntCast(curL, W, true);
                curR = builder.CreateIntCast(curR, W, true);
            }
        }

        llvm::Value* cmp;
        bool isFloat = curL->getType()->isFloatingPointTy();
        if (node.operators[i] == "<") cmp = isFloat ? builder.CreateFCmpOLT(curL, curR) : builder.CreateICmpSLT(curL, curR);
        else if (node.operators[i] == "<=") cmp = isFloat ? builder.CreateFCmpOLE(curL, curR) : builder.CreateICmpSLE(curL, curR);
        else if (node.operators[i] == ">") cmp = isFloat ? builder.CreateFCmpOGT(curL, curR) : builder.CreateICmpSGT(curL, curR);
        else if (node.operators[i] == ">=") cmp = isFloat ? builder.CreateFCmpOGE(curL, curR) : builder.CreateICmpSGE(curL, curR);
        else if (node.operators[i] == "!=") cmp = isFloat ? builder.CreateFCmpONE(curL, curR) : builder.CreateICmpNE(curL, curR);
        else cmp = isFloat ? builder.CreateFCmpOEQ(curL, curR) : builder.CreateICmpEQ(curL, curR);

        if (i + 1 < node.operators.size()) {
            llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "chain_next", F);
            phiValues.push_back({llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), builder.GetInsertBlock()});
            builder.CreateCondBr(cmp, nextBB, mergeBB);
            builder.SetInsertPoint(nextBB);
            L = R;
        } else {
            phiValues.push_back({cmp, builder.GetInsertBlock()});
            builder.CreateBr(mergeBB);
        }
    }

    F->insert(F->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), (unsigned)phiValues.size());
    for (auto& p : phiValues) phi->addIncoming(p.first, p.second);
    return lastValue = phi;
}

llvm::Value* CodeGen::visit(CallExpr& node) {
    if (node.callee == "println" || node.callee == "print") {
        return generatePrint(node.args, node.callee == "println");
    }

    if (classDecls.count(node.callee)) {
        llvm::StructType* ST = classTypes[node.callee];
        uint64_t size = module->getDataLayout().getTypeAllocSize(ST);
        
        llvm::Function* mallocFunc = module->getFunction("malloc");
        if (!mallocFunc) {
            llvm::FunctionType* mallocType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0), {llvm::Type::getInt64Ty(context)}, false);
            mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
        }
        
        llvm::Value* rawPtr = builder.CreateCall(mallocFunc, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), size)});
        llvm::Value* typedPtr = builder.CreateBitCast(rawPtr, llvm::PointerType::get(ST, 0));
        
        // Initialize VTable pointer
        std::string vtableName = node.callee + "_vtable";
        if (auto* vtableGlobal = module->getGlobalVariable(vtableName, true)) {
            llvm::Value* ptr = typedPtr;
            llvm::StructType* currentST = ST;
            while (currentST->getNumElements() > 0 && currentST->getElementType(0)->isStructTy()) {
                ptr = builder.CreateStructGEP(currentST, ptr, 0);
                currentST = llvm::cast<llvm::StructType>(currentST->getElementType(0));
            }
            llvm::Value* vtablePtrAddr = builder.CreateStructGEP(currentST, ptr, 0);
            builder.CreateStore(builder.CreateBitCast(vtableGlobal, llvm::PointerType::get(context, 0)), vtablePtrAddr);
        }

        std::string initName = node.callee + "_init";
        if (auto* initFunc = module->getFunction(initName)) {
            std::vector<llvm::Value*> initArgs;
            initArgs.push_back(typedPtr);
            for (auto* arg : node.args) initArgs.push_back(arg->accept(*this));
            builder.CreateCall(initFunc, initArgs);
        }
        return lastValue = typedPtr;
    }

    llvm::Function* callee = module->getFunction(node.callee);
    if (!callee) {
        // Check for enum variant constructor
        for (auto& pair : enumDecls) {
            for (auto& v : pair.second->variants) {
                if (v.name == node.callee) {
                    callee = module->getFunction(pair.first + "_" + node.callee);
                    if (callee) goto found_callee;
                }
            }
        }
        LuvError::error(ErrorKind::UNDEFINED_FUNCTION, "Unknown function: " + node.callee);
        return nullptr;
    }
found_callee:
    FuncDecl* fd = nullptr;
    if (functionDecls.count(node.callee)) fd = functionDecls[node.callee];
    else if (functionDecls.count(callee->getName().str())) fd = functionDecls[callee->getName().str()];

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node.args.size(); ++i) {
        llvm::Value* argVal = node.args[i]->accept(*this);
        std::string targetType = (fd && i < fd->params.size()) ? fd->params[i].type : "";
        
        if (!targetType.empty() && interfaceNames.count(targetType) && argVal->getType()->isPointerTy()) {
            std::string clsName = node.args[i]->semanticType;
            if (!clsName.empty()) {
                std::string itableName = clsName + "_as_" + targetType + "_itable";
                if (auto* itableGlobal = module->getGlobalVariable(itableName, true)) {
                    llvm::Type* fatPtrTy = getType(targetType);
                    llvm::Value* fatPtr = llvm::UndefValue::get(fatPtrTy);
                    fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(argVal, llvm::PointerType::get(context, 0)), 0);
                    fatPtr = builder.CreateInsertValue(fatPtr, builder.CreateBitCast(itableGlobal, llvm::PointerType::get(context, 0)), 1);
                    argVal = fatPtr;
                }
            }
        }
        args.push_back(argVal);
    }
    
    if (fd && args.size() < fd->params.size()) {
        for (size_t i = args.size(); i < fd->params.size(); ++i) {
            if (fd->params[i].defaultVal) args.push_back(fd->params[i].defaultVal->accept(*this));
            else args.push_back(llvm::Constant::getNullValue(getType(fd->params[i].type)));
        }
    }
    return lastValue = builder.CreateCall(callee, args);
}

llvm::Value* CodeGen::visit(GenericCallExpr& node) {
    std::string mangled = node.callee;
    if (!node.typeArgs.empty()) {
        mangled += "__";
        for (size_t i = 0; i < node.typeArgs.size(); ++i) {
            if (i) mangled += "_";
            mangled += node.typeArgs[i];
        }
    }
    llvm::Function* callee = module->getFunction(mangled);
    if (!callee) callee = module->getFunction(node.callee);
    if (!callee) {
        LuvError::error(ErrorKind::UNDEFINED_FUNCTION, "Unknown generic function: " + node.callee);
        return nullptr;
    }
    std::vector<llvm::Value*> args;
    for (auto* arg : node.args) args.push_back(arg->accept(*this));
    return lastValue = builder.CreateCall(callee, args);
}
llvm::Value* CodeGen::visit(MatchExpr& node) {
    llvm::Value* val = node.value->accept(*this);
    if (!val) return nullptr;

    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* matchEndBB = llvm::BasicBlock::Create(context, "match_end");
    
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiValues;
    llvm::Type* resultTy = getType(node.semanticType);

    for (auto& case_ : node.cases) {
        llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(context, "match_case", F);
        llvm::BasicBlock* nextCaseBB = llvm::BasicBlock::Create(context, "next_case");

        auto oldNamedValues = namedValues;
        if (!case_.pattern) {
            builder.CreateBr(caseBB);
        } else {
            if (!generatePatternMatch(val, case_.pattern, caseBB, nextCaseBB)) {
                builder.CreateBr(nextCaseBB);
            }
        }

        builder.SetInsertPoint(caseBB);
        llvm::Value* res = nullptr;
        if (case_.resultExpr) res = case_.resultExpr->accept(*this);
        else if (case_.resultBlock) res = case_.resultBlock->accept(*this);
        
        if (res && !resultTy->isVoidTy()) {
            if (res->getType() != resultTy) {
                 if (res->getType()->isIntegerTy() && resultTy->isIntegerTy()) res = builder.CreateIntCast(res, resultTy, true);
                 else if (res->getType()->isPointerTy() && resultTy->isPointerTy()) res = builder.CreateBitCast(res, resultTy);
            }
            phiValues.push_back({res, builder.GetInsertBlock()});
        }
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(matchEndBB);
        }
        namedValues = oldNamedValues;

        F->insert(F->end(), nextCaseBB);
        builder.SetInsertPoint(nextCaseBB);
    }

    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(matchEndBB);
    }

    F->insert(F->end(), matchEndBB);
    builder.SetInsertPoint(matchEndBB);
    
    if (phiValues.empty() || resultTy->isVoidTy()) return lastValue = nullptr;
    
    llvm::PHINode* phi = builder.CreatePHI(resultTy, (unsigned)phiValues.size());
    for (auto& p : phiValues) phi->addIncoming(p.first, p.second);
    return lastValue = phi;
}

llvm::Value* CodeGen::visit(IdentifierPattern& node) { return nullptr; }
llvm::Value* CodeGen::visit(TuplePattern& node) { return nullptr; }
llvm::Value* CodeGen::visit(StructPattern& node) { return nullptr; }
llvm::Value* CodeGen::visit(VariantPattern& node) { return nullptr; }
llvm::Value* CodeGen::visit(WildcardPattern& node) { return nullptr; }
llvm::Value* CodeGen::visit(LiteralPattern& node) { return nullptr; }

bool CodeGen::generatePatternMatch(llvm::Value* val, Pattern* pat, llvm::BasicBlock* successBB, llvm::BasicBlock* failBB) {
    if (!pat || dynamic_cast<WildcardPattern*>(pat)) {
        builder.CreateBr(successBB);
        return true;
    }

    if (auto* ipat = dynamic_cast<IdentifierPattern*>(pat)) {
        if (ipat->name != "_" && ipat->name != "ignore") {
            llvm::Type* T = val->getType();
            llvm::AllocaInst* alloca = builder.CreateAlloca(T, nullptr, ipat->name);
            builder.CreateStore(val, alloca);
            namedValues[ipat->name] = {alloca, T, false};
        }
        builder.CreateBr(successBB);
        return true;
    } else if (auto* lpat = dynamic_cast<LiteralPattern*>(pat)) {
        llvm::Value* patVal = lpat->literal->accept(*this);
        llvm::Value* cmp = nullptr;
        if (val->getType()->isIntegerTy()) cmp = builder.CreateICmpEQ(val, patVal);
        else if (val->getType()->isFloatingPointTy()) cmp = builder.CreateFCmpOEQ(val, patVal);
        
        if (cmp) {
            builder.CreateCondBr(cmp, successBB, failBB);
            return true;
        }
    } else if (auto* tpat = dynamic_cast<TuplePattern*>(pat)) {
        if (val->getType()->isStructTy()) {
            llvm::Function* F = builder.GetInsertBlock()->getParent();
            for (size_t i = 0; i < tpat->elements.size(); ++i) {
                llvm::Value* elem = builder.CreateExtractValue(val, {(unsigned)i});
                llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "tuple_pat_next", F);
                if (!generatePatternMatch(elem, tpat->elements[i], nextBB, failBB)) return false;
                builder.SetInsertPoint(nextBB);
            }
            builder.CreateBr(successBB);
            return true;
        }
    } else if (auto* vpat = dynamic_cast<VariantPattern*>(pat)) {
        if (val->getType()->isStructTy()) {
            // Enum match: check tag
            llvm::Value* tag = builder.CreateExtractValue(val, 0);
            int variantIdx = -1;
            EnumDecl* ed = nullptr;
            for (auto& pair : enumDecls) {
                for (size_t i = 0; i < pair.second->variants.size(); ++i) {
                    if (pair.second->variants[i].name == vpat->variantName) {
                        variantIdx = (int)i;
                        ed = pair.second;
                        break;
                    }
                }
                if (ed) break;
            }
            
            if (variantIdx != -1) {
                llvm::Function* F = builder.GetInsertBlock()->getParent();
                llvm::Value* cmp = builder.CreateICmpEQ(tag, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), variantIdx));
                llvm::BasicBlock* tagMatchBB = llvm::BasicBlock::Create(context, "tag_match", F);
                builder.CreateCondBr(cmp, tagMatchBB, failBB);
                
                builder.SetInsertPoint(tagMatchBB);
                // Payload matching
                if (!vpat->elements.empty() && val->getType()->getNumContainedTypes() > 1) {
                    llvm::Value* payload = builder.CreateExtractValue(val, 1);
                    llvm::Type* payloadTy = val->getType()->getContainedType(1);
                    llvm::Value* payloadAlloca = builder.CreateAlloca(payloadTy);
                    builder.CreateStore(payload, payloadAlloca);
                    
                    std::vector<llvm::Type*> vFields;
                    for (const auto& t : ed->variants[variantIdx].types) vFields.push_back(getType(t));
                    llvm::StructType* VS = llvm::StructType::get(context, vFields);
                    
                    llvm::Value* typedPayloadPtr = builder.CreateBitCast(payloadAlloca, llvm::PointerType::get(context, 0));
                    llvm::Value* typedVSPtr = builder.CreateBitCast(typedPayloadPtr, llvm::PointerType::get(VS, 0));
                    llvm::Value* vsVal = builder.CreateLoad(VS, typedVSPtr);
                    
                    for (size_t j = 0; j < vpat->elements.size(); ++j) {
                        llvm::Value* elem = builder.CreateExtractValue(vsVal, {(unsigned)j});
                        llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "variant_pat_next", F);
                        if (!generatePatternMatch(elem, vpat->elements[j], nextBB, failBB)) return false;
                        builder.SetInsertPoint(nextBB);
                    }
                }
                builder.CreateBr(successBB);
                return true;
            }
        }
    } else if (auto* spat = dynamic_cast<StructPattern*>(pat)) {
        if (val->getType()->isStructTy()) {
            std::string typeName = spat->structName;
            if (typeName.empty()) typeName = pat->semanticType;
            if (!typeName.empty() && structDecls.count(typeName)) {
                auto* sd = structDecls[typeName];
                llvm::Function* F = builder.GetInsertBlock()->getParent();
                for (auto& f : spat->fields) {
                    int idx = -1;
                    for (size_t i = 0; i < sd->fields.size(); ++i) {
                        if (sd->fields[i].name == f.first) { idx = (int)i; break; }
                    }
                    if (idx != -1) {
                        llvm::Value* elem = builder.CreateExtractValue(val, {(unsigned)idx});
                        llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "struct_pat_next", F);
                        if (!generatePatternMatch(elem, f.second, nextBB, failBB)) return false;
                        builder.SetInsertPoint(nextBB);
                    }
                }
                builder.CreateBr(successBB);
                return true;
            }
        }
    }
    
    return false;
}

void CodeGen::generatePatternDestructuring(llvm::Value* val, Pattern* pat, bool isMut, const std::string& baseType) {
    if (!pat) return;

    if (auto* ipat = dynamic_cast<IdentifierPattern*>(pat)) {
        if (ipat->name == "_" || ipat->name == "ignore") return;
        llvm::Type* T = val->getType();
        llvm::AllocaInst* alloca = builder.CreateAlloca(T, nullptr, ipat->name);
        builder.CreateStore(val, alloca);
        namedValues[ipat->name] = {alloca, T, isMut};
        varSemanticTypes[ipat->name] = pat->semanticType.empty() ? baseType : pat->semanticType;
    } else if (auto* tpat = dynamic_cast<TuplePattern*>(pat)) {
        if (val->getType()->isStructTy()) {
            for (size_t i = 0; i < tpat->elements.size(); ++i) {
                llvm::Value* elem = builder.CreateExtractValue(val, {(unsigned)i});
                generatePatternDestructuring(elem, tpat->elements[i], isMut, "");
            }
        }
    } else if (auto* spat = dynamic_cast<StructPattern*>(pat)) {
        std::string typeName = spat->structName;
        if (typeName.empty()) typeName = baseType;
        if (!typeName.empty() && structDecls.count(typeName)) {
             auto* sd = structDecls[typeName];
             for (auto& f : spat->fields) {
                 for (size_t i = 0; i < sd->fields.size(); ++i) {
                     if (sd->fields[i].name == f.first) {
                         llvm::Value* elem = builder.CreateExtractValue(val, {(unsigned)i});
                         generatePatternDestructuring(elem, f.second, isMut, sd->fields[i].type);
                         break;
                     }
                 }
             }
        }
    }
}

llvm::Value* CodeGen::visit(StructInstExpr& node) {
    llvm::StructType* ST = structTypes[node.structName];
    if (!ST) {
        LuvError::error(ErrorKind::UNDEFINED_VARIABLE, "Unknown struct: " + node.structName);
        return nullptr;
    }
    
    llvm::Value* structVal = llvm::UndefValue::get(ST);
    auto* sd = structDecls[node.structName];
    if (!sd) return nullptr;

    for (const auto& field : node.fields) {
        int idx = -1;
        for (size_t i = 0; i < sd->fields.size(); ++i) {
            if (sd->fields[i].name == field.first) { idx = (int)i; break; }
        }
        if (idx != -1) {
            llvm::Value* val = field.second->accept(*this);
            if (val) {
                // Type check/cast
                llvm::Type* expectedTy = ST->getElementType(idx);
                if (val->getType() != expectedTy) {
                    if (val->getType()->isIntegerTy() && expectedTy->isIntegerTy()) {
                        val = builder.CreateIntCast(val, expectedTy, true);
                    } else if (val->getType()->isPointerTy() && expectedTy->isPointerTy()) {
                        val = builder.CreateBitCast(val, expectedTy);
                    }
                }
                structVal = builder.CreateInsertValue(structVal, val, {(unsigned)idx});
            }
        }
    }
    return lastValue = structVal;
}

} // namespace luv

#include "CodeGen.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constant.h>
#include <functional>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::visit(ArrayExpr& node) {
    std::vector<llvm::Value*> elements;
    for (auto* e : node.elements) {
        elements.push_back(e->accept(*this));
    }

    llvm::Type* elemType = elements.empty() ? llvm::Type::getInt64Ty(context) : elements[0]->getType();
    uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);

    llvm::Value* len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), elements.size());
    llvm::Value* cap = len;

    // Allocate data
    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        llvm::FunctionType* mallocType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0), {llvm::Type::getInt64Ty(context)}, false);
        mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    }

    llvm::Value* totalSize = builder.CreateMul(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), elemSize));
    llvm::Value* dataPtrRaw = builder.CreateCall(mallocFunc, {totalSize});
    llvm::Value* dataPtr = builder.CreateBitCast(dataPtrRaw, llvm::PointerType::get(elemType, 0));

    for (unsigned i = 0; i < elements.size(); ++i) {
        llvm::Value* idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i);
        llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, idx);
        builder.CreateStore(elements[i], elementPtr);
    }

    // Create array struct: {T* data, i64 len, i64 cap}
    llvm::StructType* arrayStructType = llvm::StructType::get(context, {
        llvm::PointerType::get(elemType, 0),
        llvm::Type::getInt64Ty(context),
        llvm::Type::getInt64Ty(context)
    });

    llvm::Value* arrayStruct = llvm::UndefValue::get(arrayStructType);
    arrayStruct = builder.CreateInsertValue(arrayStruct, dataPtr, 0);
    arrayStruct = builder.CreateInsertValue(arrayStruct, len, 1);
    arrayStruct = builder.CreateInsertValue(arrayStruct, cap, 2);

    return lastValue = arrayStruct;
}

llvm::Value* CodeGen::visit(ArrayRepeatExpr& node) {
    llvm::Value* val = node.value->accept(*this);
    llvm::Value* count = node.count->accept(*this);
    llvm::Type* elemType = val->getType();
    uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);

    // Allocate data
    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        llvm::FunctionType* mallocType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0), {llvm::Type::getInt64Ty(context)}, false);
        mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    }

    llvm::Value* totalSize = builder.CreateMul(count, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), elemSize));
    llvm::Value* dataPtrRaw = builder.CreateCall(mallocFunc, {totalSize});
    llvm::Value* dataPtr = builder.CreateBitCast(dataPtrRaw, llvm::PointerType::get(elemType, 0));

    // Fill with value using a loop
    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "fillcond", F);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "fillbody");
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "fillcont");

    llvm::Value* iPtr = builder.CreateAlloca(llvm::Type::getInt64Ty(context));
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), iPtr);
    builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    llvm::Value* i = builder.CreateLoad(llvm::Type::getInt64Ty(context), iPtr);
    llvm::Value* cond = builder.CreateICmpSLT(i, count);
    builder.CreateCondBr(cond, bodyBB, contBB);

    F->insert(F->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);
    llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, i);
    builder.CreateStore(val, elementPtr);
    builder.CreateStore(builder.CreateAdd(i, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)), iPtr);
    builder.CreateBr(condBB);

    F->insert(F->end(), contBB);
    builder.SetInsertPoint(contBB);

    // Create array struct
    llvm::StructType* arrayStructType = llvm::StructType::get(context, {
        llvm::PointerType::get(elemType, 0),
        llvm::Type::getInt64Ty(context),
        llvm::Type::getInt64Ty(context)
    });

    llvm::Value* arrayStruct = llvm::UndefValue::get(arrayStructType);
    arrayStruct = builder.CreateInsertValue(arrayStruct, dataPtr, 0);
    arrayStruct = builder.CreateInsertValue(arrayStruct, count, 1);
    arrayStruct = builder.CreateInsertValue(arrayStruct, count, 2);

    return lastValue = arrayStruct;
}

llvm::Value* CodeGen::visit(ArrayCompExpr& node) {
    std::string semType = node.semanticType; // "[T]"
    llvm::Type* elemType = llvm::Type::getInt64Ty(context);
    if (semType.size() > 2 && semType.front() == '[' && semType.back() == ']') {
        elemType = getType(semType.substr(1, semType.size() - 2));
    }

    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        llvm::FunctionType* mallocType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0), {llvm::Type::getInt64Ty(context)}, false);
        mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    }
    uint64_t elemSize = module->getDataLayout().getTypeAllocSize(elemType);

    llvm::Function* F = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "compcond", F);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "compbody");
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(context, "compcont");

    if (node.rangeEnd) {
        // [expr for i in start..end]
        llvm::Value* start = node.iterable->accept(*this);
        llvm::Value* end = node.rangeEnd->accept(*this);
        
        llvm::Value* len;
        if (node.inclusive) len = builder.CreateAdd(builder.CreateSub(end, start), llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1));
        else len = builder.CreateSub(end, start);
        
        llvm::Value* isPos = builder.CreateICmpSGT(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        len = builder.CreateSelect(isPos, len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        
        llvm::Value* totalSize = builder.CreateMul(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), elemSize));
        llvm::Value* dataPtrRaw = builder.CreateCall(mallocFunc, {totalSize});
        llvm::Value* dataPtr = builder.CreateBitCast(dataPtrRaw, llvm::PointerType::get(elemType, 0));
        
        llvm::AllocaInst* iPtr = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, node.varName);
        builder.CreateStore(start, iPtr);
        
        llvm::AllocaInst* idxPtr = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "idx");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), idxPtr);
        
        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        llvm::Value* i = builder.CreateLoad(llvm::Type::getInt64Ty(context), iPtr);
        llvm::Value* cond = node.inclusive ? builder.CreateICmpSLE(i, end) : builder.CreateICmpSLT(i, end);
        builder.CreateCondBr(cond, bodyBB, contBB);
        
        F->insert(F->end(), bodyBB);
        builder.SetInsertPoint(bodyBB);
        
        auto oldNamedValues = namedValues;
        namedValues[node.varName] = {iPtr, llvm::Type::getInt64Ty(context), false};
        
        llvm::Value* val = node.expr->accept(*this);
        llvm::Value* idx = builder.CreateLoad(llvm::Type::getInt64Ty(context), idxPtr);
        llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, idx);
        builder.CreateStore(val, elementPtr);
        
        builder.CreateStore(builder.CreateAdd(i, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)), iPtr);
        builder.CreateStore(builder.CreateAdd(idx, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)), idxPtr);
        builder.CreateBr(condBB);
        
        F->insert(F->end(), contBB);
        builder.SetInsertPoint(contBB);
        namedValues = oldNamedValues;
        
        llvm::StructType* arrayStructType = llvm::StructType::get(context, {
            llvm::PointerType::get(elemType, 0),
            llvm::Type::getInt64Ty(context),
            llvm::Type::getInt64Ty(context)
        });
        llvm::Value* arrayStruct = llvm::UndefValue::get(arrayStructType);
        arrayStruct = builder.CreateInsertValue(arrayStruct, dataPtr, 0);
        arrayStruct = builder.CreateInsertValue(arrayStruct, len, 1);
        arrayStruct = builder.CreateInsertValue(arrayStruct, len, 2);
        return lastValue = arrayStruct;
    } else {
        // [expr for i in iterable]
        llvm::Value* iterable = node.iterable->accept(*this);
        if (!iterable || !iterable->getType()->isStructTy()) return nullptr;
        
        llvm::Value* dataPtrIn = builder.CreateExtractValue(iterable, 0);
        llvm::Value* len = builder.CreateExtractValue(iterable, 1);
        
        llvm::Type* inElemType = llvm::Type::getInt64Ty(context);
        std::string inSemType = node.iterable->semanticType;
        if (inSemType.size() >= 2 && inSemType.front() == '[' && inSemType.back() == ']') {
            inElemType = getType(inSemType.substr(1, inSemType.size() - 2));
        }

        llvm::Value* totalSize = builder.CreateMul(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), elemSize));
        llvm::Value* dataPtrRaw = builder.CreateCall(mallocFunc, {totalSize});
        llvm::Value* dataPtr = builder.CreateBitCast(dataPtrRaw, llvm::PointerType::get(elemType, 0));

        llvm::AllocaInst* idxPtr = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "idx");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), idxPtr);
        
        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        llvm::Value* idx = builder.CreateLoad(llvm::Type::getInt64Ty(context), idxPtr);
        llvm::Value* cond = builder.CreateICmpSLT(idx, len);
        builder.CreateCondBr(cond, bodyBB, contBB);
        
        F->insert(F->end(), bodyBB);
        builder.SetInsertPoint(bodyBB);
        
        llvm::Value* inElementPtr = builder.CreateGEP(inElemType, dataPtrIn, idx);
        llvm::Value* inElement = builder.CreateLoad(inElemType, inElementPtr);
        
        auto oldNamedValues = namedValues;
        llvm::AllocaInst* varAlloca = builder.CreateAlloca(inElemType, nullptr, node.varName);
        builder.CreateStore(inElement, varAlloca);
        namedValues[node.varName] = {varAlloca, inElemType, false};
        
        llvm::Value* val = node.expr->accept(*this);
        llvm::Value* outElementPtr = builder.CreateGEP(elemType, dataPtr, idx);
        builder.CreateStore(val, outElementPtr);
        
        builder.CreateStore(builder.CreateAdd(idx, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)), idxPtr);
        builder.CreateBr(condBB);
        
        F->insert(F->end(), contBB);
        builder.SetInsertPoint(contBB);
        namedValues = oldNamedValues;

        llvm::StructType* arrayStructType = llvm::StructType::get(context, {
            llvm::PointerType::get(elemType, 0),
            llvm::Type::getInt64Ty(context),
            llvm::Type::getInt64Ty(context)
        });
        llvm::Value* arrayStruct = llvm::UndefValue::get(arrayStructType);
        arrayStruct = builder.CreateInsertValue(arrayStruct, dataPtr, 0);
        arrayStruct = builder.CreateInsertValue(arrayStruct, len, 1);
        arrayStruct = builder.CreateInsertValue(arrayStruct, len, 2);
        return lastValue = arrayStruct;
    }
}

llvm::Value* CodeGen::visit(TupleExpr& node) {
    std::vector<llvm::Value*> elements;
    std::vector<llvm::Type*> elementTypes;
    for (auto* e : node.elements) {
        llvm::Value* val = e->accept(*this);
        elements.push_back(val);
        elementTypes.push_back(val->getType());
    }
    
    llvm::StructType* ST = llvm::StructType::get(context, elementTypes);
    llvm::Value* tupValue = llvm::UndefValue::get(ST);
    for (unsigned i = 0; i < elements.size(); ++i) {
        tupValue = builder.CreateInsertValue(tupValue, elements[i], i);
    }
    return lastValue = tupValue;
}

llvm::Value* CodeGen::visit(IndexExpr& node) {
    llvm::Value* target = node.target->accept(*this);
    llvm::Value* index = node.index->accept(*this);
    if (!target || !index) return nullptr;

    if (target->getType()->isStructTy()) {
        llvm::StructType* ST = llvm::cast<llvm::StructType>(target->getType());
        if (ST->getNumElements() == 3 || ST->getNumElements() == 2) {
            llvm::Value* dataPtr = builder.CreateExtractValue(target, 0);
            llvm::Value* len = builder.CreateExtractValue(target, 1);
            generateBoundsCheck(index, len);
            
            llvm::Type* elemType = llvm::Type::getInt64Ty(context);
            std::string semType = node.target->semanticType;
            if (semType.size() >= 2 && semType.front() == '[' && semType.back() == ']') {
                elemType = getType(semType.substr(1, semType.size() - 2));
            }
            
            llvm::Value* elementPtr = builder.CreateGEP(elemType, dataPtr, index);
            return lastValue = builder.CreateLoad(elemType, elementPtr);
        }
        if (auto* cidx = llvm::dyn_cast<llvm::ConstantInt>(index)) {
            uint64_t i = cidx->getZExtValue();
            if (i < ST->getNumElements()) {
                return lastValue = builder.CreateExtractValue(target, {(unsigned)i});
            }
        }
    }
    
    return nullptr;
}

llvm::Value* CodeGen::visit(SliceExpr& node) {
    llvm::Value* target = node.target->accept(*this);
    llvm::Value* start = node.start->accept(*this);
    llvm::Value* end = node.end ? node.end->accept(*this) : nullptr;
    
    if (!target || !target->getType()->isStructTy()) return nullptr;
    
    llvm::Value* dataPtr = builder.CreateExtractValue(target, 0);
    
    llvm::Type* elemType = llvm::Type::getInt64Ty(context);
    std::string semType = node.target->semanticType;
    if (semType.size() >= 2 && semType.front() == '[' && semType.back() == ']') {
        elemType = getType(semType.substr(1, semType.size() - 2));
    }

    if (!end) {
        end = builder.CreateExtractValue(target, 1); // target.len
    }
    
    llvm::Value* sliceLen;
    if (node.inclusive) {
        sliceLen = builder.CreateAdd(builder.CreateSub(end, start), llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1));
    } else {
        sliceLen = builder.CreateSub(end, start);
    }
    
    llvm::Value* sliceDataPtr = builder.CreateGEP(elemType, dataPtr, start);
    
    // Slice is {T* data, i64 len}
    llvm::StructType* sliceStructType = llvm::StructType::get(context, {
        dataPtr->getType(),
        llvm::Type::getInt64Ty(context)
    });
    
    llvm::Value* sliceStruct = llvm::UndefValue::get(sliceStructType);
    sliceStruct = builder.CreateInsertValue(sliceStruct, sliceDataPtr, 0);
    sliceStruct = builder.CreateInsertValue(sliceStruct, sliceLen, 1);
    
    return lastValue = sliceStruct;
}

llvm::Value* CodeGen::visit(PostfixExpr& node) {
    auto apply = [&](llvm::Value* oldVal) -> llvm::Value* {
        llvm::Value* one = oldVal->getType()->isFloatingPointTy()
            ? (llvm::Value*)llvm::ConstantFP::get(oldVal->getType(), 1.0)
            : (llvm::Value*)llvm::ConstantInt::get(oldVal->getType(), 1);
        if (node.op == "++") {
            return oldVal->getType()->isFloatingPointTy() ? builder.CreateFAdd(oldVal, one) : builder.CreateAdd(oldVal, one);
        }
        return oldVal->getType()->isFloatingPointTy() ? builder.CreateFSub(oldVal, one) : builder.CreateSub(oldVal, one);
    };

    if (auto* var = dynamic_cast<VarExpr*>(node.expr)) {
        if (!namedValues.count(var->name)) return nullptr;
        auto& info = namedValues[var->name];
        llvm::Value* oldVal = builder.CreateLoad(info.type, info.ptr);
        llvm::Value* newVal = apply(oldVal);
        builder.CreateStore(newVal, info.ptr);
        return lastValue = oldVal;
    }

    if (auto* prop = dynamic_cast<PropertyExpr*>(node.expr)) {
        llvm::Value* obj = prop->object->accept(*this);
        if (!obj) return nullptr;
        std::string clsName = prop->object->semanticType;
        if (!classDecls.count(clsName) || !classTypes.count(clsName)) return nullptr;
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
        if (!fieldPtr || !fieldTy) return nullptr;
        llvm::Value* oldVal = builder.CreateLoad(fieldTy, fieldPtr);
        llvm::Value* newVal = apply(oldVal);
        builder.CreateStore(newVal, fieldPtr);
        return lastValue = oldVal;
    }

    return nullptr;
}

} // namespace luv

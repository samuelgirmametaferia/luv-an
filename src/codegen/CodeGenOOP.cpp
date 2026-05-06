#include "CodeGen.h"
#include <llvm/IR/DerivedTypes.h>
#include <functional>
#include <iostream>

namespace luv {

llvm::Value* CodeGen::visit(StructDecl& node) {
    // Pass 1 already handled struct type creation
    return nullptr;
}

llvm::Value* CodeGen::visit(ClassDecl& node) {
    // Pass 2 already handled class struct type creation
    
    bool hasBase = !node.baseAndInterfaces.empty() && classDecls.count(node.baseAndInterfaces[0]);

    // ── VTable Layout ──
    std::vector<std::string> layout;
    if (hasBase) {
        layout = vtableLayouts[node.baseAndInterfaces[0]];
    }

    // Add/Override methods in layout
    for (auto* m : node.methods) {
        if (m->isStatic) continue;
        std::string methodName = m->name;
        size_t underscore = methodName.find("_");
        if (underscore != std::string::npos) {
            methodName = methodName.substr(underscore + 1);
        }
        
        bool found = false;
        for (auto& existing : layout) {
            if (existing == methodName) { found = true; break; }
        }
        if (!found) layout.push_back(methodName);
    }
    vtableLayouts[node.name] = layout;

    // Create VTable struct type
    std::vector<llvm::Type*> vtableFieldTypes;
    for (const auto& mName : layout) {
        vtableFieldTypes.push_back(llvm::PointerType::get(context, 0));
    }
    llvm::StructType* VTableST = llvm::StructType::create(context, vtableFieldTypes, node.name + "_vtable_type");
    vtableTypes[node.name] = VTableST;

    // Register methods (without body yet)
    for (auto* m : node.methods) {
        m->boundStruct = node.name;
        // Don't call accept here, just prepare for Pass 4
    }

    // Now create the actual VTable global constant
    // Since Pass 4 hasn't run yet, module->getFunction might return NULL for methods in THIS class.
    // We should use an IR-only constant or defer VTable generation.
    // Actually, we can generate method declarations in Pass 4 then build VTables.
    // But Pass 3 is VTables. 
    
    // Let's use getOrInsertFunction to get a pointer to the function even if it's not defined yet.
    std::vector<llvm::Constant*> vtableEntries;
    for (const auto& mName : layout) {
        std::string searchClass = node.name;
        llvm::Function* F = nullptr;
        while (!searchClass.empty()) {
            std::string mangledName = searchClass + "_" + mName;
            F = module->getFunction(mangledName);
            if (F) break;
            
            // If not found in module, check if it's a method in the class decls
            if (classDecls.count(searchClass)) {
                for (auto* m : classDecls[searchClass]->methods) {
                    std::string mm = m->name;
                    size_t underscore = mm.find("_");
                    if (underscore != std::string::npos) mm = mm.substr(underscore + 1);
                    if (mm == mName) {
                        // Method found, get or insert it
                        std::vector<llvm::Type*> argTypes;
                        argTypes.push_back(llvm::PointerType::get(classTypes[searchClass], 0));
                        for (auto& p : m->params) argTypes.push_back(getType(p.type));
                        llvm::FunctionType* FT = llvm::FunctionType::get(getType(m->returnType), argTypes, false);
                        F = llvm::cast<llvm::Function>(module->getOrInsertFunction(searchClass + "_" + mName, FT).getCallee());
                        break;
                    }
                }
            }
            if (F) break;

            if (classDecls.count(searchClass) && !classDecls[searchClass]->baseAndInterfaces.empty()) {
                searchClass = classDecls[searchClass]->baseAndInterfaces[0];
            } else {
                searchClass = "";
            }
        }
        
        if (F) {
            vtableEntries.push_back(llvm::ConstantExpr::getBitCast(F, llvm::PointerType::get(context, 0)));
        } else {
            vtableEntries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)));
        }
    }
    
    llvm::Constant* vtableConst = llvm::ConstantStruct::get(VTableST, vtableEntries);
    new llvm::GlobalVariable(*module, VTableST, true, llvm::GlobalValue::InternalLinkage, vtableConst, node.name + "_vtable");

    // ── ITable Generation ──
    std::set<std::string> allInterfaces;
    std::function<void(const std::string&)> collectInterfaces = [&](const std::string& clsName) {
        if (!classDecls.count(clsName)) return;
        auto* decl = classDecls[clsName];
        for (const auto& bi : decl->baseAndInterfaces) {
            if (interfaceNames.count(bi)) allInterfaces.insert(bi);
            else collectInterfaces(bi);
        }
    };
    collectInterfaces(node.name);

    for (const auto& baseName : allInterfaces) {
        if (interfaceLayouts.count(baseName)) {
            auto& iLayout = interfaceLayouts[baseName];
            std::vector<llvm::Constant*> itableEntries;
            
            for (const auto& mName : iLayout) {
                // Find implementation
                std::string searchClass = node.name;
                llvm::Function* realF = nullptr;
                // Reuse the same lookup logic as VTable
                // ...
                // For simplicity, let's just use the vtable lookup we just did
                // but we need the actual Function pointer.
                // We'll search again.
                while (!searchClass.empty()) {
                    realF = module->getFunction(searchClass + "_" + mName);
                    if (!realF && classDecls.count(searchClass)) {
                         for (auto* m : classDecls[searchClass]->methods) {
                            std::string mm = m->name;
                            size_t underscore = mm.find("_");
                            if (underscore != std::string::npos) mm = mm.substr(underscore + 1);
                            if (mm == mName) {
                                std::vector<llvm::Type*> argTypes;
                                argTypes.push_back(llvm::PointerType::get(classTypes[searchClass], 0));
                                for (auto& p : m->params) argTypes.push_back(getType(p.type));
                                llvm::FunctionType* FT = llvm::FunctionType::get(getType(m->returnType), argTypes, false);
                                realF = llvm::cast<llvm::Function>(module->getOrInsertFunction(searchClass + "_" + mName, FT).getCallee());
                                break;
                            }
                         }
                    }
                    if (realF) break;
                    if (classDecls.count(searchClass) && !classDecls[searchClass]->baseAndInterfaces.empty())
                        searchClass = classDecls[searchClass]->baseAndInterfaces[0];
                    else searchClass = "";
                }

                if (realF) {
                    // Create thunk: fn thunk({i8*, i8*} self, args...)
                    std::string thunkName = node.name + "_as_" + baseName + "_" + mName + "_thunk";
                    llvm::StructType* fatPtrTy = llvm::StructType::get(context, {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)});
                    
                    std::vector<llvm::Type*> thunkArgTypes;
                    thunkArgTypes.push_back(fatPtrTy);
                    for (size_t i = 1; i < realF->arg_size(); ++i) thunkArgTypes.push_back(realF->getArg(i)->getType());
                    
                    llvm::FunctionType* thunkFT = llvm::FunctionType::get(realF->getReturnType(), thunkArgTypes, false);
                    llvm::Function* thunkF = llvm::Function::Create(thunkFT, llvm::Function::InternalLinkage, thunkName, *module);
                    
                    llvm::BasicBlock* thunkBB = llvm::BasicBlock::Create(context, "entry", thunkF);
                    llvm::IRBuilder<> thunkBuilder(thunkBB);
                    
                    llvm::Value* objPtrRaw = thunkBuilder.CreateExtractValue(thunkF->getArg(0), 0);
                    llvm::Value* objPtr = thunkBuilder.CreateBitCast(objPtrRaw, realF->getArg(0)->getType());
                    
                    std::vector<llvm::Value*> thunkArgs;
                    thunkArgs.push_back(objPtr);
                    for (size_t i = 1; i < thunkF->arg_size(); ++i) thunkArgs.push_back(thunkF->getArg(i));
                    
                    llvm::Value* res = thunkBuilder.CreateCall(realF, thunkArgs);
                    if (thunkF->getReturnType()->isVoidTy()) thunkBuilder.CreateRetVoid();
                    else thunkBuilder.CreateRet(res);
                    
                    itableEntries.push_back(llvm::ConstantExpr::getBitCast(thunkF, llvm::PointerType::get(context, 0)));
                } else {
                    itableEntries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)));
                }
            }
            
            llvm::StructType* ITableST = vtableTypes[baseName];
            llvm::Constant* itableConst = llvm::ConstantStruct::get(ITableST, itableEntries);
            new llvm::GlobalVariable(*module, ITableST, true, llvm::GlobalValue::InternalLinkage, itableConst, node.name + "_as_" + baseName + "_itable");
        }
    }

    return nullptr;
}

llvm::Value* CodeGen::visit(EnumDecl& node) {
    llvm::StructType* ET = enumTypes[node.name];
    if (!ET) return nullptr;

    for (size_t i = 0; i < node.variants.size(); ++i) {
        auto& v = node.variants[i];
        std::vector<llvm::Type*> argTypes;
        for (auto& t : v.types) argTypes.push_back(getType(t));
        
        llvm::FunctionType* FT = llvm::FunctionType::get(ET, argTypes, false);
        std::string vName = node.name + "_" + v.name;
        llvm::Function* F = module->getFunction(vName);
        if (F && !F->empty()) continue;
        if (!F) F = llvm::Function::Create(FT, llvm::Function::InternalLinkage, vName, *module);
        
        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", F);
        auto* oldBB = builder.GetInsertBlock();
        builder.SetInsertPoint(entry);
        
        llvm::Value* enumVal = llvm::UndefValue::get(ET);
        enumVal = builder.CreateInsertValue(enumVal, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (uint32_t)i), 0);
        
        if (!v.types.empty() && ET->getNumElements() > 1) {
             llvm::Type* payloadTy = ET->getElementType(1);
             llvm::Value* payloadAlloca = builder.CreateAlloca(payloadTy);
             
             std::vector<llvm::Type*> vFields;
             for (const auto& t : v.types) vFields.push_back(getType(t));
             llvm::StructType* VS = llvm::StructType::get(context, vFields);
             
             llvm::Value* vsVal = llvm::UndefValue::get(VS);
             for (unsigned j = 0; j < (unsigned)v.types.size(); ++j) {
                 vsVal = builder.CreateInsertValue(vsVal, F->getArg(j), {j});
             }
             
             llvm::Value* typedPayloadPtr = builder.CreateBitCast(payloadAlloca, llvm::PointerType::get(context, 0));
             llvm::Value* typedVSPtr = builder.CreateBitCast(typedPayloadPtr, llvm::PointerType::get(VS, 0));
             builder.CreateStore(vsVal, typedVSPtr);
             
             llvm::Value* payloadVal = builder.CreateLoad(payloadTy, payloadAlloca);
             enumVal = builder.CreateInsertValue(enumVal, payloadVal, 1);
        }
        
        builder.CreateRet(enumVal);
        if (oldBB) builder.SetInsertPoint(oldBB);
    }
    return nullptr;
}

llvm::Value* CodeGen::visit(PropertyExpr& node) {
    llvm::Value* obj = node.object->accept(*this);
    if (!obj) return nullptr;
    
    llvm::Type* objType = obj->getType();
    
    if (objType->isStructTy()) {
        llvm::StructType* ST = llvm::cast<llvm::StructType>(objType);
        if (node.propertyName == "len") {
            if (ST->getNumElements() >= 2) return lastValue = builder.CreateExtractValue(obj, 1);
        }
        if (node.propertyName == "cap") {
            if (ST->getNumElements() >= 3) return lastValue = builder.CreateExtractValue(obj, 2);
            if (ST->getNumElements() == 2) return lastValue = builder.CreateExtractValue(obj, 1);
        }
    }

    std::string typeName = node.object->semanticType;
    if (typeName.empty()) {
        if (auto* v = dynamic_cast<VarExpr*>(node.object)) {
            auto it = varSemanticTypes.find(v->name);
            if (it != varSemanticTypes.end()) typeName = it->second;
        }
    }
    
    if (classTypes.count(typeName) && classDecls.count(typeName)) {
        llvm::Value* clsPtr = obj;
        llvm::Type* expectedPtrTy = llvm::PointerType::get(classTypes[typeName], 0);
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
                const std::string& base = decl->baseAndInterfaces[0];
                llvm::Value* basePtrField = builder.CreateStructGEP(classTypes[className], basePtr, 0);
                return buildFieldPtr(base, basePtrField, fieldName);
            }
            return {nullptr, nullptr};
        };

        auto [ptr, fieldTy] = buildFieldPtr(typeName, clsPtr, node.propertyName);
        if (ptr && fieldTy) {
            return lastValue = builder.CreateLoad(fieldTy, ptr);
        }
    }

    if (structDecls.count(typeName)) {
        llvm::Value* ptr = obj;
        if (objType->isPointerTy()) {
            ptr = obj;
        } else {
            // If it's a value, we can extract it, but for simplicity let's handle pointers too
        }

        auto* sd = structDecls[typeName];
        for (size_t i = 0; i < sd->fields.size(); ++i) {
            if (sd->fields[i].name == node.propertyName) {
                if (objType->isPointerTy()) {
                    llvm::Value* fieldPtr = builder.CreateStructGEP(structTypes[typeName], ptr, (unsigned)i);
                    return lastValue = builder.CreateLoad(getType(sd->fields[i].type), fieldPtr);
                } else {
                    return lastValue = builder.CreateExtractValue(obj, {(unsigned)i});
                }
            }
        }
    }

    return nullptr;
}

llvm::Value* CodeGen::visit(MethodCallExpr& node) {
    if (auto* var = dynamic_cast<VarExpr*>(node.object)) {
        if (var->name == "Super") {
            SuperCallExpr sc(node.methodName, node.args);
            return visit(sc);
        }
    }

    if (node.object->semanticType == "generic") {
        std::string mangledName = node.methodName;
        for (auto* arg : node.args) {
            mangledName += "_" + arg->semanticType;
        }
        llvm::Function* SF = module->getFunction(mangledName);
        if (SF) {
            std::vector<llvm::Value*> args;
            for (auto* arg : node.args) args.push_back(arg->accept(*this));
            return lastValue = builder.CreateCall(SF, args);
        }
    }

    llvm::Value* preObj = node.object->accept(*this);
    if (node.methodName == "toString") {
        return lastValue = generateToString(preObj);
    }

    std::string typeName = node.object->semanticType;
    if (typeName.empty()) {
        if (auto* v = dynamic_cast<VarExpr*>(node.object)) {
            auto it = varSemanticTypes.find(v->name);
            if (it != varSemanticTypes.end()) typeName = it->second;
        }
    }

    if (typeName == "string" || (typeName.empty() && 
        (node.methodName == "length" || node.methodName == "toUpper" || node.methodName == "toLower"))) {
        llvm::Value* obj = preObj ? preObj : node.object->accept(*this);
        if (!obj) return nullptr;
        std::string fnName;
        if (node.methodName == "length") fnName = "string_length";
        else if (node.methodName == "toUpper") fnName = "string_toUpper";
        else if (node.methodName == "toLower") fnName = "string_toLower";
        else if (node.methodName == "trim") fnName = "string_trim";
        else if (node.methodName == "contains") fnName = "string_contains";
        else if (node.methodName == "startsWith") fnName = "string_startsWith";
        else if (node.methodName == "endsWith") fnName = "string_endsWith";
        else if (node.methodName == "indexOf") fnName = "string_indexOf";
        else if (node.methodName == "replace") fnName = "string_replace";

        if (!fnName.empty()) {
            std::vector<llvm::Type*> argTypes{llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)};
            for (size_t i = 0; i < node.args.size(); ++i) {
                argTypes.push_back(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
            }
            llvm::Type* retTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            if (node.methodName == "length" || node.methodName == "indexOf") retTy = llvm::Type::getInt32Ty(context);
            if (node.methodName == "contains" || node.methodName == "startsWith" || node.methodName == "endsWith") retTy = llvm::Type::getInt1Ty(context);
            llvm::Function* RF = module->getFunction(fnName);
            if (!RF) {
                llvm::FunctionType* FT = llvm::FunctionType::get(retTy, argTypes, false);
                RF = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fnName, *module);
            }
            std::vector<llvm::Value*> args;
            args.push_back(obj);
            for (auto* arg : node.args) args.push_back(arg->accept(*this));
            llvm::Value* call = builder.CreateCall(RF, args);
            if (call->getType()->isIntegerTy(32)) {
                return lastValue = builder.CreateSExt(call, llvm::Type::getInt64Ty(context));
            }
            return lastValue = call;
        }
    }

    llvm::Value* obj = preObj ? preObj : node.object->accept(*this);
    if (!obj) return nullptr;
    
    // ── Interface Call Dispatch ──
    if (interfaceNames.count(typeName) && interfaceLayouts.count(typeName)) {
        auto& layout = interfaceLayouts[typeName];
        int itableIdx = -1;
        for (size_t i = 0; i < (int)layout.size(); ++i) {
            if (layout[i] == node.methodName) { itableIdx = (int)i; break; }
        }

        if (itableIdx != -1) {
            llvm::Value* itablePtr = builder.CreateExtractValue(obj, 1);
            llvm::Type* ITableTy = vtableTypes[typeName];
            llvm::Value* typedITablePtr = builder.CreateBitCast(itablePtr, llvm::PointerType::get(ITableTy, 0));
            llvm::Value* funcPtrAddr = builder.CreateStructGEP(ITableTy, typedITablePtr, (unsigned)itableIdx);
            llvm::Value* funcPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), funcPtrAddr);

            std::vector<llvm::Value*> args;
            args.push_back(obj); 
            for (auto* arg : node.args) args.push_back(arg->accept(*this));

            // Default parameters for interfaces
            if (interfaceDecls.count(typeName)) {
                auto* id = interfaceDecls[typeName];
                for (auto& m : id->methods) {
                    if (m.name == node.methodName) {
                        while (args.size() < m.params.size() + 1) {
                            size_t pIdx = args.size() - 1;
                            if (m.params[pIdx].defaultVal) args.push_back(m.params[pIdx].defaultVal->accept(*this));
                            else args.push_back(llvm::Constant::getNullValue(getType(m.params[pIdx].type)));
                        }
                        break;
                    }
                }
            }

            std::vector<llvm::Type*> argTypes;
            for (auto* a : args) argTypes.push_back(a->getType());

            // Get correct return type from interface decl
            llvm::Type* retTy = llvm::Type::getInt64Ty(context);
            if (interfaceDecls.count(typeName)) {
                for (const auto& m : interfaceDecls[typeName]->methods) {
                    if (m.name == node.methodName) {
                        retTy = getType(m.returnType);
                        break;
                    }
                }
            }
            llvm::FunctionType* FT = llvm::FunctionType::get(retTy, argTypes, false);
            
            llvm::Value* typedFuncPtr = builder.CreateBitCast(funcPtr, llvm::PointerType::get(FT, 0));
            return lastValue = builder.CreateCall(FT, typedFuncPtr, args);
        }
    }

    // ── Virtual Call Dispatch ──
    if (classDecls.count(typeName) && vtableLayouts.count(typeName)) {
        auto& layout = vtableLayouts[typeName];
        int vtableIdx = -1;
        for (size_t i = 0; i < (int)layout.size(); ++i) {
            if (layout[i] == node.methodName) { vtableIdx = (int)i; break; }
        }

        if (vtableIdx != -1) {
            llvm::Value* ptr = obj;
            llvm::StructType* currentST = classTypes[typeName];
            if (ptr->getType() != llvm::PointerType::get(currentST, 0)) {
                ptr = builder.CreateBitCast(ptr, llvm::PointerType::get(currentST, 0));
            }

            while (currentST->getNumElements() > 0 && currentST->getElementType(0)->isStructTy()) {
                ptr = builder.CreateStructGEP(currentST, ptr, 0);
                currentST = llvm::cast<llvm::StructType>(currentST->getElementType(0));
            }
            llvm::Value* vtablePtrAddr = builder.CreateStructGEP(currentST, ptr, 0);
            
            llvm::Value* vtablePtr = builder.CreateLoad(llvm::PointerType::get(context, 0), vtablePtrAddr);
            llvm::Type* VTableTy = vtableTypes[typeName];
            llvm::Value* typedVTablePtr = builder.CreateBitCast(vtablePtr, llvm::PointerType::get(VTableTy, 0));
            
            llvm::Value* funcPtrAddr = builder.CreateStructGEP(VTableTy, typedVTablePtr, (unsigned)vtableIdx);
            llvm::Value* funcPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), funcPtrAddr);

            std::string searchClass = typeName;
            llvm::Function* sigF = nullptr;
            while (!searchClass.empty()) {
                sigF = module->getFunction(searchClass + "_" + node.methodName);
                if (sigF) break;
                if (classDecls.count(searchClass) && !classDecls[searchClass]->baseAndInterfaces.empty())
                    searchClass = classDecls[searchClass]->baseAndInterfaces[0];
                else searchClass = "";
            }

            if (sigF) {
                std::vector<llvm::Value*> args;
                args.push_back(obj);
                for (auto* arg : node.args) args.push_back(arg->accept(*this));
                
                // Default parameters for virtual calls
                if (args.size() < sigF->arg_size()) {
                    if (functionDecls.count(sigF->getName().str())) {
                        auto* fd = functionDecls[sigF->getName().str()];
                        for (size_t i = args.size(); i < sigF->arg_size(); ++i) {
                            size_t pIdx = i - 1;
                            if (pIdx < fd->params.size() && fd->params[pIdx].defaultVal) args.push_back(fd->params[pIdx].defaultVal->accept(*this));
                            else args.push_back(llvm::Constant::getNullValue(sigF->getArg(i)->getType()));
                        }
                    } else {
                        while (args.size() < sigF->arg_size()) args.push_back(llvm::Constant::getNullValue(sigF->getArg(args.size())->getType()));
                    }
                }

                if (args[0]->getType() != sigF->getArg(0)->getType()) {
                    args[0] = builder.CreateBitCast(args[0], sigF->getArg(0)->getType());
                }

                llvm::Value* typedFuncPtr = builder.CreateBitCast(funcPtr, sigF->getType());
                return lastValue = builder.CreateCall(sigF->getFunctionType(), typedFuncPtr, args);
            }
        }
    }

    std::string mangledName = typeName + "_" + node.methodName;
    llvm::Function* F = module->getFunction(mangledName);
    if (!F) F = module->getFunction(node.methodName);
    if (!F && structTypes.count(typeName)) {
        F = module->getFunction(typeName + "_" + node.methodName);
    }
    
    if (!F) {
        auto* currentClass = classDecls[typeName];
        while (currentClass && !F) {
            if (!currentClass->baseAndInterfaces.empty()) {
                std::string baseName = currentClass->baseAndInterfaces[0];
                F = module->getFunction(baseName + "_" + node.methodName);
                currentClass = classDecls[baseName];
            } else {
                currentClass = nullptr;
            }
        }
    }
    
    if (!F) {
        LuvError::error(ErrorKind::UNDEFINED_FUNCTION, "Method not found: " + node.methodName + " in " + typeName);
        return nullptr;
    }
    
    std::vector<llvm::Value*> args;
    args.push_back(obj);
    for (auto* arg : node.args) args.push_back(arg->accept(*this));

    // Handle default parameters for methods
    if (F && args.size() < F->arg_size()) {
        if (functionDecls.count(F->getName().str())) {
            auto* fd = functionDecls[F->getName().str()];
            // Adjust index: F->arg(0) is self, fd->params[0] is first explicit param
            for (size_t i = args.size(); i < F->arg_size(); ++i) {
                size_t paramIdx = i - 1; // self is at index 0
                if (paramIdx < fd->params.size() && fd->params[paramIdx].defaultVal) {
                    args.push_back(fd->params[paramIdx].defaultVal->accept(*this));
                } else {
                    args.push_back(llvm::Constant::getNullValue(F->getArg(i)->getType()));
                }
            }
        } else {
             while (args.size() < F->arg_size()) {
                args.push_back(llvm::Constant::getNullValue(F->getArg(args.size())->getType()));
             }
        }
    }
    
    if (!F->arg_empty()) {
        llvm::Type* selfTy = F->getArg(0)->getType();
        if (args[0]->getType() != selfTy && args[0]->getType()->isPointerTy() && selfTy->isPointerTy()) {
            args[0] = builder.CreateBitCast(args[0], selfTy);
        }
    }
    
    return lastValue = builder.CreateCall(F, args);
}

llvm::Value* CodeGen::visit(SuperCallExpr& node) {
    if (classContextStack.empty()) return nullptr;
    std::string currentClass = classContextStack.back();
    if (!classDecls.count(currentClass)) return nullptr;
    ClassDecl* cls = classDecls[currentClass];
    if (cls->baseAndInterfaces.empty()) return nullptr;
    std::string base = cls->baseAndInterfaces[0];
    std::string fnName = base + "_" + node.methodName;
    llvm::Function* F = module->getFunction(fnName);
    if (!F) return nullptr;

    if (!namedValues.count("self")) return nullptr;
    auto& selfInfo = namedValues["self"];
    llvm::Value* selfVal = builder.CreateLoad(selfInfo.type, selfInfo.ptr);

    std::vector<llvm::Value*> args;
    args.push_back(selfVal);
    for (auto* arg : node.args) args.push_back(arg->accept(*this));
    if (!F->arg_empty()) {
        llvm::Type* selfTy = F->getArg(0)->getType();
        if (args[0]->getType() != selfTy && args[0]->getType()->isPointerTy() && selfTy->isPointerTy()) {
            args[0] = builder.CreateBitCast(args[0], selfTy);
        }
    }
    return lastValue = builder.CreateCall(F, args);
}

llvm::Value* CodeGen::visit(InterfaceDecl& node) {
    std::vector<std::string> layout;
    std::vector<llvm::Type*> vtableFieldTypes;
    for (const auto& m : node.methods) {
        layout.push_back(m.name);
        vtableFieldTypes.push_back(llvm::PointerType::get(context, 0));
    }
    interfaceLayouts[node.name] = layout;
    
    llvm::StructType* VTableST = llvm::StructType::create(context, vtableFieldTypes, node.name + "_itable_type");
    vtableTypes[node.name] = VTableST;
    
    return nullptr;
}

} // namespace luv

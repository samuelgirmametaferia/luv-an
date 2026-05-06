#include "CodeGen.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constant.h>
#include <llvm/Support/raw_ostream.h>
#include <iostream>

namespace luv {

// ── Helpers ──

llvm::Type* CodeGen::getType(const std::string& typeName) {
    if (typeName.empty()) return llvm::Type::getInt64Ty(context);
    if (typeName.back() == '?') {
        std::string baseTypeName = typeName.substr(0, typeName.size() - 1);
        llvm::Type* baseType = getType(baseTypeName);
        return llvm::PointerType::get(baseType, 0);
    }
    if (typeName.empty() || typeName == "int" || typeName == "i64") return llvm::Type::getInt64Ty(context);
    if (typeName == "i32") return llvm::Type::getInt32Ty(context);
    if (typeName == "float" || typeName == "f64") return llvm::Type::getDoubleTy(context);
    if (typeName == "bool") return llvm::Type::getInt1Ty(context);
    if (typeName == "char" || typeName == "i8") return llvm::Type::getInt8Ty(context);
    if (typeName == "string") return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    if (typeName.size() >= 2 && typeName.front() == '[' && typeName.back() == ']') {
        std::string elemTypeName = typeName.substr(1, typeName.size() - 2);
        size_t semi = elemTypeName.find(';');
        if (semi != std::string::npos) elemTypeName = elemTypeName.substr(0, semi);
        llvm::Type* elemType = getType(elemTypeName);
        return llvm::StructType::get(context, {
            llvm::PointerType::get(elemType, 0),
            llvm::Type::getInt64Ty(context),
            llvm::Type::getInt64Ty(context)
        });
    }
    if (typeName.size() >= 2 && typeName.front() == '(' && typeName.back() == ')') {
        std::vector<llvm::Type*> elems;
        std::string inner = typeName.substr(1, typeName.size() - 2);
        size_t start = 0;
        while (start < inner.size()) {
            size_t end = inner.find(',', start);
            std::string part = (end == std::string::npos) ? inner.substr(start) : inner.substr(start, end - start);
            size_t l = part.find_first_not_of(" \t");
            size_t r = part.find_last_not_of(" \t");
            if (l != std::string::npos) part = part.substr(l, r - l + 1);
            if (!part.empty()) elems.push_back(getType(part));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        if (elems.empty()) return llvm::Type::getInt64Ty(context);
        return llvm::StructType::get(context, elems);
    }
    if (typeName == "void") return llvm::Type::getVoidTy(context);
    
    if (interfaceNames.count(typeName)) {
        // Interface is a fat pointer: {i8*, itable*}
        return llvm::StructType::get(context, {
            llvm::PointerType::get(context, 0), // object pointer
            llvm::PointerType::get(context, 0)  // itable pointer
        });
    }

    if (structTypes.count(typeName)) return structTypes[typeName];
    if (enumTypes.count(typeName)) return enumTypes[typeName];
    if (classTypes.count(typeName)) return llvm::PointerType::get(classTypes[typeName], 0);
    
    // Default fallback
    return llvm::Type::getInt64Ty(context); 
}

std::string CodeGen::getTypeNameFromLLVM(llvm::Type* t) {
    if (!t) return "void";
    if (t->isIntegerTy(64)) return "int";
    if (t->isIntegerTy(32)) return "i32";
    if (t->isDoubleTy()) return "float";
    if (t->isIntegerTy(1)) return "bool";
    if (t->isIntegerTy(8)) return "char";
    if (t->isPointerTy()) return "string";
    return "void";
}

llvm::Value* CodeGen::toBool(llvm::Value* val) {
    if (!val) return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), false);
    llvm::Type* t = val->getType();
    if (t->isIntegerTy(1)) return val;
    if (t->isIntegerTy()) return builder.CreateICmpNE(val, llvm::ConstantInt::get(t, 0));
    if (t->isFloatingPointTy()) return builder.CreateFCmpONE(val, llvm::ConstantFP::get(t, 0.0));
    if (t->isPointerTy()) return builder.CreateICmpNE(val, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(t)));
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), true);
}

// ── Root Visitors ──

llvm::Value* CodeGen::visit(Program& node) {
    if (!module->getGlobalVariable("__name__")) {
        llvm::Type* strTy = getType("string");
        auto* init = builder.CreateGlobalStringPtr("__main__", "", 0, module.get());
        new llvm::GlobalVariable(*module, strTy, false,
            llvm::GlobalValue::InternalLinkage, llvm::dyn_cast<llvm::Constant>(init), "__name__");
    }
    if (!module->getGlobalVariable("__arg__")) {
        llvm::Type* arrTy = getType("[string]");
        llvm::Constant* zero = llvm::Constant::getNullValue(arrTy);
        new llvm::GlobalVariable(*module, arrTy, false,
            llvm::GlobalValue::InternalLinkage, zero, "__arg__");
    }

    auto isDecl = [](Stmt* stmt) -> bool {
        return dynamic_cast<FuncDecl*>(stmt) || dynamic_cast<StructDecl*>(stmt) ||
               dynamic_cast<ClassDecl*>(stmt) || dynamic_cast<InterfaceDecl*>(stmt) ||
               dynamic_cast<ExternDecl*>(stmt) || dynamic_cast<ModuleDeclStmt*>(stmt) ||
               dynamic_cast<UseStmt*>(stmt);
    };

    // Pass 1: Register all types and interface names
    std::function<void(StructDecl*)> preRegisterStruct = [&](StructDecl* sd) {
        if (structTypes.count(sd->name)) return;
        structTypes[sd->name] = llvm::StructType::create(context, sd->name);
        structDecls[sd->name] = sd;
        for (auto* nd : sd->nestedDecls) {
            if (auto* nsd = dynamic_cast<StructDecl*>(nd)) preRegisterStruct(nsd);
            else if (auto* ned = dynamic_cast<EnumDecl*>(nd)) {
                enumDecls[ned->name] = ned;
                enumTypes[ned->name] = llvm::StructType::create(context, ned->name);
            }
        }
    };

    std::function<void(StructDecl*)> defineStruct = [&](StructDecl* sd) {
        std::vector<llvm::Type*> fields;
        for (auto& f : sd->fields) fields.push_back(getType(f.type));
        structTypes[sd->name]->setBody(fields);
        for (auto* nd : sd->nestedDecls) {
            if (auto* nsd = dynamic_cast<StructDecl*>(nd)) defineStruct(nsd);
            else if (auto* ned = dynamic_cast<EnumDecl*>(nd)) {
                size_t maxSize = 0;
                for (auto& v : ned->variants) {
                    size_t s = 0;
                    for (auto& t : v.types) s += 8;
                    if (s > maxSize) maxSize = s;
                }
                std::vector<llvm::Type*> eFields;
                eFields.push_back(llvm::Type::getInt32Ty(context));
                if (maxSize > 0) eFields.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(context), maxSize));
                enumTypes[ned->name]->setBody(eFields);
            }
        }
    };

    for (auto* stmt : node.statements) {
        if (!stmt) continue;
        if (auto* id = dynamic_cast<InterfaceDecl*>(stmt)) {
            interfaceNames.insert(id->name);
            interfaceDecls[id->name] = id;
        } else if (auto* sd = dynamic_cast<StructDecl*>(stmt)) {
            preRegisterStruct(sd);
        } else if (auto* ed = dynamic_cast<EnumDecl*>(stmt)) {
            enumDecls[ed->name] = ed;
            enumTypes[ed->name] = llvm::StructType::create(context, ed->name);
        } else if (auto* cd = dynamic_cast<ClassDecl*>(stmt)) {
            classDecls[cd->name] = cd;
        }
    }

    for (auto* stmt : node.statements) {
        if (auto* sd = dynamic_cast<StructDecl*>(stmt)) defineStruct(sd);
        else if (auto* ed = dynamic_cast<EnumDecl*>(stmt)) {
            size_t maxSize = 0;
            for (auto& v : ed->variants) {
                size_t s = 0;
                for (auto& t : v.types) s += 8;
                if (s > maxSize) maxSize = s;
            }
            std::vector<llvm::Type*> eFields;
            eFields.push_back(llvm::Type::getInt32Ty(context));
            if (maxSize > 0) eFields.push_back(llvm::ArrayType::get(llvm::Type::getInt8Ty(context), maxSize));
            enumTypes[ed->name]->setBody(eFields);
        }
    }

    // Pass 2: Define Class struct types (now that base classes might be registered)
    std::function<void(ClassDecl*)> defineClassType = [&](ClassDecl* cd) {
        if (classTypes.count(cd->name)) return;
        std::vector<llvm::Type*> fields;
        fields.push_back(llvm::PointerType::get(context, 0)); // VTable*
        if (!cd->baseAndInterfaces.empty() && classDecls.count(cd->baseAndInterfaces[0])) {
            defineClassType(classDecls[cd->baseAndInterfaces[0]]);
            fields[0] = classTypes[cd->baseAndInterfaces[0]];
        }
        for (auto& f : cd->fields) fields.push_back(getType(f.type));
        classTypes[cd->name] = llvm::StructType::create(context, fields, cd->name);
    };
    for (auto& pair : classDecls) defineClassType(pair.second);

    // Pass 3: Generate VTables and ITables
    for (auto* stmt : node.statements) {
        if (stmt && (dynamic_cast<ClassDecl*>(stmt) || dynamic_cast<InterfaceDecl*>(stmt))) {
            stmt->accept(*this);
        }
    }

    // Pass 3.5: Declare all Functions and Externs
    for (auto* stmt : node.statements) {
        if (auto* fd = dynamic_cast<FuncDecl*>(stmt)) {
            std::vector<llvm::Type*> argTypes;
            bool needsImplicitSelf = !fd->boundStruct.empty() && 
                (classDecls.count(fd->boundStruct) || structDecls.count(fd->boundStruct)) && 
                !fd->isStatic && (fd->params.empty() || fd->params[0].name != "self");
            if (needsImplicitSelf) {
                llvm::Type* selfTy = classDecls.count(fd->boundStruct) ? 
                    (llvm::Type*)llvm::PointerType::get(classTypes[fd->boundStruct], 0) :
                    (llvm::Type*)llvm::PointerType::get(structTypes[fd->boundStruct], 0);
                argTypes.push_back(selfTy);
            }
            for (const auto& p : fd->params) argTypes.push_back(getType(p.type));
            llvm::FunctionType* FT = llvm::FunctionType::get(getType(fd->returnType), argTypes, false);
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, fd->name, *module);
            functionDecls[fd->name] = fd;
        } else if (auto* ed = dynamic_cast<ExternDecl*>(stmt)) {
            std::vector<llvm::Type*> argTypes;
            for (const auto& p : ed->params) argTypes.push_back(getType(p.type));
            llvm::FunctionType* FT = llvm::FunctionType::get(getType(ed->returnType), argTypes, false);
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, ed->name, *module);
        } else if (auto* ed = dynamic_cast<EnumDecl*>(stmt)) {
            // Variant constructors are declared here
            llvm::StructType* ET = enumTypes[ed->name];
            if (ET) {
                for (auto& v : ed->variants) {
                    std::vector<llvm::Type*> vArgTypes;
                    for (auto& t : v.types) vArgTypes.push_back(getType(t));
                    llvm::FunctionType* vFT = llvm::FunctionType::get(ET, vArgTypes, false);
                    llvm::Function::Create(vFT, llvm::Function::InternalLinkage, ed->name + "_" + v.name, *module);
                }
            }
        } else if (auto* cd = dynamic_cast<ClassDecl*>(stmt)) {
            for (auto* m : cd->methods) {
                std::vector<llvm::Type*> argTypes;
                bool needsImplicitSelf = !m->isStatic && (m->params.empty() || m->params[0].name != "self");
                if (needsImplicitSelf) argTypes.push_back(llvm::PointerType::get(classTypes[cd->name], 0));
                for (const auto& p : m->params) argTypes.push_back(getType(p.type));
                llvm::FunctionType* FT = llvm::FunctionType::get(getType(m->returnType), argTypes, false);
                llvm::Function::Create(FT, llvm::Function::ExternalLinkage, m->name, *module);
                functionDecls[m->name] = m;
            }
        }
    }

    // Pass 4: Generate Functions and Externs body
    for (auto* stmt : node.statements) {
        if (auto* cd = dynamic_cast<ClassDecl*>(stmt)) {
            classContextStack.push_back(cd->name);
            for (auto* m : cd->methods) m->accept(*this);
            classContextStack.pop_back();
        }
        if (stmt && (dynamic_cast<FuncDecl*>(stmt) || dynamic_cast<ExternDecl*>(stmt) || dynamic_cast<VarDecl*>(stmt) || dynamic_cast<EnumDecl*>(stmt))) {
            stmt->accept(*this);
        }
    }

    // Pass 5: Script statements
    std::vector<Stmt*> scriptStatements;
    for (auto* stmt : node.statements) {
        if (stmt && !isDecl(stmt)) scriptStatements.push_back(stmt);
    }
    if (!scriptStatements.empty()) {
        llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
        scriptInitFunc = llvm::Function::Create(FT, llvm::Function::InternalLinkage, "__luv_script_init", *module);
        llvm::BasicBlock* BB = llvm::BasicBlock::Create(context, "entry", scriptInitFunc);
        auto* oldBB = builder.GetInsertBlock();
        builder.SetInsertPoint(BB);
        for (auto* stmt : scriptStatements) {
            stmt->accept(*this);
        }
        if (!builder.GetInsertBlock()->getTerminator()) builder.CreateRetVoid();
        if (oldBB) builder.SetInsertPoint(oldBB);
    }
    generateMainWrapper();
    return nullptr;
}

llvm::Value* CodeGen::visit(Block& node) {
    llvm::Value* last = nullptr;
    for (auto* stmt : node.statements) {
        if (stmt) last = stmt->accept(*this);
    }
    return blockLastValue = last;
}

llvm::Value* CodeGen::visit(StmtExpr& node) {
    return node.stmt ? node.stmt->accept(*this) : nullptr;
}

llvm::Value* CodeGen::visit(ModuleDeclStmt& node) { return nullptr; }
llvm::Value* CodeGen::visit(UseStmt& node) { return nullptr; }

// ── Infrastructure ──

void CodeGen::generateMainWrapper() {
    // Check if we have a user-defined main
    llvm::Function* userMain = module->getFunction("main");
    if (userMain) return;
    llvm::FunctionType* mainTy = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), false);
    llvm::Function* mainFn = llvm::Function::Create(mainTy, llvm::Function::ExternalLinkage, "main", *module);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", mainFn);
    auto* oldBB = builder.GetInsertBlock();
    builder.SetInsertPoint(entry);
    if (scriptInitFunc) builder.CreateCall(scriptInitFunc);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
    if (oldBB) builder.SetInsertPoint(oldBB);
}

} // namespace luv

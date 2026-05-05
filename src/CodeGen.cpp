#include "CodeGen.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>
#include <iostream>

namespace luv {

llvm::Type* CodeGen::getType(const std::string& rawTypeName) {
    std::string typeName = rawTypeName;
    if (typeSubstitutions.count(typeName)) {
        typeName = typeSubstitutions[typeName];
    }
    if (typeName == "u8") return builder.getInt8Ty();
    if (typeName == "u16") return builder.getInt16Ty();
    if (typeName == "u32") return builder.getInt32Ty();
    if (typeName == "u64") return builder.getInt64Ty();
    if (typeName == "u128") return builder.getInt128Ty();
    if (typeName == "u256") return llvm::IntegerType::get(context, 256);

    if (typeName == "i8") return builder.getInt8Ty();
    if (typeName == "i16") return builder.getInt16Ty();
    if (typeName == "i32" || typeName == "int") return builder.getInt32Ty();
    if (typeName == "i64") return builder.getInt64Ty();
    if (typeName == "i128") return builder.getInt128Ty();
    if (typeName == "i256") return llvm::IntegerType::get(context, 256);

    if (typeName == "f16") return builder.getHalfTy();
    if (typeName == "f32" || typeName == "float") return builder.getFloatTy();
    if (typeName == "f64") return builder.getDoubleTy();
    if (typeName == "f80") return llvm::Type::getX86_FP80Ty(context);
    if (typeName == "f128") return llvm::Type::getFP128Ty(context);

    if (typeName == "bool" || typeName == "bit") return builder.getInt1Ty();
    if (typeName == "char") return builder.getInt8Ty();
    if (typeName == "string" || typeName == "bytes") return llvm::PointerType::get(context, 0);
    if (typeName == "void") return builder.getVoidTy();
    if (typeName == "dyn") return llvm::PointerType::get(context, 0);

    if (structTypes.count(typeName)) return structTypes[typeName];
    if (classTypes.count(typeName)) return llvm::PointerType::get(context, 0); // Classes are reference types
    
    return builder.getInt32Ty(); // Default
}

llvm::Value* CodeGen::visit(IntExpr& node) {
    return lastValue = llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(getType("int")), node.value, 10);
}

llvm::Value* CodeGen::visit(FloatExpr& node) {
    return lastValue = llvm::ConstantFP::get(context, llvm::APFloat(std::stod(node.value)));
}

// ── Escape sequence processor ──
// Handles all standard C/Python escape sequences:
//   \n  newline           \t  tab              \r  carriage return
//   \0  null byte         \\  literal backslash \' single quote
//   \"  double quote      \a  bell/alert       \b  backspace
//   \f  form feed         \v  vertical tab     \e  ANSI escape (0x1B)
//   \xHH    hex byte (1-2 hex digits)
//   \uHHHH  unicode codepoint (4 hex digits, UTF-8 encoded)
//   \UHHHHHHHH  unicode codepoint (8 hex digits, UTF-8 encoded)
//   \ooo    octal byte (1-3 octal digits)
//
static void encodeUTF8(uint32_t codepoint, std::string& out) {
    if (codepoint <= 0x7F) {
        out += (char)codepoint;
    } else if (codepoint <= 0x7FF) {
        out += (char)(0xC0 | (codepoint >> 6));
        out += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        out += (char)(0xE0 | (codepoint >> 12));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        out += (char)(0xF0 | (codepoint >> 18));
        out += (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out += (char)(0x80 | (codepoint & 0x3F));
    }
}

static std::string processEscapes(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    size_t len = raw.size();

    for (size_t i = 0; i < len; ++i) {
        if (raw[i] != '\\' || i + 1 >= len) {
            out += raw[i];
            continue;
        }

        char next = raw[i + 1];
        switch (next) {
            // ── Standard single-character escapes ──
            case 'n':  out += '\n'; ++i; break;  // newline
            case 't':  out += '\t'; ++i; break;  // tab
            case 'r':  out += '\r'; ++i; break;  // carriage return
            case '0':  out += '\0'; ++i; break;  // null byte
            case '\\': out += '\\'; ++i; break;  // literal backslash
            case '"':  out += '"';  ++i; break;  // double quote
            case '\'': out += '\''; ++i; break;  // single quote
            case 'a':  out += '\a'; ++i; break;  // bell / alert
            case 'b':  out += '\b'; ++i; break;  // backspace
            case 'f':  out += '\f'; ++i; break;  // form feed
            case 'v':  out += '\v'; ++i; break;  // vertical tab
            case 'e':  out += '\x1B'; ++i; break; // ANSI escape (for terminal colors/control)

            // ── \xHH — hex byte (1-2 hex digits) ──
            case 'x': {
                ++i; // skip 'x'
                unsigned val = 0;
                int digits = 0;
                while (digits < 2 && i + 1 < len && std::isxdigit((unsigned char)raw[i + 1])) {
                    ++i;
                    char c = raw[i];
                    val <<= 4;
                    if (c >= '0' && c <= '9') val |= (c - '0');
                    else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
                    ++digits;
                }
                out += (char)(val & 0xFF);
                break;
            }

            // ── \uHHHH — unicode codepoint (4 hex digits, UTF-8 encoded) ──
            case 'u': {
                ++i; // skip 'u'
                uint32_t cp = 0;
                int digits = 0;
                while (digits < 4 && i + 1 < len && std::isxdigit((unsigned char)raw[i + 1])) {
                    ++i;
                    char c = raw[i];
                    cp <<= 4;
                    if (c >= '0' && c <= '9') cp |= (c - '0');
                    else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
                    ++digits;
                }
                encodeUTF8(cp, out);
                break;
            }

            // ── \UHHHHHHHH — unicode codepoint (8 hex digits, UTF-8 encoded) ──
            case 'U': {
                ++i; // skip 'U'
                uint32_t cp = 0;
                int digits = 0;
                while (digits < 8 && i + 1 < len && std::isxdigit((unsigned char)raw[i + 1])) {
                    ++i;
                    char c = raw[i];
                    cp <<= 4;
                    if (c >= '0' && c <= '9') cp |= (c - '0');
                    else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
                    ++digits;
                }
                encodeUTF8(cp, out);
                break;
            }

            // ── Octal: \0oo — up to 3 octal digits after the backslash ──
            // Triggered by digits 1-7 (note: \0 is already handled above as null)
            default:
                if (next >= '1' && next <= '7') {
                    unsigned val = 0;
                    int digits = 0;
                    while (digits < 3 && i + 1 < len && raw[i + 1] >= '0' && raw[i + 1] <= '7') {
                        ++i;
                        val = (val << 3) | (raw[i] - '0');
                        ++digits;
                    }
                    out += (char)(val & 0xFF);
                } else {
                    // Unknown escape — keep the backslash + character as-is
                    out += raw[i];
                }
                break;
        }
    }
    return out;
}

llvm::Value* CodeGen::visit(StringExpr& node) {
    return lastValue = builder.CreateGlobalString(processEscapes(node.value), "", 0, module.get());
}

llvm::Value* CodeGen::visit(CharExpr& node) {
    return lastValue = builder.getInt8(node.value);
}

llvm::Value* CodeGen::visit(BoolExpr& node) {
    return lastValue = builder.getInt1(node.value);
}

llvm::Value* CodeGen::visit(NullExpr& node) {
    return lastValue = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
}

llvm::Value* CodeGen::visit(VarExpr& node) {
    // Propagate semantic type for class instances
    if (varSemanticTypes.count(node.name)) {
        node.semanticType = varSemanticTypes[node.name];
    }
    if (namedValues.count(node.name)) {
        auto& info = namedValues[node.name];
        return lastValue = builder.CreateLoad(info.type, info.ptr, node.name.c_str());
    }
    if (auto* gVar = module->getGlobalVariable(node.name)) {
        return lastValue = builder.CreateLoad(gVar->getValueType(), gVar, node.name.c_str());
    }
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(BinaryExpr& node) {
    llvm::Value* L = node.left->accept(*this);
    llvm::Value* R = node.right->accept(*this);
    if (!L || !R) return nullptr;

    if (L->getType()->isStructTy()) {
        llvm::StructType* stType = llvm::cast<llvm::StructType>(L->getType());
        std::string opFuncName = stType->getName().str() + "_" + node.op;
        llvm::Function* callee = module->getFunction(opFuncName);
        if (callee) {
            return lastValue = builder.CreateCall(callee, {L, R}, "opcall");
        }
    } else if (L->getType()->isPointerTy()) {
        std::string semanticType = "";
        if (auto* var = dynamic_cast<VarExpr*>(node.left.get())) {
            if (varSemanticTypes.count(var->name)) {
                semanticType = varSemanticTypes[var->name];
            }
        } else if (node.left->semanticType.size()) {
            semanticType = node.left->semanticType;
        }
        
        if (semanticType.size() && classDecls.count(semanticType)) {
            std::string opFuncName = semanticType + "_" + node.op;
            llvm::Function* callee = module->getFunction(opFuncName);
            
            // Walk inheritance chain to find inherited operators
            if (!callee) {
                std::string searchType = semanticType;
                while (!callee && classDecls.count(searchType)) {
                    ClassDecl* cls = classDecls[searchType];
                    if (!cls->baseAndInterfaces.empty()) {
                        searchType = cls->baseAndInterfaces[0];
                        callee = module->getFunction(searchType + "_" + node.op);
                    } else {
                        break;
                    }
                }
            }
            
            if (callee) {
                return lastValue = builder.CreateCall(callee, {L, R}, "opcall");
            } else {
                std::cerr << "error: callee " << opFuncName << " not found!\n";
            }
        }
    }

    if (node.op == "+") {
        if (L->getType()->isPointerTy() && R->getType()->isPointerTy()) {
            return lastValue = builder.CreateCall(getOrCreateStringConcat(), {L, R}, "concat");
        }
        return lastValue = builder.CreateAdd(L, R, "addtmp");
    }
    if (node.op == "-") return lastValue = builder.CreateSub(L, R, "subtmp");
    if (node.op == "*") return lastValue = builder.CreateMul(L, R, "multmp");
    if (node.op == "/") return lastValue = builder.CreateSDiv(L, R, "divtmp");
    if (node.op == "%") return lastValue = builder.CreateSRem(L, R, "remtmp");
    
    if (node.op == "==") return lastValue = builder.CreateICmpEQ(L, R, "eqtmp");
    if (node.op == "!=") return lastValue = builder.CreateICmpNE(L, R, "netmp");
    if (node.op == "<") return lastValue = builder.CreateICmpSLT(L, R, "lttmp");
    if (node.op == ">") return lastValue = builder.CreateICmpSGT(L, R, "gttmp");
    if (node.op == "<=") return lastValue = builder.CreateICmpSLE(L, R, "letmp");
    if (node.op == ">=") return lastValue = builder.CreateICmpSGE(L, R, "getmp");

    if (node.op == "&&" || node.op == "and") return lastValue = builder.CreateAnd(L, R, "andtmp");
    if (node.op == "||" || node.op == "or") return lastValue = builder.CreateOr(L, R, "ortmp");
    if (node.op == "&") return lastValue = builder.CreateAnd(L, R, "bandtmp");
    if (node.op == "|") return lastValue = builder.CreateOr(L, R, "bortmp");
    if (node.op == "^") return lastValue = builder.CreateXor(L, R, "bxortmp");
    if (node.op == "<<") return lastValue = builder.CreateShl(L, R, "shltmp");
    if (node.op == ">>") return lastValue = builder.CreateAShr(L, R, "shrtmp");

    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(UnaryExpr& node) {
    llvm::Value* V = node.expr->accept(*this);
    if (node.op == "-" ) return lastValue = builder.CreateNeg(V, "negtmp");
    if (node.op == "!" || node.op == "not" || node.op == "~") return lastValue = builder.CreateNot(V, "nottmp");
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(CallExpr& node) {
    // Try direct lookup first; fall back to mangled name mapping for C++/Rust FFI
    llvm::Function* callee = module->getFunction(node.callee);
    if (!callee && mangledNames.count(node.callee)) {
        callee = module->getFunction(mangledNames[node.callee]);
    }
    
    // Class Instantiation!
    if (!callee && classTypes.count(node.callee)) {
        // 1. Allocate memory on the heap
        llvm::StructType* clsType = classTypes[node.callee];
        llvm::Function* mallocFunc = module->getFunction("malloc");
        if (!mallocFunc) {
            llvm::FunctionType* mallocType = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {builder.getInt64Ty()}, false);
            mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", module.get());
        }
        
        const llvm::DataLayout& dl = module->getDataLayout();
        uint64_t size = dl.getTypeAllocSize(clsType);
        llvm::Value* sizeVal = builder.getInt64(size);
        llvm::Value* objPtr = builder.CreateCall(mallocFunc, {sizeVal}, "newobj");
        
        // 2. Call init method if it exists
        std::string initName = node.callee + "_init";
        llvm::Function* initFunc = module->getFunction(initName);
        if (initFunc) {
            std::vector<llvm::Value*> initArgs;
            initArgs.push_back(objPtr); // 'self' is the first argument
            for (auto& argExpr : node.args) {
                initArgs.push_back(argExpr->accept(*this));
            }
            builder.CreateCall(initFunc, initArgs);
        }
        
        node.semanticType = node.callee;
        return lastValue = objPtr;
    }
    
    if (!callee) return nullptr;
    
    std::vector<bool> mutFlags;
    if (funcMutParams.count(node.callee)) {
        mutFlags = funcMutParams[node.callee];
    } else {
        mutFlags.resize(node.args.size(), false);
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node.args.size(); ++i) {
        bool expectsMut = (i < mutFlags.size() && mutFlags[i]);
        if (expectsMut) {
            if (auto* varExpr = dynamic_cast<VarExpr*>(node.args[i].get())) {
                if (namedValues.count(varExpr->name)) {
                    auto& vInfo = namedValues[varExpr->name];
                    if (vInfo.isMut) {
                        args.push_back(vInfo.ptr);
                    } else {
                        llvm::Value* val = builder.CreateLoad(vInfo.type, vInfo.ptr);
                        llvm::AllocaInst* temp = builder.CreateAlloca(vInfo.type);
                        builder.CreateStore(val, temp);
                        args.push_back(temp);
                    }
                } else if (auto* gVar = module->getGlobalVariable(varExpr->name)) {
                    if (gVar->isConstant()) {
                        llvm::Value* val = builder.CreateLoad(gVar->getValueType(), gVar);
                        llvm::AllocaInst* temp = builder.CreateAlloca(gVar->getValueType());
                        builder.CreateStore(val, temp);
                        args.push_back(temp);
                    } else {
                        args.push_back(gVar);
                    }
                } else {
                    llvm::Value* val = node.args[i]->accept(*this);
                    llvm::AllocaInst* temp = builder.CreateAlloca(val->getType());
                    builder.CreateStore(val, temp);
                    args.push_back(temp);
                }
            } else {
                llvm::Value* val = node.args[i]->accept(*this);
                llvm::AllocaInst* temp = builder.CreateAlloca(val->getType());
                builder.CreateStore(val, temp);
                args.push_back(temp);
            }
        } else {
            args.push_back(node.args[i]->accept(*this));
        }
    }

    // Void functions can't have a name on the call result
    if (callee->getReturnType()->isVoidTy()) {
        builder.CreateCall(callee, args);
        return lastValue = nullptr;
    }
    return lastValue = builder.CreateCall(callee, args, "calltmp");
}

// Helper for basic Itanium C++ Name Mangling
static std::string mangleCXX(const std::string& name, const std::vector<Param>& params) {
    std::string mangled = "_Z" + std::to_string(name.length()) + name;
    if (params.empty()) {
        mangled += "v";
    } else {
        for (const auto& p : params) {
            std::string t = p.type.empty() ? "int" : p.type;
            if (p.isMutable) mangled += "P";
            
            if (t == "int" || t == "i32") mangled += "i";
            else if (t == "float" || t == "f32") mangled += "f";
            else if (t == "char") mangled += "c";
            else if (t == "bool") mangled += "b";
            else if (t == "double" || t == "f64") mangled += "d";
            else if (t == "void") mangled += "v";
            else if (t == "string") mangled += "Pc";
            else mangled += "i"; // fallback
        }
    }
    return mangled;
}

// ── extern fn: declares a function with external linkage ──
// Compatible with any LLVM-based language (C, C++, Rust, Zig, etc.)
llvm::Value* CodeGen::visit(ExternDecl& node) {
    std::vector<llvm::Type*> paramTypes;
    std::vector<bool> mutFlags;
    for (auto& p : node.params) {
        mutFlags.push_back(p.isMutable);
        if (p.isMutable) {
            paramTypes.push_back(llvm::PointerType::get(context, 0));
        } else if (p.isDynamic || p.type == "dyn") {
            paramTypes.push_back(getType("dyn"));
        } else if (p.type.empty()) {
            paramTypes.push_back(getType("int"));
        } else {
            paramTypes.push_back(getType(p.type));
        }
    }
    funcMutParams[node.name] = mutFlags;

    llvm::Type* rt = node.returnType.empty() ? getType("int") : getType(node.returnType);
    bool isVarArg = (node.name == "printf" || node.name == "fprintf" || 
                     node.name == "sprintf" || node.name == "scanf");
    llvm::FunctionType* FT = llvm::FunctionType::get(rt, paramTypes, isVarArg);

    std::string funcName = node.name;
    if (node.abi == "C++") {
        funcName = mangleCXX(node.name, node.params);
    } else if (node.abi == "Rust") {
        // Rust native v0 mangling is complex (_R...). For now we just use the name directly
        // since Rust FFI is predominantly done via `extern "C"`. If needed, Rust mangling can be expanded here.
        funcName = node.name;
    }

    llvm::Function* func = module->getFunction(funcName);
    if (!func) {
        func = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, funcName, *module);
        
        // Adjust calling conventions based on ABI
        if (node.abi == "C") {
            func->setCallingConv(llvm::CallingConv::C);
        } else if (node.abi == "C++") {
            func->setCallingConv(llvm::CallingConv::C); // C++ standard functions use C calling convention
        } else if (node.abi == "Rust") {
            func->setCallingConv(llvm::CallingConv::C); // Standard FFI convention
        }
    }
    
    // Store the mangled name so `CallExpr` can find it by its Luv name.
    if (funcName != node.name) {
        mangledNames[node.name] = funcName;
    }

    return lastValue = nullptr;
}

// ── asm { "..." }: inline assembly expression ──
// The asm string is passed to LLVM's InlineAsm. The result is returned
// via the "=r" output constraint (general-purpose register), making the
// entire asm{} block evaluate to a value that can be assigned.
//
// Example:  x = asm { "mov $0, 42" }   →  x == 42
//
llvm::Value* CodeGen::visit(AsmExpr& node) {
    // Return type: i64 (general purpose result register)
    llvm::Type* retType = builder.getInt64Ty();

    llvm::FunctionType* asmFuncType = llvm::FunctionType::get(retType, false);
    llvm::InlineAsm* inlineAsm = llvm::InlineAsm::get(
        asmFuncType,
        node.asmString,
        "=r,~{dirflag},~{fpsr},~{flags}", // constraints
        true,       // hasSideEffects
        false,      // isAlignStack
        llvm::InlineAsm::AD_Intel // asmDialect
    );

    return lastValue = builder.CreateCall(asmFuncType, inlineAsm, {}, "asmresult");
}

// ── @name(): intrinsic call from the builtin table ──
// These are compiler-provided functions injected at codegen time.
// The table maps names to implementations that emit LLVM IR directly.
//
llvm::Value* CodeGen::visit(IntrinsicCallExpr& node) {
    // Evaluate all arguments first
    std::vector<llvm::Value*> args;
    for (auto& arg : node.args) {
        args.push_back(arg->accept(*this));
    }

    // ── Intrinsic dispatch table ──
    if (node.callee == "sizeof") {
        // @sizeof(type_value) → returns size in bytes as i64
        if (args.empty()) return lastValue = builder.getInt64(0);
        llvm::Type* t = args[0]->getType();
        auto* dl = &module->getDataLayout();
        uint64_t size = dl->getTypeAllocSize(t);
        return lastValue = builder.getInt64(size);
    }

    if (node.callee == "alignof") {
        // @alignof(type_value) → returns alignment in bytes as i64
        if (args.empty()) return lastValue = builder.getInt64(0);
        llvm::Type* t = args[0]->getType();
        auto* dl = &module->getDataLayout();
        uint64_t align = dl->getABITypeAlign(t).value();
        return lastValue = builder.getInt64(align);
    }

    if (node.callee == "bitcast") {
        // @bitcast(value) → reinterpret as i64
        if (args.empty()) return lastValue = builder.getInt64(0);
        llvm::Value* v = args[0];
        if (v->getType()->isIntegerTy()) {
            return lastValue = builder.CreateIntCast(v, builder.getInt64Ty(), true, "bitcast");
        }
        if (v->getType()->isFloatingPointTy()) {
            return lastValue = builder.CreateBitCast(v, builder.getInt64Ty(), "bitcast");
        }
        if (v->getType()->isPointerTy()) {
            return lastValue = builder.CreatePtrToInt(v, builder.getInt64Ty(), "ptr2int");
        }
        return lastValue = builder.getInt64(0);
    }

    if (node.callee == "trap") {
        // @trap() → emit a trap instruction (instant crash for debugging)
        llvm::Function* trapFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::trap);
        builder.CreateCall(trapFn);
        return lastValue = nullptr;
    }

    if (node.callee == "abs") {
        // @abs(x) → absolute value (integer)
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Value* v = args[0];
        llvm::Value* neg = builder.CreateNeg(v, "negtmp");
        llvm::Value* cmp = builder.CreateICmpSLT(v, llvm::ConstantInt::get(v->getType(), 0), "isNeg");
        return lastValue = builder.CreateSelect(cmp, neg, v, "abs");
    }

    if (node.callee == "min") {
        // @min(a, b)
        if (args.size() < 2) return lastValue = builder.getInt32(0);
        llvm::Value* cmp = builder.CreateICmpSLT(args[0], args[1], "mintmp");
        return lastValue = builder.CreateSelect(cmp, args[0], args[1], "min");
    }

    if (node.callee == "max") {
        // @max(a, b)
        if (args.size() < 2) return lastValue = builder.getInt32(0);
        llvm::Value* cmp = builder.CreateICmpSGT(args[0], args[1], "maxtmp");
        return lastValue = builder.CreateSelect(cmp, args[0], args[1], "max");
    }

    if (node.callee == "likely" || node.callee == "expect") {
        // @likely(cond) → branch prediction hint
        if (args.empty()) return lastValue = builder.getInt1(true);
        llvm::Function* expectFn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::expect, {builder.getInt1Ty()});
        return lastValue = builder.CreateCall(expectFn, {args[0], builder.getInt1(true)}, "likely");
    }

    if (node.callee == "unlikely") {
        // @unlikely(cond) → branch prediction hint (cold path)
        if (args.empty()) return lastValue = builder.getInt1(false);
        llvm::Function* expectFn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::expect, {builder.getInt1Ty()});
        return lastValue = builder.CreateCall(expectFn, {args[0], builder.getInt1(false)}, "unlikely");
    }

    if (node.callee == "ctpop" || node.callee == "popcount") {
        // @popcount(x) → count set bits
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
        return lastValue = builder.CreateCall(fn, {args[0]}, "popcount");
    }

    if (node.callee == "ctlz" || node.callee == "clz") {
        // @clz(x) → count leading zeros
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::ctlz, {args[0]->getType()});
        return lastValue = builder.CreateCall(fn, {args[0], builder.getInt1(false)}, "clz");
    }

    if (node.callee == "cttz" || node.callee == "ctz") {
        // @ctz(x) → count trailing zeros
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::cttz, {args[0]->getType()});
        return lastValue = builder.CreateCall(fn, {args[0], builder.getInt1(false)}, "ctz");
    }

    if (node.callee == "bswap") {
        // @bswap(x) → byte swap (endianness conversion)
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Function* fn = llvm::Intrinsic::getOrInsertDeclaration(
            module.get(), llvm::Intrinsic::bswap, {args[0]->getType()});
        return lastValue = builder.CreateCall(fn, {args[0]}, "bswap");
    }

    if (node.callee == "print" || node.callee == "println") {
        if (args.empty()) return lastValue = builder.getInt32(0);
        llvm::Function* printfFunc = module->getFunction("printf");
        if (!printfFunc) {
            llvm::FunctionType* printfType = llvm::FunctionType::get(builder.getInt32Ty(), {builder.getPtrTy()}, true);
            printfFunc = llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", module.get());
        }
        llvm::Value* arg = args[0];
        llvm::Value* formatStr = nullptr;
        
        if (arg->getType()->isIntegerTy()) {
            if (arg->getType()->getIntegerBitWidth() == 1) {
                formatStr = builder.CreateGlobalStringPtr(node.callee == "println" ? "%d\n" : "%d");
                arg = builder.CreateZExt(arg, builder.getInt32Ty());
            } else {
                formatStr = builder.CreateGlobalStringPtr(node.callee == "println" ? "%d\n" : "%d");
            }
        } else if (arg->getType()->isFloatingPointTy()) {
            formatStr = builder.CreateGlobalStringPtr(node.callee == "println" ? "%f\n" : "%f");
            if (arg->getType()->isFloatTy()) arg = builder.CreateFPExt(arg, builder.getDoubleTy());
        } else if (arg->getType()->isPointerTy()) {
            formatStr = builder.CreateGlobalStringPtr(node.callee == "println" ? "%s\n" : "%s");
        } else {
            formatStr = builder.CreateGlobalStringPtr(node.callee == "println" ? "%p\n" : "%p");
        }
        
        return lastValue = builder.CreateCall(printfFunc, {formatStr, arg});
    }

    // Unknown intrinsic
    std::cerr << "warning: unknown intrinsic '@" << node.callee << "', returning 0\n";
    return lastValue = builder.getInt32(0);
}

llvm::Value* CodeGen::visit(Block& node) {
    llvm::Value* val = nullptr;
    for (auto& stmt : node.statements) {
        val = stmt->accept(*this);
        blockLastValue = val;
    }
    return lastValue = val;
}

llvm::Value* CodeGen::visit(VarDecl& node) {
    llvm::Type* type = nullptr;
    llvm::Value* initVal = nullptr;

    if (!node.type.empty()) {
        type = getType(node.type);
    } else if (node.isDynamic) {
        type = getType("dyn");
    }

    if (node.init) {
        initVal = node.init->accept(*this);
        if (!type && initVal) {
            type = initVal->getType();
        }
    }

    if (!type) {
        if (auto* vp = dynamic_cast<VarPattern*>(node.pattern)) {
            delayedVars[vp->name] = &node;
        }
        return lastValue = nullptr;
    }

    if (builder.GetInsertBlock()) {
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        if (initVal) {
            llvm::BasicBlock* failBB = llvm::BasicBlock::Create(context, "var_fail", func);
            auto savedIP = builder.saveIP();
            builder.SetInsertPoint(failBB);
            llvm::Function* trapFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::trap);
            builder.CreateCall(trapFn);
            builder.CreateUnreachable();
            builder.restoreIP(savedIP);

            generatePatternMatch(node.pattern, initVal, failBB);
            
            if (auto* vp = dynamic_cast<VarPattern*>(node.pattern)) {
                if (!node.type.empty() && classDecls.count(node.type)) {
                    varSemanticTypes[vp->name] = node.type;
                } else if (node.init && node.init->semanticType.size() && classDecls.count(node.init->semanticType)) {
                    varSemanticTypes[vp->name] = node.init->semanticType;
                }
            }
            return lastValue = initVal;
        } else {
            if (auto* vp = dynamic_cast<VarPattern*>(node.pattern)) {
                llvm::IRBuilder<> tmp(&func->getEntryBlock(), func->getEntryBlock().begin());
                llvm::AllocaInst* alloca = tmp.CreateAlloca(type, nullptr, vp->name);
                namedValues[vp->name] = {alloca, type, node.isMutable};
                if (type->isPointerTy()) {
                    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(type)), alloca);
                }
                return lastValue = alloca;
            }
        }
    } else {
        if (auto* vp = dynamic_cast<VarPattern*>(node.pattern)) {
            llvm::Constant* constInit = nullptr;
            if (initVal) {
                constInit = llvm::dyn_cast<llvm::Constant>(initVal);
                if (!constInit) constInit = llvm::Constant::getNullValue(type);
            } else {
                constInit = llvm::Constant::getNullValue(type);
            }
            llvm::GlobalVariable* gVar = new llvm::GlobalVariable(
                *module, type, node.isConst,
                llvm::GlobalValue::ExternalLinkage,
                constInit, vp->name
            );
            return lastValue = gVar;
        }
    }
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(Assignment& node) {
    llvm::Value* val = node.value->accept(*this);
    if (!val || node.targets.empty()) return nullptr;

    // Support single target assignment for now
    Expr* target = node.targets[0];
    if (auto* varExpr = dynamic_cast<VarExpr*>(target)) {
        std::string name = varExpr->name;
        // Track semantic type for class instances
        if (node.value->semanticType.size() && classDecls.count(node.value->semanticType)) {
            varSemanticTypes[name] = node.value->semanticType;
        }
        if (namedValues.count(name)) {
            builder.CreateStore(val, namedValues[name].ptr);
        } else if (delayedVars.count(name)) {
            llvm::Function* func = builder.GetInsertBlock()->getParent();
            llvm::IRBuilder<> tmp(&func->getEntryBlock(), func->getEntryBlock().begin());
            llvm::AllocaInst* alloca = tmp.CreateAlloca(val->getType(), nullptr, name);
            namedValues[name] = {alloca, val->getType(), delayedVars[name]->isMutable};
            builder.CreateStore(val, alloca);
            delayedVars.erase(name);
        } else {
            // Variable not found, implicit declaration (e.g. x = 42)
            if (builder.GetInsertBlock()) {
                llvm::Function* func = builder.GetInsertBlock()->getParent();
                llvm::IRBuilder<> tmp(&func->getEntryBlock(), func->getEntryBlock().begin());
                llvm::AllocaInst* alloca = tmp.CreateAlloca(val->getType(), nullptr, name);
                namedValues[name] = {alloca, val->getType(), false};
                builder.CreateStore(val, alloca);
            } else {
                // Top-level implicit declaration
                llvm::Constant* constInit = llvm::dyn_cast<llvm::Constant>(val);
                if (!constInit) constInit = llvm::Constant::getNullValue(val->getType());
                llvm::GlobalVariable* gVar = new llvm::GlobalVariable(
                    *module, val->getType(), false,
                    llvm::GlobalValue::InternalLinkage, constInit, name
                );
                return lastValue = gVar;
            }
        }
    } else if (auto* propExpr = dynamic_cast<PropertyExpr*>(target)) {
        llvm::Value* obj = propExpr->object->accept(*this);
        if (!obj) return nullptr;

        std::string semanticType = propExpr->object->semanticType;
        if (obj->getType()->isStructTy()) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(obj->getType());
            std::string structName = structType->getName().str();
            if (structDecls.count(structName)) {
                StructDecl* decl = structDecls[structName];
                for (size_t i = 0; i < decl->fields.size(); ++i) {
                    if (decl->fields[i].name == propExpr->propertyName) {
                        std::cerr << "error: assigning to struct properties requires mutable references\n";
                        return nullptr;
                    }
                }
            }
        } else if (obj->getType()->isPointerTy() && classDecls.count(semanticType)) {
            ClassDecl* decl = classDecls[semanticType];
            for (size_t i = 0; i < decl->fields.size(); ++i) {
                if (decl->fields[i].name == propExpr->propertyName) {
                    if (decl->fields[i].isPrivate) {
                        bool canAccess = false;
                        if (varSemanticTypes.count("self") > 0) {
                            std::string currentClass = varSemanticTypes["self"];
                            std::string searchType = currentClass;
                            while (!searchType.empty() && classDecls.count(searchType)) {
                                if (searchType == semanticType) { canAccess = true; break; }
                                ClassDecl* c = classDecls[searchType];
                                searchType = c->baseAndInterfaces.empty() ? "" : c->baseAndInterfaces[0];
                            }
                        }
                        if (!canAccess) {
                            std::cerr << "error: property '" << propExpr->propertyName << "' is private to '" << semanticType << "'\n";
                            return nullptr;
                        }
                    }
                    llvm::StructType* structType = classTypes[semanticType];
                    llvm::Value* ptr = builder.CreateStructGEP(structType, obj, i, "prop_ptr");
                    builder.CreateStore(val, ptr);
                    return lastValue = val;
                }
            }
        }
    }
    return lastValue = val;
}

llvm::Value* CodeGen::visit(MatchExpr& node) {
    llvm::Value* valueToMatch = node.value->accept(*this);
    if (!valueToMatch) return nullptr;

    llvm::Function* func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "matchcont");
    
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiValues;

    for (auto& c : node.cases) {
        llvm::BasicBlock* matchThenBB = llvm::BasicBlock::Create(context, "matchthen", func);
        llvm::BasicBlock* nextCaseBB = llvm::BasicBlock::Create(context, "matchnext", func);
        
        if (c.pattern) {
            generatePatternMatch(c.pattern, valueToMatch, nextCaseBB);
            builder.CreateBr(matchThenBB);
        } else {
            // Default case
            builder.CreateBr(matchThenBB);
        }
        
        builder.SetInsertPoint(matchThenBB);
        llvm::Value* resVal = nullptr;
        if (c.resultExpr) {
            resVal = c.resultExpr->accept(*this);
        } else if (c.resultBlock) {
            c.resultBlock->accept(*this);
            resVal = blockLastValue;
        }
        
        llvm::BasicBlock* currentThenBB = builder.GetInsertBlock();
        if (!currentThenBB->getTerminator()) {
            builder.CreateBr(mergeBB);
        }
        if (resVal) phiValues.push_back({resVal, currentThenBB});
        
        builder.SetInsertPoint(nextCaseBB);
    }
    
    builder.CreateBr(mergeBB);
    func->insert(func->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);

    if (!phiValues.empty()) {
        llvm::Type* phiType = phiValues[0].first->getType();
        bool typesMatch = true;
        for (auto& p : phiValues) {
            if (p.first->getType() != phiType) { typesMatch = false; break; }
        }
        if (typesMatch && phiType->isFirstClassType()) {
            llvm::PHINode* phi = builder.CreatePHI(phiType, phiValues.size(), "matchphi");
            for (auto& p : phiValues) phi->addIncoming(p.first, p.second);
            return lastValue = blockLastValue = phi;
        }
    }
    return lastValue = blockLastValue = nullptr;
}

llvm::Value* CodeGen::visit(IfExpr& node) {
    llvm::Value* cond = node.cond->accept(*this);
    if (!cond) return nullptr;
    llvm::Function* func = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", func);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont");
    
    std::vector<llvm::BasicBlock*> efCondBBs;
    std::vector<llvm::BasicBlock*> efThenBBs;
    for (size_t i = 0; i < node.efs.size(); ++i) {
        efCondBBs.push_back(llvm::BasicBlock::Create(context, "efcond", func));
        efThenBBs.push_back(llvm::BasicBlock::Create(context, "efthen", func));
    }
    
    llvm::BasicBlock* elseBB = node.elseBlock ? llvm::BasicBlock::Create(context, "else") : nullptr;

    builder.CreateCondBr(cond, thenBB, efCondBBs.empty() ? (elseBB ? elseBB : mergeBB) : efCondBBs[0]);

    builder.SetInsertPoint(thenBB);
    node.thenBlock->accept(*this);
    llvm::Value* thenVal = blockLastValue;
    thenBB = builder.GetInsertBlock();
    if (!thenBB->getTerminator()) builder.CreateBr(mergeBB);

    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phiValues;
    if (thenVal && thenVal->getType()->isFirstClassType()) phiValues.push_back({thenVal, thenBB});

    for (size_t i = 0; i < node.efs.size(); ++i) {
        builder.SetInsertPoint(efCondBBs[i]);
        llvm::Value* efCond = node.efs[i].cond->accept(*this);
        llvm::BasicBlock* nextCond = (i + 1 < efCondBBs.size()) ? efCondBBs[i+1] : (elseBB ? elseBB : mergeBB);
        builder.CreateCondBr(efCond, efThenBBs[i], nextCond);
        
        builder.SetInsertPoint(efThenBBs[i]);
        node.efs[i].block->accept(*this);
        llvm::Value* efVal = blockLastValue;
        llvm::BasicBlock* currentEfThenBB = builder.GetInsertBlock();
        if (efVal && efVal->getType()->isFirstClassType()) phiValues.push_back({efVal, currentEfThenBB});
        if (!currentEfThenBB->getTerminator()) builder.CreateBr(mergeBB);
    }

    if (elseBB) {
        func->insert(func->end(), elseBB);
        builder.SetInsertPoint(elseBB);
        node.elseBlock->accept(*this);
        llvm::Value* elseVal = blockLastValue;
        elseBB = builder.GetInsertBlock();
        if (elseVal && elseVal->getType()->isFirstClassType()) phiValues.push_back({elseVal, elseBB});
        if (!elseBB->getTerminator()) builder.CreateBr(mergeBB);
    }

    func->insert(func->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
    
    size_t expectedBranches = 1 + node.efs.size() + (elseBB ? 1 : 0);
    if (elseBB && phiValues.size() == expectedBranches && phiValues[0].first) {
        llvm::Type* phiType = phiValues[0].first->getType();
        bool typesMatch = true;
        for (auto& p : phiValues) {
            if (p.first->getType() != phiType) { typesMatch = false; break; }
        }
        if (typesMatch && phiType->isFirstClassType()) {
            llvm::PHINode* phi = builder.CreatePHI(phiType, phiValues.size(), "ifphi");
            for (auto& p : phiValues) phi->addIncoming(p.first, p.second);
            return lastValue = blockLastValue = phi;
        }
    }

    return lastValue = blockLastValue = nullptr;
}

llvm::Value* CodeGen::visit(WhileExpr& node) {
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    auto condBB = llvm::BasicBlock::Create(context, "wcond", func);
    auto loopBB = llvm::BasicBlock::Create(context, "wloop", func);
    auto afterBB = llvm::BasicBlock::Create(context, "wafter", func);
    
    loopStack.push_back({condBB, afterBB, extractLabels(node.attributes)});
    
    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    builder.CreateCondBr(node.cond->accept(*this), loopBB, afterBB);
    
    builder.SetInsertPoint(loopBB);
    node.body->accept(*this);
    builder.CreateBr(condBB);
    
    loopStack.pop_back();
    builder.SetInsertPoint(afterBB);
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(ForRangeExpr& node) {
    llvm::Value* startVal = node.start->accept(*this);
    llvm::Value* endVal = node.end->accept(*this);
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    llvm::Type* varType = startVal ? startVal->getType() : getType("int");
    
    llvm::IRBuilder<> tmp(&func->getEntryBlock(), func->getEntryBlock().begin());
    auto alloca = tmp.CreateAlloca(varType, nullptr, "range_idx");
    builder.CreateStore(startVal, alloca);

    auto condBB = llvm::BasicBlock::Create(context, "fcond", func);
    auto loopBB = llvm::BasicBlock::Create(context, "floop", func);
    auto afterBB = llvm::BasicBlock::Create(context, "fafter", func);
    
    loopStack.push_back({condBB, afterBB, extractLabels(node.attributes)});
    
    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    
    auto curr = builder.CreateLoad(varType, alloca);
    auto cond = node.inclusive ? builder.CreateICmpSLE(curr, endVal) : builder.CreateICmpSLT(curr, endVal);
    builder.CreateCondBr(cond, loopBB, afterBB);
    
    builder.SetInsertPoint(loopBB);
    
    llvm::BasicBlock* patFailBB = llvm::BasicBlock::Create(context, "range_pat_fail", func);
    auto savedIP = builder.saveIP();
    builder.SetInsertPoint(patFailBB);
    builder.CreateBr(afterBB);
    builder.restoreIP(savedIP);

    generatePatternMatch(node.pattern, curr, patFailBB);
    node.body->accept(*this);
    
    llvm::Value* stepVal = llvm::ConstantInt::get(varType, 1);
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(varType, alloca), stepVal), alloca);
    builder.CreateBr(condBB);
    
    loopStack.pop_back();
    builder.SetInsertPoint(afterBB);
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(ForCStyleExpr& node) {
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    if (node.init) node.init->accept(*this);
    auto condBB = llvm::BasicBlock::Create(context, "ccond", func);
    auto stepBB = llvm::BasicBlock::Create(context, "cstep", func);
    auto loopBB = llvm::BasicBlock::Create(context, "cloop", func);
    auto afterBB = llvm::BasicBlock::Create(context, "cafter", func);
    
    loopStack.push_back({stepBB, afterBB, extractLabels(node.attributes)});
    
    builder.CreateBr(condBB);
    builder.SetInsertPoint(condBB);
    builder.CreateCondBr(node.cond ? node.cond->accept(*this) : builder.getInt1(true), loopBB, afterBB);
    
    builder.SetInsertPoint(loopBB);
    node.body->accept(*this);
    builder.CreateBr(stepBB);
    
    builder.SetInsertPoint(stepBB);
    if (node.step) node.step->accept(*this);
    builder.CreateBr(condBB);
    
    loopStack.pop_back();
    builder.SetInsertPoint(afterBB);
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(ForInExpr& node) {
    llvm::Value* iterable = node.iterable->accept(*this);
    if (!iterable) return nullptr;
    
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    auto afterBB = llvm::BasicBlock::Create(context, "iafter", func);
    auto condBB = llvm::BasicBlock::Create(context, "icond", func);
    
    loopStack.push_back({condBB, afterBB, extractLabels(node.attributes)});
    // Placeholder for actual iteration logic
    builder.CreateBr(afterBB);
    loopStack.pop_back();
    
    builder.SetInsertPoint(afterBB);
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(BreakStmt& node) {
    if (loopStack.empty()) return nullptr;
    if (node.label.empty()) {
        builder.CreateBr(loopStack.back().exitBB);
    } else {
        for (auto it = loopStack.rbegin(); it != loopStack.rend(); ++it) {
            for (const auto& L : it->labels) {
                if (L == node.label) {
                    builder.CreateBr(it->exitBB);
                    return nullptr;
                }
            }
        }
    }
    // Need a dummy block if we are in the middle of nowhere, but normally semantic analyzer catches this
    return nullptr;
}

llvm::Value* CodeGen::visit(ContinueStmt& node) {
    if (loopStack.empty()) return nullptr;
    if (node.label.empty()) {
        builder.CreateBr(loopStack.back().continueBB);
    } else {
        for (auto it = loopStack.rbegin(); it != loopStack.rend(); ++it) {
            for (const auto& L : it->labels) {
                if (L == node.label) {
                    builder.CreateBr(it->continueBB);
                    return nullptr;
                }
            }
        }
    }
    return nullptr;
}

llvm::Value* CodeGen::visit(ReturnStmt& node) {
    return lastValue = node.value ? builder.CreateRet(node.value->accept(*this)) : builder.CreateRetVoid();
}

llvm::Value* CodeGen::visit(ExprStmt& node) {
    return lastValue = node.expr->accept(*this);
}

llvm::Value* CodeGen::visit(GenericCallExpr& node) {
    std::string mangledName = node.callee;
    for (const auto& ta : node.typeArgs) {
        mangledName += "_" + ta;
    }

    llvm::Function* callee = module->getFunction(mangledName);
    if (!callee && genericFunctions.count(node.callee)) {
        FuncDecl* genFunc = genericFunctions[node.callee];
        if (genFunc->typeParams.size() == node.typeArgs.size()) {
            std::map<std::string, std::string> oldSubstitutions = typeSubstitutions;
            for (size_t i = 0; i < node.typeArgs.size(); ++i) {
                typeSubstitutions[genFunc->typeParams[i]] = node.typeArgs[i];
            }

            std::string oldName = genFunc->name;
            genFunc->name = mangledName;
            std::vector<std::string> savedTypeParams = genFunc->typeParams;
            genFunc->typeParams.clear(); // Treat as non-generic during instantiation

            llvm::BasicBlock* savedBlock = builder.GetInsertBlock();
            genFunc->accept(*this); // Generate IR for instantiated function
            builder.SetInsertPoint(savedBlock);

            // Restore AST
            genFunc->typeParams = savedTypeParams;
            genFunc->name = oldName;
            typeSubstitutions = oldSubstitutions;

            callee = module->getFunction(mangledName);
        }
    }

    if (!callee) return nullptr;

    std::vector<bool> mutFlags;
    if (funcMutParams.count(mangledName)) {
        mutFlags = funcMutParams[mangledName];
    } else {
        mutFlags.resize(node.args.size(), false);
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node.args.size(); ++i) {
        bool expectsMut = (i < mutFlags.size() && mutFlags[i]);
        if (expectsMut) {
            if (auto* varExpr = dynamic_cast<VarExpr*>(node.args[i].get())) {
                if (namedValues.count(varExpr->name)) {
                    auto& vInfo = namedValues[varExpr->name];
                    if (vInfo.isMut) {
                        args.push_back(vInfo.ptr);
                    } else {
                        llvm::Value* val = builder.CreateLoad(vInfo.type, vInfo.ptr);
                        llvm::AllocaInst* temp = builder.CreateAlloca(vInfo.type);
                        builder.CreateStore(val, temp);
                        args.push_back(temp);
                    }
                } else if (auto* gVar = module->getGlobalVariable(varExpr->name)) {
                    if (gVar->isConstant()) {
                        llvm::Value* val = builder.CreateLoad(gVar->getValueType(), gVar);
                        llvm::AllocaInst* temp = builder.CreateAlloca(gVar->getValueType());
                        builder.CreateStore(val, temp);
                        args.push_back(temp);
                    } else {
                        args.push_back(gVar);
                    }
                } else {
                    llvm::Value* val = node.args[i]->accept(*this);
                    llvm::AllocaInst* temp = builder.CreateAlloca(val->getType());
                    builder.CreateStore(val, temp);
                    args.push_back(temp);
                }
            } else {
                llvm::Value* val = node.args[i]->accept(*this);
                llvm::AllocaInst* temp = builder.CreateAlloca(val->getType());
                builder.CreateStore(val, temp);
                args.push_back(temp);
            }
        } else {
            args.push_back(node.args[i]->accept(*this));
        }
    }

    if (callee->getReturnType()->isVoidTy()) {
        builder.CreateCall(callee, args);
        return lastValue = nullptr;
    }
    return lastValue = builder.CreateCall(callee, args, "calltmp");
}

llvm::Value* CodeGen::visit(FuncDecl& node) {
    if (!node.typeParams.empty()) {
        genericFunctions[node.name] = &node;
        return nullptr;
    }
    std::vector<llvm::Type*> pts;
    std::vector<bool> mutFlags;
    for (auto& p : node.params) {
        mutFlags.push_back(p.isMutable);
        if (p.isMutable) {
            pts.push_back(llvm::PointerType::get(context, 0));
        } else if (p.isDynamic || p.type == "dyn") {
            pts.push_back(getType("dyn"));
        } else if (p.type.empty()) {
            pts.push_back(getType("dyn"));
        } else {
            pts.push_back(getType(p.type));
        }
    }
    funcMutParams[node.name] = mutFlags;
    
    llvm::Type* rt = nullptr;
    if (node.returnType == "dyn" || node.returnType.empty()) {
        rt = getType("dyn");
    } else {
        rt = getType(node.returnType);
    }

    llvm::FunctionType* FT = llvm::FunctionType::get(rt, pts, false);

    // Determine linkage based on visibility
    llvm::Function::LinkageTypes linkage = llvm::Function::ExternalLinkage;
    if (node.visibility == ASTVisibility::PRIVATE) {
        linkage = llvm::Function::InternalLinkage;
    }

    llvm::Function* func = llvm::Function::Create(FT, linkage, node.name, *module);
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", func));
    
    auto oldNamedValues = namedValues;
    namedValues.clear(); 
    auto oldVarSemanticTypes = varSemanticTypes;
    varSemanticTypes.clear();
    blockLastValue = nullptr;
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        std::string paramName = "arg" + std::to_string(idx);
        if (auto* vp = dynamic_cast<VarPattern*>(node.params[idx].pattern)) {
            paramName = vp->name;
        }
        arg.setName(paramName);
        
        if (node.params[idx].isMutable) {
            llvm::Type* valType = getType(node.params[idx].type.empty() ? "dyn" : node.params[idx].type);
            namedValues[paramName] = {&arg, valType, true};
        } else {
            llvm::BasicBlock* failBB = llvm::BasicBlock::Create(context, "param_fail", func);
            auto savedIP = builder.saveIP();
            builder.SetInsertPoint(failBB);
            llvm::Function* trapFn = llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::trap);
            builder.CreateCall(trapFn);
            builder.CreateUnreachable();
            builder.restoreIP(savedIP);

            generatePatternMatch(node.params[idx].pattern, &arg, failBB);
        }
        
        // Track semantic type for class-typed parameters (e.g. self: Animal)
        std::string paramType = node.params[idx].type;
        if (classDecls.count(paramType)) {
            varSemanticTypes[paramName] = paramType;
        }
        idx++;
    }
    
    node.body->accept(*this);
    llvm::Value* retVal = blockLastValue;
    if (!builder.GetInsertBlock()->getTerminator()) {
        if (rt->isVoidTy()) builder.CreateRetVoid();
        else builder.CreateRet(retVal ? retVal : llvm::Constant::getNullValue(rt));
    }
    
    namedValues = oldNamedValues;
    varSemanticTypes = oldVarSemanticTypes;
    builder.ClearInsertionPoint();
    return lastValue = func;
}

llvm::Value* CodeGen::visit(ModuleDeclStmt& node) {
    // Module declaration is purely declarative — no IR generated
    // The module name is already captured in Program::moduleName
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(UseStmt& node) {
    // Use statements are resolved before codegen by the ModuleResolver.
    // At codegen time, imported symbols are already registered as externals.
    // This is a no-op in IR generation.
    return lastValue = nullptr;
}

llvm::Value* CodeGen::visit(Program& node) {
    // Inject string runtime standard library declarations
    auto ptrTy = llvm::PointerType::get(context, 0);
    auto i32Ty = builder.getInt32Ty();
    auto boolTy = builder.getInt1Ty();
    module->getOrInsertFunction("string_length", llvm::FunctionType::get(i32Ty, {ptrTy}, false));
    module->getOrInsertFunction("string_concat", llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy}, false));
    module->getOrInsertFunction("string_substring", llvm::FunctionType::get(ptrTy, {ptrTy, i32Ty, i32Ty}, false));
    module->getOrInsertFunction("string_toUpper", llvm::FunctionType::get(ptrTy, {ptrTy}, false));
    module->getOrInsertFunction("string_toLower", llvm::FunctionType::get(ptrTy, {ptrTy}, false));
    module->getOrInsertFunction("string_indexOf", llvm::FunctionType::get(i32Ty, {ptrTy, ptrTy}, false));
    module->getOrInsertFunction("string_contains", llvm::FunctionType::get(boolTy, {ptrTy, ptrTy}, false));
    module->getOrInsertFunction("string_startsWith", llvm::FunctionType::get(boolTy, {ptrTy, ptrTy}, false));
    module->getOrInsertFunction("string_endsWith", llvm::FunctionType::get(boolTy, {ptrTy, ptrTy}, false));
    module->getOrInsertFunction("string_trim", llvm::FunctionType::get(ptrTy, {ptrTy}, false));
    module->getOrInsertFunction("string_replace", llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy, ptrTy}, false));

    // Just generate code for all statements
    for (auto& stmt : node.statements) stmt->accept(*this);
    return lastValue = nullptr;
}

std::string CodeGen::getTypeNameFromLLVM(llvm::Type* t) {
    if (t->isIntegerTy()) return "int";
    if (t->isFloatingPointTy()) return "float";
    if (t->isPointerTy()) return "string"; // Use 'string' so C runtime methods like 'string_length' hook up to pointers!
    if (t->isStructTy()) {
        llvm::StructType* st = llvm::cast<llvm::StructType>(t);
        if (st->hasName()) return st->getName().str();
    }
    return "unknown";
}

llvm::Value* CodeGen::generateToString(llvm::Value* val) {
    llvm::Type* t = val->getType();
    if (t->isPointerTy()) return lastValue = val;
    
    llvm::FunctionType* snprintfType = llvm::FunctionType::get(
        builder.getInt32Ty(), 
        {llvm::PointerType::get(context, 0), builder.getInt64Ty(), llvm::PointerType::get(context, 0)}, 
        true);
    llvm::Function* snprintfFunc = module->getFunction("snprintf");
    if (!snprintfFunc) {
        snprintfFunc = llvm::Function::Create(snprintfType, llvm::Function::ExternalLinkage, "snprintf", *module);
    }
    
    llvm::FunctionType* mallocType = llvm::FunctionType::get(llvm::PointerType::get(context, 0), {builder.getInt64Ty()}, false);
    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    }

    llvm::Value* size = builder.getInt64(32);
    llvm::Value* buf = builder.CreateCall(mallocFunc, {size}, "buf");
    
    llvm::Value* formatStr = nullptr;
    if (t->isIntegerTy()) {
        formatStr = builder.CreateGlobalString("%ld", "", 0, module.get());
        val = builder.CreateIntCast(val, builder.getInt64Ty(), true);
    } else if (t->isFloatingPointTy()) {
        formatStr = builder.CreateGlobalString("%f", "", 0, module.get());
        val = builder.CreateFPCast(val, builder.getDoubleTy());
    } else {
        formatStr = builder.CreateGlobalString("?", "", 0, module.get());
    }
    
    builder.CreateCall(snprintfFunc, {buf, size, formatStr, val});
    return lastValue = buf;
}

llvm::Value* CodeGen::visit(MethodCallExpr& node) {
    llvm::Value* obj = node.object->accept(*this);
    std::string typeName = node.object->semanticType;
    
    // If obj is null but we have a typeName, it's a static class call (e.g. Math.max())
    bool isStatic = (obj == nullptr && !typeName.empty() && classDecls.count(typeName));
    
    if (!obj && !isStatic) return nullptr;
    
    if (typeName.empty() && obj) {
        typeName = getTypeNameFromLLVM(obj->getType());
    }
    std::string funcName = typeName + "_" + node.methodName;
    
    llvm::Function* callee = module->getFunction(funcName);
    
    // Walk inheritance chain to find inherited methods
    if (!callee && classDecls.count(typeName)) {
        std::string searchType = typeName;
        while (!callee && classDecls.count(searchType)) {
            ClassDecl* cls = classDecls[searchType];
            if (!cls->baseAndInterfaces.empty()) {
                searchType = cls->baseAndInterfaces[0];
                callee = module->getFunction(searchType + "_" + node.methodName);
            } else {
                break;
            }
        }
    }
    
    if (!callee) callee = module->getFunction(node.methodName);
    
    if (!callee && node.methodName == "toString" && obj) {
        return generateToString(obj);
    }
    
    if (!callee) {
        std::cerr << "error: unknown method '" << node.methodName << "' on type '" << typeName << "'\n";
        return nullptr;
    }

    // Check private method access
    if (classDecls.count(typeName)) {
        std::string searchType = typeName;
        FuncDecl* foundMethod = nullptr;
        while (!searchType.empty() && classDecls.count(searchType)) {
            ClassDecl* cls = classDecls[searchType];
            for (auto& m : cls->methods) {
                if (m->name == searchType + "_" + node.methodName) {
                    foundMethod = m.get();
                    break;
                }
            }
            if (foundMethod) break;
            searchType = cls->baseAndInterfaces.empty() ? "" : cls->baseAndInterfaces[0];
        }
        
        if (foundMethod && foundMethod->visibility == ASTVisibility::PRIVATE) {
            bool canAccess = false;
            if (varSemanticTypes.count("self") > 0) {
                std::string currentClass = varSemanticTypes["self"];
                std::string st = currentClass;
                while (!st.empty() && classDecls.count(st)) {
                    if (st == searchType) { canAccess = true; break; }
                    ClassDecl* c = classDecls[st];
                    st = c->baseAndInterfaces.empty() ? "" : c->baseAndInterfaces[0];
                }
            }
            if (!canAccess) {
                std::cerr << "error: method '" << node.methodName << "' is private to '" << searchType << "'\n";
                return nullptr;
            }
        }
    }
    
    std::vector<llvm::Value*> args;
    if (!isStatic) {
        args.push_back(obj);
    }
    for (auto& arg : node.args) args.push_back(arg->accept(*this));
    
    if (callee->getReturnType()->isVoidTy()) {
        builder.CreateCall(callee, args);
        return lastValue = nullptr;
    }
    return lastValue = builder.CreateCall(callee, args, "methodcall");
}

llvm::Value* CodeGen::visit(SuperCallExpr& node) {
    if (varSemanticTypes.count("self") == 0) {
        std::cerr << "error: 'super' can only be used inside class methods\n";
        return nullptr;
    }
    std::string currentClass = varSemanticTypes["self"];
    if (!classDecls.count(currentClass) || classDecls[currentClass]->baseAndInterfaces.empty()) {
        std::cerr << "error: class '" << currentClass << "' has no superclass\n";
        return nullptr;
    }
    std::string parentClass = classDecls[currentClass]->baseAndInterfaces[0];
    std::string funcName = parentClass + "_" + node.methodName;
    
    llvm::Function* callee = module->getFunction(funcName);
    
    // Walk further up if not in immediate parent
    if (!callee) {
        std::string searchType = parentClass;
        while (!callee && classDecls.count(searchType)) {
            ClassDecl* cls = classDecls[searchType];
            if (!cls->baseAndInterfaces.empty()) {
                searchType = cls->baseAndInterfaces[0];
                callee = module->getFunction(searchType + "_" + node.methodName);
            } else {
                break;
            }
        }
    }
    
    if (!callee) {
        std::cerr << "error: unknown method '" << node.methodName << "' in superclass chain of '" << currentClass << "'\n";
        return nullptr;
    }
    
    std::vector<llvm::Value*> args;
    // Load 'self' pointer
    auto& info = namedValues["self"];
    llvm::Value* selfObj = builder.CreateLoad(info.type, info.ptr, "self");
    args.push_back(selfObj);
    
    for (auto& arg : node.args) args.push_back(arg->accept(*this));
    
    if (callee->getReturnType()->isVoidTy()) {
        builder.CreateCall(callee, args);
        return lastValue = nullptr;
    }
    return lastValue = builder.CreateCall(callee, args, "supercall");
}

llvm::Value* CodeGen::visit(PropertyExpr& node) {
    llvm::Value* obj = node.object->accept(*this);
    if (!obj) return nullptr;

    std::string semanticType = node.object->semanticType;
    if (obj->getType()->isStructTy()) {
        llvm::StructType* structType = llvm::cast<llvm::StructType>(obj->getType());
        std::string structName = structType->getName().str();
        if (structDecls.count(structName)) {
            StructDecl* decl = structDecls[structName];
            for (size_t i = 0; i < decl->fields.size(); ++i) {
                if (decl->fields[i].name == node.propertyName) {
                    return lastValue = builder.CreateExtractValue(obj, i, "prop");
                }
            }
        }
    } else if (obj->getType()->isPointerTy() && classDecls.count(semanticType)) {
        ClassDecl* decl = classDecls[semanticType];
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            if (decl->fields[i].name == node.propertyName) {
                if (decl->fields[i].isPrivate) {
                    bool canAccess = false;
                    if (varSemanticTypes.count("self") > 0) {
                        std::string currentClass = varSemanticTypes["self"];
                        // Allowed if we are inside a method of this class or a subclass
                        std::string searchType = currentClass;
                        while (!searchType.empty() && classDecls.count(searchType)) {
                            if (searchType == semanticType) { canAccess = true; break; }
                            ClassDecl* c = classDecls[searchType];
                            searchType = c->baseAndInterfaces.empty() ? "" : c->baseAndInterfaces[0];
                        }
                    }
                    if (!canAccess) {
                        std::cerr << "error: property '" << node.propertyName << "' is private to '" << semanticType << "'\n";
                        return nullptr;
                    }
                }
                // For classes, obj is a pointer to the struct.
                // We use GetElementPtr to get the pointer to the field, then load it.
                llvm::StructType* structType = classTypes[semanticType];
                llvm::Value* ptr = builder.CreateStructGEP(structType, obj, i, "prop_ptr");
                llvm::Type* fieldType = getType(decl->fields[i].type);
                return lastValue = builder.CreateLoad(fieldType, ptr, "prop");
            }
        }
    }

    std::string typeName = getTypeNameFromLLVM(obj->getType());
    std::string funcName = typeName + "_" + node.propertyName;
    llvm::Function* callee = module->getFunction(funcName);
    if (!callee) callee = module->getFunction(node.propertyName);
    if (callee) {
        std::vector<llvm::Value*> args;
        args.push_back(obj);
        return lastValue = builder.CreateCall(callee, args, "propcall");
    }
    std::cerr << "error: unknown property '" << node.propertyName << "' on type '" << typeName << "'\n";
    return nullptr;
}

llvm::Value* CodeGen::visit(CastExpr& node) {
    llvm::Value* val = node.expr->accept(*this);
    if (!val) return nullptr;
    llvm::Type* srcType = val->getType();
    llvm::Type* dstType = getType(node.targetType);

    if (srcType == dstType) return lastValue = val;

    if (node.isForced) {
        if (srcType->isPointerTy() && dstType->isIntegerTy()) {
            return lastValue = builder.CreatePtrToInt(val, dstType, "cast");
        }
        if (srcType->isIntegerTy() && dstType->isPointerTy()) {
            return lastValue = builder.CreateIntToPtr(val, dstType, "cast");
        }
        if (srcType->isIntegerTy() && dstType->isIntegerTy()) {
            return lastValue = builder.CreateIntCast(val, dstType, true, "cast");
        }
        if (srcType->isFloatingPointTy() && dstType->isFloatingPointTy()) {
            return lastValue = builder.CreateFPCast(val, dstType, "cast");
        }
        return lastValue = builder.CreateBitCast(val, dstType, "cast");
    } else {
        if (srcType->isIntegerTy() && dstType->isIntegerTy()) {
            unsigned srcBits = srcType->getIntegerBitWidth();
            unsigned dstBits = dstType->getIntegerBitWidth();
            if (dstBits < srcBits) {
                std::cerr << "warning: safe cast from i" << srcBits << " to i" << dstBits 
                          << " may truncate value. Use 'as!' to silence.\n";
            }
            return lastValue = builder.CreateIntCast(val, dstType, true, "cast");
        }
        if (srcType->isIntegerTy() && dstType->isFloatingPointTy()) {
            return lastValue = builder.CreateSIToFP(val, dstType, "cast");
        }
        if (srcType->isFloatingPointTy() && dstType->isIntegerTy()) {
            return lastValue = builder.CreateFPToSI(val, dstType, "cast");
        }
        if (srcType->isFloatingPointTy() && dstType->isFloatingPointTy()) {
            if (srcType->getFPMantissaWidth() > dstType->getFPMantissaWidth()) {
                std::cerr << "warning: precision loss in float cast.\n";
            }
            return lastValue = builder.CreateFPCast(val, dstType, "cast");
        }
        if (srcType->isPointerTy() && dstType->isPointerTy()) {
            return lastValue = val;
        }

        std::cerr << "error: unsafe cast from " << getTypeNameFromLLVM(srcType) 
                  << " to " << node.targetType << ". Use 'as!' to force.\n";
        return lastValue = nullptr;
    }
}

llvm::Function* CodeGen::getOrCreateStringConcat() {
    llvm::Function* f = module->getFunction("string_concat");
    if (!f) {
        llvm::FunctionType* ft = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)},
            false
        );
        f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "string_concat", *module);
    }
    return f;
}

llvm::Value* CodeGen::visit(StringInterpolationExpr& node) {
    if (node.parts.empty()) return lastValue = builder.CreateGlobalString("", "", 0, module.get());
    
    llvm::Value* res = node.parts[0]->accept(*this);
    if (!res) return nullptr;
    if (!res->getType()->isPointerTy()) {
        res = generateToString(res);
    }
    
    llvm::Function* concatFn = getOrCreateStringConcat();
    for (size_t i = 1; i < node.parts.size(); ++i) {
        llvm::Value* partVal = node.parts[i]->accept(*this);
        if (!partVal) continue;
        if (!partVal->getType()->isPointerTy()) {
            partVal = generateToString(partVal);
        }
        res = builder.CreateCall(concatFn, {res, partVal}, "interp");
    }
    return lastValue = res;
}

llvm::Value* CodeGen::visit(StructDecl& node) {
    std::vector<llvm::Type*> fieldTypes;
    for (auto& field : node.fields) {
        fieldTypes.push_back(getType(field.type));
    }
    llvm::StructType* structType = llvm::StructType::create(context, fieldTypes, node.name);
    structTypes[node.name] = structType;
    structDecls[node.name] = &node;
    return nullptr;
}

llvm::Value* CodeGen::visit(ClassDecl& node) {
    // 1. Collect inherited fields from base class (single inheritance)
    //    Base classes are already processed (compilation order), so their
    //    fields already include all ancestor fields. Just copy them directly.
    std::vector<ClassField> ownFields = node.fields; // save original
    std::vector<ClassField> allFields;
    if (!node.baseAndInterfaces.empty()) {
        std::string baseName = node.baseAndInterfaces[0];
        if (classDecls.count(baseName)) {
            ClassDecl* baseDecl = classDecls[baseName];
            // Base's fields already include all inherited ancestor fields
            allFields = baseDecl->fields;
        }
    }
    // Add this class's own fields
    for (auto& f : ownFields) {
        allFields.push_back(f);
    }
    // Replace the node's fields with the full inherited set
    node.fields = allFields;

    // 2. Create the struct type for the class (state)
    std::vector<llvm::Type*> fieldTypes;
    for (auto& field : node.fields) {
        fieldTypes.push_back(getType(field.type));
    }
    llvm::StructType* structType = llvm::StructType::create(context, fieldTypes, node.name + "_class");
    classTypes[node.name] = structType;
    classDecls[node.name] = &node;
    
    // 3. Generate all the methods
    for (auto& method : node.methods) {
        method->accept(*this);
    }
    return nullptr;
}

llvm::Value* CodeGen::visit(InterfaceDecl& node) {
    // Interfaces don't emit code themselves, they are for semantic checking and vtables
    return nullptr;
}

llvm::Value* CodeGen::visit(StructInstExpr& node) {
    if (!structTypes.count(node.structName)) {
        return nullptr;
    }
    llvm::StructType* type = structTypes[node.structName];
    StructDecl* decl = structDecls[node.structName];
    
    llvm::Value* alloc = builder.CreateAlloca(type, nullptr, "structtmp");
    
    // Evaluate fields and store them in the struct
    for (size_t i = 0; i < node.fields.size(); ++i) {
        const std::string& fieldName = node.fields[i].first;
        Expr* expr = node.fields[i].second.get();
        
        int fieldIndex = -1;
        for (size_t j = 0; j < decl->fields.size(); ++j) {
            if (decl->fields[j].name == fieldName) {
                fieldIndex = j;
                break;
            }
        }
        
        if (fieldIndex != -1) {
            llvm::Value* val = expr->accept(*this);
            llvm::Value* gep = builder.CreateStructGEP(type, alloc, fieldIndex);
            builder.CreateStore(val, gep);
        }
    }
    
    // Return the struct value by loading it from the alloca
    return lastValue = builder.CreateLoad(type, alloc);
}


// ─────────────────────────────────────────────────────────
//  New features implementation
// ─────────────────────────────────────────────────────────

llvm::Value* CodeGen::visit(VarPattern& node) {
    auto alloca = builder.CreateAlloca(matchValue->getType(), nullptr, node.name);
    builder.CreateStore(matchValue, alloca);
    namedValues[node.name] = {alloca, matchValue->getType(), true};
    return matchValue;
}

llvm::Value* CodeGen::visit(TuplePattern& node) {
    if (!matchValue->getType()->isStructTy()) {
        builder.CreateBr(matchFailBB);
        return nullptr;
    }
    llvm::Value* originalMatchValue = matchValue;
    for (size_t i = 0; i < node.patterns.size(); ++i) {
        matchValue = builder.CreateExtractValue(originalMatchValue, (unsigned)i);
        node.patterns[i]->accept(*this);
    }
    matchValue = originalMatchValue;
    return matchValue;
}

llvm::Value* CodeGen::visit(StructPattern& node) {
    if (!matchValue->getType()->isStructTy()) {
        builder.CreateBr(matchFailBB);
        return nullptr;
    }
    llvm::Value* originalMatchValue = matchValue;
    for (size_t i = 0; i < node.fields.size(); ++i) {
        matchValue = builder.CreateExtractValue(originalMatchValue, (unsigned)i);
        node.fields[i].second->accept(*this);
    }
    matchValue = originalMatchValue;
    return matchValue;
}

llvm::Value* CodeGen::visit(ArrayPattern& node) {
    builder.CreateBr(matchFailBB);
    return nullptr;
}

llvm::Value* CodeGen::visit(WildcardPattern& node) {
    return matchValue;
}

llvm::Value* CodeGen::visit(ConstantPattern& node) {
    llvm::Value* patVal = node.expr->accept(*this);
    llvm::Value* cond = builder.CreateICmpEQ(matchValue, patVal);
    llvm::BasicBlock* nextBB = llvm::BasicBlock::Create(context, "matchnext", builder.GetInsertBlock()->getParent());
    builder.CreateCondBr(cond, nextBB, matchFailBB);
    builder.SetInsertPoint(nextBB);
    return matchValue;
}

void CodeGen::generatePatternMatch(Pattern* p, llvm::Value* val, llvm::BasicBlock* failBB) {
    matchValue = val;
    matchFailBB = failBB;
    p->accept(*this);
}

llvm::Value* CodeGen::visit(EnumDecl& node) {
    std::vector<llvm::Type*> elements;
    elements.push_back(builder.getInt32Ty()); // tag
    elements.push_back(builder.getInt64Ty()); // payload
    llvm::StructType* st = llvm::StructType::create(context, elements, node.name);
    enumTypes[node.name] = st;
    enumDecls[node.name] = &node;
    return nullptr;
}

std::vector<std::string> CodeGen::extractLabels(const std::vector<Expr*>& attrs) {
    std::vector<std::string> labels;
    for (auto* attr : attrs) {
        if (auto* var = dynamic_cast<VarExpr*>(attr)) labels.push_back(var->name);
    }
    return labels;
}

} // namespace luv

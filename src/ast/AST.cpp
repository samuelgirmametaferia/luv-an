#include "ast/AST.h"

namespace luv {

llvm::Value* IntExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* FloatExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* StringExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* CharExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* BoolExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* NullExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* VarExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* BinaryExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* UnaryExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* CallExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* GenericCallExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* StructInstExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* IntrinsicCallExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* AsmExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* MethodCallExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* SuperCallExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* PropertyExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* CastExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* TernaryExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ComparisonChainExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* StmtExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* StringInterpolationExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* Block::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* MatchExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* VarDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* Assignment::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* IfExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* WhileExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ForRangeExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ForCStyleExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ForInExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ReturnStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* BreakStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ContinueStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ExprStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* FuncDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* StructDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ClassDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* InterfaceDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ExternDecl::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ModuleDeclStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* IndexExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* PostfixExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* UseStmt::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* Program::accept(ASTVisitor& visitor) { return visitor.visit(*this); }

llvm::Value* ArrayExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ArrayRepeatExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* ArrayCompExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* TupleExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }
llvm::Value* SliceExpr::accept(ASTVisitor& visitor) { return visitor.visit(*this); }

} // namespace luv

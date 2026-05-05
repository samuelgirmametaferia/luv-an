
// Generated from src/Luv.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuvVisitor.h"


/**
 * This class provides an empty implementation of LuvVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  LuvBaseVisitor : public LuvVisitor {
public:

  virtual std::any visitProgram(LuvParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTopLevel(LuvParser::TopLevelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModuleDecl(LuvParser::ModuleDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseFromStmt(LuvParser::UseFromStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUsePathStmt(LuvParser::UsePathStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseAllPublic(LuvParser::UseAllPublicContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseAllPrivate(LuvParser::UseAllPrivateContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseSingle(LuvParser::UseSingleContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseSet(LuvParser::UseSetContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseList(LuvParser::UseListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModulePath(LuvParser::ModulePathContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVisibilityDecl(LuvParser::VisibilityDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternDecl(LuvParser::ExternDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(LuvParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(LuvParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(LuvParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(LuvParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDecl(LuvParser::StructDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructField(LuvParser::StructFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassDecl(LuvParser::ClassDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassMember(LuvParser::ClassMemberContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassField(LuvParser::ClassFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterfaceDecl(LuvParser::InterfaceDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterfaceMember(LuvParser::InterfaceMemberContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarDecl(LuvParser::VarDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBindingPattern(LuvParser::BindingPatternContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTuplePattern(LuvParser::TuplePatternContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModifier(LuvParser::ModifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttribute(LuvParser::AttributeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemoryHint(LuvParser::MemoryHintContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttrList(LuvParser::AttrListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttr(LuvParser::AttrContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOverloadableOp(LuvParser::OverloadableOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncName(LuvParser::FuncNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockFunc(LuvParser::BlockFuncContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowFunc(LuvParser::ArrowFuncContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeParams(LuvParser::TypeParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParams(LuvParser::ParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(LuvParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitType(LuvParser::TypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeCore(LuvParser::TypeCoreContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfExpr(LuvParser::IfExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEfExpr(LuvParser::EfExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileExpr(LuvParser::WhileExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForRangeExpr(LuvParser::ForRangeExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForRangeIncExpr(LuvParser::ForRangeIncExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForInExpr(LuvParser::ForInExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForCStyle(LuvParser::ForCStyleContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(LuvParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(LuvParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructInstFields(LuvParser::StructInstFieldsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentTarget(LuvParser::AssignmentTargetContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignment(LuvParser::AssignmentContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpr(LuvParser::CastExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitwiseOrExpr(LuvParser::BitwiseOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericCallExpr(LuvParser::GenericCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMatchExpr(LuvParser::MatchExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitwiseAndExpr(LuvParser::BitwiseAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAsmExpr(LuvParser::AsmExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAdditiveExpr(LuvParser::AdditiveExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMethodCallExpr(LuvParser::MethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryExpr(LuvParser::UnaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimaryExpr(LuvParser::PrimaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTernaryExpr(LuvParser::TernaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPropertyExpr(LuvParser::PropertyExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSuperCallExpr(LuvParser::SuperCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfExprAlternative(LuvParser::IfExprAlternativeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfixExpr(LuvParser::PostfixExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallExpr(LuvParser::CallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForExprAlternative(LuvParser::ForExprAlternativeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitwiseXorExpr(LuvParser::BitwiseXorExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalAndExpr(LuvParser::LogicalAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitComparisonExpr(LuvParser::ComparisonExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSliceExpr(LuvParser::SliceExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShiftExpr(LuvParser::ShiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalOrExpr(LuvParser::LogicalOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(LuvParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileExprAlternative(LuvParser::WhileExprAlternativeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMultiplicativeExpr(LuvParser::MultiplicativeExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructInstExpr(LuvParser::StructInstExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMatchCase(LuvParser::MatchCaseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgs(LuvParser::ArgsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntLiteral(LuvParser::IntLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFloatLiteral(LuvParser::FloatLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStringLiteral(LuvParser::StringLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStringInterpolationExpr(LuvParser::StringInterpolationExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCharLiteral(LuvParser::CharLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBoolLiteral(LuvParser::BoolLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNullLiteral(LuvParser::NullLiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdentifier(LuvParser::IdentifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGroupingExpr(LuvParser::GroupingExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleExpr(LuvParser::TupleExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayExpr(LuvParser::ArrayExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayRepeatExpr(LuvParser::ArrayRepeatExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayCompExpr(LuvParser::ArrayCompExprContext *ctx) override {
    return visitChildren(ctx);
  }


};


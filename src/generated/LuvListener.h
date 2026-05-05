
// Generated from src/Luv.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuvParser.h"


/**
 * This interface defines an abstract listener for a parse tree produced by LuvParser.
 */
class  LuvListener : public antlr4::tree::ParseTreeListener {
public:

  virtual void enterProgram(LuvParser::ProgramContext *ctx) = 0;
  virtual void exitProgram(LuvParser::ProgramContext *ctx) = 0;

  virtual void enterTopLevel(LuvParser::TopLevelContext *ctx) = 0;
  virtual void exitTopLevel(LuvParser::TopLevelContext *ctx) = 0;

  virtual void enterModuleDecl(LuvParser::ModuleDeclContext *ctx) = 0;
  virtual void exitModuleDecl(LuvParser::ModuleDeclContext *ctx) = 0;

  virtual void enterUseFromStmt(LuvParser::UseFromStmtContext *ctx) = 0;
  virtual void exitUseFromStmt(LuvParser::UseFromStmtContext *ctx) = 0;

  virtual void enterUsePathStmt(LuvParser::UsePathStmtContext *ctx) = 0;
  virtual void exitUsePathStmt(LuvParser::UsePathStmtContext *ctx) = 0;

  virtual void enterUseAllPublic(LuvParser::UseAllPublicContext *ctx) = 0;
  virtual void exitUseAllPublic(LuvParser::UseAllPublicContext *ctx) = 0;

  virtual void enterUseAllPrivate(LuvParser::UseAllPrivateContext *ctx) = 0;
  virtual void exitUseAllPrivate(LuvParser::UseAllPrivateContext *ctx) = 0;

  virtual void enterUseSingle(LuvParser::UseSingleContext *ctx) = 0;
  virtual void exitUseSingle(LuvParser::UseSingleContext *ctx) = 0;

  virtual void enterUseSet(LuvParser::UseSetContext *ctx) = 0;
  virtual void exitUseSet(LuvParser::UseSetContext *ctx) = 0;

  virtual void enterUseList(LuvParser::UseListContext *ctx) = 0;
  virtual void exitUseList(LuvParser::UseListContext *ctx) = 0;

  virtual void enterModulePath(LuvParser::ModulePathContext *ctx) = 0;
  virtual void exitModulePath(LuvParser::ModulePathContext *ctx) = 0;

  virtual void enterVisibilityDecl(LuvParser::VisibilityDeclContext *ctx) = 0;
  virtual void exitVisibilityDecl(LuvParser::VisibilityDeclContext *ctx) = 0;

  virtual void enterExternDecl(LuvParser::ExternDeclContext *ctx) = 0;
  virtual void exitExternDecl(LuvParser::ExternDeclContext *ctx) = 0;

  virtual void enterStatement(LuvParser::StatementContext *ctx) = 0;
  virtual void exitStatement(LuvParser::StatementContext *ctx) = 0;

  virtual void enterBreakStmt(LuvParser::BreakStmtContext *ctx) = 0;
  virtual void exitBreakStmt(LuvParser::BreakStmtContext *ctx) = 0;

  virtual void enterContinueStmt(LuvParser::ContinueStmtContext *ctx) = 0;
  virtual void exitContinueStmt(LuvParser::ContinueStmtContext *ctx) = 0;

  virtual void enterBlock(LuvParser::BlockContext *ctx) = 0;
  virtual void exitBlock(LuvParser::BlockContext *ctx) = 0;

  virtual void enterStructDecl(LuvParser::StructDeclContext *ctx) = 0;
  virtual void exitStructDecl(LuvParser::StructDeclContext *ctx) = 0;

  virtual void enterStructField(LuvParser::StructFieldContext *ctx) = 0;
  virtual void exitStructField(LuvParser::StructFieldContext *ctx) = 0;

  virtual void enterClassDecl(LuvParser::ClassDeclContext *ctx) = 0;
  virtual void exitClassDecl(LuvParser::ClassDeclContext *ctx) = 0;

  virtual void enterClassMember(LuvParser::ClassMemberContext *ctx) = 0;
  virtual void exitClassMember(LuvParser::ClassMemberContext *ctx) = 0;

  virtual void enterClassField(LuvParser::ClassFieldContext *ctx) = 0;
  virtual void exitClassField(LuvParser::ClassFieldContext *ctx) = 0;

  virtual void enterInterfaceDecl(LuvParser::InterfaceDeclContext *ctx) = 0;
  virtual void exitInterfaceDecl(LuvParser::InterfaceDeclContext *ctx) = 0;

  virtual void enterInterfaceMember(LuvParser::InterfaceMemberContext *ctx) = 0;
  virtual void exitInterfaceMember(LuvParser::InterfaceMemberContext *ctx) = 0;

  virtual void enterVarDecl(LuvParser::VarDeclContext *ctx) = 0;
  virtual void exitVarDecl(LuvParser::VarDeclContext *ctx) = 0;

  virtual void enterBindingPattern(LuvParser::BindingPatternContext *ctx) = 0;
  virtual void exitBindingPattern(LuvParser::BindingPatternContext *ctx) = 0;

  virtual void enterTuplePattern(LuvParser::TuplePatternContext *ctx) = 0;
  virtual void exitTuplePattern(LuvParser::TuplePatternContext *ctx) = 0;

  virtual void enterModifier(LuvParser::ModifierContext *ctx) = 0;
  virtual void exitModifier(LuvParser::ModifierContext *ctx) = 0;

  virtual void enterAttribute(LuvParser::AttributeContext *ctx) = 0;
  virtual void exitAttribute(LuvParser::AttributeContext *ctx) = 0;

  virtual void enterMemoryHint(LuvParser::MemoryHintContext *ctx) = 0;
  virtual void exitMemoryHint(LuvParser::MemoryHintContext *ctx) = 0;

  virtual void enterAttrList(LuvParser::AttrListContext *ctx) = 0;
  virtual void exitAttrList(LuvParser::AttrListContext *ctx) = 0;

  virtual void enterAttr(LuvParser::AttrContext *ctx) = 0;
  virtual void exitAttr(LuvParser::AttrContext *ctx) = 0;

  virtual void enterOverloadableOp(LuvParser::OverloadableOpContext *ctx) = 0;
  virtual void exitOverloadableOp(LuvParser::OverloadableOpContext *ctx) = 0;

  virtual void enterFuncName(LuvParser::FuncNameContext *ctx) = 0;
  virtual void exitFuncName(LuvParser::FuncNameContext *ctx) = 0;

  virtual void enterBlockFunc(LuvParser::BlockFuncContext *ctx) = 0;
  virtual void exitBlockFunc(LuvParser::BlockFuncContext *ctx) = 0;

  virtual void enterArrowFunc(LuvParser::ArrowFuncContext *ctx) = 0;
  virtual void exitArrowFunc(LuvParser::ArrowFuncContext *ctx) = 0;

  virtual void enterTypeParams(LuvParser::TypeParamsContext *ctx) = 0;
  virtual void exitTypeParams(LuvParser::TypeParamsContext *ctx) = 0;

  virtual void enterParams(LuvParser::ParamsContext *ctx) = 0;
  virtual void exitParams(LuvParser::ParamsContext *ctx) = 0;

  virtual void enterParam(LuvParser::ParamContext *ctx) = 0;
  virtual void exitParam(LuvParser::ParamContext *ctx) = 0;

  virtual void enterType(LuvParser::TypeContext *ctx) = 0;
  virtual void exitType(LuvParser::TypeContext *ctx) = 0;

  virtual void enterTypeCore(LuvParser::TypeCoreContext *ctx) = 0;
  virtual void exitTypeCore(LuvParser::TypeCoreContext *ctx) = 0;

  virtual void enterIfExpr(LuvParser::IfExprContext *ctx) = 0;
  virtual void exitIfExpr(LuvParser::IfExprContext *ctx) = 0;

  virtual void enterEfExpr(LuvParser::EfExprContext *ctx) = 0;
  virtual void exitEfExpr(LuvParser::EfExprContext *ctx) = 0;

  virtual void enterWhileExpr(LuvParser::WhileExprContext *ctx) = 0;
  virtual void exitWhileExpr(LuvParser::WhileExprContext *ctx) = 0;

  virtual void enterForRangeExpr(LuvParser::ForRangeExprContext *ctx) = 0;
  virtual void exitForRangeExpr(LuvParser::ForRangeExprContext *ctx) = 0;

  virtual void enterForRangeIncExpr(LuvParser::ForRangeIncExprContext *ctx) = 0;
  virtual void exitForRangeIncExpr(LuvParser::ForRangeIncExprContext *ctx) = 0;

  virtual void enterForInExpr(LuvParser::ForInExprContext *ctx) = 0;
  virtual void exitForInExpr(LuvParser::ForInExprContext *ctx) = 0;

  virtual void enterForCStyle(LuvParser::ForCStyleContext *ctx) = 0;
  virtual void exitForCStyle(LuvParser::ForCStyleContext *ctx) = 0;

  virtual void enterReturnStmt(LuvParser::ReturnStmtContext *ctx) = 0;
  virtual void exitReturnStmt(LuvParser::ReturnStmtContext *ctx) = 0;

  virtual void enterExprStmt(LuvParser::ExprStmtContext *ctx) = 0;
  virtual void exitExprStmt(LuvParser::ExprStmtContext *ctx) = 0;

  virtual void enterStructInstFields(LuvParser::StructInstFieldsContext *ctx) = 0;
  virtual void exitStructInstFields(LuvParser::StructInstFieldsContext *ctx) = 0;

  virtual void enterAssignmentTarget(LuvParser::AssignmentTargetContext *ctx) = 0;
  virtual void exitAssignmentTarget(LuvParser::AssignmentTargetContext *ctx) = 0;

  virtual void enterAssignment(LuvParser::AssignmentContext *ctx) = 0;
  virtual void exitAssignment(LuvParser::AssignmentContext *ctx) = 0;

  virtual void enterCastExpr(LuvParser::CastExprContext *ctx) = 0;
  virtual void exitCastExpr(LuvParser::CastExprContext *ctx) = 0;

  virtual void enterBitwiseOrExpr(LuvParser::BitwiseOrExprContext *ctx) = 0;
  virtual void exitBitwiseOrExpr(LuvParser::BitwiseOrExprContext *ctx) = 0;

  virtual void enterGenericCallExpr(LuvParser::GenericCallExprContext *ctx) = 0;
  virtual void exitGenericCallExpr(LuvParser::GenericCallExprContext *ctx) = 0;

  virtual void enterMatchExpr(LuvParser::MatchExprContext *ctx) = 0;
  virtual void exitMatchExpr(LuvParser::MatchExprContext *ctx) = 0;

  virtual void enterBitwiseAndExpr(LuvParser::BitwiseAndExprContext *ctx) = 0;
  virtual void exitBitwiseAndExpr(LuvParser::BitwiseAndExprContext *ctx) = 0;

  virtual void enterAsmExpr(LuvParser::AsmExprContext *ctx) = 0;
  virtual void exitAsmExpr(LuvParser::AsmExprContext *ctx) = 0;

  virtual void enterAdditiveExpr(LuvParser::AdditiveExprContext *ctx) = 0;
  virtual void exitAdditiveExpr(LuvParser::AdditiveExprContext *ctx) = 0;

  virtual void enterMethodCallExpr(LuvParser::MethodCallExprContext *ctx) = 0;
  virtual void exitMethodCallExpr(LuvParser::MethodCallExprContext *ctx) = 0;

  virtual void enterUnaryExpr(LuvParser::UnaryExprContext *ctx) = 0;
  virtual void exitUnaryExpr(LuvParser::UnaryExprContext *ctx) = 0;

  virtual void enterPrimaryExpr(LuvParser::PrimaryExprContext *ctx) = 0;
  virtual void exitPrimaryExpr(LuvParser::PrimaryExprContext *ctx) = 0;

  virtual void enterTernaryExpr(LuvParser::TernaryExprContext *ctx) = 0;
  virtual void exitTernaryExpr(LuvParser::TernaryExprContext *ctx) = 0;

  virtual void enterPropertyExpr(LuvParser::PropertyExprContext *ctx) = 0;
  virtual void exitPropertyExpr(LuvParser::PropertyExprContext *ctx) = 0;

  virtual void enterSuperCallExpr(LuvParser::SuperCallExprContext *ctx) = 0;
  virtual void exitSuperCallExpr(LuvParser::SuperCallExprContext *ctx) = 0;

  virtual void enterIfExprAlternative(LuvParser::IfExprAlternativeContext *ctx) = 0;
  virtual void exitIfExprAlternative(LuvParser::IfExprAlternativeContext *ctx) = 0;

  virtual void enterPostfixExpr(LuvParser::PostfixExprContext *ctx) = 0;
  virtual void exitPostfixExpr(LuvParser::PostfixExprContext *ctx) = 0;

  virtual void enterCallExpr(LuvParser::CallExprContext *ctx) = 0;
  virtual void exitCallExpr(LuvParser::CallExprContext *ctx) = 0;

  virtual void enterForExprAlternative(LuvParser::ForExprAlternativeContext *ctx) = 0;
  virtual void exitForExprAlternative(LuvParser::ForExprAlternativeContext *ctx) = 0;

  virtual void enterIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext *ctx) = 0;
  virtual void exitIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext *ctx) = 0;

  virtual void enterBitwiseXorExpr(LuvParser::BitwiseXorExprContext *ctx) = 0;
  virtual void exitBitwiseXorExpr(LuvParser::BitwiseXorExprContext *ctx) = 0;

  virtual void enterLogicalAndExpr(LuvParser::LogicalAndExprContext *ctx) = 0;
  virtual void exitLogicalAndExpr(LuvParser::LogicalAndExprContext *ctx) = 0;

  virtual void enterComparisonExpr(LuvParser::ComparisonExprContext *ctx) = 0;
  virtual void exitComparisonExpr(LuvParser::ComparisonExprContext *ctx) = 0;

  virtual void enterSliceExpr(LuvParser::SliceExprContext *ctx) = 0;
  virtual void exitSliceExpr(LuvParser::SliceExprContext *ctx) = 0;

  virtual void enterShiftExpr(LuvParser::ShiftExprContext *ctx) = 0;
  virtual void exitShiftExpr(LuvParser::ShiftExprContext *ctx) = 0;

  virtual void enterLogicalOrExpr(LuvParser::LogicalOrExprContext *ctx) = 0;
  virtual void exitLogicalOrExpr(LuvParser::LogicalOrExprContext *ctx) = 0;

  virtual void enterIndexExpr(LuvParser::IndexExprContext *ctx) = 0;
  virtual void exitIndexExpr(LuvParser::IndexExprContext *ctx) = 0;

  virtual void enterWhileExprAlternative(LuvParser::WhileExprAlternativeContext *ctx) = 0;
  virtual void exitWhileExprAlternative(LuvParser::WhileExprAlternativeContext *ctx) = 0;

  virtual void enterMultiplicativeExpr(LuvParser::MultiplicativeExprContext *ctx) = 0;
  virtual void exitMultiplicativeExpr(LuvParser::MultiplicativeExprContext *ctx) = 0;

  virtual void enterStructInstExpr(LuvParser::StructInstExprContext *ctx) = 0;
  virtual void exitStructInstExpr(LuvParser::StructInstExprContext *ctx) = 0;

  virtual void enterMatchCase(LuvParser::MatchCaseContext *ctx) = 0;
  virtual void exitMatchCase(LuvParser::MatchCaseContext *ctx) = 0;

  virtual void enterArgs(LuvParser::ArgsContext *ctx) = 0;
  virtual void exitArgs(LuvParser::ArgsContext *ctx) = 0;

  virtual void enterIntLiteral(LuvParser::IntLiteralContext *ctx) = 0;
  virtual void exitIntLiteral(LuvParser::IntLiteralContext *ctx) = 0;

  virtual void enterFloatLiteral(LuvParser::FloatLiteralContext *ctx) = 0;
  virtual void exitFloatLiteral(LuvParser::FloatLiteralContext *ctx) = 0;

  virtual void enterStringLiteral(LuvParser::StringLiteralContext *ctx) = 0;
  virtual void exitStringLiteral(LuvParser::StringLiteralContext *ctx) = 0;

  virtual void enterStringInterpolationExpr(LuvParser::StringInterpolationExprContext *ctx) = 0;
  virtual void exitStringInterpolationExpr(LuvParser::StringInterpolationExprContext *ctx) = 0;

  virtual void enterCharLiteral(LuvParser::CharLiteralContext *ctx) = 0;
  virtual void exitCharLiteral(LuvParser::CharLiteralContext *ctx) = 0;

  virtual void enterBoolLiteral(LuvParser::BoolLiteralContext *ctx) = 0;
  virtual void exitBoolLiteral(LuvParser::BoolLiteralContext *ctx) = 0;

  virtual void enterNullLiteral(LuvParser::NullLiteralContext *ctx) = 0;
  virtual void exitNullLiteral(LuvParser::NullLiteralContext *ctx) = 0;

  virtual void enterIdentifier(LuvParser::IdentifierContext *ctx) = 0;
  virtual void exitIdentifier(LuvParser::IdentifierContext *ctx) = 0;

  virtual void enterGroupingExpr(LuvParser::GroupingExprContext *ctx) = 0;
  virtual void exitGroupingExpr(LuvParser::GroupingExprContext *ctx) = 0;

  virtual void enterTupleExpr(LuvParser::TupleExprContext *ctx) = 0;
  virtual void exitTupleExpr(LuvParser::TupleExprContext *ctx) = 0;

  virtual void enterArrayExpr(LuvParser::ArrayExprContext *ctx) = 0;
  virtual void exitArrayExpr(LuvParser::ArrayExprContext *ctx) = 0;

  virtual void enterArrayRepeatExpr(LuvParser::ArrayRepeatExprContext *ctx) = 0;
  virtual void exitArrayRepeatExpr(LuvParser::ArrayRepeatExprContext *ctx) = 0;

  virtual void enterArrayCompExpr(LuvParser::ArrayCompExprContext *ctx) = 0;
  virtual void exitArrayCompExpr(LuvParser::ArrayCompExprContext *ctx) = 0;


};


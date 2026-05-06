
// Generated from src/Luv.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuvListener.h"


/**
 * This class provides an empty implementation of LuvListener,
 * which can be extended to create a listener which only needs to handle a subset
 * of the available methods.
 */
class  LuvBaseListener : public LuvListener {
public:

  virtual void enterProgram(LuvParser::ProgramContext * /*ctx*/) override { }
  virtual void exitProgram(LuvParser::ProgramContext * /*ctx*/) override { }

  virtual void enterTopLevel(LuvParser::TopLevelContext * /*ctx*/) override { }
  virtual void exitTopLevel(LuvParser::TopLevelContext * /*ctx*/) override { }

  virtual void enterModuleDecl(LuvParser::ModuleDeclContext * /*ctx*/) override { }
  virtual void exitModuleDecl(LuvParser::ModuleDeclContext * /*ctx*/) override { }

  virtual void enterUseFromStmt(LuvParser::UseFromStmtContext * /*ctx*/) override { }
  virtual void exitUseFromStmt(LuvParser::UseFromStmtContext * /*ctx*/) override { }

  virtual void enterUsePathStmt(LuvParser::UsePathStmtContext * /*ctx*/) override { }
  virtual void exitUsePathStmt(LuvParser::UsePathStmtContext * /*ctx*/) override { }

  virtual void enterUseAllPublic(LuvParser::UseAllPublicContext * /*ctx*/) override { }
  virtual void exitUseAllPublic(LuvParser::UseAllPublicContext * /*ctx*/) override { }

  virtual void enterUseAllPrivate(LuvParser::UseAllPrivateContext * /*ctx*/) override { }
  virtual void exitUseAllPrivate(LuvParser::UseAllPrivateContext * /*ctx*/) override { }

  virtual void enterUseSingle(LuvParser::UseSingleContext * /*ctx*/) override { }
  virtual void exitUseSingle(LuvParser::UseSingleContext * /*ctx*/) override { }

  virtual void enterUseSet(LuvParser::UseSetContext * /*ctx*/) override { }
  virtual void exitUseSet(LuvParser::UseSetContext * /*ctx*/) override { }

  virtual void enterUseList(LuvParser::UseListContext * /*ctx*/) override { }
  virtual void exitUseList(LuvParser::UseListContext * /*ctx*/) override { }

  virtual void enterModulePath(LuvParser::ModulePathContext * /*ctx*/) override { }
  virtual void exitModulePath(LuvParser::ModulePathContext * /*ctx*/) override { }

  virtual void enterVisibilityDecl(LuvParser::VisibilityDeclContext * /*ctx*/) override { }
  virtual void exitVisibilityDecl(LuvParser::VisibilityDeclContext * /*ctx*/) override { }

  virtual void enterExternDecl(LuvParser::ExternDeclContext * /*ctx*/) override { }
  virtual void exitExternDecl(LuvParser::ExternDeclContext * /*ctx*/) override { }

  virtual void enterStatement(LuvParser::StatementContext * /*ctx*/) override { }
  virtual void exitStatement(LuvParser::StatementContext * /*ctx*/) override { }

  virtual void enterBreakStmt(LuvParser::BreakStmtContext * /*ctx*/) override { }
  virtual void exitBreakStmt(LuvParser::BreakStmtContext * /*ctx*/) override { }

  virtual void enterContinueStmt(LuvParser::ContinueStmtContext * /*ctx*/) override { }
  virtual void exitContinueStmt(LuvParser::ContinueStmtContext * /*ctx*/) override { }

  virtual void enterBlock(LuvParser::BlockContext * /*ctx*/) override { }
  virtual void exitBlock(LuvParser::BlockContext * /*ctx*/) override { }

  virtual void enterStructDecl(LuvParser::StructDeclContext * /*ctx*/) override { }
  virtual void exitStructDecl(LuvParser::StructDeclContext * /*ctx*/) override { }

  virtual void enterStructMember(LuvParser::StructMemberContext * /*ctx*/) override { }
  virtual void exitStructMember(LuvParser::StructMemberContext * /*ctx*/) override { }

  virtual void enterStructField(LuvParser::StructFieldContext * /*ctx*/) override { }
  virtual void exitStructField(LuvParser::StructFieldContext * /*ctx*/) override { }

  virtual void enterEnumDecl(LuvParser::EnumDeclContext * /*ctx*/) override { }
  virtual void exitEnumDecl(LuvParser::EnumDeclContext * /*ctx*/) override { }

  virtual void enterEnumVariant(LuvParser::EnumVariantContext * /*ctx*/) override { }
  virtual void exitEnumVariant(LuvParser::EnumVariantContext * /*ctx*/) override { }

  virtual void enterTypeList(LuvParser::TypeListContext * /*ctx*/) override { }
  virtual void exitTypeList(LuvParser::TypeListContext * /*ctx*/) override { }

  virtual void enterClassDecl(LuvParser::ClassDeclContext * /*ctx*/) override { }
  virtual void exitClassDecl(LuvParser::ClassDeclContext * /*ctx*/) override { }

  virtual void enterClassMember(LuvParser::ClassMemberContext * /*ctx*/) override { }
  virtual void exitClassMember(LuvParser::ClassMemberContext * /*ctx*/) override { }

  virtual void enterClassField(LuvParser::ClassFieldContext * /*ctx*/) override { }
  virtual void exitClassField(LuvParser::ClassFieldContext * /*ctx*/) override { }

  virtual void enterDeclaration(LuvParser::DeclarationContext * /*ctx*/) override { }
  virtual void exitDeclaration(LuvParser::DeclarationContext * /*ctx*/) override { }

  virtual void enterInterfaceDecl(LuvParser::InterfaceDeclContext * /*ctx*/) override { }
  virtual void exitInterfaceDecl(LuvParser::InterfaceDeclContext * /*ctx*/) override { }

  virtual void enterInterfaceMember(LuvParser::InterfaceMemberContext * /*ctx*/) override { }
  virtual void exitInterfaceMember(LuvParser::InterfaceMemberContext * /*ctx*/) override { }

  virtual void enterVarDecl(LuvParser::VarDeclContext * /*ctx*/) override { }
  virtual void exitVarDecl(LuvParser::VarDeclContext * /*ctx*/) override { }

  virtual void enterBindingPatternList(LuvParser::BindingPatternListContext * /*ctx*/) override { }
  virtual void exitBindingPatternList(LuvParser::BindingPatternListContext * /*ctx*/) override { }

  virtual void enterIdentifierPattern(LuvParser::IdentifierPatternContext * /*ctx*/) override { }
  virtual void exitIdentifierPattern(LuvParser::IdentifierPatternContext * /*ctx*/) override { }

  virtual void enterTuplePattern(LuvParser::TuplePatternContext * /*ctx*/) override { }
  virtual void exitTuplePattern(LuvParser::TuplePatternContext * /*ctx*/) override { }

  virtual void enterStructPattern(LuvParser::StructPatternContext * /*ctx*/) override { }
  virtual void exitStructPattern(LuvParser::StructPatternContext * /*ctx*/) override { }

  virtual void enterVariantPattern(LuvParser::VariantPatternContext * /*ctx*/) override { }
  virtual void exitVariantPattern(LuvParser::VariantPatternContext * /*ctx*/) override { }

  virtual void enterWildcardPattern(LuvParser::WildcardPatternContext * /*ctx*/) override { }
  virtual void exitWildcardPattern(LuvParser::WildcardPatternContext * /*ctx*/) override { }

  virtual void enterLiteralPattern(LuvParser::LiteralPatternContext * /*ctx*/) override { }
  virtual void exitLiteralPattern(LuvParser::LiteralPatternContext * /*ctx*/) override { }

  virtual void enterStructBindingList(LuvParser::StructBindingListContext * /*ctx*/) override { }
  virtual void exitStructBindingList(LuvParser::StructBindingListContext * /*ctx*/) override { }

  virtual void enterStructBinding(LuvParser::StructBindingContext * /*ctx*/) override { }
  virtual void exitStructBinding(LuvParser::StructBindingContext * /*ctx*/) override { }

  virtual void enterModifier(LuvParser::ModifierContext * /*ctx*/) override { }
  virtual void exitModifier(LuvParser::ModifierContext * /*ctx*/) override { }

  virtual void enterAttribute(LuvParser::AttributeContext * /*ctx*/) override { }
  virtual void exitAttribute(LuvParser::AttributeContext * /*ctx*/) override { }

  virtual void enterMemoryHint(LuvParser::MemoryHintContext * /*ctx*/) override { }
  virtual void exitMemoryHint(LuvParser::MemoryHintContext * /*ctx*/) override { }

  virtual void enterAttrList(LuvParser::AttrListContext * /*ctx*/) override { }
  virtual void exitAttrList(LuvParser::AttrListContext * /*ctx*/) override { }

  virtual void enterAttr(LuvParser::AttrContext * /*ctx*/) override { }
  virtual void exitAttr(LuvParser::AttrContext * /*ctx*/) override { }

  virtual void enterOverloadableOp(LuvParser::OverloadableOpContext * /*ctx*/) override { }
  virtual void exitOverloadableOp(LuvParser::OverloadableOpContext * /*ctx*/) override { }

  virtual void enterFuncName(LuvParser::FuncNameContext * /*ctx*/) override { }
  virtual void exitFuncName(LuvParser::FuncNameContext * /*ctx*/) override { }

  virtual void enterBlockFunc(LuvParser::BlockFuncContext * /*ctx*/) override { }
  virtual void exitBlockFunc(LuvParser::BlockFuncContext * /*ctx*/) override { }

  virtual void enterArrowFunc(LuvParser::ArrowFuncContext * /*ctx*/) override { }
  virtual void exitArrowFunc(LuvParser::ArrowFuncContext * /*ctx*/) override { }

  virtual void enterTypeParams(LuvParser::TypeParamsContext * /*ctx*/) override { }
  virtual void exitTypeParams(LuvParser::TypeParamsContext * /*ctx*/) override { }

  virtual void enterParams(LuvParser::ParamsContext * /*ctx*/) override { }
  virtual void exitParams(LuvParser::ParamsContext * /*ctx*/) override { }

  virtual void enterParam(LuvParser::ParamContext * /*ctx*/) override { }
  virtual void exitParam(LuvParser::ParamContext * /*ctx*/) override { }

  virtual void enterType(LuvParser::TypeContext * /*ctx*/) override { }
  virtual void exitType(LuvParser::TypeContext * /*ctx*/) override { }

  virtual void enterTypeCore(LuvParser::TypeCoreContext * /*ctx*/) override { }
  virtual void exitTypeCore(LuvParser::TypeCoreContext * /*ctx*/) override { }

  virtual void enterIfExpr(LuvParser::IfExprContext * /*ctx*/) override { }
  virtual void exitIfExpr(LuvParser::IfExprContext * /*ctx*/) override { }

  virtual void enterEfExpr(LuvParser::EfExprContext * /*ctx*/) override { }
  virtual void exitEfExpr(LuvParser::EfExprContext * /*ctx*/) override { }

  virtual void enterWhileExpr(LuvParser::WhileExprContext * /*ctx*/) override { }
  virtual void exitWhileExpr(LuvParser::WhileExprContext * /*ctx*/) override { }

  virtual void enterForRangeExpr(LuvParser::ForRangeExprContext * /*ctx*/) override { }
  virtual void exitForRangeExpr(LuvParser::ForRangeExprContext * /*ctx*/) override { }

  virtual void enterForRangeIncExpr(LuvParser::ForRangeIncExprContext * /*ctx*/) override { }
  virtual void exitForRangeIncExpr(LuvParser::ForRangeIncExprContext * /*ctx*/) override { }

  virtual void enterForInExpr(LuvParser::ForInExprContext * /*ctx*/) override { }
  virtual void exitForInExpr(LuvParser::ForInExprContext * /*ctx*/) override { }

  virtual void enterForCStyle(LuvParser::ForCStyleContext * /*ctx*/) override { }
  virtual void exitForCStyle(LuvParser::ForCStyleContext * /*ctx*/) override { }

  virtual void enterReturnStmt(LuvParser::ReturnStmtContext * /*ctx*/) override { }
  virtual void exitReturnStmt(LuvParser::ReturnStmtContext * /*ctx*/) override { }

  virtual void enterExprStmt(LuvParser::ExprStmtContext * /*ctx*/) override { }
  virtual void exitExprStmt(LuvParser::ExprStmtContext * /*ctx*/) override { }

  virtual void enterStructInstFields(LuvParser::StructInstFieldsContext * /*ctx*/) override { }
  virtual void exitStructInstFields(LuvParser::StructInstFieldsContext * /*ctx*/) override { }

  virtual void enterAssignmentTarget(LuvParser::AssignmentTargetContext * /*ctx*/) override { }
  virtual void exitAssignmentTarget(LuvParser::AssignmentTargetContext * /*ctx*/) override { }

  virtual void enterAssignment(LuvParser::AssignmentContext * /*ctx*/) override { }
  virtual void exitAssignment(LuvParser::AssignmentContext * /*ctx*/) override { }

  virtual void enterCastExpr(LuvParser::CastExprContext * /*ctx*/) override { }
  virtual void exitCastExpr(LuvParser::CastExprContext * /*ctx*/) override { }

  virtual void enterBitwiseOrExpr(LuvParser::BitwiseOrExprContext * /*ctx*/) override { }
  virtual void exitBitwiseOrExpr(LuvParser::BitwiseOrExprContext * /*ctx*/) override { }

  virtual void enterGenericCallExpr(LuvParser::GenericCallExprContext * /*ctx*/) override { }
  virtual void exitGenericCallExpr(LuvParser::GenericCallExprContext * /*ctx*/) override { }

  virtual void enterMatchExpr(LuvParser::MatchExprContext * /*ctx*/) override { }
  virtual void exitMatchExpr(LuvParser::MatchExprContext * /*ctx*/) override { }

  virtual void enterBitwiseAndExpr(LuvParser::BitwiseAndExprContext * /*ctx*/) override { }
  virtual void exitBitwiseAndExpr(LuvParser::BitwiseAndExprContext * /*ctx*/) override { }

  virtual void enterAsmExpr(LuvParser::AsmExprContext * /*ctx*/) override { }
  virtual void exitAsmExpr(LuvParser::AsmExprContext * /*ctx*/) override { }

  virtual void enterAdditiveExpr(LuvParser::AdditiveExprContext * /*ctx*/) override { }
  virtual void exitAdditiveExpr(LuvParser::AdditiveExprContext * /*ctx*/) override { }

  virtual void enterMethodCallExpr(LuvParser::MethodCallExprContext * /*ctx*/) override { }
  virtual void exitMethodCallExpr(LuvParser::MethodCallExprContext * /*ctx*/) override { }

  virtual void enterUnaryExpr(LuvParser::UnaryExprContext * /*ctx*/) override { }
  virtual void exitUnaryExpr(LuvParser::UnaryExprContext * /*ctx*/) override { }

  virtual void enterPrimaryExpr(LuvParser::PrimaryExprContext * /*ctx*/) override { }
  virtual void exitPrimaryExpr(LuvParser::PrimaryExprContext * /*ctx*/) override { }

  virtual void enterTernaryExpr(LuvParser::TernaryExprContext * /*ctx*/) override { }
  virtual void exitTernaryExpr(LuvParser::TernaryExprContext * /*ctx*/) override { }

  virtual void enterPropertyExpr(LuvParser::PropertyExprContext * /*ctx*/) override { }
  virtual void exitPropertyExpr(LuvParser::PropertyExprContext * /*ctx*/) override { }

  virtual void enterSuperCallExpr(LuvParser::SuperCallExprContext * /*ctx*/) override { }
  virtual void exitSuperCallExpr(LuvParser::SuperCallExprContext * /*ctx*/) override { }

  virtual void enterIfExprAlternative(LuvParser::IfExprAlternativeContext * /*ctx*/) override { }
  virtual void exitIfExprAlternative(LuvParser::IfExprAlternativeContext * /*ctx*/) override { }

  virtual void enterPostfixExpr(LuvParser::PostfixExprContext * /*ctx*/) override { }
  virtual void exitPostfixExpr(LuvParser::PostfixExprContext * /*ctx*/) override { }

  virtual void enterCallExpr(LuvParser::CallExprContext * /*ctx*/) override { }
  virtual void exitCallExpr(LuvParser::CallExprContext * /*ctx*/) override { }

  virtual void enterForExprAlternative(LuvParser::ForExprAlternativeContext * /*ctx*/) override { }
  virtual void exitForExprAlternative(LuvParser::ForExprAlternativeContext * /*ctx*/) override { }

  virtual void enterIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext * /*ctx*/) override { }
  virtual void exitIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext * /*ctx*/) override { }

  virtual void enterBitwiseXorExpr(LuvParser::BitwiseXorExprContext * /*ctx*/) override { }
  virtual void exitBitwiseXorExpr(LuvParser::BitwiseXorExprContext * /*ctx*/) override { }

  virtual void enterLogicalAndExpr(LuvParser::LogicalAndExprContext * /*ctx*/) override { }
  virtual void exitLogicalAndExpr(LuvParser::LogicalAndExprContext * /*ctx*/) override { }

  virtual void enterComparisonExpr(LuvParser::ComparisonExprContext * /*ctx*/) override { }
  virtual void exitComparisonExpr(LuvParser::ComparisonExprContext * /*ctx*/) override { }

  virtual void enterSliceExpr(LuvParser::SliceExprContext * /*ctx*/) override { }
  virtual void exitSliceExpr(LuvParser::SliceExprContext * /*ctx*/) override { }

  virtual void enterShiftExpr(LuvParser::ShiftExprContext * /*ctx*/) override { }
  virtual void exitShiftExpr(LuvParser::ShiftExprContext * /*ctx*/) override { }

  virtual void enterLogicalOrExpr(LuvParser::LogicalOrExprContext * /*ctx*/) override { }
  virtual void exitLogicalOrExpr(LuvParser::LogicalOrExprContext * /*ctx*/) override { }

  virtual void enterIndexExpr(LuvParser::IndexExprContext * /*ctx*/) override { }
  virtual void exitIndexExpr(LuvParser::IndexExprContext * /*ctx*/) override { }

  virtual void enterWhileExprAlternative(LuvParser::WhileExprAlternativeContext * /*ctx*/) override { }
  virtual void exitWhileExprAlternative(LuvParser::WhileExprAlternativeContext * /*ctx*/) override { }

  virtual void enterMultiplicativeExpr(LuvParser::MultiplicativeExprContext * /*ctx*/) override { }
  virtual void exitMultiplicativeExpr(LuvParser::MultiplicativeExprContext * /*ctx*/) override { }

  virtual void enterStructInstExpr(LuvParser::StructInstExprContext * /*ctx*/) override { }
  virtual void exitStructInstExpr(LuvParser::StructInstExprContext * /*ctx*/) override { }

  virtual void enterMatchCase(LuvParser::MatchCaseContext * /*ctx*/) override { }
  virtual void exitMatchCase(LuvParser::MatchCaseContext * /*ctx*/) override { }

  virtual void enterArgs(LuvParser::ArgsContext * /*ctx*/) override { }
  virtual void exitArgs(LuvParser::ArgsContext * /*ctx*/) override { }

  virtual void enterIntLit(LuvParser::IntLitContext * /*ctx*/) override { }
  virtual void exitIntLit(LuvParser::IntLitContext * /*ctx*/) override { }

  virtual void enterFloatLit(LuvParser::FloatLitContext * /*ctx*/) override { }
  virtual void exitFloatLit(LuvParser::FloatLitContext * /*ctx*/) override { }

  virtual void enterStringLit(LuvParser::StringLitContext * /*ctx*/) override { }
  virtual void exitStringLit(LuvParser::StringLitContext * /*ctx*/) override { }

  virtual void enterCharLit(LuvParser::CharLitContext * /*ctx*/) override { }
  virtual void exitCharLit(LuvParser::CharLitContext * /*ctx*/) override { }

  virtual void enterBoolLit(LuvParser::BoolLitContext * /*ctx*/) override { }
  virtual void exitBoolLit(LuvParser::BoolLitContext * /*ctx*/) override { }

  virtual void enterNullLit(LuvParser::NullLitContext * /*ctx*/) override { }
  virtual void exitNullLit(LuvParser::NullLitContext * /*ctx*/) override { }

  virtual void enterPrimaryLiteral(LuvParser::PrimaryLiteralContext * /*ctx*/) override { }
  virtual void exitPrimaryLiteral(LuvParser::PrimaryLiteralContext * /*ctx*/) override { }

  virtual void enterStringInterpolationExpr(LuvParser::StringInterpolationExprContext * /*ctx*/) override { }
  virtual void exitStringInterpolationExpr(LuvParser::StringInterpolationExprContext * /*ctx*/) override { }

  virtual void enterIdentifier(LuvParser::IdentifierContext * /*ctx*/) override { }
  virtual void exitIdentifier(LuvParser::IdentifierContext * /*ctx*/) override { }

  virtual void enterGroupingExpr(LuvParser::GroupingExprContext * /*ctx*/) override { }
  virtual void exitGroupingExpr(LuvParser::GroupingExprContext * /*ctx*/) override { }

  virtual void enterTupleExpr(LuvParser::TupleExprContext * /*ctx*/) override { }
  virtual void exitTupleExpr(LuvParser::TupleExprContext * /*ctx*/) override { }

  virtual void enterArrayExpr(LuvParser::ArrayExprContext * /*ctx*/) override { }
  virtual void exitArrayExpr(LuvParser::ArrayExprContext * /*ctx*/) override { }

  virtual void enterArrayRepeatExpr(LuvParser::ArrayRepeatExprContext * /*ctx*/) override { }
  virtual void exitArrayRepeatExpr(LuvParser::ArrayRepeatExprContext * /*ctx*/) override { }

  virtual void enterArrayCompExpr(LuvParser::ArrayCompExprContext * /*ctx*/) override { }
  virtual void exitArrayCompExpr(LuvParser::ArrayCompExprContext * /*ctx*/) override { }


  virtual void enterEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void exitEveryRule(antlr4::ParserRuleContext * /*ctx*/) override { }
  virtual void visitTerminal(antlr4::tree::TerminalNode * /*node*/) override { }
  virtual void visitErrorNode(antlr4::tree::ErrorNode * /*node*/) override { }

};


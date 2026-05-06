
// Generated from src/Luv.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuvParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by LuvParser.
 */
class  LuvVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by LuvParser.
   */
    virtual std::any visitProgram(LuvParser::ProgramContext *context) = 0;

    virtual std::any visitTopLevel(LuvParser::TopLevelContext *context) = 0;

    virtual std::any visitModuleDecl(LuvParser::ModuleDeclContext *context) = 0;

    virtual std::any visitUseFromStmt(LuvParser::UseFromStmtContext *context) = 0;

    virtual std::any visitUsePathStmt(LuvParser::UsePathStmtContext *context) = 0;

    virtual std::any visitUseAllPublic(LuvParser::UseAllPublicContext *context) = 0;

    virtual std::any visitUseAllPrivate(LuvParser::UseAllPrivateContext *context) = 0;

    virtual std::any visitUseSingle(LuvParser::UseSingleContext *context) = 0;

    virtual std::any visitUseSet(LuvParser::UseSetContext *context) = 0;

    virtual std::any visitUseList(LuvParser::UseListContext *context) = 0;

    virtual std::any visitModulePath(LuvParser::ModulePathContext *context) = 0;

    virtual std::any visitVisibilityDecl(LuvParser::VisibilityDeclContext *context) = 0;

    virtual std::any visitExternDecl(LuvParser::ExternDeclContext *context) = 0;

    virtual std::any visitStatement(LuvParser::StatementContext *context) = 0;

    virtual std::any visitBreakStmt(LuvParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(LuvParser::ContinueStmtContext *context) = 0;

    virtual std::any visitBlock(LuvParser::BlockContext *context) = 0;

    virtual std::any visitStructDecl(LuvParser::StructDeclContext *context) = 0;

    virtual std::any visitStructMember(LuvParser::StructMemberContext *context) = 0;

    virtual std::any visitStructField(LuvParser::StructFieldContext *context) = 0;

    virtual std::any visitEnumDecl(LuvParser::EnumDeclContext *context) = 0;

    virtual std::any visitEnumVariant(LuvParser::EnumVariantContext *context) = 0;

    virtual std::any visitTypeList(LuvParser::TypeListContext *context) = 0;

    virtual std::any visitClassDecl(LuvParser::ClassDeclContext *context) = 0;

    virtual std::any visitClassMember(LuvParser::ClassMemberContext *context) = 0;

    virtual std::any visitClassField(LuvParser::ClassFieldContext *context) = 0;

    virtual std::any visitDeclaration(LuvParser::DeclarationContext *context) = 0;

    virtual std::any visitInterfaceDecl(LuvParser::InterfaceDeclContext *context) = 0;

    virtual std::any visitInterfaceMember(LuvParser::InterfaceMemberContext *context) = 0;

    virtual std::any visitVarDecl(LuvParser::VarDeclContext *context) = 0;

    virtual std::any visitBindingPatternList(LuvParser::BindingPatternListContext *context) = 0;

    virtual std::any visitIdentifierPattern(LuvParser::IdentifierPatternContext *context) = 0;

    virtual std::any visitTuplePattern(LuvParser::TuplePatternContext *context) = 0;

    virtual std::any visitStructPattern(LuvParser::StructPatternContext *context) = 0;

    virtual std::any visitVariantPattern(LuvParser::VariantPatternContext *context) = 0;

    virtual std::any visitWildcardPattern(LuvParser::WildcardPatternContext *context) = 0;

    virtual std::any visitLiteralPattern(LuvParser::LiteralPatternContext *context) = 0;

    virtual std::any visitStructBindingList(LuvParser::StructBindingListContext *context) = 0;

    virtual std::any visitStructBinding(LuvParser::StructBindingContext *context) = 0;

    virtual std::any visitModifier(LuvParser::ModifierContext *context) = 0;

    virtual std::any visitAttribute(LuvParser::AttributeContext *context) = 0;

    virtual std::any visitMemoryHint(LuvParser::MemoryHintContext *context) = 0;

    virtual std::any visitAttrList(LuvParser::AttrListContext *context) = 0;

    virtual std::any visitAttr(LuvParser::AttrContext *context) = 0;

    virtual std::any visitOverloadableOp(LuvParser::OverloadableOpContext *context) = 0;

    virtual std::any visitFuncName(LuvParser::FuncNameContext *context) = 0;

    virtual std::any visitBlockFunc(LuvParser::BlockFuncContext *context) = 0;

    virtual std::any visitArrowFunc(LuvParser::ArrowFuncContext *context) = 0;

    virtual std::any visitTypeParams(LuvParser::TypeParamsContext *context) = 0;

    virtual std::any visitParams(LuvParser::ParamsContext *context) = 0;

    virtual std::any visitParam(LuvParser::ParamContext *context) = 0;

    virtual std::any visitType(LuvParser::TypeContext *context) = 0;

    virtual std::any visitTypeCore(LuvParser::TypeCoreContext *context) = 0;

    virtual std::any visitIfExpr(LuvParser::IfExprContext *context) = 0;

    virtual std::any visitEfExpr(LuvParser::EfExprContext *context) = 0;

    virtual std::any visitWhileExpr(LuvParser::WhileExprContext *context) = 0;

    virtual std::any visitForRangeExpr(LuvParser::ForRangeExprContext *context) = 0;

    virtual std::any visitForRangeIncExpr(LuvParser::ForRangeIncExprContext *context) = 0;

    virtual std::any visitForInExpr(LuvParser::ForInExprContext *context) = 0;

    virtual std::any visitForCStyle(LuvParser::ForCStyleContext *context) = 0;

    virtual std::any visitReturnStmt(LuvParser::ReturnStmtContext *context) = 0;

    virtual std::any visitExprStmt(LuvParser::ExprStmtContext *context) = 0;

    virtual std::any visitStructInstFields(LuvParser::StructInstFieldsContext *context) = 0;

    virtual std::any visitAssignmentTarget(LuvParser::AssignmentTargetContext *context) = 0;

    virtual std::any visitAssignment(LuvParser::AssignmentContext *context) = 0;

    virtual std::any visitCastExpr(LuvParser::CastExprContext *context) = 0;

    virtual std::any visitBitwiseOrExpr(LuvParser::BitwiseOrExprContext *context) = 0;

    virtual std::any visitGenericCallExpr(LuvParser::GenericCallExprContext *context) = 0;

    virtual std::any visitMatchExpr(LuvParser::MatchExprContext *context) = 0;

    virtual std::any visitBitwiseAndExpr(LuvParser::BitwiseAndExprContext *context) = 0;

    virtual std::any visitAsmExpr(LuvParser::AsmExprContext *context) = 0;

    virtual std::any visitAdditiveExpr(LuvParser::AdditiveExprContext *context) = 0;

    virtual std::any visitMethodCallExpr(LuvParser::MethodCallExprContext *context) = 0;

    virtual std::any visitUnaryExpr(LuvParser::UnaryExprContext *context) = 0;

    virtual std::any visitPrimaryExpr(LuvParser::PrimaryExprContext *context) = 0;

    virtual std::any visitTernaryExpr(LuvParser::TernaryExprContext *context) = 0;

    virtual std::any visitPropertyExpr(LuvParser::PropertyExprContext *context) = 0;

    virtual std::any visitSuperCallExpr(LuvParser::SuperCallExprContext *context) = 0;

    virtual std::any visitIfExprAlternative(LuvParser::IfExprAlternativeContext *context) = 0;

    virtual std::any visitPostfixExpr(LuvParser::PostfixExprContext *context) = 0;

    virtual std::any visitCallExpr(LuvParser::CallExprContext *context) = 0;

    virtual std::any visitForExprAlternative(LuvParser::ForExprAlternativeContext *context) = 0;

    virtual std::any visitIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext *context) = 0;

    virtual std::any visitBitwiseXorExpr(LuvParser::BitwiseXorExprContext *context) = 0;

    virtual std::any visitLogicalAndExpr(LuvParser::LogicalAndExprContext *context) = 0;

    virtual std::any visitComparisonExpr(LuvParser::ComparisonExprContext *context) = 0;

    virtual std::any visitSliceExpr(LuvParser::SliceExprContext *context) = 0;

    virtual std::any visitShiftExpr(LuvParser::ShiftExprContext *context) = 0;

    virtual std::any visitLogicalOrExpr(LuvParser::LogicalOrExprContext *context) = 0;

    virtual std::any visitIndexExpr(LuvParser::IndexExprContext *context) = 0;

    virtual std::any visitWhileExprAlternative(LuvParser::WhileExprAlternativeContext *context) = 0;

    virtual std::any visitMultiplicativeExpr(LuvParser::MultiplicativeExprContext *context) = 0;

    virtual std::any visitStructInstExpr(LuvParser::StructInstExprContext *context) = 0;

    virtual std::any visitMatchCase(LuvParser::MatchCaseContext *context) = 0;

    virtual std::any visitArgs(LuvParser::ArgsContext *context) = 0;

    virtual std::any visitIntLit(LuvParser::IntLitContext *context) = 0;

    virtual std::any visitFloatLit(LuvParser::FloatLitContext *context) = 0;

    virtual std::any visitStringLit(LuvParser::StringLitContext *context) = 0;

    virtual std::any visitCharLit(LuvParser::CharLitContext *context) = 0;

    virtual std::any visitBoolLit(LuvParser::BoolLitContext *context) = 0;

    virtual std::any visitNullLit(LuvParser::NullLitContext *context) = 0;

    virtual std::any visitPrimaryLiteral(LuvParser::PrimaryLiteralContext *context) = 0;

    virtual std::any visitStringInterpolationExpr(LuvParser::StringInterpolationExprContext *context) = 0;

    virtual std::any visitIdentifier(LuvParser::IdentifierContext *context) = 0;

    virtual std::any visitGroupingExpr(LuvParser::GroupingExprContext *context) = 0;

    virtual std::any visitTupleExpr(LuvParser::TupleExprContext *context) = 0;

    virtual std::any visitArrayExpr(LuvParser::ArrayExprContext *context) = 0;

    virtual std::any visitArrayRepeatExpr(LuvParser::ArrayRepeatExprContext *context) = 0;

    virtual std::any visitArrayCompExpr(LuvParser::ArrayCompExprContext *context) = 0;


};


#pragma once
#include "LuvBaseVisitor.h"
#include "ast/AST.h"
#include "LuvError.h"
#include "Arena.h"
#include <memory>
#include <any>
#include <iostream>

namespace luv {

class LuvVisitorImpl : public LuvBaseVisitor {
public:
    LuvVisitorImpl(Arena& arena) : arena_(arena) {}

    // Set current file for error reporting
    void setCurrentFile(const std::string& file) { currentFile_ = file; }

    std::any visitProgram(LuvParser::ProgramContext *ctx) override {
        std::string moduleName;
        std::vector<UseStmt*> useStmts;
        std::vector<Stmt*> statements;

        // Module declaration (optional, first thing in file)
        if (ctx->moduleDecl()) {
            auto modCtx = ctx->moduleDecl();
            auto pathCtx = modCtx->modulePath();
            std::vector<std::string> path;
            for (auto id : pathCtx->IDENTIFIER()) {
                path.push_back(id->getText());
            }
            // Join path segments as module name
            moduleName = path[0];
            for (size_t i = 1; i < path.size(); ++i) {
                moduleName += "::" + path[i];
            }
        }

        // Top-level items
        for (auto topCtx : ctx->topLevel()) {
            if (topCtx->useStmt()) {
                auto result = visit(topCtx->useStmt());
                if (result.has_value()) {
                    useStmts.push_back(std::any_cast<UseStmt*>(result));
                }
            } else if (topCtx->visibilityDecl()) {
                auto result = visit(topCtx->visibilityDecl());
                if (result.has_value()) {
                    statements.push_back(std::any_cast<Stmt*>(result));
                }
            } else if (topCtx->externDecl()) {
                auto result = visit(topCtx->externDecl());
                if (result.has_value()) {
                    statements.push_back(std::any_cast<Stmt*>(result));
                }
            } else if (topCtx->enumDecl()) {
                auto result = visit(topCtx->enumDecl());
                if (result.has_value()) {
                    statements.push_back(std::any_cast<Stmt*>(result));
                }
            } else if (topCtx->statement()) {
                auto result = visit(topCtx->statement());
                if (result.has_value()) {
                    statements.push_back(std::any_cast<Stmt*>(result));
                }
            }
        }

        return (Program*)arena_.alloc<Program>(moduleName, std::move(useStmts), std::move(statements));
    }

    std::any visitStatement(LuvParser::StatementContext *ctx) override {
        if (ctx->funcDecl()) return visit(ctx->funcDecl());
        if (ctx->structDecl()) return visit(ctx->structDecl());
        if (ctx->classDecl()) return visit(ctx->classDecl());
        if (ctx->interfaceDecl()) return visit(ctx->interfaceDecl());
        if (ctx->enumDecl()) return visit(ctx->enumDecl());
        if (ctx->assignment()) return visit(ctx->assignment());
        if (ctx->exprStmt()) return visit(ctx->exprStmt());
        if (ctx->returnStmt()) return visit(ctx->returnStmt());
        if (ctx->breakStmt()) return visit(ctx->breakStmt());
        if (ctx->continueStmt()) return visit(ctx->continueStmt());
        if (ctx->block()) return visit(ctx->block());
        if (ctx->varDecl()) return visit(ctx->varDecl());

        // Control-flow constructs are expressions in the AST and become statements when
        // they appear in the statement grammar.
        if (ctx->ifExpr()) {
            Expr* expr = std::any_cast<Expr*>(visit(ctx->ifExpr()));
            return (Stmt*)arena_.alloc<ExprStmt>(expr);
        }
        if (ctx->whileExpr()) {
            Expr* expr = std::any_cast<Expr*>(visit(ctx->whileExpr()));
            return (Stmt*)arena_.alloc<ExprStmt>(expr);
        }
        if (ctx->forExpr()) {
            Expr* expr = std::any_cast<Expr*>(visit(ctx->forExpr()));
            return (Stmt*)arena_.alloc<ExprStmt>(expr);
        }

        return std::any{};
    }

    // ── Patterns ──
    std::any visitVarPattern(LuvParser::VarPatternContext *ctx) override {
        return (Pattern*)arena_.alloc<VarPattern>(ctx->IDENTIFIER()->getText());
    }

    std::any visitTuplePattern(LuvParser::TuplePatternContext *ctx) override {
        std::vector<Pattern*> patterns;
        for (auto p : ctx->pattern()) {
            patterns.push_back(std::any_cast<Pattern*>(visit(p)));
        }
        return (Pattern*)arena_.alloc<TuplePattern>(std::move(patterns));
    }

    std::any visitStructPattern(LuvParser::StructPatternContext *ctx) override {
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<std::pair<std::string, Pattern*>> fields;
        for (auto f : ctx->fieldPattern()) {
            std::string fname = f->IDENTIFIER()->getText();
            Pattern* p = f->pattern() ? std::any_cast<Pattern*>(visit(f->pattern())) : arena_.alloc<VarPattern>(fname);
            fields.push_back({fname, p});
        }
        return (Pattern*)arena_.alloc<StructPattern>(name, std::move(fields));
    }

    std::any visitArrayPattern(LuvParser::ArrayPatternContext *ctx) override {
        std::vector<Pattern*> patterns;
        for (auto p : ctx->pattern()) {
            patterns.push_back(std::any_cast<Pattern*>(visit(p)));
        }
        return (Pattern*)arena_.alloc<ArrayPattern>(std::move(patterns));
    }

    std::any visitWildcardPattern(LuvParser::WildcardPatternContext *ctx) override {
        return (Pattern*)arena_.alloc<WildcardPattern>();
    }

    std::any visitConstantPattern(LuvParser::ConstantPatternContext *ctx) override {
        return (Pattern*)arena_.alloc<ConstantPattern>(std::any_cast<Expr*>(visit(ctx->primary())));
    }

    // ── Visibility declarations ──
    std::any visitVisibilityDecl(LuvParser::VisibilityDeclContext *ctx) override {
        ASTVisibility vis = ASTVisibility::DEFAULT;
        if (ctx->getText().substr(0, 3) == "pub") {
            vis = ASTVisibility::PUBLIC;
        } else if (ctx->getText().substr(0, 4) == "priv") {
            vis = ASTVisibility::PRIVATE;
        }

        if (ctx->funcDecl()) {
            auto result = visit(ctx->funcDecl());
            FuncDecl* func = std::any_cast<FuncDecl*>(result);
            func->visibility = vis;
            return (Stmt*)func;
        }
        if (ctx->varDecl()) {
            auto result = visit(ctx->varDecl());
            VarDecl* var = std::any_cast<VarDecl*>(result);
            return (Stmt*)var;
        }
        if (ctx->classDecl()) {
            auto result = visit(ctx->classDecl());
            ClassDecl* cls = std::any_cast<ClassDecl*>(result);
            return (Stmt*)cls;
        }
        if (ctx->enumDecl()) {
            auto result = visit(ctx->enumDecl());
            EnumDecl* enm = std::any_cast<EnumDecl*>(result);
            enm->visibility = vis;
            return (Stmt*)enm;
        }
        return std::any{};
    }

    // ── External declarations ──
    std::any visitExternDecl(LuvParser::ExternDeclContext *ctx) override {
        std::string abi = ctx->STRING() ? ctx->STRING()->getText() : "\"C\"";
        // Strip quotes
        abi = abi.substr(1, abi.length() - 2);

        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<Param> params;
        if (ctx->params()) {
            for (auto paramCtx : ctx->params()->param()) {
                bool pDyn = paramCtx->getText().find("dyn") != std::string::npos;
                bool pMut = paramCtx->getText().find("mut") != std::string::npos;
                std::vector<Expr*> pAttrs;
                for (auto* m : paramCtx->modifier()) {
                    if (m->attribute()) {
                        auto attrs = collectAttributes({m->attribute()});
                        pAttrs.insert(pAttrs.end(), attrs.begin(), attrs.end());
                    }
                }
                params.push_back({
                    std::any_cast<Pattern*>(visit(paramCtx->pattern())),
                    paramCtx->type() ? paramCtx->type()->getText() : "int",
                    pDyn, pMut, pAttrs
                });
            }
        }
        std::string returnType = ctx->type() ? ctx->type()->getText() : "int";
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        return (Stmt*)arena_.alloc<ExternDecl>(abi, name, std::move(params), returnType, std::move(attrs));
    }

    // ── Declarations ──
    std::any visitStructDecl(LuvParser::StructDeclContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<StructField> fields;
        for (auto fieldCtx : ctx->structField()) {
            fields.push_back({
                fieldCtx->IDENTIFIER()->getText(),
                fieldCtx->type()->getText()
            });
        }
        return (Stmt*)arena_.alloc<StructDecl>(name, std::move(fields), std::move(attrs));
    }

    std::any visitClassDecl(LuvParser::ClassDeclContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->IDENTIFIER(0)->getText();
        bool isAbstract = ctx->getText().find("abstract") != std::string::npos;
        std::vector<std::string> baseAndInterfaces;
        // Skip first IDENTIFIER (class name)
        for (size_t i = 1; i < ctx->IDENTIFIER().size(); ++i) {
            baseAndInterfaces.push_back(ctx->IDENTIFIER(i)->getText());
        }
        std::vector<ClassField> fields;
        std::vector<FuncDecl*> methods;
        for (auto memberCtx : ctx->classMember()) {
            std::vector<Expr*> mAttrs = collectAttributes(memberCtx->attribute());
            if (memberCtx->classField()) {
                auto f = memberCtx->classField();
                fields.push_back({
                    f->IDENTIFIER()->getText(),
                    f->type()->getText(),
                    memberCtx->getText().find("priv") != std::string::npos,
                    mAttrs
                });
            } else if (memberCtx->funcDecl()) {
                auto result = visit(memberCtx->funcDecl());
                FuncDecl* method = std::any_cast<FuncDecl*>(result);
                method->boundStruct = name; // Mark as method of this class
                method->isOverride = memberCtx->getText().find("override") != std::string::npos;
                method->isStatic = memberCtx->getText().find("static") != std::string::npos;
                // Prepend member-level attributes
                method->attributes.insert(method->attributes.begin(), mAttrs.begin(), mAttrs.end());
                methods.push_back(method);
            }
        }
        return (Stmt*)arena_.alloc<ClassDecl>(name, isAbstract, std::move(baseAndInterfaces), std::move(fields), std::move(methods), std::move(attrs));
    }

    std::any visitInterfaceDecl(LuvParser::InterfaceDeclContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<InterfaceMethod> methods;
        for (auto memberCtx : ctx->interfaceMember()) {
            std::string mname = memberCtx->IDENTIFIER()->getText();
            std::vector<Param> params;
            if (memberCtx->params()) {
                for (auto p : memberCtx->params()->param()) {
                    bool pDyn = p->getText().find("dyn") != std::string::npos;
                    bool pMut = p->getText().find("mut") != std::string::npos;
                    std::vector<Expr*> pAttrs;
                    for (auto* m : p->modifier()) {
                        if (m->attribute()) {
                             auto attrs = collectAttributes({m->attribute()});
                             pAttrs.insert(pAttrs.end(), attrs.begin(), attrs.end());
                        }
                    }
                    Expr* defaultVal = p->expr() ? std::any_cast<Expr*>(visit(p->expr())) : nullptr;
                    params.push_back({
                        std::any_cast<Pattern*>(visit(p->pattern())),
                        p->type() ? p->type()->getText() : "int",
                        pDyn, pMut, pAttrs, defaultVal
                    });
                }
            }
            methods.push_back({
                mname, std::move(params),
                memberCtx->type() ? memberCtx->type()->getText() : "int"
            });
        }
        return (Stmt*)arena_.alloc<InterfaceDecl>(name, std::move(methods), std::move(attrs));
    }

    std::any visitEnumDecl(LuvParser::EnumDeclContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<std::string> typeParams;
        if (ctx->typeParams()) {
            for (auto tp : ctx->typeParams()->IDENTIFIER()) typeParams.push_back(tp->getText());
        }
        std::vector<EnumCase> cases;
        for (auto caseCtx : ctx->enumCase()) {
            std::vector<Param> params;
            if (caseCtx->params()) {
                for (auto p : caseCtx->params()->param()) {
                    params.push_back({
                        std::any_cast<Pattern*>(visit(p->pattern())),
                        p->type() ? p->type()->getText() : "int",
                        false, false, {}
                    });
                }
            }
            cases.push_back({caseCtx->IDENTIFIER()->getText(), std::move(params)});
        }
        return (Stmt*)arena_.alloc<EnumDecl>(name, std::move(typeParams), std::move(cases), std::move(attrs));
    }

    std::any visitVarDecl(LuvParser::VarDeclContext *ctx) override {
        bool isMut = false, isConst = false, isDyn = false;
        std::vector<Expr*> attrs;
        for (auto m : ctx->modifier()) {
            if (m->getText() == "mut") isMut = true;
            else if (m->getText() == "const") isConst = true;
            else if (m->getText() == "dyn") isDyn = true;
            else if (m->attribute()) {
                auto a = collectAttributes({m->attribute()});
                attrs.insert(attrs.end(), a.begin(), a.end());
            }
        }
        std::string type = ctx->type() ? ctx->type()->getText() : "";
        Expr* init = ctx->expr() ? std::any_cast<Expr*>(visit(ctx->expr())) : arena_.alloc<NullExpr>();
        Pattern* p = std::any_cast<Pattern*>(visit(ctx->pattern()));
        return (Stmt*)arena_.alloc<VarDecl>(p, type, isMut, isConst, isDyn, attrs, init);
    }

    std::any visitBlockFunc(LuvParser::BlockFuncContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->funcName()->getText();
        std::string boundStruct = ctx->boundStruct ? ctx->boundStruct->getText() : "";
        
        std::vector<std::string> typeParams;
        if (ctx->typeParams()) {
            for (auto t : ctx->typeParams()->IDENTIFIER()) {
                typeParams.push_back(t->getText());
            }
        }
        std::vector<Param> params;
        if (ctx->params()) {
            for (auto paramCtx : ctx->params()->param()) {
                Expr* defaultVal = paramCtx->expr() ? std::any_cast<Expr*>(visit(paramCtx->expr())) : nullptr;
                std::vector<Expr*> pAttrs;
                for (auto* m : paramCtx->modifier()) {
                    if (m->attribute()) {
                        auto a = collectAttributes({m->attribute()});
                        pAttrs.insert(pAttrs.end(), a.begin(), a.end());
                    }
                }
                params.push_back({
                    std::any_cast<Pattern*>(visit(paramCtx->pattern())),
                    paramCtx->type() ? paramCtx->type()->getText() : "int",
                    paramCtx->getText().find("dyn") != std::string::npos,
                    paramCtx->getText().find("mut") != std::string::npos,
                    pAttrs, defaultVal
                });
            }
        }
        std::string returnType = ctx->type() ? ctx->type()->getText() : "int";
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        return (Stmt*)arena_.alloc<FuncDecl>(boundStruct, name, std::move(typeParams), std::move(params), returnType, body, std::move(attrs));
    }

    std::any visitArrowFunc(LuvParser::ArrowFuncContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        std::string name = ctx->funcName()->getText();
        std::string boundStruct = ctx->boundStruct ? ctx->boundStruct->getText() : "";
        
        std::vector<std::string> typeParams;
        if (ctx->typeParams()) {
            for (auto t : ctx->typeParams()->IDENTIFIER()) {
                typeParams.push_back(t->getText());
            }
        }
        std::vector<Param> params;
        if (ctx->params()) {
            for (auto paramCtx : ctx->params()->param()) {
                Expr* defaultVal = paramCtx->expr() ? std::any_cast<Expr*>(visit(paramCtx->expr())) : nullptr;
                std::vector<Expr*> pAttrs;
                for (auto* m : paramCtx->modifier()) {
                    if (m->attribute()) {
                        auto a = collectAttributes({m->attribute()});
                        pAttrs.insert(pAttrs.end(), a.begin(), a.end());
                    }
                }
                params.push_back({
                    std::any_cast<Pattern*>(visit(paramCtx->pattern())),
                    paramCtx->type() ? paramCtx->type()->getText() : "int",
                    paramCtx->getText().find("dyn") != std::string::npos,
                    paramCtx->getText().find("mut") != std::string::npos,
                    pAttrs, defaultVal
                });
            }
        }
        std::string returnType = ctx->type() ? ctx->type()->getText() : "int";
        Expr* result = std::any_cast<Expr*>(visit(ctx->expr()));
        std::vector<Stmt*> stmts;
        stmts.push_back(arena_.alloc<ReturnStmt>(result));
        Block* body = arena_.alloc<Block>(std::move(stmts));
        return (Stmt*)arena_.alloc<FuncDecl>(boundStruct, name, std::move(typeParams), std::move(params), returnType, body, std::move(attrs));
    }

    // ── Assignments ──
    std::any visitAssignment(LuvParser::AssignmentContext *ctx) override {
        std::vector<Expr*> targets;
        for (auto eCtx : ctx->target->expr()) {
            targets.push_back(std::any_cast<Expr*>(visit(eCtx)));
        }
        Expr* value = std::any_cast<Expr*>(visit(ctx->value));
        std::string op = ctx->op->getText();
        return (Stmt*)arena_.alloc<Assignment>(std::move(targets), value, op);
    }
    std::any visitIndexExpr(LuvParser::IndexExprContext *ctx) override {
        Expr* target = std::any_cast<Expr*>(visit(ctx->expr(0)));
        Expr* index = std::any_cast<Expr*>(visit(ctx->expr(1)));
        return (Expr*)arena_.alloc<IndexExpr>(target, index);
    }

    std::any visitPostfixExpr(LuvParser::PostfixExprContext *ctx) override {
        Expr* expr = std::any_cast<Expr*>(visit(ctx->expr()));
        return (Expr*)arena_.alloc<PostfixExpr>(expr, ctx->op->getText());
    }

    // ── If / ef / else ──
    std::any visitIfExpr(LuvParser::IfExprContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        Expr* cond = std::any_cast<Expr*>(visit(ctx->expr()));
        Block* thenBlock = std::any_cast<Block*>(visitBlock(ctx->block(0)));
        std::vector<EfExpr> efs;
        for (auto efCtx : ctx->efExpr()) {
            efs.push_back({
                std::any_cast<Expr*>(visit(efCtx->expr())),
                std::any_cast<Block*>(visitBlock(efCtx->block()))
            });
        }
        Block* elseBlock = nullptr;
        if (ctx->block().size() > (1 + ctx->efExpr().size())) {
            elseBlock = std::any_cast<Block*>(visitBlock(ctx->block().back()));
        }
        return (Expr*)arena_.alloc<IfExpr>(cond, thenBlock, std::move(efs), elseBlock, std::move(attrs));
    }

    // ── While ──
    std::any visitWhileExpr(LuvParser::WhileExprContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        Expr* cond = std::any_cast<Expr*>(visit(ctx->expr()));
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        return (Expr*)arena_.alloc<WhileExpr>(cond, body, std::move(attrs));
    }

    // ── For loops ──
    std::any visitForRangeExpr(LuvParser::ForRangeExprContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        bool isDynamic = false;
        for (auto* m : ctx->modifier()) {
            if (m->getText() == "dyn") isDynamic = true;
            if (m->attribute()) {
                auto a = collectAttributes({m->attribute()});
                attrs.insert(attrs.end(), a.begin(), a.end());
            }
        }
        Expr* start = std::any_cast<Expr*>(visit(ctx->start));
        Expr* end = std::any_cast<Expr*>(visit(ctx->end));
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        Pattern* p = std::any_cast<Pattern*>(visit(ctx->pattern()));
        return (Expr*)arena_.alloc<ForRangeExpr>(p, isDynamic, start, end, false, body, std::move(attrs));
    }

    std::any visitForRangeIncExpr(LuvParser::ForRangeIncExprContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        bool isDynamic = false;
        for (auto* m : ctx->modifier()) {
            if (m->getText() == "dyn") isDynamic = true;
            if (m->attribute()) {
                auto a = collectAttributes({m->attribute()});
                attrs.insert(attrs.end(), a.begin(), a.end());
            }
        }
        Expr* start = std::any_cast<Expr*>(visit(ctx->start));
        Expr* end = std::any_cast<Expr*>(visit(ctx->end));
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        Pattern* p = std::any_cast<Pattern*>(visit(ctx->pattern()));
        return (Expr*)arena_.alloc<ForRangeExpr>(p, isDynamic, start, end, true, body, std::move(attrs));
    }

    std::any visitForInExpr(LuvParser::ForInExprContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        bool isDynamic = false;
        for (auto* m : ctx->modifier()) {
            if (m->getText() == "dyn") isDynamic = true;
            if (m->attribute()) {
                auto a = collectAttributes({m->attribute()});
                attrs.insert(attrs.end(), a.begin(), a.end());
            }
        }
        Expr* iterable = std::any_cast<Expr*>(visit(ctx->expr()));
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        Pattern* p = std::any_cast<Pattern*>(visit(ctx->pattern()));
        return (Expr*)arena_.alloc<ForInExpr>(p, isDynamic, iterable, body, std::move(attrs));
    }

    std::any visitForCStyle(LuvParser::ForCStyleContext *ctx) override {
        std::vector<Expr*> attrs = collectAttributes(ctx->attribute());
        Stmt* init;
        Stmt* step;
        if (ctx->varDecl()) {
            init = std::any_cast<Stmt*>(visit(ctx->varDecl()));
            step = std::any_cast<Stmt*>(visit(ctx->assignment(0)));
        } else {
            init = std::any_cast<Stmt*>(visit(ctx->assignment(0)));
            step = std::any_cast<Stmt*>(visit(ctx->assignment(1)));
        }
        Expr* cond = std::any_cast<Expr*>(visit(ctx->expr()));
        Block* body = std::any_cast<Block*>(visitBlock(ctx->block()));
        return (Expr*)arena_.alloc<ForCStyleExpr>(init, cond, step, body, std::move(attrs));
    }

    // ── Return ──
    std::any visitReturnStmt(LuvParser::ReturnStmtContext *ctx) override {
        Expr* value = ctx->expr() ? std::any_cast<Expr*>(visit(ctx->expr())) : arena_.alloc<NullExpr>();
        return (Stmt*)arena_.alloc<ReturnStmt>(value);
    }

    std::any visitBreakStmt(LuvParser::BreakStmtContext *ctx) override {
        std::string label = ctx->IDENTIFIER() ? ctx->IDENTIFIER()->getText() : "";
        return (Stmt*)arena_.alloc<BreakStmt>(label);
    }

    std::any visitContinueStmt(LuvParser::ContinueStmtContext *ctx) override {
        std::string label = ctx->IDENTIFIER() ? ctx->IDENTIFIER()->getText() : "";
        return (Stmt*)arena_.alloc<ContinueStmt>(label);
    }

    std::any visitExprStmt(LuvParser::ExprStmtContext *ctx) override {
        return (Stmt*)arena_.alloc<ExprStmt>(std::any_cast<Expr*>(visit(ctx->expr())));
    }

    // ── Block ──
    std::any visitBlock(LuvParser::BlockContext *ctx) override {
        std::vector<Stmt*> statements;
        for (auto stmtCtx : ctx->statement()) {
            auto result = visit(stmtCtx);
            if (result.has_value()) {
                statements.push_back(std::any_cast<Stmt*>(result));
            }
        }
        return (Block*)arena_.alloc<Block>(std::move(statements));
    }

    // ── Expression visitors ──
    std::any visitLogicalOrExpr(LuvParser::LogicalOrExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitLogicalAndExpr(LuvParser::LogicalAndExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitBitwiseOrExpr(LuvParser::BitwiseOrExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitBitwiseXorExpr(LuvParser::BitwiseXorExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitBitwiseAndExpr(LuvParser::BitwiseAndExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitShiftExpr(LuvParser::ShiftExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitAdditiveExpr(LuvParser::AdditiveExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitMultiplicativeExpr(LuvParser::MultiplicativeExprContext *ctx) override {
        return (Expr*)arena_.alloc<BinaryExpr>(std::any_cast<Expr*>(visit(ctx->left)), ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->right)));
    }
    std::any visitUnaryExpr(LuvParser::UnaryExprContext *ctx) override {
        return (Expr*)arena_.alloc<UnaryExpr>(ctx->op->getText(), std::any_cast<Expr*>(visit(ctx->expr())));
    }
    std::any visitTernaryExpr(LuvParser::TernaryExprContext *ctx) override {
        Expr* cond = std::any_cast<Expr*>(visit(ctx->cond));
        Expr* thenE = std::any_cast<Expr*>(visit(ctx->thenExpr));
        Expr* elseE = std::any_cast<Expr*>(visit(ctx->elseExpr));
        return (Expr*)arena_.alloc<TernaryExpr>(cond, thenE, elseE);
    }
    std::any visitComparisonExpr(LuvParser::ComparisonExprContext *ctx) override {
        Expr* L = std::any_cast<Expr*>(visit(ctx->left));
        Expr* R = std::any_cast<Expr*>(visit(ctx->right));
        std::string op = ctx->op->getText();
        if (op == "=") op = "==";
        
        if (auto* chain = dynamic_cast<ComparisonChainExpr*>(L)) {
            chain->operands.push_back(R);
            chain->operators.push_back(op);
            return (Expr*)chain;
        }
        
        std::vector<Expr*> operands = {L, R};
        std::vector<std::string> operators = {op};
        return (Expr*)arena_.alloc<ComparisonChainExpr>(std::move(operands), std::move(operators));
    }
    std::any visitMatchExpr(LuvParser::MatchExprContext *ctx) override {
        Expr* value = std::any_cast<Expr*>(visit(ctx->expr()));
        std::vector<MatchCase> cases;
        for (auto mctx : ctx->matchCase()) {
            MatchCase mc;
            mc.pattern = std::any_cast<Pattern*>(visit(mctx->pattern()));

            if (mctx->resultExpr) {
                mc.resultExpr = std::any_cast<Expr*>(visit(mctx->resultExpr));
            } else if (mctx->resultBlock) {
                mc.resultBlock = std::any_cast<Block*>(visitBlock(mctx->resultBlock));
            }
            cases.push_back(mc);
        }
        return (Expr*)arena_.alloc<MatchExpr>(value, std::move(cases));
    }

    std::any visitCallExpr(LuvParser::CallExprContext *ctx) override {
        std::string callee = ctx->IDENTIFIER()->getText();
        std::vector<Expr*> args;
        if (ctx->args()) {
            for (auto argCtx : ctx->args()->expr()) {
                args.push_back(std::any_cast<Expr*>(visit(argCtx)));
            }
        }
        return (Expr*)arena_.alloc<CallExpr>(callee, std::move(args));
    }

    std::any visitGenericCallExpr(LuvParser::GenericCallExprContext *ctx) override {
        std::string callee = ctx->IDENTIFIER()->getText();
        std::vector<std::string> typeArgs;
        for (auto t : ctx->type()) {
            typeArgs.push_back(t->getText());
        }
        std::vector<Expr*> args;
        if (ctx->args()) {
            for (auto argCtx : ctx->args()->expr()) {
                args.push_back(std::any_cast<Expr*>(visit(argCtx)));
            }
        }
        return (Expr*)arena_.alloc<GenericCallExpr>(callee, std::move(typeArgs), std::move(args));
    }

    std::any visitStructInstExpr(LuvParser::StructInstExprContext *ctx) override {
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<std::pair<std::string, Expr*>> fields;
        if (ctx->structInstFields()) {
            auto ids = ctx->structInstFields()->IDENTIFIER();
            auto exprs = ctx->structInstFields()->expr();
            for (size_t i = 0; i < ids.size(); ++i) {
                fields.push_back({
                    ids[i]->getText(),
                    std::any_cast<Expr*>(visit(exprs[i]))
                });
            }
        }
        return (Expr*)arena_.alloc<StructInstExpr>(name, std::move(fields));
    }

    std::any visitIntrinsicCallExpr(LuvParser::IntrinsicCallExprContext *ctx) override {
        std::string callee = ctx->IDENTIFIER()->getText();
        std::vector<Expr*> args;
        if (ctx->args()) {
            for (auto argCtx : ctx->args()->expr()) {
                args.push_back(std::any_cast<Expr*>(visit(argCtx)));
            }
        }
        return (Expr*)arena_.alloc<IntrinsicCallExpr>(callee, std::move(args));
    }

    std::any visitAsmExpr(LuvParser::AsmExprContext *ctx) override {
        std::string s = ctx->STRING()->getText();
        return (Expr*)arena_.alloc<AsmExpr>(s.substr(1, s.length() - 2));
    }

    std::any visitCastExpr(LuvParser::CastExprContext *ctx) override {
        Expr* expr = std::any_cast<Expr*>(visit(ctx->expr()));
        std::string targetType = ctx->type()->getText();
        std::string op = ctx->op->getText();
        bool isForced = (op == "as!" || op == "|>");
        return (Expr*)arena_.alloc<CastExpr>(expr, targetType, isForced);
    }

    std::any visitPropertyExpr(LuvParser::PropertyExprContext *ctx) override {
        Expr* obj = std::any_cast<Expr*>(visit(ctx->expr()));
        std::string name = ctx->IDENTIFIER()->getText();
        return (Expr*)arena_.alloc<PropertyExpr>(obj, name);
    }

    std::any visitMethodCallExpr(LuvParser::MethodCallExprContext *ctx) override {
        Expr* obj = std::any_cast<Expr*>(visit(ctx->expr()));
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<Expr*> args;
        if (ctx->args()) {
            for (auto argCtx : ctx->args()->expr()) {
                args.push_back(std::any_cast<Expr*>(visit(argCtx)));
            }
        }
        return (Expr*)arena_.alloc<MethodCallExpr>(obj, name, std::move(args));
    }

    std::any visitSuperCallExpr(LuvParser::SuperCallExprContext *ctx) override {
        std::string name = ctx->IDENTIFIER()->getText();
        std::vector<Expr*> args;
        if (ctx->args()) {
            for (auto argCtx : ctx->args()->expr()) {
                args.push_back(std::any_cast<Expr*>(visit(argCtx)));
            }
        }
        return (Expr*)arena_.alloc<SuperCallExpr>(name, std::move(args));
    }

    // ── Literals ──
    std::any visitIntLiteral(LuvParser::IntLiteralContext *ctx) override {
        return (Expr*)arena_.alloc<IntExpr>(ctx->INT()->getText());
    }

    std::any visitFloatLiteral(LuvParser::FloatLiteralContext *ctx) override {
        return (Expr*)arena_.alloc<FloatExpr>(ctx->FLOAT()->getText());
    }

    std::any visitStringLiteral(LuvParser::StringLiteralContext *ctx) override {
        std::string s;
        if (ctx->STRING()) s = ctx->STRING()->getText();
        else s = ctx->BACKTICK_STRING()->getText();
        return (Expr*)arena_.alloc<StringExpr>(s.substr(1, s.length() - 2));
    }

    std::any visitStringInterpolationExpr(LuvParser::StringInterpolationExprContext *ctx) override {
        std::string s;
        if (ctx->STRING()) s = ctx->STRING()->getText();
        else s = ctx->BACKTICK_STRING()->getText();
        // Strip delimiters (either " or `)
        s = s.substr(1, s.length() - 2);
        
        std::vector<Expr*> parts;
        size_t i = 0;
        while (i < s.length()) {
            size_t start = s.find('{', i);
            if (start == std::string::npos) {
                parts.push_back(arena_.alloc<StringExpr>(s.substr(i)));
                break;
            }
            if (start > i) {
                parts.push_back(arena_.alloc<StringExpr>(s.substr(i, start - i)));
            }
            size_t end = s.find('}', start);
            if (end == std::string::npos) {
                parts.push_back(arena_.alloc<StringExpr>(s.substr(start)));
                break;
            }
            
            std::string exprStr = s.substr(start + 1, end - start - 1);
            antlr4::ANTLRInputStream input(exprStr);
            LuvLexer lexer(&input);
            antlr4::CommonTokenStream tokens(&lexer);
            LuvParser parser(&tokens);
            LuvParser::ExprContext* exprCtx = parser.expr();
            
            if (exprCtx) {
                std::any result = visit(exprCtx);
                parts.push_back(std::any_cast<Expr*>(result));
            }
            i = end + 1;
        }
        return (Expr*)arena_.alloc<StringInterpolationExpr>(std::move(parts));
    }

    std::any visitCharLiteral(LuvParser::CharLiteralContext *ctx) override {
        return (Expr*)arena_.alloc<CharExpr>(ctx->CHAR()->getText()[1]);
    }

    std::any visitBoolLiteral(LuvParser::BoolLiteralContext *ctx) override {
        return (Expr*)arena_.alloc<BoolExpr>(ctx->BOOL()->getText() == "true");
    }

    std::any visitNullLiteral(LuvParser::NullLiteralContext *ctx) override {
        return (Expr*)arena_.alloc<NullExpr>();
    }

    std::any visitIdentifier(LuvParser::IdentifierContext *ctx) override {
        return (Expr*)arena_.alloc<VarExpr>(ctx->IDENTIFIER()->getText());
    }

    std::any visitGroupingExpr(LuvParser::GroupingExprContext *ctx) override {
        return visit(ctx->expr());
    }

    std::any visitTupleExpr(LuvParser::TupleExprContext *ctx) override {
        std::vector<Expr*> elements;
        for (auto exprCtx : ctx->expr()) {
            elements.push_back(std::any_cast<Expr*>(visit(exprCtx)));
        }
        return (Expr*)arena_.alloc<TupleExpr>(std::move(elements));
    }

    std::any visitArrayExpr(LuvParser::ArrayExprContext *ctx) override {
        std::vector<Expr*> elements;
        if (ctx->args()) {
            for (auto exprCtx : ctx->args()->expr()) {
                elements.push_back(std::any_cast<Expr*>(visit(exprCtx)));
            }
        }
        return (Expr*)arena_.alloc<ArrayExpr>(std::move(elements));
    }

    std::any visitArrayRepeatExpr(LuvParser::ArrayRepeatExprContext *ctx) override {
        Expr* value = std::any_cast<Expr*>(visit(ctx->expr(0)));
        Expr* count = std::any_cast<Expr*>(visit(ctx->expr(1)));
        return (Expr*)arena_.alloc<ArrayRepeatExpr>(value, count);
    }

    std::any visitArrayCompExpr(LuvParser::ArrayCompExprContext *ctx) override {
        Expr* expr = std::any_cast<Expr*>(visit(ctx->expr(0)));
        std::string varName = ctx->IDENTIFIER()->getText();
        Expr* iterable = std::any_cast<Expr*>(visit(ctx->expr(1)));
        
        Expr* rangeEnd = nullptr;
        bool inc = false;
        Expr* step = nullptr;
        
        if (ctx->expr().size() > 2) {
            rangeEnd = std::any_cast<Expr*>(visit(ctx->expr(2)));
            inc = ctx->RANGE_INC() != nullptr;
            if (ctx->expr().size() > 3) {
                step = std::any_cast<Expr*>(visit(ctx->expr(3)));
            }
        }
        
        return (Expr*)arena_.alloc<ArrayCompExpr>(expr, varName, iterable, rangeEnd, inc, step);
    }

    std::any visitSliceExpr(LuvParser::SliceExprContext *ctx) override {
        Expr* target = std::any_cast<Expr*>(visit(ctx->expr(0)));
        Expr* start = std::any_cast<Expr*>(visit(ctx->expr(1)));
        Expr* end = std::any_cast<Expr*>(visit(ctx->expr(2)));
        bool inc = ctx->RANGE_INC() != nullptr;
        Expr* step = nullptr;
        if (ctx->expr().size() > 3) {
            step = std::any_cast<Expr*>(visit(ctx->expr(3)));
        }
        return (Expr*)arena_.alloc<SliceExpr>(target, start, end, inc, step);
    }

    std::any visitUseFromStmt(LuvParser::UseFromStmtContext *ctx) override {
        auto kind = UseStmt::SINGLE;
        std::vector<UseStmt::SymbolAlias> symbols;
        if (ctx->useTarget()->useAllPublic()) kind = UseStmt::ALL_PUBLIC;
        else if (ctx->useTarget()->useAllPrivate()) kind = UseStmt::ALL_PRIVATE;
        else if (ctx->useTarget()->useSingle()) {
            kind = UseStmt::SINGLE;
            symbols.push_back({ctx->useTarget()->IDENTIFIER()->getText(), ""});
        } else if (ctx->useTarget()->useSet()) {
            kind = UseStmt::SET;
            for (auto item : ctx->useTarget()->useList()->useListItem()) {
                symbols.push_back({item->IDENTIFIER()->getText(), item->aliasing() ? item->aliasing()->IDENTIFIER()->getText() : ""});
            }
        }

        std::vector<std::string> modulePath;
        for (auto id : ctx->modulePath(0)->IDENTIFIER()) modulePath.push_back(id->getText());
        
        std::string moduleAlias = ctx->aliasing() ? ctx->aliasing()->IDENTIFIER()->getText() : "";
        
        std::vector<std::string> thenTo;
        if (ctx->modulePath().size() > 1) {
            for (auto id : ctx->modulePath(1)->IDENTIFIER()) thenTo.push_back(id->getText());
        }

        return (Stmt*)arena_.alloc<UseStmt>(kind, std::move(symbols), std::move(modulePath), moduleAlias, std::move(thenTo));
    }

    std::any visitUsePathStmt(LuvParser::UsePathStmtContext *ctx) override {
        std::vector<std::string> modulePath;
        for (auto id : ctx->modulePath()->IDENTIFIER()) modulePath.push_back(id->getText());
        std::string alias = ctx->aliasing() ? ctx->aliasing()->IDENTIFIER()->getText() : "";
        return (Stmt*)arena_.alloc<UseStmt>(UseStmt::PATH, std::vector<UseStmt::SymbolAlias>{}, std::move(modulePath), alias);
    }

private:
    std::vector<Expr*> collectAttributes(const std::vector<LuvParser::AttributeContext*>& attrCtxs) {
        std::vector<Expr*> attrs;
        for (auto* ctx : attrCtxs) {
            if (ctx->AT()) {
                attrs.push_back(arena_.alloc<VarExpr>(ctx->IDENTIFIER()->getText()));
                if (ctx->IDENTIFIER().size() > 1) {
                    // Handle @attr(val) - for now just as a call
                     std::vector<Expr*> args;
                     args.push_back(arena_.alloc<VarExpr>(ctx->IDENTIFIER(1)->getText()));
                     attrs.back() = arena_.alloc<CallExpr>(ctx->IDENTIFIER(0)->getText(), std::move(args));
                }
            } else if (ctx->attrList()) {
                for (auto* a : ctx->attrList()->attr()) {
                    attrs.push_back(std::any_cast<Expr*>(visit(a)));
                }
            }
        }
        return attrs;
    }

    std::string currentFile_;
    Arena& arena_;
};

} // namespace luv

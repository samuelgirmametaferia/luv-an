grammar Luv;

program: (moduleDecl)? topLevel* EOF;

topLevel
    : useStmt
    | visibilityDecl
    | externDecl
    | statement
    ;

// Module declaration (optional, defaults to filename)
moduleDecl: 'module' modulePath;

// Use/import statements
useStmt
    : 'use' useTarget 'from' modulePath    # useFromStmt
    | 'use' modulePath                      # usePathStmt
    ;

useTarget
    : '*'                                   # useAllPublic
    | AT                                    # useAllPrivate
    | IDENTIFIER                            # useSingle
    | '{' useList '}'                       # useSet
    ;

useList: IDENTIFIER (',' IDENTIFIER)*;

modulePath: IDENTIFIER (PATH_SEP IDENTIFIER)*;

// Visibility wrapper for top-level declarations
visibilityDecl: ('pub' | 'priv') (funcDecl | varDecl | externDecl | structDecl | classDecl | interfaceDecl | enumDecl);

externDecl: 'extern' (STRING)? 'fn' IDENTIFIER '(' params? ')' (':' type)?;

statement
    : funcDecl
    | structDecl
    | enumDecl
    | classDecl
    | interfaceDecl
    | assignment
    | exprStmt
    | ifExpr
    | whileExpr
    | forExpr
    | returnStmt
    | breakStmt
    | continueStmt
    | block
    | varDecl
    ;

breakStmt: 'break' IDENTIFIER? ;
continueStmt: 'continue' IDENTIFIER? ;

block: '{' statement* '}';

structDecl: (attribute)* 'struct'? IDENTIFIER '{' structMember* '}' ;
structMember: (attribute | 'pub' | 'priv')* (structField | declaration) ;
structField: IDENTIFIER ':' type ;

enumDecl: (attribute)* 'enum' IDENTIFIER '{' enumVariant* '}' ;
enumVariant: IDENTIFIER ('(' typeList ')')? ','? ;
typeList: type (',' type)* ;

classDecl: (attribute)* 'abstract'? 'class'? IDENTIFIER (':' IDENTIFIER (',' IDENTIFIER)*)? '{' classMember* '}' ;
classMember: (attribute | 'pub' | 'priv' | 'override' | 'static')* (funcDecl | classField | declaration) ;
classField: IDENTIFIER ':' type ;

declaration: structDecl | enumDecl | classDecl | interfaceDecl | funcDecl | varDecl ;

interfaceDecl: (attribute)* 'interface' IDENTIFIER '{' interfaceMember* '}' ;
interfaceMember: 'fn' IDENTIFIER '(' params? ')' (':' type)? ;

varDecl: (modifier | 'var')* bindingPatternList (':' type)? ('=' expr)? ;
bindingPatternList: bindingPattern (',' bindingPattern)* ;
bindingPattern
    : IDENTIFIER                                    # identifierPattern
    | '(' bindingPatternList ')'                    # tuplePattern
    | IDENTIFIER '{' structBindingList? '}'         # structPattern
    | IDENTIFIER '(' bindingPatternList? ')'        # variantPattern
    | '_'                                           # wildcardPattern
    | literal                                       # literalPattern
    ;

structBindingList: structBinding (',' structBinding)*;
structBinding: IDENTIFIER (':' bindingPattern)?;

modifier: 'mut' | 'const' | 'dyn' | 'var' | memoryHint | attribute ;
attribute
    : AT IDENTIFIER ('(' IDENTIFIER ')')?
    | '![' attrList ']'
    ;

memoryHint: AT ('stack' | 'heap' | 'rc' | 'arc' | 'gc' | 'pool' | 'static') ;

attrList: attr (',' attr)*;
attr: IDENTIFIER ('(' IDENTIFIER ')')?;

overloadableOp: '+' | '-' | '*' | '/' | '%' | '==' | '!=' | '<' | '>' | '<=' | '>=';
funcName: IDENTIFIER | overloadableOp;

funcDecl
    : (attribute)* ('fn')? ('&'? boundStruct=IDENTIFIER)? funcName typeParams? '(' params? ')' (':' type)? block               # blockFunc
    | (attribute)* ('fn')? ('&'? boundStruct=IDENTIFIER)? funcName typeParams? '(' params? ')' (':' type)? '->' expr          # arrowFunc
    ;

typeParams: '[' IDENTIFIER (',' IDENTIFIER)* ';';

params: param (',' param)*;
param: modifier* IDENTIFIER (':' type)? ('=' expr)?;

type
    : typeCore '?'?
    ;

typeCore
    : 'u8' | 'u16' | 'u32' | 'u64' | 'u128' | 'u256'
    | 'i8' | 'i16' | 'i32' | 'i64' | 'i128' | 'i256'
    | 'f16' | 'f32' | 'f64' | 'f80' | 'f128'
    | 'int' | 'uint' | 'float'
    | 'string' | 'char' | 'bool' | 'bit' | 'bytes'
    | 'void' | 'dyn'
    | IDENTIFIER   // for type parameters like T, U, K, V
    | '[' type (';' expr)? ']'                     // Array type
    | '(' type (',' type)* ')'                     // Tuple type
    ;

ifExpr: (attribute)* 'if' expr block (efExpr)* ('else' block)?;
efExpr: 'ef' expr block;

whileExpr: (attribute)* 'while' expr block;

forExpr
    : (attribute)* 'for' modifier* bindingPatternList 'in' start=expr RANGE end=expr block     # forRangeExpr
    | (attribute)* 'for' modifier* bindingPatternList 'in' start=expr RANGE_INC end=expr block # forRangeIncExpr
    | (attribute)* 'for' modifier* bindingPatternList 'in' expr block                          # forInExpr
    | (attribute)* 'for' (varDecl | assignment) ';' expr ';' assignment block            # forCStyle
    ;

returnStmt: 'return' expr?;
exprStmt: expr;

structInstFields: IDENTIFIER ':' expr (',' IDENTIFIER ':' expr)*;

assignmentTarget: expr (',' expr)* ;
assignment: target=assignmentTarget op=('=' | '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' | '<<=' | '>>=') value=expr;
expr
    : 'super' '.' IDENTIFIER '(' args? ')'                       # superCallExpr
    | expr ('.' | '->') IDENTIFIER '(' args? ')'                 # methodCallExpr
    | expr ('.' | '->') IDENTIFIER                               # propertyExpr
    | expr '[' expr ']'                                          # indexExpr
    | expr '[' expr (RANGE | RANGE_INC) expr (':' expr)? ']'     # sliceExpr
    | expr op=('++' | '--')                                      # postfixExpr
    | expr op=('as' | '->' | 'as!' | '|>') type                  # castExpr

    | op=('!' | 'not' | '~' | '-') expr                          # unaryExpr
    | left=expr op=('*' | '/' | '%') right=expr                  # multiplicativeExpr
    | left=expr op=('+' | '-') right=expr                        # additiveExpr
    | left=expr op=('<<' | '>>') right=expr                      # shiftExpr
    | left=expr op=('<' | '>' | '<=' | '>=' | '==' | '!=' | '=') right=expr          # comparisonExpr
    | left=expr op='&' right=expr                                # bitwiseAndExpr
    | left=expr op='^' right=expr                                # bitwiseXorExpr
    | left=expr op='|' right=expr                                # bitwiseOrExpr
    | left=expr op=('&&' | 'and') right=expr                     # logicalAndExpr
    | left=expr op=('||' | 'or') right=expr                      # logicalOrExpr
    | cond=expr '?' thenExpr=expr ':' elseExpr=expr             # ternaryExpr
    | IDENTIFIER '{' structInstFields? '}'                       # structInstExpr
    | IDENTIFIER '(' args? ')'                                   # callExpr
    | IDENTIFIER '[' type (',' type)* ']' '(' args? ')'          # genericCallExpr
    | AT IDENTIFIER '(' args? ')'                                # intrinsicCallExpr
    | 'asm' '{' STRING '}'                                       # asmExpr
    | 'match' expr '{' matchCase* '}'                            # matchExpr
    | ifExpr                                                     # ifExprAlternative
    | forExpr                                                    # forExprAlternative
    | whileExpr                                                  # whileExprAlternative
    | primary                                                    # primaryExpr
    ;

matchCase: (pattern=bindingPattern) '=>' (resultExpr=expr | resultBlock=block) ','? ;

args: expr (',' expr)*;

literal
    : INT                                  # intLit
    | FLOAT                                # floatLit
    | STRING                               # stringLit
    | BACKTICK_STRING                      # stringLit
    | CHAR                                 # charLit
    | BOOL                                 # boolLit
    | 'nen'                                # nullLit
    ;

primary
    : literal                              # primaryLiteral
    | '&' STRING                           # stringInterpolationExpr
    | '&' BACKTICK_STRING                  # stringInterpolationExpr
    | IDENTIFIER                           # identifier
    | '(' expr ')'                         # groupingExpr
    | '(' expr (',' expr)+ ')'             # tupleExpr
    | '[' args? ']'                        # arrayExpr
    | '[' expr ';' expr ']'                # arrayRepeatExpr
    | '[' expr 'for' IDENTIFIER 'in' expr (RANGE | RANGE_INC)? expr? ']' # arrayCompExpr
    ;

// Lexer
// Block comments /* ... */ (including nested) are stripped by
// the preprocessSource() step in main.cpp before ANTLR lexing.
// Unclosed string detection is also handled by the preprocessor.
AT: '@';
PATH_SEP: '::';
RANGE_INC: '...';
RANGE: '..';
FLOAT: [0-9]+ '.' [0-9]+;
INT: [0-9]+;
STRING: '"' (~["\\\r\n] | '\\' .)* '"';
BACKTICK_STRING: '`' (~[`\\] | '\\' .)* '`';
CHAR: '\'' (~['\\\r\n] | '\\' .) '\'';
BOOL: 'true' | 'false';
IDENTIFIER: [a-zA-Z_] [a-zA-Z0-9_]*;

WS: [ \t\r\n]+ -> skip;
COMMENT: '#' ~[\r\n]* -> skip;
LINE_COMMENT: '//' ~[\r\n]* -> skip;

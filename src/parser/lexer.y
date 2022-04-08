/**
 * @file
 * SQL tokenizer definition.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */

%{
/* need this for the call to atof() below */
#include <math.h>
#include "parser.h"
#include "parser/common.h"
%}

DIGIT    [0-9]
ID       [a-z][a-z0-9_]*

%%

(({DIGIT}+"."{DIGIT}*)|({DIGIT}*"."{DIGIT}+)|{DIGIT}+)"e"("+"|"-")?{DIGIT}+ {
    return FLOAT_CONSTANT;
}

({DIGIT}+"."{DIGIT}*)|({DIGIT}*"."{DIGIT}+) {
    return DECIMAL_CONSTANT;
}

{DIGIT}+ {
    return INTEGER_CONSTANT;
}

(\"([^\\\"]|\\.)*\")|(\'([^\\\']|\\.)*\') {
    return STRING_CONSTANT;
}

"select" {
    return SELECT_TK;
}

"from" {
    return FROM;
}

"where" {
    return WHERE;
}

"group by" {
    return GROUPBY;
}

"order by" {
    return ORDERBY;
}

"limit" {
    return LIMIT_TK;
}

"asc" {
    return ASC_TK;
}

"desc" {
    return DESC_TK;
}

"create table" {
    return CREATE_TABLE_TK;
}

"bulk insert" {
    return BULK_INSERT_TK;
}

"fieldterminator" {
    return FIELDTERMINATOR_TK;
}

"firstrow" {
    return FIRSTROW_TK;
}

"with" {
    return WITH_TK;
}

"sum" {
    return SUM_TK;
}

"count" {
    return COUNT_TK;
}

"avg" {
    return AVG_TK;
}

"min" {
    return MIN_TK;
}

"max" {
    return MAX_TK;
}

"between" {
    return BETWEEN_TK;
}

"(" {
    return LPAREN;
}

")" {
    return RPAREN;
}

"+" {
    return PLUS_TK;
}

"-" {
    return MINUS_TK;
}

"*" {
    return MUL_TK;
}

"/" {
    return DIV_TK;
}

">=" {
    return GE_TK;
}

">" {
    return GT_TK;
}

"<=" {
    return LE_TK;
}

"<" {
    return LT_TK;
}

"=" {
    return EQ_TK;
}

"<>" {
    return NEQ_TK;
}

"," {
    return COMMA;
}

"::" {
    return TYPECAST_TK;
}

"and" {
    return AND_TK;
}

"in" {
    return IN_TK;
}

"like" {
    return LIKE_TK;
}

"or" {
    return OR_TK;
}

"as" {
    return AS_TK;
}

"bigint" {
    return BIGINT_TK;
}

"int" {
    return INT_TK;
}

"date" {
    return DATE_TK;
}

"decimal" {
    return DECIMAL_TK;
}

"char" {
    return CHAR_TK;
}

"varchar" {
    return VARCHAR_TK;
}

"case" {
    return CASE_TK;
}

"when" {
    return WHEN_TK;
}

"then" {
    return THEN_TK;
}

"else" {
    return ELSE_TK;
}

"end" {
    return END_TK;
}

{ID} {
    return IDENTIFIER;
}

"--"[^\n]*"\n"     /* eat up one-line comments */

[ \t\n]+          /* eat up whitespace */

. {
    printf( "Unrecognized character: %s\n", yytext );
    return ERROR;
}

%%

/**
 * @file
 * SQL parser definition.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
%token_type      { Expr* }
%extra_argument  { Query* query } 
%syntax_error    { query->parseError = true; }

%left OR_TK.
%left AND_TK.
%left LT_TK GT_TK LE_TK GE_TK.
%left EQ_TK NEQ_TK.
%left BETWEEN_TK.
%left IN_TK.
%left PLUS_TK MINUS_TK.
%left MUL_TK DIV_TK.
%left LIKE_TK.
%left TYPECAST_TK.
%left SUM_TK.

/* import */
entry ::= CREATE_TABLE_TK IDENTIFIER(A) LPAREN schema(B) RPAREN.
{
    query->tag        = Query::CREATE_TABLE;
    query->tableName  = A->symbol;
    query->schemaExpr = B;
}

entry ::= BULK_INSERT_TK IDENTIFIER(A) FROM STRING_CONSTANT(B) importWith.
{
    query->tag = Query::BULK_INSERT;
    query->tableName = A->symbol;
    query->fileName = B->symbol;
}

/* query */
entry ::= select from where groupby orderby limit.
{
    query->tag = Query::SELECT; 
}


importWith  ::= .
importWith  ::= WITH_TK LPAREN csvSpecList RPAREN.
csvSpecList ::= csvSpec COMMA csvSpecList.
csvSpecList ::= csvSpec.
csvSpec     ::= FIRSTROW_TK EQ_TK INTEGER_CONSTANT(A).       { query->firstRow        = A->value.bigintData; }
csvSpec     ::= FIELDTERMINATOR_TK EQ_TK STRING_CONSTANT(A). { query->fieldTerminator = A->symbol; }

schema(A)      ::= schemaElem(B) COMMA schema(C).  { B->next = C; A = B; } 
schema(A)      ::= schemaElem(B).                  { A = B; }
schemaElem(A)  ::= IDENTIFIER(B) type(C).          { A = ExprGen::attr ( B->symbol ); A->type = C->type; } 

select      ::= SELECT_TK namableExprList(A).      { query->selectExpr = A; }
select      ::= SELECT_TK MUL_TK.                  { query->selectExpr = ExprGen::star(); }
from        ::= FROM    tableList(A).              { query->fromExpr   = A; }
from        ::= .
where       ::= WHERE   expr(A).                   { query->whereExpr  = A; }
where       ::= .
groupby     ::= GROUPBY exprList(A).               { query->groupbyExpr  = A; }
groupby     ::= .
orderby     ::= ORDERBY exprList(A).               { query->orderbyExpr  = A; }
orderby     ::= .
limit       ::= LIMIT_TK INTEGER_CONSTANT(A).      { 
    query->useLimit = true; 
    query->limit = A->value.bigintData; 
}
limit       ::= .

tableList(A) ::= table(B) COMMA tableList(C).     { B->next = C; A = B; }
tableList(A) ::= table(B).                        { A = B; }
table(A)     ::= IDENTIFIER.                      { A = ExprGen::table ( A->symbol ); }

namableExprList(A) ::= namableExpr(B) COMMA namableExprList(C).
{ 
    B->next = C; A = B; 
}
namableExprList(A) ::= namableExpr(B).            { A = B; } 
namableExpr(A) ::= expr(B) AS_TK IDENTIFIER(C).   { A = ExprGen::as ( C->symbol, B ); }
namableExpr(A) ::= expr(B).                       { A = B; }

exprList(A) ::= expr(B) COMMA exprList(C).        { B->next = C; A = B; }
exprList(A) ::= expr(B).                          { A = B; }
expr(A)     ::= expr(B) PLUS_TK     expr(C).      { A = ExprGen::add  ( B, C ); }
expr(A)     ::= expr(B) MINUS_TK    expr(C).      { A = ExprGen::sub  ( B, C ); }
expr(A)     ::= expr(B) MUL_TK      expr(C).      { A = ExprGen::mul  ( B, C ); }
expr(A)     ::= expr(B) DIV_TK      expr(C).      { A = ExprGen::div  ( B, C ); }
expr(A)     ::= expr(B) EQ_TK       expr(C).      { A = ExprGen::eq   ( B, C ); }
expr(A)     ::= expr(B) NEQ_TK      expr(C).      { A = ExprGen::neq  ( B, C ); }
expr(A)     ::= expr(B) GT_TK       expr(C).      { A = ExprGen::gt   ( B, C ); }
expr(A)     ::= expr(B) GE_TK       expr(C).      { A = ExprGen::ge   ( B, C ); }
expr(A)     ::= expr(B) LT_TK       expr(C).      { A = ExprGen::lt   ( B, C ); }
expr(A)     ::= expr(B) LE_TK       expr(C).      { A = ExprGen::le   ( B, C ); }
expr(A)     ::= expr(B) AND_TK      expr(C).      { A = ExprGen::and_ ( B, C ); }
expr(A)     ::= expr(B) OR_TK       expr(C).      { A = ExprGen::or_  ( B, C ); }
expr(A)     ::= expr(B) TYPECAST_TK type(C).      { A = ExprGen::typecast ( C->type, B ); } 
expr(A)     ::= expr(B) LIKE_TK STRING_CONSTANT(C).      
{ 
    A = ExprGen::like ( B, C ); 
} 
expr(A) ::= expr(B) BETWEEN_TK expr(C) AND_TK expr(D).
{ 
    A = ExprGen::and_ ( ExprGen::ge ( copyExpr ( B ), C ), ExprGen::le ( B, D ) );
} 

expr(A)     ::= SUM_TK LPAREN expr(B) RPAREN.     { A = ExprGen::sum   ( B ); } 
expr(A)     ::= AVG_TK LPAREN expr(B) RPAREN.     { A = ExprGen::avg   ( B ); } 
expr(A)     ::= MIN_TK LPAREN expr(B) RPAREN.     { A = ExprGen::min   ( B ); } 
expr(A)     ::= MAX_TK LPAREN expr(B) RPAREN.     { A = ExprGen::max   ( B ); } 
expr(A)     ::= COUNT_TK LPAREN starOrExpr(B) RPAREN.   
{ 
    A = ExprGen::count ( B ); 
} 

expr(A)     ::= LPAREN expr(B) RPAREN.            { A = B; }
expr(A)     ::= IDENTIFIER.                       { A = ExprGen::attr ( A->symbol ); }
expr(A)     ::= value(B).                         { A = B; }
expr(A)     ::= IDENTIFIER ASC_TK.                { A = ExprGen::asc  ( ExprGen::attr ( A->symbol ) ); }
expr(A)     ::= IDENTIFIER DESC_TK.               { A = ExprGen::desc ( ExprGen::attr ( A->symbol ) ); }
/*todo..*/
/*expr(A)     ::= expr(B) ASC_TK.                { A = ExprGen::asc  ( child ); }*/
/*expr(A)     ::= expr(B) DESC_TK.               { A = ExprGen::desc ( child ); }*/

starOrExpr(A) ::= MUL_TK.                         { A = ExprGen::star ( ); }
starOrExpr(A) ::= expr(B).                        { A = B; }

expr(A) ::= expr(B) IN_TK LPAREN exprList(C) RPAREN. {
    Expr* res = ExprGen::eq ( ExprGen::copy(B), ExprGen::copy(C) );
    Expr* entry = C->next;
    while ( entry != nullptr ) {
        res = ExprGen::or_ (
            ExprGen::eq ( ExprGen::copy(B), ExprGen::copy(entry) ),
            res
        );
        entry = entry->next;
    }
    A = res;
}

/* case */
expr(A) ::= CASE_TK whenThenList(B) else(C) END_TK. {
    B->next = C;
    A = ExprGen::case_ ( B ); 
}
else(A) ::= .                                     { A = nullptr; }
else(A) ::= ELSE_TK expr(B).                      { A = B;       }
whenThenList(A) ::= whenThen(B).                  { A = B; }
whenThenList(A) ::= whenThen(B) whenThenList(C).  { B->next = C; A = B; }
whenThen(A) ::= WHEN_TK expr(B) THEN_TK expr(C).  { A = ExprGen::whenThen ( B, C ); }

/* constants */
value(A)    ::= MINUS_TK INTEGER_CONSTANT(B).     { A = B; A->value.bigintData  *= -1; }
value(A)    ::= MINUS_TK DECIMAL_CONSTANT(B).     { A = B; A->value.decimalData *= -1; }
value(A)    ::= MINUS_TK FLOAT_CONSTANT(B).       { A = B; A->value.floatData   *= -1.0; }
value       ::= INTEGER_CONSTANT.
value       ::= DECIMAL_CONSTANT.
value       ::= FLOAT_CONSTANT.
value       ::= STRING_CONSTANT.
value(A)    ::= DATE_TK STRING_CONSTANT(B).       { A = ExprGen::constant ( B->symbol, SqlType::DATE ); }

/* types for typecast and schemas */
type(A)     ::= INT_TK.                          { A = ExprGen::type ( TypeInit::INT() ); }
type(A)     ::= BIGINT_TK.                       { A = ExprGen::type ( TypeInit::BIGINT() ); }
type(A)     ::= DATE_TK.                         { A = ExprGen::type ( TypeInit::DATE() ); }
type(A)     ::= CHAR_TK LPAREN INTEGER_CONSTANT(B) RPAREN. 
{
    A = ExprGen::type ( TypeInit::CHAR ( B->value.bigintData ) );
}
type(A)     ::= VARCHAR_TK LPAREN INTEGER_CONSTANT(B) RPAREN. 
{
    A = ExprGen::type ( TypeInit::VARCHAR ( B->value.bigintData ) );
}
type(A)     ::= DECIMAL_TK LPAREN INTEGER_CONSTANT(B) COMMA INTEGER_CONSTANT(C) RPAREN. 
{
    A = ExprGen::type ( TypeInit::DECIMAL ( B->value.bigintData, C->value.bigintData ) );
}

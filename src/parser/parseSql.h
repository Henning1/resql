/**
 * @file
 * Interface between SQL tokenizer and parser. Defines query datastructure.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#include <stdio.h>
#include <iostream>
#include <set>
#include "common.h"
#include "expressions.h"


class RelOperator;


extern char* yytext;
extern FILE *yyin;
typedef struct yy_buffer_state *YY_BUFFER_STATE;


extern "C" int             yylex( void );
extern "C" YY_BUFFER_STATE yy_scan_string( const char * );
extern "C" void            yy_delete_buffer( YY_BUFFER_STATE );


struct PlanOperator {
    RelOperator* op;
    Schema       schema;
};


struct Query {

    enum Tag {
        UNKNOWN,
        CONTROL,
        SELECT,
        CREATE_TABLE,
        BULK_INSERT
    };
    Tag tag;

    /* Select statements */
    Expr* selectExpr;
    Expr* fromExpr;
    Expr* whereExpr;
    Expr* groupbyExpr;
    Expr* orderbyExpr;

    /* Table name for create table and import */
    std::string tableName;

    /* Create table statement */
    Expr* schemaExpr;

    /* Bulk insert statement */
    std::string fileName;
    std::string fieldTerminator;
    size_t      firstRow;

    /* Parsing and plan building status */
    bool parseError;
    bool planError;

    /*** From here: query processing ***/
    RelOperator* plan;

    /* Indicates which table is contained in which plan piece */
    std::map < std::string, PlanOperator > planTables;
    
    /* Query plan pieces */
    std::set < RelOperator* > planPieces;

    /* Indicates whether all symbols are requested *
     * to the root i.e. for 'select *'             */
    bool requestAll;    
    
    /* Limit clauses in query */
    bool useLimit;
    size_t limit;

};


#include "parser.h"
#include "parser.c"


Expr* initializeIfConstant ( int tokenType, std::string symbol ) {

    switch ( tokenType ) {

        case DECIMAL_CONSTANT:
            return ExprGen::constant ( symbol, SqlType::DECIMAL ); 
        
        case FLOAT_CONSTANT:
            return ExprGen::constant ( symbol, SqlType::FLOAT ); 

        case INTEGER_CONSTANT:
            return ExprGen::constant ( symbol, SqlType::BIGINT ); 

        case STRING_CONSTANT: {
            /* remove quotes */
            std::string str =  symbol.substr ( 1, symbol.length() - 2 );
             
            /* Handle single character strings */
            if ( str.length() == 1 ) {
                return ExprGen::constant ( str, SqlType::CHAR ); 
            }
            
            /* Handle multi-character strings as varchar. *
             * Varchar is more general, because it is     *
             * terminated by \0. Therefore Varchar can    *
             * be easily converted to Nchar.              */
            else {
                return ExprGen::constant ( str, SqlType::VARCHAR ); 
            }
        }

        default:
            // pass symbol into grammar rule
            return ExprGen::undef ( symbol ); 
    }
}


Query parseSql ( std::string sql ) {

    Query query = { 
        Query::UNKNOWN, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        nullptr, 
        "", 
        nullptr, 
        "",
        ",",
        0,
        false, 
        false, 
        nullptr, 
        {}, 
        {},
        false,
        false,
        0
    };

    void *parser = ParseAlloc ( malloc );
    int tokenType;
    yy_scan_string ( sql.c_str() );

    while ( ( tokenType = yylex ( ) ) != 0 ) { 
        if ( tokenType == ERROR ) {
            query.parseError = true;
            break;
        } 
        Expr* node = initializeIfConstant ( tokenType, yytext );
        Parse ( parser, tokenType, node, &query ); 
    }
    Parse ( parser, 0, 0, &query );
    ParseFree ( parser, free );
    return query;
}

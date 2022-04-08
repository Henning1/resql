#include "expressions.h"
#include "dbdata.h"
#include "schema.h"
#include "qlib/qlib.h"
#include "ExpressionsJitFlounder.h"
#include "ValuesJitFlounder.h"

#include "test_common.h"


using namespace ExprGen;



void testJitConstants () {

    Expr* expr0 = constant ( "2021/01/18", SqlType::DATE );
    executeAndCheckExpression ( "A", expr0, "2021/01/18" );
    
    Expr* expr1 = constant ( "true", SqlType::BOOL );
    executeAndCheckExpression ( "B", expr1, "true" );

    Expr* expr2 = constant ( "1515.1414", SqlType::DECIMAL );
    executeAndCheckExpression ( "C", expr2, "1515.1414" );

}

void testJitArithmetic () {

    Expr* expr0 = add (
        constant (  "11.111", SqlType::DECIMAL ),
        constant ( "321.12" , SqlType::DECIMAL )
    );
    executeAndCheckExpression ( "D", expr0, "332.231" );
    
    Expr* expr1 = lt (
        constant (  "11.111", SqlType::DECIMAL ),
        constant ( "321.12" , SqlType::DECIMAL )
    );
    executeAndCheckExpression ( "E", expr1, "true" );
    
    Expr* expr2 = lt (
        constant ( "11.111", SqlType::DECIMAL ),
        constant ( "11.111" , SqlType::DECIMAL )
    );
    executeAndCheckExpression ( "F", expr2, "false" );
    
    Expr* expr3 =
        eq (
            constant ( "1.111", SqlType::DECIMAL ),
            constant ( "111.1" , SqlType::DECIMAL )
        );
    executeAndCheckExpression ( "G1", expr3, "false" );
    
    Expr* expr4 =
        gt (
            constant ( "12.3", SqlType::DECIMAL ),
            constant ( "13" , SqlType::BIGINT )
        );
    executeAndCheckExpression ( "G2", expr4, "false" );
    
    Expr* expr5 =
        lt (
            constant ( "12.3", SqlType::DECIMAL ),
            constant ( "13" , SqlType::BIGINT )
        );
    executeAndCheckExpression ( "G3", expr5, "true" );
}


void testJitArithmeticCombined () {
    
    Expr* expr0 = 
        lt ( 
            // 30.0267
            mul (
                constant ( "90.99", SqlType::DECIMAL ),
                constant (  "0.33", SqlType::DECIMAL )
            ),
            // 30.5
            mul (
                add (
                    constant ( "120", SqlType::BIGINT ),
                    constant ( "285", SqlType::BIGINT )
                ),
                constant ( "0.1" , SqlType::DECIMAL )
            )
        );
    executeAndCheckExpression ( "H1", expr0, "true" );
    
    Expr* expr1 = 
        gt ( 
            // 30.0267
            mul (
                constant ( "90.99", SqlType::DECIMAL ),
                constant (  "0.33", SqlType::DECIMAL )
            ),
            // 30.5
            mul (
                add (
                    constant ( "120", SqlType::BIGINT ),
                    constant ( "285", SqlType::BIGINT )
                ),
                constant ( "0.1" , SqlType::DECIMAL )
            )
        );
    executeAndCheckExpression ( "H2", expr1, "false" );
}



void testExpressions() {
    testJitConstants ();
    testJitArithmetic ();
    testJitArithmeticCombined ();
}


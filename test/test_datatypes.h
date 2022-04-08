
#include "expressions.h"
#include "schema.h"
#include "qlib/qlib.h"

#include "test_common.h"


using namespace ExprGen;


void testDatatypes () {
   
    Expr* e1 = constant ( "100.10", SqlType::DECIMAL );
    checkSerialized ( "A1", serializeType ( e1->type ),                "DECIMAL(5,2)" );
    checkSerialized ( "A2", serializeSqlValue ( e1->value, e1->type ), "100.10" );
    
    Expr* e2 = constant ( "12.6719274", SqlType::DECIMAL );
    checkSerialized ( "B1", serializeType ( e2->type ),                "DECIMAL(9,7)" );
    checkSerialized ( "B2", serializeSqlValue ( e2->value, e2->type ), "12.6719274" );

    Expr* expr = mul ( e1, e2 );
    checkSerialized ( "C", serializeExpr ( expr ), 
                      "{MUL,,"
                          "{CONSTANT,DECIMAL(5,2),100.10},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );

    deriveExpressionTypes ( expr );
    checkSerialized ( "D", serializeExpr ( expr ), 
                      "{MUL,DECIMAL(14,9),"
                          "{CONSTANT,DECIMAL(5,2),100.10},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );
    
    Expr* expr2 = add ( e1, e2 );
    checkSerialized ( "E", serializeExpr ( expr2 ), 
                      "{ADD,,"
                          "{CONSTANT,DECIMAL(5,2),100.10},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );
    
    deriveExpressionTypes ( expr2 );
    checkSerialized ( "F", serializeExpr ( expr2 ), 
                      "{ADD,DECIMAL(11,7),"
                          "{TYPECAST,DECIMAL(10,7),"
                              "{CONSTANT,DECIMAL(5,2),100.10}"
                          "},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );
    

    Expr* expr21 = lt ( e1, e2 );
    checkSerialized ( "E2", serializeExpr ( expr21 ), 
                      "{LT,,"
                          "{CONSTANT,DECIMAL(5,2),100.10},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );
    
    deriveExpressionTypes ( expr21 );
    checkSerialized ( "F2", serializeExpr ( expr21 ), 
                      "{LT,BOOL,"
                          "{TYPECAST,DECIMAL(10,7),"
                              "{CONSTANT,DECIMAL(5,2),100.10}"
                          "},"
                          "{CONSTANT,DECIMAL(9,7),12.6719274}"
                      "}" );

    Expr* expr3 = add ( e2, e1 );
    deriveExpressionTypes ( expr3 );
    checkSerialized ( "G", serializeExpr ( expr3 ), 
                      "{ADD,DECIMAL(11,7),"
                          "{CONSTANT,DECIMAL(9,7),12.6719274},"
                          "{TYPECAST,DECIMAL(10,7),"
                              "{CONSTANT,DECIMAL(5,2),100.10}"
                          "}"
                      "}" 
                    );
    

    Expr* e3 = constant ( "123", SqlType::BIGINT );
    Expr* expr4 = mul ( add ( e2, e3 ), e1 );
    deriveExpressionTypes ( expr4 );

  
    checkSerialized ( "H", serializeExpr ( expr4 ), 
                        "{MUL,DECIMAL(19,9),"
                            "{ADD,DECIMAL(19,7),"
                                "{CONSTANT,DECIMAL(9,7),12.6719274},"
                                "{TYPECAST,DECIMAL(19,7),"
                                    "{CONSTANT,BIGINT,123}"
                                "}"
                            "},"
                            "{CONSTANT,DECIMAL(5,2),100.10}"
                        "}"
                    );
}


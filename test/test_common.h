#pragma once

#include "operators/JitOperators.h"
#include "expressions.h"
#include "dbdata.h"
#include "schema.h"
#include "execute.h"
#include "values.h"


DBConfig testConfig;


void fail_test () {
    std::cerr << " - Test failed" << std::endl;
    exit(1);
}


void checkSerialized ( std::string name, std::string v, std::string expected ) {
    std::cout << "Test " << name << ": " << v << " should be " << expected << " ";
    if ( v.compare ( expected ) != 0 ) {
        fail_test();
    }
    std::cout << " OK" << std::endl;
}


/** Helper function for writing test cases *
  * a bit unsafe to use for other purposes *
  * because the type is not returned.      *
  * Be sure that the result value has the  *
  * intended type spec.                    *
  *                                        *
  * For strings that initialize DECIMAL    *
  * values, the number of digits after '.' *
  * has to be the same as the scale!       */
SqlValue valInit ( std::string value, SqlType::Tag typeCategory ) {
    Expr* e = ExprGen::constant ( value, typeCategory ); 
    parseConstant ( e, e->type.tag );
    SqlValue res = e->value;
    freeExpr ( e );
    return res;
}


/** Helper function for writing test cases *
  * a bit unsafe for other purposes        *
  * (see valInit)                          */
Relation relationFromStrings ( Schema schema, 
                               std::vector < std::vector < std::string > > data ) {

    Relation rel = Relation ( schema );
    std::vector<AttributeIterator> atts = AttributeIterator::getAll ( rel._schema );
    Relation::AppendIterator appendIt ( &rel );    
    for ( size_t i=0; i<data.size(); i++) {
        Data* t = appendIt.get();
        for ( uint32_t j=0; j<atts.size(); j++ ) {
            SqlValue val = valInit ( data[i][j], atts[j].attribute.type.tag );
            ValueMoves::toAddress ( atts[j].getPtr ( t ), val, atts[j].attribute.type );
        }
    }
    return rel;
}


SqlValue compileAndEvaluateScalarExpression ( Expr* expr ) {
    Schema schema = Schema ( { { "ExprResult", expr->type } } );
    Relation rel ( schema );
    Relation::AppendIterator appendIt ( &rel );
    JitContextFlounder ctx ( testConfig.jit ); 
    ir_node* exprResult = emitExpression ( ctx, expr );
    Values::storeToMem ( expr->type, 
                         exprResult, 
                         constLoad ( constAddress ( appendIt.get() ) ), 
                         0, 
                         Values::relationMatConfig, 
                         ctx );
    ctx.clear ( exprResult );
    ctx.compile();
    ctx.execute();
    Relation::ReadIterator readIt ( &rel );
    return ValueMoves::fromAddress ( expr->type, readIt.get() ); 
}


void executeAndCheckExpression ( std::string name, Expr* expr, std::string reference ) {
    deriveExpressionTypes ( expr );
    SqlValue res = compileAndEvaluateScalarExpression ( expr );
    checkSerialized ( name, serializeSqlValue ( res, expr->type ), reference );
}




bool compareTuples ( Data* tA, Data* tB, std::vector < AttributeIterator > atts ) {
    for ( size_t j=0; j < atts.size(); j++ ) {
        std::string attA = atts[j].serialize ( tA ); 
        std::string attB = atts[j].serialize ( tB );
        
        if ( attB.compare ( attA ) != 0 ) {
            return false;
        }
    }
    return true;
}


bool compareRelationShape ( Relation& a, Relation& b ) {

    if ( !a._schema.compare ( b._schema, false ) ) {
        std::cout << "schema diff" << std::endl;
        return false;
    }

    if ( a.tupleNum() != b.tupleNum() ) {
        std::cout << "len diff" << std::endl;
        return false;
    }

    return true;
}


bool compareRelationsInOrder ( Relation& a, Relation& b ) {

    if ( ! compareRelationShape ( a, b ) ) {
        return false;
    }
    
    std::vector < AttributeIterator > atts = AttributeIterator::getAll ( a._schema ); 

    Relation::ReadIterator readItA ( &a );
    Relation::ReadIterator readItB ( &b );
    int i=0;
    Data* tupleA = readItA.get();
    Data* tupleB = readItB.get();
    while ( tupleA != nullptr && tupleB != nullptr ) {
        if ( ! compareTuples ( tupleA, tupleB, atts ) ) {
            std::cout << "tuple diff in tuple " << i << std::endl;
            return false;
        }
        tupleA = readItA.get();
        tupleB = readItB.get();
        i++;
    }
    return true;
} 


// todo: replace with sort-based comparison
bool compareRelations ( Relation& a, Relation& b ) {
    
    if ( ! compareRelationShape ( a, b ) ) {
        return false;
    }

    std::cout << "!compare any order" << std::endl;
    
    std::vector < AttributeIterator > atts = AttributeIterator::getAll ( a._schema ); 

    Relation::ReadIterator readItA ( &a );
    Data* tupleA = readItA.get();
    while ( tupleA != nullptr ) {
        size_t countA = 0, countB = 0;
        Relation::ReadIterator readItA2 ( &a );
        Data* tupleA2 = readItA2.get();
        while ( tupleA2 != nullptr ) {
            if ( compareTuples ( tupleA, tupleA2, atts ) ) {
                countA++;
            }
            tupleA2 = readItA2.get();
        }
        
        Relation::ReadIterator readItB ( &b );
        Data* tupleB = readItB.get();
        while ( tupleB != nullptr ) {
            if ( compareTuples ( tupleA, tupleB, atts ) ) {
                countB++;
            }
            tupleB = readItB.get();
        }
        if ( countA != countB ) {
            std::cout << "tuple diff" << std::endl;
            return false;
        }
        tupleA = readItA.get();
    }
    return true;
} 


void checkRelations ( std::string   name, 
                      Relation&     rel,
                      Relation&     reference,
                      bool          inOrder ) {
    
    std::cout << std::endl << "-------- Test " << name << " --------" << std::endl;
    std::cout << std::endl << "Result" << std::endl;
    printRelation ( std::cout, rel );
    std::cout << "Should match";
    if ( !inOrder ) std::cout << " (in any order)";
    std::cout << std::endl;
    printRelation ( std::cout, reference );

    bool pass;
    if ( inOrder )
        pass = compareRelationsInOrder ( rel, reference );
    else
        pass = compareRelations ( rel, reference );
        
    if ( pass ) {
       std::cout << " OK" << std::endl; 
    }
    else {
        std::cout << name << std::endl;
        fail_test();
    }
}


void executeSelectAndCheckRelation ( std::string   name, 
                                     RelOperator*  root, 
                                     Database&     db,
                                     Relation&     reference, 
                                     bool          inOrder=false ) {

    std::unique_ptr < SelectResult > qres = executeSelectPlan ( root, true, db, testConfig );
    checkRelations ( name, *qres->relation, reference, inOrder );   
}




#include "operators/JitOperators.h"
#include "expressions.h"
#include "dbdata.h"
#include "schema.h"
#include "JitContextFlounder.h"


#include "test_common.h"


using namespace ExprGen;


void testScan () {
    Database db;
    db["rel"] = genDataTypeMix ( 26 );
    RelOperator* root = new MaterializeOp ( new ScanOp ( &db["rel"] ) );
    executeSelectAndCheckRelation ( "SCAN", root, db, db["rel"] );
}


void testSelectionDecimal () {

    Schema schema = Schema ( { 
        { "quantity",   TypeInit::DECIMAL(5,1) }, 
        { "date",       TypeInit::DATE()       }
    } );

    std::vector < std::vector < std::string > > relData = { 
        {  "999.9", "2011/01/11" },
        { "1000.0", "2011/01/11" },
        { "1000.1", "2011/01/11" },
        {    "1.9", "2011/01/11" },
        {  "123.4", "2011/01/11" },
        { "1234.5", "2011/01/11" },
        { "1000.0", "2011/01/11" },
        { "9999.9", "2011/01/11" } 
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    std::vector < std::vector < std::string > > referenceData = { 
        { "1000.1", "2011/01/11" },
        {    "1.9", "2011/01/11" },
        { "1234.5", "2011/01/11" },
        { "9999.9", "2011/01/11" } 
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new SelectionOp ( 
               /* scalar */  
               or_ (
                   lt (
                       attr ( "quantity" ),
                       constant ( "10.0", SqlType::DECIMAL )
                   ),
                   gt (
                       attr ( "quantity" ),
                       constant ( "1000.0", SqlType::DECIMAL )
                   )
               ),
               /* scalar end */
               new ScanOp ( &db["rel"] )
           )
       );
    

    executeSelectAndCheckRelation ( "SELECTION_DECIMAL", root, db, reference );
}


void testSelectionDecimal2 () {
    
    Schema schema = Schema ( { 
        { "rateA",   TypeInit::DECIMAL(4,3) }, 
        { "rateB",   TypeInit::DECIMAL(2,1) }, 
    } );

    std::vector < std::vector < std::string > > relData = { 
        {  "0.123", "0.1" },
        {  "0.223", "0.2" },
        {  "0.123", "0.3" },
        {  "0.223", "0.1" },
        {  "0.123", "0.2" },
        {  "0.223", "0.3" },
        {  "0.123", "0.1" },
        {  "0.123", "0.2" },
        {  "0.123", "0.3" }
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    std::vector < std::vector < std::string > > referenceData = { 
        {  "0.123", "0.3" },
        {  "0.123", "0.2" },
        {  "0.223", "0.3" },
        {  "0.123", "0.2" },
        {  "0.123", "0.3" }
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new SelectionOp ( 
               /* scalar */  
               lt (
                   attr ( "rateA" ),
                   attr ( "rateB" )
               ),
               /* scalar end */
               new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "SELECTION_DECIMAL", root, db, reference );
}


void testSelectionDate () {
    
    Schema schema = Schema ( { 
        { "quantity",   TypeInit::DECIMAL(5,1) }, 
        { "date",       TypeInit::DATE()       }
    } );

    std::vector < std::vector < std::string > > relData = { 
        {  "999.9", "1988/10/24" },
        { "1000.0", "1988/10/25" },
        { "1000.1", "1988/10/26" },
        {    "1.9", "2011/01/11" },
        {  "123.4", "1966/01/11" },
        {  "123.4", "1966/06/14" },
        {  "123.4", "1966/06/15" },
        {  "123.4", "1966/05/15" },
        {  "123.4", "1966/07/15" },
        {  "123.4", "1966/06/16" },
        { "1234.5", "1997/01/12" },
        { "1000.0", "2006/02/30" },
        { "9999.9", "1973/08/09" } 
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    std::vector < std::vector < std::string > > referenceData = { 
        {  "999.9", "1988/10/24" },
        { "1000.0", "1988/10/25" },
        {  "123.4", "1966/06/15" },
        {  "123.4", "1966/07/15" },
        {  "123.4", "1966/06/16" },
        { "9999.9", "1973/08/09" } 
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new SelectionOp ( 
               /* scalar */  
               and_ (
                   ge (
                       attr ( "date" ),
                       constant ( "1966/06/15", SqlType::DATE )
                   ),
                   le (
                       attr ( "date" ),
                       constant ( "1988/10/25", SqlType::DATE )
                   )
               ),
               /* scalar end */
               new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "SELECTION_DATE", root, db, reference );
}



void testSelectionCombined () {
    
    Schema schema = Schema ( { 
        { "ratio",      TypeInit::DECIMAL(5,3) }, 
        { "date",       TypeInit::DATE()       },
        { "quantity",   TypeInit::BIGINT()     }
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "0.123", "1999/01/12", "250" },
        { "0.123", "2000/01/12", "100" },
        { "0.123", "2000/01/12", "250" },
        { "0.250", "2000/01/12", "100" },
        { "0.250", "1999/01/12", "250" },
        { "0.250", "2000/01/12", "100" },
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    std::vector < std::vector < std::string > > referenceData = { 
        { "0.123", "1999/01/12", "250" },
        { "0.123", "2000/01/12", "100" }
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new SelectionOp ( 
               /* scalar */  
               and_ (
                   lt (
                       attr ( "ratio" ),
                       constant ( "0.222", SqlType::DECIMAL )
                   ),
                   or_ (
                       lt (
                           attr ( "date" ),
                           constant ( "2000/01/01", SqlType::DATE )
                       ),
                       lt (
                           attr ( "quantity" ),
                           constant ( "120", SqlType::BIGINT )
                       )
                   )
               ),
               /* scalar end */
               new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "SELECTION_COMBINED", root, db, reference );
}


struct JoinTestData {
    Database db;
    Relation reference;
};


JoinTestData getJoinData () {
    JoinTestData res;
    Schema schemaS = Schema ( { 
        { "attributeC", TypeInit::BIGINT() },
        { "attributeD", TypeInit::BIGINT() }
    } );
    Schema schemaR = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() }
    } );
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() },
        { "attributeC", TypeInit::BIGINT() },
        { "attributeD", TypeInit::BIGINT() }
    } );

    std::vector < std::vector < std::string > > relDataR = { 
        { "1", "100" },
        { "2", "200" },
        { "2", "100" },
        { "3", "200" },
        { "4", "100" },
        { "5", "200" },
    };
    res.db["R"] = relationFromStrings ( schemaR, relDataR );

    std::vector < std::vector < std::string > > relDataS = { 
        { "1", "300" },
        { "2", "400" },
        { "3", "300" },
        { "1", "400" },
        { "2", "300" },
        { "3", "400" }
    };
    res.db["S"] = relationFromStrings ( schemaS, relDataS );
    std::vector < std::vector < std::string > > referenceData = { 
        { "1", "100", "1", "300" },
        { "1", "100", "1", "400" },
        { "2", "200", "2", "400" },
        { "2", "200", "2", "300" },
        { "2", "100", "2", "400" },
        { "2", "100", "2", "300" },
        { "3", "200", "3", "300" },
        { "3", "200", "3", "400" }
    };
    res.reference = relationFromStrings ( schema, referenceData );
    return res;
}




void testNestedLoopsJoin () {

    JoinTestData testData = getJoinData ();

    RelOperator* root = 
        new MaterializeOp (
            new NestedLoopsJoinOp ( 
                /* scalar */  
                eq (
                    attr ( "attributeA" ),
                    attr ( "attributeC" )
                ),
                /* scalar end */
                new ScanOp ( &testData.db["R"] ),
                new ScanOp ( &testData.db["S"] )
            )
        );
    
    executeSelectAndCheckRelation ( "NESTEDLOOPSJOIN1", root, testData.db, testData.reference );
}


void testNestedLoopsJoin2 () {
    
    Schema schemaR = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
    } );
    Schema schemaS = Schema ( { 
        { "attributeB", TypeInit::BIGINT() },
    } );
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() },
    } );

    std::vector < std::vector < std::string > > relDataR = { 
        { "1" },
        { "2" },
        { "3" },
        { "4" },
        { "5" },
        { "6" }
    };
    std::vector < std::vector < std::string > > relDataS = { 
        { "1" },
        { "2" },
        { "3" },
        { "4" },
        { "5" },
        { "6" }
    };
    Database db;
    db["R"] = relationFromStrings ( schemaR, relDataR );
    db["S"] = relationFromStrings ( schemaS, relDataS );

    std::vector < std::vector < std::string > > referenceData = { 
        { "1", "2" },
        { "1", "3" },
        { "2", "3" },
        { "1", "4" },
        { "2", "4" },
        { "3", "4" },
        { "1", "5" },
        { "2", "5" },
        { "3", "5" },
        { "4", "5" },
        { "1", "6" },
        { "2", "6" },
        { "3", "6" },
        { "4", "6" },
        { "5", "6" }
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new NestedLoopsJoinOp ( 
                /* scalar */  
                lt (
                    attr ( "attributeA" ),
                    attr ( "attributeB" )
                ),
                /* scalar end */
                new ScanOp ( &db["R"] ),
                new ScanOp ( &db["S"] )
            )
        );
    
    executeSelectAndCheckRelation ( "NESTEDLOOPSJOIN2", root, db, reference );
}


void testNestedLoopsJoin3 () {
    
    Schema schemaR = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
    } );
    Schema schemaS = Schema ( { 
        { "attributeB", TypeInit::BIGINT() },
    } );
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() },
    } );

    std::vector < std::vector < std::string > > relDataR = { 
        { "1" },
        { "2" },
        { "3" },
    };
    std::vector < std::vector < std::string > > relDataS = { 
        { "1" },
        { "2" },
        { "3" },
    };
    Database db;
    db["R"] = relationFromStrings ( schemaR, relDataR );
    db["S"] = relationFromStrings ( schemaS, relDataS );

    std::vector < std::vector < std::string > > referenceData = { 
        { "1", "1" },
        { "1", "2" },
        { "1", "3" },
        { "2", "1" },
        { "2", "2" },
        { "2", "3" },
        { "3", "1" },
        { "3", "2" },
        { "3", "3" }
    };
    Relation reference = relationFromStrings ( schema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new NestedLoopsJoinOp ( 
                /* scalar */  
                nullptr,
                /* scalar end */
                new ScanOp ( &db["R"] ),
                new ScanOp ( &db["S"] )
            )
        );

    std::cout << "run" << std::endl;
    
    executeSelectAndCheckRelation ( "NESTEDLOOPSJOIN3", root, db, reference );
}


void testHashJoin () {
    
    JoinTestData testData = getJoinData ();

    RelOperator* root = 
        new MaterializeOp (
            new HashJoinOp ( 
                /* vector of scalar */  
                {
                    eq (
                        attr ( "attributeA" ),
                        attr ( "attributeC" )
                    )
                },
                /* end */
                new ScanOp ( &testData.db["R"] ),
                new ScanOp ( &testData.db["S"] )
            )
        );
    
    executeSelectAndCheckRelation ( "HASHJOINOP", root, testData.db, testData.reference );
}


void testHashJoin2 () {
   
    Database db; 
    db["R"] = genDataTypeMix ( 1000, "r_" );
    db["S"] = genDataTypeMix ( 1000, "s_" );

    RelOperator* rootNLJ = 
        new MaterializeOp (
            new NestedLoopsJoinOp ( 
                /* scalar */  
                eq (
                    attr ( "r_salesvalue" ),
                    attr ( "s_salesvalue" )
                ),
                /* end */
                new ScanOp ( &db["R"] ),
                new ScanOp ( &db["S"] )
            )
        );
    
    QueryResult reference = executeSelectPlan ( rootNLJ, true, db );

    std::cout << "REFERENCE NUM TUPLES: " << reference.selectResult()->relation->tupleNum() << std::endl;

    RelOperator* rootHJ = 
        new MaterializeOp (
            new HashJoinOp ( 
                /* vector of scalar */  
                {
                    eq (
                        attr ( "r_salesvalue" ),
                        attr ( "s_salesvalue" )
                    )
                },
                /* end */
                new ScanOp ( &db["R"] ),
                new ScanOp ( &db["S"] )
            )
        );
    
    executeSelectAndCheckRelation ( "HASHJOINOP2", rootHJ, db, *reference.selectResult()->relation );
}


void testAggregation () {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "2", "1" },
        { "3", "1" },
        { "3", "2" },
        { "4", "1" },
        { "4", "2" },
        { "4", "3" },
        { "5", "1" },
        { "5", "2" },
        { "5", "3" },
        { "5", "4" },
        { "6", "1" },
        { "6", "2" },
        { "6", "3" },
        { "6", "4" },
        { "6", "5" }
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "attributeA",      TypeInit::BIGINT() },
        { "sum(attributeB)", TypeInit::BIGINT() },
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        { "2",  "1" },
        { "3",  "3" },
        { "4",  "6" },
        { "5", "10" },
        { "6", "15" }
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new AggregationOp ( 
                /* scalar aggregation and grouping expression */  
                {
                    sum (
                        attr ( "attributeB" )
                    )
                },
                {
                    attr ( "attributeA" )
                },
                /* scalar end */
                new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "AGGREGATION", root, db, reference );
}


void testAggregation2 () {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::DECIMAL(6,1) },
        { "attributeC", TypeInit::DECIMAL(6,2) },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "2", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "4", "1.1", "0.250" },
        { "4", "1.1", "0.250" },
        { "4", "1.2", "0.250" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.250" },
        { "5", "1.1", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" }
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "attributeA",        TypeInit::BIGINT()      },
        { "attributeB",        TypeInit::DECIMAL(6,1)  },
        { "sum(attributeC)",   TypeInit::DECIMAL(19,2) },
        { "count(attributeC)", TypeInit::BIGINT() }
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        { "2", "1.1", "0.123", "1" },
        { "3", "1.1", "0.246", "2" },
        { "4", "1.1", "0.500", "2" },
        { "4", "1.2", "0.250", "1" },
        { "5", "1.1", "0.869", "5" },
        { "6", "1.2", "1.250", "5" },
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new AggregationOp ( 
                /* scalar aggregation and grouping expression */  
                {
                    sum (
                        attr ( "attributeC" )
                    ),
                    count (
                        attr ( "attributeC" )
                    ),
                },
                {
                    attr ( "attributeA" ),
                    attr ( "attributeB" )
                },
                /* scalar end */
                new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "AGGREGATION2", root, db, reference );
}


void testAggregation3 () {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::DECIMAL(6,1) },
        { "attributeC", TypeInit::DECIMAL(6,2) },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "2", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "4", "1.1", "0.250" },
        { "4", "1.1", "0.250" },
        { "4", "1.2", "0.250" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.250" },
        { "5", "1.1", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" }
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "sum(attributeC)",   TypeInit::DECIMAL(19,2) },
        { "count(attributeC)", TypeInit::BIGINT() }
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        { "3.238", "16" }
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new AggregationOp ( 
                /* scalar aggregation and grouping expression */  
                {
                    sum (
                        attr ( "attributeC" )
                    ),
                    count (
                        attr ( "attributeC" )
                    ),
                },
                { }, // no groups
                /* scalar end */
                new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "AGGREGATION3", root, db, reference );
}


void testAggregation4 () {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::DECIMAL(6,1) },
        { "attributeC", TypeInit::DECIMAL(6,2) },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "2", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "3", "1.1", "0.123" },
        { "4", "1.1", "0.250" },
        { "4", "1.1", "0.250" },
        { "4", "1.2", "0.250" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.123" },
        { "5", "1.1", "0.250" },
        { "5", "1.1", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" },
        { "6", "1.2", "0.250" }
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "attributeA",        TypeInit::BIGINT()      },
        { "attributeB",        TypeInit::DECIMAL(6,1)  }
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        { "2", "1.1" },
        { "3", "1.1" },
        { "4", "1.1" },
        { "4", "1.2" },
        { "5", "1.1" },
        { "6", "1.2" }
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new AggregationOp ( 
                /* scalar aggregation and grouping expression */  
                {}, // no aggregates
                {
                    attr ( "attributeA" ),
                    attr ( "attributeB" )
                },
                /* scalar end */
                new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "AGGREGATION4", root, db, reference );
}


void testAggregation5 () {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
        { "attributeB", TypeInit::BIGINT() },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "2", "1" }, // 3
        { "3", "1" }, // 4
        { "3", "2" }, // 5
        { "4", "1" }, // 5
        { "4", "2" }, // 6
        { "4", "3" }, // 7
        { "5", "1" }, // 6
        { "5", "2" }, // 7
        { "5", "3" }, // 8
        { "5", "4" }, // 9
        { "6", "1" }, // 7
        { "6", "2" }, // 8
        { "6", "3" }, // 9
        { "6", "4" }, //10
        { "6", "5" }  //11    
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "attributeA",      TypeInit::BIGINT() },
        { "sum(attributeB)", TypeInit::BIGINT() },
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        {  "3",  "3" },
        {  "4",  "4" },
        {  "5", "10" },
        {  "6", "12" },
        {  "7", "21" },
        {  "8", "16" },
        {  "9", "18" },
        { "10", "10" },
        { "11", "11" },
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new MaterializeOp (
            new AggregationOp ( 
                /* scalar aggregation and grouping expression */  
                {
                    sum (
                        add (
                            attr ( "attributeA" ),
                            attr ( "attributeB" )
                        )
                    )
                },
                {
                    add (
                        attr ( "attributeA" ),
                        attr ( "attributeB" )
                    )
                },
                /* scalar end */
                new ScanOp ( &db["rel"] )
            )
        );
    
    executeSelectAndCheckRelation ( "AGGREGATION5", root, db, reference );
}


void testOrderBy() {
    
    Schema schema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
    } );

    std::vector < std::vector < std::string > > relData = { 
        { "1" },
        { "1" },
        { "2" },
        { "1" },
        { "2" },
        { "3" },
        { "1" },
        { "2" },
        { "3" },
        { "4" },
        { "1" },
        { "2" },
        { "3" },
        { "4" },
        { "5" }     
    };
    Database db;
    db["rel"] = relationFromStrings ( schema, relData );

    Schema refSchema = Schema ( { 
        { "attributeA", TypeInit::BIGINT() },
    } );
    std::vector < std::vector < std::string > > referenceData = { 
        { "1" },
        { "1" },
        { "1" },
        { "1" },
        { "1" },
        { "2" },
        { "2" },
        { "2" },
        { "2" },
        { "3" },
        { "3" },
        { "3" },
        { "4" },
        { "4" },
        { "5" }     
    };
    Relation reference = relationFromStrings ( refSchema, referenceData );

    RelOperator* root = 
        new OrderByOp ( { attr ( "attributeA" ) },
            new ScanOp ( &db["rel"] )
        );
    
    executeSelectAndCheckRelation ( "ORDERBY", root, db, reference, true );
}


void testOperators() {
    testScan();
    testSelectionDecimal();  // lt or gt
    testSelectionDecimal2(); // lt (attr)
    testSelectionDate();     // le and ge
    testSelectionCombined(); // lt and ( lt or lt )
    testNestedLoopsJoin();
    testNestedLoopsJoin2();
    testNestedLoopsJoin3();
    testHashJoin();
    testHashJoin2();
    testAggregation();  // simple grouped aggregation
    testAggregation2(); // multiple group attributes
    testAggregation3(); // aggregation without groups
    testAggregation4(); // grouping without aggregation
    testAggregation5(); // calculated grouping and aggregation inputs
    testOrderBy();      // basic ordering of bigints
}


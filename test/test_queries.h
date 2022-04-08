#include "execute.h"
#include "planner.h"


void q1 ( Database& db ) {
    Schema schema = Schema ( { 
        { "l_returnflag",   TypeInit::CHAR(1) }, 
        { "l_linestatus",   TypeInit::CHAR(1) },
        { "sum_qty",        TypeInit::DECIMAL(19,0) },
        { "sum_base_price", TypeInit::DECIMAL(19,2) },
        { "sum_disc_price", TypeInit::DECIMAL(19,4) },
        { "sum_charge",     TypeInit::DECIMAL(19,6) },
        { "avg_qty",        TypeInit::DECIMAL(14,2) },
        { "avg_price",      TypeInit::DECIMAL(14,4) },
        { "avg_dic",        TypeInit::DECIMAL(14,4) },
        { "count_order",    TypeInit::BIGINT() }
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q1.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q1.sql", db, testConfig );
    checkRelations ( "TPC-H Q1", *res.selectResult()->relation, ref, true );
}


void q3 ( Database& db ) {
    Schema schema = Schema ( { 
        { "l_orderkey",     TypeInit::INT() }, 
        { "revenue",        TypeInit::DECIMAL(19,4) },
        { "o_orderdate",    TypeInit::DATE() },
        { "o_shippriority", TypeInit::INT() }
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q3.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q3.sql", db, testConfig );
    checkRelations ( "TPC-H Q3", *res.selectResult()->relation, ref, true );
}


void q5 ( Database& db ) {
    Schema schema = Schema ( { 
        { "n_name",   TypeInit::CHAR(25) }, 
        { "revenue",  TypeInit::DECIMAL(19,4) }
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q5.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q5.sql", db, testConfig );
    checkRelations ( "TPC-H Q5", *res.selectResult()->relation, ref, true );
}


void q6 ( Database& db ) {
    Schema schema = Schema ( { 
        { "revenue",  TypeInit::DECIMAL(19,4) }
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q6.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q6.sql", db, testConfig );
    checkRelations ( "TPC-H Q6", *res.selectResult()->relation, ref, true );
}


void q10 ( Database& db ) {
    Schema schema = Schema ( { 
        { "c_custkey", TypeInit::INT() },
        { "c_name",    TypeInit::VARCHAR(25) },
        { "revenue",   TypeInit::DECIMAL(19,4) },
        { "c_acctbal", TypeInit::DECIMAL(12,2) },
        { "n_name",    TypeInit::CHAR(25) },
        { "c_address", TypeInit::VARCHAR(40) },
        { "c_phone",   TypeInit::CHAR(15) },
        { "c_comment", TypeInit::VARCHAR(117) },
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q10.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q10.sql", db, testConfig );
    checkRelations ( "TPC-H Q10", *res.selectResult()->relation, ref, true );
}


void q12 ( Database& db ) {
    Schema schema = Schema ( { 
        { "l_shipmode",      TypeInit::CHAR(10) },
        { "high_line_count", TypeInit::BIGINT() },
        { "low_line_count",  TypeInit::BIGINT() },
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q12.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q12.sql", db, testConfig );
    checkRelations ( "TPC-H Q12", *res.selectResult()->relation, ref, true );
}


void q19 ( Database& db ) {
    Schema schema = Schema ( { 
        { "revenue", TypeInit::DECIMAL(19,4) },
    } );
    Relation ref = relationFromFile ( schema, "test/reference/q19.tbl", "|" );
    QueryResult res = executeFileAndGetLastResult ( "tpch/queries/q19.sql", db, testConfig );
    checkRelations ( "TPC-H Q19", *res.selectResult()->relation, ref, true );
}


void testQueries() {

    Database db;
    executeFileAndGetLastResult ( "tpch/create.sql", db, testConfig );
    executeFileAndGetLastResult ( "tpch/load_sf001.sql", db, testConfig );
    
     q1 ( db );
     q3 ( db );
     q5 ( db );
     q6 ( db );
    q10 ( db );
    q12 ( db );
    q19 ( db );
}



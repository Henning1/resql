/**
 * @file
 * Query execution interface.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <fstream>
#include "parser/parseSql.h"
#include "planner.h"

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>
#include "util/string.h"


struct DBConfig {
    JitConfig jit = JitConfig();
    bool showPlan = false;
    bool writeResultsToFile = false;
};


struct ControlResult {

    bool actionDone = false;
    std::string message = "";
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( actionDone, message );
    } 
};


struct SelectResult {

    /* constuctors */
    SelectResult() = default;

    SelectResult ( JitExecutionReport& rep, std::unique_ptr < Relation > rel, std::string plan ) 
        : jitReport ( rep ), relation ( std::move ( rel ) ), queryPlan ( plan ) {};

    /* attributes */
    JitExecutionReport            jitReport;
    std::unique_ptr < Relation >  relation;
    std::string                   queryPlan;
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( jitReport, relation, queryPlan );
    } 
};


struct CreateTableResult {
    
    /* constuctors */
    CreateTableResult() = default;   

    CreateTableResult ( std::string& tblName ) 
        : tableName ( tblName ) {};

    /* attributes */
    std::string tableName;
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( tableName );
    } 
    
};


struct BulkInsertResult {

    /* constuctors */
    BulkInsertResult() = default;
 
    BulkInsertResult ( uint64_t numInserts, double insertTimeMs ) 
        : numInserts ( numInserts ), insertTimeMs ( insertTimeMs ) {};

    /* attributes */
    uint64_t         numInserts;
    double           insertTimeMs;

    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( numInserts, insertTimeMs );
    } 
};


struct QueryResult {

    /* constuctors */
    QueryResult ( ) : tag ( Query::UNKNOWN ) {};
    
    QueryResult ( ControlResult ctr ) 
      : tag ( Query::CONTROL ), 
        content ( ctr ) {};

    QueryResult ( std::unique_ptr < SelectResult > sel ) 
      : tag ( Query::SELECT ), 
        content ( std::move ( sel ) ) {};
    
    QueryResult ( CreateTableResult cr ) 
      : tag ( Query::CREATE_TABLE ), 
        content ( cr ) {};
    
    QueryResult ( BulkInsertResult blk ) 
      : tag ( Query::BULK_INSERT ), 
        content ( blk ) {};
    
    QueryResult ( std::runtime_error e ) 
      : content(ControlResult()), error ( true ), errorMessage ( e.what() ) {};
    
    QueryResult ( ResqlError e ) 
      : content(ControlResult()), error ( true ), errorMessage ( e.message() ) {};
    
    /* attributes */
    Query::Tag tag;

    std::variant <
        std::monostate,
        ControlResult,
        std::unique_ptr < SelectResult >,
        CreateTableResult,
        BulkInsertResult
    > content;
    
    std::string controlMessage () { 
        return std::get<ControlResult> ( content ).message;
    }

    SelectResult* selectResult () { 
        return (std::get<std::unique_ptr < SelectResult >> ( content )).get(); 
    }

    CreateTableResult& createTableResult () { 
        return std::get<CreateTableResult> ( content ); 
    }

    BulkInsertResult& bulkInsertResult () { 
        return std::get<BulkInsertResult> ( content ); 
    }

    bool error = false;
    std::string errorMessage;

    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( tag,
             content,
             error, 
             errorMessage 
        );
    } 
};


void printQueryResult ( QueryResult& res ) {
    if ( res.error ) {
        std::cout << "Query error: " << res.errorMessage << std::endl;
        return;
    }
    else if ( res.tag == Query::SELECT ) {
        SelectResult* sel = res.selectResult();
        std::cout << sel->queryPlan << std::endl;
        showReport ( sel->jitReport );
        printRelation ( std::cout, *sel->relation );
    }
    else if ( res.tag == Query::CREATE_TABLE ) {
        std::cout << "Created table " 
                  << res.createTableResult().tableName 
                  << std::endl;
    }
    else if ( res.tag == Query::BULK_INSERT ) {
        std::cout << "Inserted " 
                  << res.bulkInsertResult().numInserts
                  << " tuples" 
                  << std::endl;
    }
    else if ( res.tag == Query::CONTROL ) {
        std::cout << res.controlMessage();
    } else {
        std::cout << "Undefined query result." << std::endl;
    }
}


void writeRelationToFile ( Relation& rel, std::string filename ) {
    std::ofstream f;
    f.open ( filename, std::ifstream::out );
    if ( !f.is_open() ) {
        throw ResqlError ( "Could not open file " + filename );
    }
    serializeRelation ( rel, f );
}


std::unique_ptr < SelectResult >  executeSelectPlan ( RelOperator*  root, 
                                                      bool          requestAll,
                                                      Database&     db,
                                                      DBConfig      config=DBConfig() ) {
    std::stringstream plan;
    if ( config.showPlan ) {
        root->print ( plan );
    }

    ExpressionContext exprCtx;
    root->defineExpressionsForPlan ( exprCtx );
    std::map <std::string, SqlType> identifiers = mapIdentifierTypes ( db );
    exprCtx.deriveExpressionTypes ( identifiers );
    JitContextFlounder ctx ( config.jit );
    ctx.requestAll = requestAll;
    try {
        root->produceFlounder ( ctx, {} );
        ctx.compile();
    }
    catch ( ResqlError& err ) {
        std::cerr << err.message();
        root->deletePlan();
        throw;
    }
    ctx.execute();
    std::unique_ptr < Relation > rel = root->retrieveResult();
    root->deletePlan();
    if ( config.writeResultsToFile ) {
        writeRelationToFile ( *rel, "qres.tbl" ); 
    }
    return std::make_unique < SelectResult > ( ctx.report, std::move ( rel ), plan.str() );
}


std::unique_ptr < SelectResult > executeSelect ( Query&     query, 
                                                 Database&  db,
                                                 DBConfig&  config ) {
    buildQuery ( query, db );
    if ( query.plan != nullptr ) {
        return executeSelectPlan ( query.plan, query.requestAll, db, config );    
    }
    else {
        throw ResqlError ( "Could not generate query plan." );
    }
}


CreateTableResult executeCreateTable ( Query&     query, 
                                       Database&  db ) {

    if ( db.relations.count ( query.tableName ) ) {
        throw ResqlError ( "Table " + query.tableName + " already exists." );
    }
    if ( query.schemaExpr == nullptr ) {
        throw ResqlError ( "Create table needs at least one schema element." );
    }
    std::vector < Attribute > atts;
    Expr* expr = query.schemaExpr;
    while ( expr ) {
        atts.push_back ( { expr->symbol, expr->type } );
        expr = expr->next; 
    }
    Schema s = Schema ( atts );
    db.relations.try_emplace ( query.tableName, s );
    return { query.tableName };
} 


Relation relationFromFile ( Schema& s, std::string filename, std::string terminator ) {

    Relation table ( s );
    
    /* Access and iterate file tokens */
    std::ifstream f ( filename );
    if ( !f.is_open() ) {
        throw ResqlError ( "Could not open file " + filename );
    }
    std::string line;
    auto appendIt = Relation::AppendIterator ( &table );
    auto atts = AttributeIterator::getAll ( s );

    while ( std::getline ( f, line) ) {
    
        Data* tupleAddr = appendIt.get();

        /* Iterate tokens in line and relation attributes */ 
        std::istringstream lineStream ( line );
        std::string token;
        size_t attIdx = 0;
        while ( std::getline ( lineStream, token, terminator[0] ) ) {

            if ( attIdx >= table._schema._nElems ) {
                throw ResqlError ( "Line " + std::to_string ( table.tupleNum() ) + " in " + 
                                           filename + "contains extra attributes." );
            }

            SqlType type = atts [ attIdx ].attribute.type;
            /* todo: insert typecast? */
            Expr* e = ExprGen::constant ( token, type.tag );

            ValueMoves::toAddress ( atts [ attIdx ].getPtr ( tupleAddr ), 
                                    e->value, 
                                    type );
            attIdx++;
        }

        if ( attIdx < atts.size() ) {
            throw ResqlError ( "Line " + std::to_string ( table.tupleNum() ) + " in " + 
                                       filename + " is missing attributes." );
        }
    }
    return table;
}



BulkInsertResult executeBulkInsert ( Query&     query, 
                                     Database&  db ) {

    if ( db.relations.count ( query.tableName ) == 0 ) {
        throw ResqlError ( "Table " + query.tableName + " does not exist." ); 
    }
    if ( query.fieldTerminator.length() > 1 ) { 
        throw ResqlError ( "Bulk insert only supports single-character field terminators." );
    }

    /* Access and iterate file tokens */
    std::ifstream f ( query.fileName );
    if ( !f.is_open() ) {
        throw ResqlError ( "Could not open file " + query.fileName );
    }
    char terminator = query.fieldTerminator[0]; 
    std::string line;

    /* Access and iterate table elements */
    Relation& table = db.relations [ query.tableName ];
    auto appendIt = Relation::AppendIterator ( &table );
    auto atts = AttributeIterator::getAll ( table._schema );
    size_t numInserts = 0;
        
    while ( std::getline ( f, line) ) {

        Data* tupleAddr = appendIt.get();

        /* Iterate tokens in line and relation attributes */ 
        std::istringstream lineStream ( line );
        std::string token;
        size_t attIdx = 0;
        while ( std::getline ( lineStream, token, terminator ) ) {

            if ( attIdx >= table._schema._nElems ) {
                throw ResqlError ( "Line " + std::to_string ( table.tupleNum() ) + " in " + 
                                           query.fileName + "contains extra attributes." );
            }

            SqlType type = atts [ attIdx ].attribute.type;
            /* todo: insert typecast? */
            Expr* e = ExprGen::constant ( token, type.tag );

            ValueMoves::toAddress ( atts [ attIdx ].getPtr ( tupleAddr ), 
                                    e->value, 
                                    type );
            attIdx++;
        }

        if ( attIdx < atts.size() ) {
            throw ResqlError ( "Line " + std::to_string ( table.tupleNum() ) + " in " + 
                                       query.fileName + " is missing attributes." );
        }
        numInserts++;
    }
    return { numInserts, 0.0 };
} 


void showTables ( Database& db, std::stringstream& out ) {

    std::vector < std::string > table;
    table.push_back ( "Table name" );
    table.push_back ( "Number of attributes" );
    table.push_back ( "Number of tuples" );
    for ( auto& elem : db.relations ) {
        table.push_back ( elem.first );
        table.push_back ( std::to_string ( elem.second._schema._attribs.size() ) );
        table.push_back ( std::to_string ( elem.second.tupleNum() ) );
    }
    std::string sub = std::to_string ( db.relations.size() ) + " tables";
    printStringTable ( out, table, 3, 1, sub );
}


void setBoolVar ( std::string         cmd, 
                  std::string         search, 
                  bool&               var,
                  bool&               doneAction,
                  std::stringstream&  out ) {

   cmd.erase(remove(cmd.begin(), cmd.end(), ' '), cmd.end());
   auto find = cmd.rfind ( search );
   if ( find == std::string::npos ) return;
   doneAction = true;
   if ( cmd.length() == search.length() ) {
       if ( var )  out << "true" << std::endl;
       if ( !var ) out << "false" << std::endl;
       return;
   } 
   if ( cmd.at ( search.length() ) != '=' ) {
       throw ResqlError ( "Expected varname=value" );
   }
   std::string val = cmd.substr ( search.length()+1 );
   if ( val.compare ( "true" )  == 0 )      var = true;
   else if ( val.compare ( "false" ) == 0 ) var = false;
   else throw ResqlError ( "Expected true or false" );
}


ControlResult processControl ( std::string  line, 
                               Database&    db,
                               DBConfig&    config ) {
    bool actionDone = false;
    std::stringstream out;
    setBoolVar ( line, "showplan", config.showPlan, actionDone, out );       
    setBoolVar ( line, "tofile",   config.writeResultsToFile, actionDone, out );       
    setBoolVar ( line, "parallel", config.jit.parallel, actionDone, out );       
    setBoolVar ( line, "showperf", config.jit.printPerformance, actionDone, out );       
    setBoolVar ( line, "showasm",  config.jit.printAssembly, actionDone, out );       
    setBoolVar ( line, "showfln",  config.jit.printFlounder, actionDone, out );       
    setBoolVar ( line, "optimize", config.jit.optimizeFlounder, actionDone, out );       
    setBoolVar ( line, "emitmc",   config.jit.emitMachineCode, actionDone, out );       
    
    if ( line.compare ( "tables" ) == 0 ) {
        showTables ( db, out );
        actionDone = true;
    }

    return { actionDone, out.str() };
}


std::vector < std::string > expandExecStatements ( std::string statement ) {
    rtrim ( statement );
    ltrim ( statement );
    std::vector < std::string > result;
    if ( statement.find ( "exec " ) == 0 ) {
        std::string filename = statement.substr ( 5 );
        ltrim ( filename );
        std::ifstream f;
        f.open ( filename, std::ifstream::in );
        if ( !f.is_open() ) {
            throw ResqlError ( "Could not open file " 
                + filename + " referenced in exec statement." );
        }
        /* iterate statements in file                            *
         * This part does not support ';' in string literals     */
        std::string subStatem;
        while ( std::getline ( f, subStatem, ';' ) ) {
            bool whiteSpace = std::all_of ( subStatem.begin(), 
                                            subStatem.end(), 
                                            isspace ); 
            if ( whiteSpace ) continue;
            auto expanded = expandExecStatements ( subStatem );
            result.insert ( result.end(), expanded.begin(), expanded.end() );
        }
    }
    else {
        result.push_back ( statement ); 
    }
    return result;
}


QueryResult executeStatement ( std::string  statement, 
                               Database&    db,
                               DBConfig&    config ) {
    try {
        /* handle control statements */
        ControlResult res = processControl ( statement, db, config );
        if ( res.actionDone ) {
            return res;
        }

        /* parse query statement */
        Query query = parseSql ( statement );
        if ( query.parseError ) {
            throw ResqlError ( "Syntax error." );    
        }

        /* execute statement */
        if ( query.tag == Query::SELECT ) {
            return executeSelect ( query, db, config );
        }
        else if ( query.tag == Query::CREATE_TABLE ) {
            return executeCreateTable ( query, db );
        }
        else if ( query.tag == Query::BULK_INSERT ) {
            return executeBulkInsert ( query, db );
        }
    }
    catch ( std::runtime_error& e ) {
        return QueryResult ( e );
    }
    catch ( ResqlError& e ) {
        return QueryResult ( e );
    }
    return ResqlError ( "unwanted fallthrough" );
}


QueryResult executeFileAndGetLastResult ( std::string  filename,
                                          Database&    db,
                                          DBConfig&    config ) {
    QueryResult res;
    auto statements = expandExecStatements ( "exec " + filename );
    for ( auto s : statements ) {
        res = executeStatement ( s, db, config );
    }
    return res;
}


void executeFileAndPrintResults ( std::string  filename,
                                  Database&    db,
                                  DBConfig&    config ) {
    QueryResult res;
    auto statements = expandExecStatements ( "exec " + filename );
    for ( auto s : statements ) {
        res = executeStatement ( s, db, config );
        printQueryResult ( res );
    }
}




/**
 * @file
 * Interface for the Resql database system. * 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */

#include <map>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <readline/readline.h>
#include <readline/history.h>
#include <cxxopts/cxxopts.hpp>
#include <chrono>
#include <thread>
#include "operators/JitOperators.h"
#include "execute.h"
#include "network/server.h"
#include "network/client.h"
#include "network/handler.h"


size_t DataBlock::Size = 2 << 20;


enum ExecMode {
    SERVER,
    CLIENT,
    INTERACTIVE
};


bool checkExit ( std::string command ) {
    if ( command.compare ( "exit" ) == 0 ) return true;
    if ( command.compare ( "q" ) == 0 ) return true;
    return false;
}


void runStartup ( std::string       filename,
                  Database&         db, 
                  DBConfig&  config ) {

    std::ifstream ifile;
    ifile.open ( filename );
    if(ifile) {
        executeFileAndPrintResults ( filename, db, config );
    }
}

    
int runServer ( Database&         db, 
                DBConfig&  config, 
                std::uint16_t     port ) {

    runStartup ( "startup.sql", db, config );
    ResqlHandler handler ( db, config );
    network::Server server ( port, handler );
    server.listen();
    return 0;
}


void runClient ( std::string    host,
                 std::uint16_t  port ) {

    network::Client client ( host, port );
    if ( !client.connect() ) {
        std::cout << "Could not connect to " << ":" << std::to_string ( port ) << std::endl;
        return;
    }
    while ( true ) {
        char* line = readline ( ">" );
        if ( !line ) break;
        if ( *line ) add_history ( line );
        std::string inputLine = line;
        if ( inputLine.size() == 0 ) continue;
        if ( checkExit ( line ) ) return;
        try {
            auto statements = expandExecStatements ( inputLine );
            for ( auto s : statements ) {
                s = remove_extra_whitespaces ( s );
                std::string response = client.send ( s );
                std::stringstream response_stream ( response );
                cereal::BinaryInputArchive iarchive ( response_stream );
                QueryResult res;
                iarchive ( res );
                printQueryResult ( res );
            }
        }
        catch ( ResqlError& e ) {
            std::cout << e.message() << std::endl;
        }
    }

    client.disconnect();
}


void runInteractive ( Database&  db, 
                      DBConfig&  config ) {

    runStartup ( "startup.sql", db, config );
    while ( true ) {
        char* line = readline ( ">" );
        if ( !line ) break;
        if ( *line ) add_history ( line );
        std::string inputLine = line;
        if ( inputLine.size() == 0 ) continue;
        if ( checkExit ( line ) ) return;
        try {
            auto statements = expandExecStatements ( inputLine );
            for ( auto s : statements ) {
                QueryResult res = executeStatement ( s, db, config );
                printQueryResult ( res );
            }
        }
        catch ( ResqlError& e ) {
            std::cout << e.message() << std::endl;
        }
    }
}


int main ( int argc, char** argv ) {

    cxxopts::Options options ( "resql", "ReSQL database system" );
    options.add_options()
        ( "s,server",      "Start server" )
        ( "a,attach",      "Start client, connect to server", 
            cxxopts::value<std::string>()->implicit_value ( "localhost" ) )
        ( "p,port",        "Port for client/server", 
            cxxopts::value<std::uint16_t>()->default_value ( "4000" ) )
        ( "i,interactive", "Start command line (default)" )
        ( "h,help",        "Print usage" )
    ;
    auto opt = options.parse ( argc, argv );
    if ( opt.count ( "help" ) ) {
        std::cout << options.help();
        exit ( 0 );
    }
 
    /* decide on mode */
    ExecMode mode;
    if ( opt.count ( "server" ) ) mode = SERVER;
    else if ( opt.count ( "attach" ) ) mode = CLIENT;
    else mode = INTERACTIVE;

    /* run mode */
    if ( mode == INTERACTIVE || mode == SERVER ) {
        Database db;
        DBConfig config;
        if ( mode == SERVER ) {
            std::uint16_t port = opt ["port"].as<std::uint16_t>();
            runServer ( db, config, port );
        }
        else if ( mode == INTERACTIVE ) {
            runInteractive ( db, config );
        }
    } 
    else if ( mode == CLIENT ) {
        std::string   host = opt ["attach"].as<std::string>();
        std::uint16_t port = opt ["port"].as<std::uint16_t>();
        runClient ( host, port );
    }
    return 0;
}


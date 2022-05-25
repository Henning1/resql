/**
 * @file
 * Holds generated Flounder IR and code generation status.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once
#include <map>
#include <string>
#include <vector>

#include <sstream>
#include <inttypes.h>
#include <sys/mman.h>
#include <spawn.h>
#include <cstring>
#include <sys/wait.h>
#include <thread>

#include "flounder/flounder.h"
#include "flounder/translate.h"
#include "flounder/asm_emitter.h"
#include "asmjit/asmjit.h"

#include "util/Timer.h"
#include "qlib/qlib.h"
#include "RelationalContext.h"


/* debug flag for printing Flounder to stdout before execution */
//#define ALWAYS_PRINT


/* Pointer to environment variables for              *
 * spawn_thread (..) used when  machine code is      *
 * generated via nasm.                               */
extern char **environ;


/* Represents a configuration for the JIT  *
 * compilaton.                             */
struct JitConfig {

    JitConfig() = default;

    /* Show Flounder IR and result assembly */
    bool printAssembly = false;
    bool printFlounder = false;
    bool printPerformance = false;
    
    /* Number of threads used for execution */
    uint16_t numThreads = 1U;
    
    /* Emit machine code directly (true) or use nasm     *
     * assembler (false).                                */
    bool emitMachineCode = false;
    
    /* Apply optimzations to Flounder IR (currently unavailable) */
    bool optimizeFlounder = false;
    
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( printAssembly, printFlounder, printPerformance, numThreads, emitMachineCode, optimizeFlounder );
    } 
};


/* Represents execution details, such as peformance *
 * and details on code representations.             */
struct JitExecutionReport {
    
    JitExecutionReport() = default;

    JitConfig          config = JitConfig();
    std::string        printCode;
    uint64_t           numMachineInstructions = 0U;
    double             compilationTime = 0.0;
    double             executionTime = 0.0;
    double             nasmTime = 0.0;
    
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( config, printCode, numMachineInstructions, compilationTime, executionTime, nasmTime );
    } 
};


void showReport ( JitExecutionReport& report ) {
    JitConfig& config = report.config;
    if ( config.printAssembly || config.printFlounder ) {
        std::cout << report.printCode;
    }
    if ( config.printPerformance ) {
        if ( config.emitMachineCode ) {

            std::cout << "Emitted " << report.numMachineInstructions 
                      << " machine instructions. " << std::endl;
        }
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "compile: " << report.compilationTime << " ms" << std::endl;
        if ( !config.emitMachineCode ) {
            std::cout << "nasm:    " << report.nasmTime << " ms" << std::endl;
        }
        std::cout << "execute: " << report.executionTime << " ms" << std::endl;
    }
}


/* Contextual information for Flounder IR-based code *
 * generation. The class holds the generated code,   * 
 * Insertion points, symbol table, and more.         *
 *                                                   *
 * The provided functionality includes:              *
 *  - Construct IR code from ir_node* elements.      *
 *  - Compile to machine code and execute.           *
 */
class JitContextFlounder {

public:
    
    /* Contextual information that is rather relational  *
     * than related to code generation, e.g. the type    *
     * of symbols in the query.                          */ 
    RelationalContext rel;


    /* Indicates that all attributes should be retrieved *
     * as in 'SELECT * FROM ...'.                        */
    bool requestAll = false;


    /* Configuration and performance report */
    JitConfig config;
    JitExecutionReport report;


    /* Code frames and insertion points for overall code */
    ir_node* codeTree = NULL;
    ir_node* codeHeader = NULL;
    ir_node* codeFooter = NULL;


    /* code frames and insertion points per pipeline     */
    ir_node* insPipeHeader = NULL;
    ir_node* pipeHeader = NULL;
    ir_node* pipeFooter = NULL;


    /* Label that is used for jumps in several           *
     * operators to skip processing of the current tuple *
     * e.g. selection.                                   *
     *                                                   *
     * Assigned by the current loop in a pipeline e.g.   *
     * scan or probe.                                    */
    ir_node* labelNextTuple = NULL;
    

    /* Table with Flounder IR nodes that are assigned    *
     * to symbols in the query.                          */ 
    std::map<std::string, ir_node*> symbolTable;


    /* Set of vregs (ids) that hold symbols */
    std::set < int > vregSymbols;
            
    
    /* Emits binary representation of assembly via       *
     * asmjit.                                           */
    Emitter mCodeEmitter;
    

    /* JIT function that points to the binary from the   *
     * from the external assembler.                      */
    void (*nasmJitFunc)();
       
 
    /* size of the jit function */
    size_t funcSize;


    JitContextFlounder() : JitContextFlounder ( JitConfig() ) {};

 
    JitContextFlounder ( JitConfig config ) {
        
        this->config = config;

        /* initialize global variables from flounder library */
        allocateAllNodes();    
        ifId = 0;
        loopId = 0;
        vRegNum = 0;

        /* initialize global context from flounder */
        codeTree   = irRoot();
        codeHeader = irRoot();
        codeFooter = irRoot();

        insPipeHeader = codeTree->lastChild;
    }


    ~JitContextFlounder() {
        
        /* release global variables from flounder library */
        freeAllNodes();
    }


    size_t numThreads() {
        return config.numThreads;
    }


    void showReport () {
        ::showReport ( report );
    }


    void openPipeline () {
       insPipeHeader = codeTree->lastChild;
       pipeHeader    = irRoot();
       pipeFooter    = irRoot();
    }


    void closePipeline () {
        transferNodes ( codeTree, insPipeHeader, pipeHeader );
        transferNodes ( codeTree, codeTree->lastChild, pipeFooter );
    }


    void yield ( ir_node* node ) {
        addChild ( codeTree, node );
    }
    

    void comment ( std::string msg ) {
        addChild ( codeTree, commentLine ( msg.c_str() ) );
    }


    void comment ( char* msg ) {
        addChild ( codeTree, commentLine ( msg ) );
    }

    
    void comment ( const char* msg ) {
        addChild ( codeTree, commentLine ( msg ) );
    }


    void yieldPipeHead ( ir_node* node ) {
        addChild ( pipeHeader, node );
    }


    void yieldPipeFoot ( ir_node* node ) {
        addChild ( pipeFooter, node );
    }


    void yieldCodeHead ( ir_node* node ) {
        addChild ( codeHeader, node );
    }


    void yieldCodeFoot ( ir_node* node ) {
        addChild ( codeFooter, node );
    }


    ir_node* request ( ir_node* vr ) {
        yield ( ::request ( vr ) );
        return vr;
    }
 
    
    void clear ( ir_node* vr ) {
        yield ( ::clear ( vr ) );
    }


    ir_node* vregForType ( SqlType type, 
                           bool    explicit_=true ) {

        ir_node* res = nullptr; 

        switch ( type.tag ) {
            
            case SqlType::INT:
                res = vreg32 ( "IntAttribute" );
                if ( explicit_ ) request ( res );
                break;

            case SqlType::BIGINT:
                res = vreg64 ( "BigintAttribute" );
                if ( explicit_ ) request ( res );
                break;

            case SqlType::DECIMAL:
                res = vreg64 ( "DecimalAttribute" );
                if ( explicit_ ) request ( res );
                break;

            case SqlType::BOOL:
                res = vreg8 ( "BoolAttribute" );
                if ( explicit_ ) request ( res );
                break;

            case SqlType::DATE:
                res = vreg32 ( "DateAttribute" );
                if ( explicit_ ) request ( res );
                break;
            
            case SqlType::CHAR:
                if ( type.charSpec().num > 1 ) {
                    res = vreg64 ( "CharAttribute" );
                    if ( explicit_ ) request ( res );
                }
                else {
                    res = vreg8 ( "Char1Attribute" );
                    if ( explicit_ ) request ( res );
                }
                break;
            
            case SqlType::VARCHAR:
                res = vreg64 ( "VarcharAttribute" );
                if ( explicit_ ) request ( res );
                break;

            default:
                error_msg ( NOT_IMPLEMENTED, "vregForType(..) not implemented for datatype" );
        }
        return res;
    }


    void showSymbols ( ) {
        std::vector < std::string > table;
        table.push_back ( "symbol" );
        table.push_back ( "ir_node" );
        table.push_back ( "type" );
        for ( auto it = symbolTable.begin(); it != symbolTable.end(); ++it) {
            std::string symbol = it->first;
            ir_node*    node   = it->second;
            SqlType     type   = rel.symbolTypes [ symbol ];
            table.push_back ( symbol );
            table.push_back ( call_emit ( node ) );
            table.push_back ( serializeType ( type ) );
        }
        std::string sub = std::to_string ( symbolTable.size () ) + " symbols";
        printStringTable ( std::cout, table, 3, 1, sub );
    }


    void finishCode() {
        /* prepend header */
        transferNodes ( codeTree, nullptr, codeHeader );
        /* append footer */
        transferNodes ( codeTree, codeTree->lastChild, codeFooter );
        yield ( ret () );
    }
    

    void compile() {
        #ifdef ALWAYS_PRINT
        char* code;
        code = call_emit ( codeTree ); 
        printFormattedFlounder ( code, true, std::cerr );
        #endif

        /* translate to assembly */
        std::stringstream outs;
        report.config = config;
        Timer tEmit = Timer();
        finishCode();

        try {
            translateFlounderToMachineIR ( codeTree, 
                                           outs, 
                                           config.optimizeFlounder, 
                                           config.printFlounder, 
                                           config.printAssembly );
        }
        catch ( ResqlError& err ) {
            char* code;
            code = call_emit ( codeTree ); 
            std::cerr <<  "---------Error during translation of FLOUNDER IR ---------" << std::endl;
            std::cerr << err.message();
            std::cerr << "Flounder code so far:" << std::endl;
            printFormattedFlounder ( code, true, std::cerr );
            free ( code );
            throw err;
        }

        if ( config.emitMachineCode ) {
            /* emit binary representation */
            report.numMachineInstructions = mCodeEmitter.emit ( codeTree );
            report.compilationTime = tEmit.get();
        } else {
            /* emit asm characters */
            char* code = call_emit ( codeTree ); 
            report.compilationTime = tEmit.get();
            /* run external assembler to emit binary */
            Timer tNasm = Timer();
            nasmJitFunc = (void (*)()) execNasmAndLoad ( code );
            free ( code );
            report.nasmTime = tNasm.get();
        }
        report.printCode += outs.str();
    }
    
 
    void execute() {
        Timer tExec = Timer();

        /* execute binary query */
                auto threads = std::vector<std::thread>{};
                threads.resize(config.numThreads);
        for (auto thread_id = 0U; thread_id < config.numThreads; ++thread_id) {

            /* asmjit */
            if ( config.emitMachineCode ) {
                    threads[thread_id] = std::thread( &Emitter::execute, std::ref ( mCodeEmitter ) );
                }
                
            /* nasm */
            else {
                threads[thread_id] = std::thread ( nasmJitFunc );
            }
        }
        
                /* wait for threads to finish */
        for (auto& thread : threads) {
                    thread.join();
                }

        if ( !config.emitMachineCode ) {
            munmap ( (void*) nasmJitFunc, funcSize );
        }
        report.executionTime = tExec.get();
    }


    void* execNasmAndLoad ( char* code ) {
        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_ANONYMOUS | MAP_PRIVATE;
        char templateName[] = "jitnasmXXXXXX";
        int fd = mkstemp(templateName);
        FILE *assembly = fdopen(fd, "w");
        fprintf ( assembly, "%s", code );
        fclose(assembly);
        char command[128];
        sprintf(command, "nasm -o qjit.o %s", templateName);
        pid_t child_pid;
        char *argv[] = {"sh", "-c", command, NULL};
        posix_spawn ( &child_pid, "/bin/sh", NULL, NULL, argv, environ );
        waitpid ( child_pid, NULL, 0 );
        FILE *mcode = fopen ( "qjit.o", "rb" );
        fseek(mcode, 0L, SEEK_END);
        funcSize = ftell(mcode);
        fseek(mcode, 0L, SEEK_SET);
        void *buf = mmap(NULL, funcSize, prot, flags, -1, 0);
        if ( buf == MAP_FAILED ) {
            error_msg ( CODEGEN_ERROR, "mmap() failed" );
        } 
        fread ( buf, funcSize, 1, mcode );
        unlink(templateName);
        mprotect ( buf, funcSize, PROT_READ | PROT_EXEC );
        return buf;
    }
};



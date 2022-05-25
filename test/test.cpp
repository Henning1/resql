#include "test_datatypes.h"
#include "test_expressions.h"
#include "test_operators.h"
#include "test_queries.h"


size_t DataBlock::Size = 2 << 20;


void runTests() {
    
    std::cout << "Test Datatypes" << std::endl;  
    testDatatypes();
    std::cout << std::endl;

    std::cout << "Test Expressions" << std::endl;  
    testExpressions();
    std::cout << std::endl;

    std::cout << "Test Operators" << std::endl;  
    testOperators();
    std::cout << std::endl;
    
    std::cout << "Test Queries" << std::endl;  
    testQueries();
    std::cout << std::endl;
}


int main() {

    /* test with nasm assembler */
    testConfig.jit.emitMachineCode = false;
    testConfig.writeResultsToFile = true;
    runTests();

    /* test with asmjit */
    testConfig.jit.emitMachineCode = true;
    runTests();
    
    /* test with small blocks (1024 bytes) */
    DataBlock::Size = 2 << 10;
    runTests();
    
    /* test parallel */
    testConfig.jit.numThreads = 8;
    runTests();

    return 0;
}

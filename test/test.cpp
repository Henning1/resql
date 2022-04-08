#include "test_datatypes.h"
#include "test_expressions.h"
#include "test_operators.h"
#include "test_queries.h"


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

    return 0;
}

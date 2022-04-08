/**
 * @file
 * Holds query compilation context that is independent from the IR.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <map>
#include <string>


class RelationalContext {
public:
    int nameIdx = 0;
    int innerScanCount= 0;
    int exprIdGen = 1; // start with 1 because 0 means undefined 
    std::map < std::string, SqlType > symbolTypes;
};


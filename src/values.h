/**
 * @file
 * Handle SQL values in non-JIT code.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <vector>
#include <iostream>
//#include "dbdata.h"
#include "types.h"
#include "util/defs.h"


union SqlValue {
    uint32_t       dateData;
    unsigned char  boolData;
    int32_t        intData; 
    int64_t        bigintData; 
    int64_t        decimalData; 
    double         floatData;
    char*          charData;
    char*          varcharData;
};


const SqlValue emptySqlValue = { 0 };
            

static std::string serializeSqlValue ( SqlValue val, SqlType type ) {
                
    std::ostringstream ser;
   
    switch ( type.tag ) {

        case SqlType::CHAR: {
            size_t i = 0;
            while ( val.charData[i] != '\0' ) {
                ser << val.charData[i];
                i++;
            }
            /* fill in spaces */
            for ( ; i < type.charSpec().num; i++ ) {
                ser << " "; 
            }
            break;
        }

        case SqlType::VARCHAR: {
            ser << val.varcharData;
            break;
        }

        case SqlType::DATE: {
            unsigned int v = val.dateData;
            
            // year
            ser << v / 10000 << "/";
            ser.fill( '0' );    
            ser.width( 2 );
            
            // month
            ser << v / 100 % 100;
            ser.width( 0 );
            ser << "/";

            // day
            ser.width( 2 );
            ser << v % 100;
            ser.width( 0 );
            break;
        }
        
        case SqlType::INT: {
            int32_t b = val.intData;
            ser << b;
            break;
        }

        case SqlType::BIGINT: {
            int64_t b = val.bigintData;
            ser << b;
            break;
        }
        
        case SqlType::FLOAT: {
            double b = val.floatData;
            ser << b;
            break;
        }

        case SqlType::BOOL: {
            unsigned char b = val.boolData;
            if ( b ) 
                ser << "true";
            else 
                ser << "false";
            break;
        }

        case SqlType::DECIMAL: {
            //bool negative = false;
            int64_t v = val.decimalData;
            if ( val.decimalData < 0 ) {
                //negative = true;
                v = v * -1;
                ser << "-";
            }
            std::string dec = std::to_string ( v );
            if ( dec.length() <= type.decimalSpec().scale ) {
                ser << "0.";
                for ( unsigned int i=dec.length(); i<type.decimalSpec().scale; i++ ) {
                    ser << "0";
                }
            } 
            else if ( type.decimalSpec().scale > 0 ) {
                dec.insert ( dec.length() - type.decimalSpec().scale, "." );
            }
            ser << dec;
            break;
        }
        default:
            error_msg ( NOT_IMPLEMENTED, "serializeSqlValue(..) not implemented for datatype." );
    }

    return ser.str();
}






namespace ValueMoves {

    static void writeString ( char*   string, 
                              char*   addr, 
                              size_t  max ) {

        char* from = string;
        char* to   = (char*) addr;
        size_t i = 0;
        for ( ; i < max; i++ ) {
            to[i] = from[i];
            if ( from[i] == '\0' ) break;
        }
        to[i] = '\0';
    } 


    static void toAddress ( Data* addr, SqlValue val, SqlType type ) {

        switch ( type.tag ) {

            case SqlType::DATE:
                *((uint32_t *)addr) = val.dateData;
                break;
            
            case SqlType::BOOL:
                *((unsigned char *)addr) = val.boolData;
                break;
            
            case SqlType::INT:
                *((int32_t *)addr) = val.intData;
                break;

            case SqlType::BIGINT:
                *((int64_t *)addr) = val.bigintData;
                break;

            case SqlType::DECIMAL:
                *((int64_t *)addr) = val.decimalData;
                break;
            
            case SqlType::FLOAT:
                *((double *)addr) = val.floatData;
                break;
            
            case SqlType::CHAR: {
                /* +1 for terminating '\0' */
                writeString ( val.charData, 
                              (char*) addr, 
                              type.charSpec().num );
                break;
            }

            case SqlType::VARCHAR: {
                /* +1 for terminating '\0' */
                writeString ( val.varcharData, 
                              (char*) addr, 
                              type.varcharSpec().num );
                break;
            }

            default:
                std::cerr << "toAddress(..) not implemented for datatype." << std::endl;
        }
    }


    static SqlValue fromAddress ( SqlType type, Data* addr ) {
        switch ( type.tag ) {

            case SqlType::DATE:
                return { .dateData = *((uint32_t *)addr) };
            
            case SqlType::BOOL:
                return { .boolData = *((unsigned char *)addr) };

            case SqlType::INT:
                return { .intData = *((int32_t *)addr) };

            case SqlType::BIGINT:
                return { .bigintData = *((int64_t *)addr) };

            case SqlType::DECIMAL:
                return { .decimalData = *((int64_t *)addr) };
            
            case SqlType::FLOAT:
                return { .floatData = *((double *)addr) };
            
            case SqlType::CHAR:
                return { .charData = (char *)addr };
            
            case SqlType::VARCHAR:
                return { .varcharData = (char *)addr };
            
            default:
                std::cerr << "fromAddress(..) not implemented for datatype." << std::endl;
                return emptySqlValue; 
        }
    } 
}




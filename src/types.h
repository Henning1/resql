/**
 * @file
 * Representation for SQL types.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <variant>
#include <iostream>
#include <sstream>
#include <cstddef>
#include <cstring>

#include <cereal/types/variant.hpp>

struct EmptySpec {

    uint8_t foo;

    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( foo );
    }
};


struct DecimalSpec {

    uint8_t precision;
    uint8_t scale;
    
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( precision, scale );
    }
};


struct CharSpec {

    size_t num;
    
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( num );
    }
};


struct VarcharSpec {
    size_t num;
    
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( num );
    }
};


struct SqlType {
    
    // Tag: Type category of the SQL value
    // 
    // - The enum value encodes the type precedence.
    //
    enum Tag {
        VARCHAR,    // lowest precedence
        CHAR,  
        BOOL,
        INT,
        BIGINT,
        DECIMAL,
        FLOAT,
        DATE,    // highest precedence
        NT       // (no type)
    };
    Tag tag;  

    SqlType () : tag(NT) {};

    SqlType ( Tag tag ) : tag(tag) {};
    
    SqlType ( Tag tag, DecimalSpec spec ) : tag(tag), spec(spec) {};

    SqlType ( Tag tag, CharSpec spec ) : tag(tag), spec(spec) {};
    
    SqlType ( Tag tag, VarcharSpec spec ) : tag(tag), spec(spec) {};

    // Type specification
    //
    // - For types with additional parameters the concrete type 
    //   is specified by the type specification.
    std::variant < EmptySpec, DecimalSpec, CharSpec, VarcharSpec > spec;

    DecimalSpec& decimalSpec () { return std::get<DecimalSpec> ( spec ); }
    CharSpec& charSpec () { return std::get<CharSpec> ( spec ); }
    VarcharSpec& varcharSpec () { return std::get<VarcharSpec> ( spec ); }
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        //ar ( tag, emptySpec, decimalSpec, charSpec, varcharSpec );
        ar ( tag, spec );
    }
};


const char* typeTagNames[] {
        "VARCHAR",
        "CHAR",
        "BOOL",
        "INT",
        "BIGINT",
        "DECIMAL",
        "FLOAT",
        "DATE",
        "" // NT (no type)
};


std::string serializeType ( SqlType type ) {

    std::stringstream ss;
    ss << typeTagNames [type.tag];

    switch ( type.tag ) { 
        case SqlType::DECIMAL:
            ss << "(" 
               << std::to_string ( type.decimalSpec().precision )
               << ","
               << std::to_string ( type.decimalSpec().scale )
               << ")";
            break;
        case SqlType::CHAR:
            ss << "(" 
               << type.charSpec().num
               << ")";
            break;
        case SqlType::VARCHAR:
            ss << "(" 
               << type.varcharSpec().num
               << ")";
            break;
        default:
            /* Nothing to do for types without parameters. *
             * Just add the tag to typeTagNames[].         */
            break;
    }
    return ss.str();
}


bool equalTypes ( SqlType& a, SqlType& b ) {

    if ( a.tag != b.tag ) {
        return false;
    }

    if ( a.tag == SqlType::DECIMAL ) {
        return ( a.decimalSpec().precision == b.decimalSpec().precision ) &&
               ( a.decimalSpec().scale     == b.decimalSpec().scale );
    }
    
    if ( a.tag == SqlType::CHAR ) {
        return ( a.charSpec().num == b.charSpec().num );
    }
    
    if ( a.tag == SqlType::VARCHAR ) {
        return ( a.varcharSpec().num == b.varcharSpec().num );
    }
                
    return true; 
}



namespace TypeInit {
   
    /* types without parameters */ 
    static SqlType INT () { return { SqlType ( SqlType::INT ) }; }

    static SqlType BIGINT () { return { SqlType ( SqlType::BIGINT ) }; }

    static SqlType DATE () { return { SqlType ( SqlType::DATE ) }; }
    
    static SqlType BOOL () { return { SqlType ( SqlType::BOOL ) }; }

    static SqlType FLOAT ( ) { return { SqlType ( SqlType::FLOAT ) }; } 
    
    /* types with parameters */ 
    static SqlType DECIMAL ( uint8_t precision, uint8_t scale ) { 
        return SqlType ( SqlType::DECIMAL, DecimalSpec { precision, scale } ); 
    }
    
    static SqlType DECIMAL ( DecimalSpec spec ) { 
        return SqlType ( SqlType::DECIMAL, spec ); 
    }
    
    static SqlType CHAR ( size_t len ) { 
        return SqlType ( SqlType::CHAR, CharSpec { len } ); 
    }
    
    static SqlType VARCHAR ( size_t len ) { 
        return SqlType ( SqlType::VARCHAR, VarcharSpec { len } );
    }
}


const SqlType emptySqlType = SqlType ( SqlType::NT );



static int getSizeInTuple ( SqlType t, bool stringsByVal ) {
    switch ( t.tag ) {

        case SqlType::BOOL:
            return 1;

        case SqlType::DATE:
            return 4;

        case SqlType::DECIMAL:
            return 8;

        case SqlType::INT:
            return 4;

        case SqlType::BIGINT:
            return 8;
        
        case SqlType::FLOAT:
            return 8;

        case SqlType::CHAR: {
            if ( t.charSpec().num == 1 ) return 2;
            if ( stringsByVal ) {
                /* always store terminating '\0' for strings */
                size_t len = t.charSpec().num + 1;
                return len;
            }
            else {
                return 8;
            }
        }
         
        case SqlType::VARCHAR: {
            if ( stringsByVal ) {
                /* always store terminating '\0' for strings */
                size_t len = t.varcharSpec().num + 1;
                return len;
            }
            else {
                return 8;
            }
        }

        case SqlType::NT:
            error_msg ( ELEMENT_NOT_FOUND, "getSizeInTuple(..) for undefined type." );
            return 0;
    }
}


template<typename T>
std::int8_t compare ( void* left, 
                      void* right ) {

  const auto l = (*reinterpret_cast<T*>(left));
  const auto r = (*reinterpret_cast<T*>(right));

  if(l < r) {
      return -1;
  }

  if(l > r) {
      return 1;
  }

  return 0;
}


template < SqlType::Tag T >
std::int8_t compare ( void* left, 
                      void* right ) {
  return 0;
}


template<>
std::int8_t compare < SqlType::BIGINT > ( void* left, 
                                          void* right) {

    return compare < long long int > (left, right);
}


template<>
std::int8_t compare < SqlType::DECIMAL > ( void* left, 
                                           void* right) {

    return compare < long long int > ( left, right );
}


template<>
std::int8_t compare < SqlType::INT > ( void* left, 
                                       void* right) {

    return compare < int > ( left, right );
}


template<>
std::int8_t compare < SqlType::BOOL > ( void* left, 
                                        void* right) {

    return compare < bool > ( left, right );
}


template<>
std::int8_t compare < SqlType::FLOAT > ( void* left, 
                                         void* right) {

    return compare < double > ( left, right );
}


template<>
std::int8_t compare < SqlType::DATE > ( void* left, 
                                        void* right ) {

    return compare < int > ( left, right );
}


template<>
std::int8_t compare<SqlType::CHAR> ( void* left, 
                                     void* right) {

    return std::strcmp ( reinterpret_cast < char* > ( left ), 
                         reinterpret_cast < char* > ( right ) );
}


template<>
std::int8_t compare < SqlType::VARCHAR > ( void* left, 
                                           void* right ) {

  return std::strcmp( reinterpret_cast < char* > ( left ), 
                      reinterpret_cast < char* > ( right ) );
}

